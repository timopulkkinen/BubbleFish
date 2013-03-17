// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/local_omnibox_popup_source.h"

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/string_util.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_client.h"
#include "googleurl/src/gurl.h"
#include "grit/browser_resources.h"
#include "net/url_request/url_request.h"

namespace {

const char kHTMLFilename[] = "local-omnibox-popup.html";
const char kJsFilename[] = "local-omnibox-popup.js";
const char kCSSFilename[] = "local-omnibox-popup.css";

}  // namespace

LocalOmniboxPopupSource::LocalOmniboxPopupSource() {
}

LocalOmniboxPopupSource::~LocalOmniboxPopupSource() {
}

std::string LocalOmniboxPopupSource::GetSource() {
  return chrome::kChromeSearchLocalOmniboxPopupHost;
}

void LocalOmniboxPopupSource::StartDataRequest(
    const std::string& path,
    bool is_incognito,
    const content::URLDataSource::GotDataCallback& callback) {
  int identifier = -1;
  if (path == kHTMLFilename) {
    identifier = IDR_LOCAL_OMNIBOX_POPUP_HTML;
  } else if (path == kJsFilename) {
    identifier = IDR_LOCAL_OMNIBOX_POPUP_JS;
  } else if (path == kCSSFilename) {
    identifier = IDR_LOCAL_OMNIBOX_POPUP_CSS;
  } else {
    callback.Run(NULL);
    return;
  }

  scoped_refptr<base::RefCountedStaticMemory> response(
      content::GetContentClient()->GetDataResourceBytes(identifier));
  callback.Run(response);
}

std::string LocalOmniboxPopupSource::GetMimeType(
    const std::string& path) const {
  if (path == kHTMLFilename)
    return "text/html";
  if (path == kJsFilename)
    return "application/javascript";
  if (path == kCSSFilename)
    return "text/css";
  return "";
}

bool LocalOmniboxPopupSource::ShouldServiceRequest(
    const net::URLRequest* request) const {
  DCHECK(request->url().host() == chrome::kChromeSearchLocalOmniboxPopupHost);

  if (request->url().SchemeIs(chrome::kChromeSearchScheme)) {
    DCHECK(StartsWithASCII(request->url().path(), "/", true));
    std::string filename = request->url().path().substr(1);
    return filename == kHTMLFilename || filename == kJsFilename ||
        filename == kCSSFilename;
  }
  return false;
}
