// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/session_commands.h"

#include <list>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"  // For CHECK macros.
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/test/chromedriver/basic_types.h"
#include "chrome/test/chromedriver/chrome/chrome.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/web_view.h"
#include "chrome/test/chromedriver/session.h"
#include "chrome/test/chromedriver/session_map.h"

namespace {

const char kWindowHandlePrefix[] = "CDwindow-";

std::string WebViewIdToWindowHandle(const std::string& web_view_id) {
  return kWindowHandlePrefix + web_view_id;
}

bool WindowHandleToWebViewId(const std::string& window_handle,
                             std::string* web_view_id) {
  if (window_handle.find(kWindowHandlePrefix) != 0u)
    return false;
  *web_view_id = window_handle.substr(
      std::string(kWindowHandlePrefix).length());
  return true;
}

void ExecuteOnSessionThread(
    const SessionCommand& command,
    Session* session,
    const base::DictionaryValue* params,
    scoped_ptr<base::Value>* value,
    Status* status,
    base::WaitableEvent* event) {
  *status = command.Run(session, *params, value);
  event->Signal();
}

}  // namespace

Status ExecuteSessionCommand(
    SessionMap* session_map,
    const SessionCommand& command,
    const base::DictionaryValue& params,
    const std::string& session_id,
    scoped_ptr<base::Value>* out_value,
    std::string* out_session_id) {
  *out_session_id = session_id;
  scoped_refptr<SessionAccessor> session_accessor;
  if (!session_map->Get(session_id, &session_accessor))
    return Status(kNoSuchSession, session_id);
  scoped_ptr<base::AutoLock> session_lock;
  Session* session = session_accessor->Access(&session_lock);
  if (!session)
    return Status(kNoSuchSession, session_id);

  Status status(kUnknownError);
  base::WaitableEvent event(false, false);
  session->thread.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&ExecuteOnSessionThread, command, session,
                 &params, out_value, &status, &event));
  event.Wait();
  if (status.IsError() && session->chrome)
    status.AddDetails("Session info: chrome=" + session->chrome->GetVersion());
  // Delete the session, because concurrent requests might hold a reference to
  // the SessionAccessor already.
  if (!session_map->Has(session_id))
    session_accessor->DeleteSession();
  return status;
}

Status ExecuteGetSessionCapabilities(
    SessionMap* session_map,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  value->reset(session->capabilities->DeepCopy());
  return Status(kOk);
}

Status ExecuteQuit(
    SessionMap* session_map,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  CHECK(session_map->Remove(session->id));
  return session->chrome->Quit();
}

Status ExecuteGetCurrentWindowHandle(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  value->reset(new StringValue(WebViewIdToWindowHandle(web_view->GetId())));
  return Status(kOk);
}

Status ExecuteClose(
    SessionMap* session_map,
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::list<WebView*> web_views;
  Status status = session->chrome->GetWebViews(&web_views);
  if (status.IsError())
    return status;
  bool is_last_web_view = web_views.size() == 1u;
  web_views.clear();

  WebView* web_view = NULL;
  status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->Close();
  if (status.IsError())
    return status;

  status = session->chrome->GetWebViews(&web_views);
  if ((status.code() == kChromeNotReachable && is_last_web_view) ||
      (status.IsOk() && web_views.empty())) {
    CHECK(session_map->Remove(session->id));
    return Status(kOk);
  }
  return status;
}

Status ExecuteGetWindowHandles(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::list<WebView*> web_views;
  Status status = session->chrome->GetWebViews(&web_views);
  if (status.IsError())
    return status;
  base::ListValue window_ids;
  for (std::list<WebView*>::const_iterator it = web_views.begin();
       it != web_views.end(); ++it) {
    window_ids.AppendString(WebViewIdToWindowHandle((*it)->GetId()));
  }
  value->reset(window_ids.DeepCopy());
  return Status(kOk);
}

Status ExecuteSwitchToWindow(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string name;
  if (!params.GetString("name", &name) || name.empty())
    return Status(kUnknownError, "'name' must be a nonempty string");

  std::list<WebView*> web_views;
  Status status = session->chrome->GetWebViews(&web_views);
  if (status.IsError())
    return status;

  WebView* web_view = NULL;

  std::string web_view_id;
  if (WindowHandleToWebViewId(name, &web_view_id)) {
    // Check if any web_view matches |web_view_id|.
    for (std::list<WebView*>::const_iterator it = web_views.begin();
         it != web_views.end(); ++it) {
      if ((*it)->GetId() == web_view_id) {
        web_view = *it;
        break;
      }
    }
  } else {
    // Check if any of the tab window names match |name|.
    const char* kGetWindowNameScript = "function() { return window.name; }";
    base::ListValue args;
    for (std::list<WebView*>::const_iterator it = web_views.begin();
         it != web_views.end(); ++it) {
      scoped_ptr<base::Value> result;
      status = (*it)->CallFunction("", kGetWindowNameScript, args, &result);
      if (status.IsError())
        return status;
      std::string window_name;
      if (!result->GetAsString(&window_name))
        return Status(kUnknownError, "failed to get window name");
      if (window_name == name) {
        web_view = *it;
        break;
      }
    }
  }

  if (!web_view)
    return Status(kNoSuchWindow);
  session->window = web_view->GetId();
  session->SwitchToTopFrame();
  session->mouse_position = WebPoint(0, 0);
  return Status(kOk);
}

Status ExecuteSetTimeout(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  int ms;
  if (!params.GetInteger("ms", &ms) || ms < 0)
    return Status(kUnknownError, "'ms' must be a non-negative integer");
  std::string type;
  if (!params.GetString("type", &type))
    return Status(kUnknownError, "'type' must be a string");
  if (type == "implicit")
    session->implicit_wait = ms;
  else if (type == "script")
    session->script_timeout = ms;
  else if (type == "page load")
    session->page_load_timeout = ms;
  else
    return Status(kUnknownError, "unknown type of timeout:" + type);
  return Status(kOk);
}

Status ExecuteSetScriptTimeout(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  int ms;
  if (!params.GetInteger("ms", &ms) || ms < 0)
    return Status(kUnknownError, "'ms' must be a non-negative integer");
  session->script_timeout = ms;
  return Status(kOk);
}

Status ExecuteImplicitlyWait(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  int ms;
  if (!params.GetInteger("ms", &ms) || ms < 0)
    return Status(kUnknownError, "'ms' must be a non-negative integer");
  session->implicit_wait = ms;
  return Status(kOk);
}

Status ExecuteGetAlert(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  bool is_open;
  Status status = session->chrome->IsJavaScriptDialogOpen(&is_open);
  if (status.IsError())
    return status;
  value->reset(base::Value::CreateBooleanValue(is_open));
  return Status(kOk);
}

Status ExecuteGetAlertText(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string message;
  Status status = session->chrome->GetJavaScriptDialogMessage(&message);
  if (status.IsError())
    return status;
  value->reset(base::Value::CreateStringValue(message));
  return Status(kOk);
}

Status ExecuteSetAlertValue(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  std::string text;
  if (!params.GetString("text", &text))
    return Status(kUnknownError, "missing or invalid 'text'");

  bool is_open;
  Status status = session->chrome->IsJavaScriptDialogOpen(&is_open);
  if (status.IsError())
    return status;
  if (!is_open)
    return Status(kNoAlertOpen);

  session->prompt_text = text;
  return Status(kOk);
}

Status ExecuteAcceptAlert(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  Status status = session->chrome->HandleJavaScriptDialog(
      true, session->prompt_text);
  session->prompt_text = "";
  return status;
}

Status ExecuteDismissAlert(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  Status status = session->chrome->HandleJavaScriptDialog(
      false, session->prompt_text);
  session->prompt_text = "";
  return status;
}

Status ExecuteIsLoading(
    Session* session,
    const base::DictionaryValue& params,
    scoped_ptr<base::Value>* value) {
  WebView* web_view = NULL;
  Status status = session->GetTargetWindow(&web_view);
  if (status.IsError())
    return status;

  status = web_view->ConnectIfNecessary();
  if (status.IsError())
    return status;

  bool is_pending;
  status = web_view->IsPendingNavigation(
      session->GetCurrentFrameId(), &is_pending);
  if (status.IsError())
    return status;
  value->reset(new base::FundamentalValue(is_pending));
  return Status(kOk);
}
