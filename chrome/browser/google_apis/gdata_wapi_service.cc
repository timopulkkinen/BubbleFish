// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/gdata_wapi_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/message_loop_proxy.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/values.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_operations.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_url_generator.h"
#include "chrome/browser/google_apis/operation_runner.h"
#include "chrome/browser/google_apis/time_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/url_util.h"

using content::BrowserThread;

namespace google_apis {

namespace {

// OAuth2 scopes for the documents API.
const char kDocsListScope[] = "https://docs.google.com/feeds/";
const char kSpreadsheetsScope[] = "https://spreadsheets.google.com/feeds/";
const char kUserContentScope[] = "https://docs.googleusercontent.com/";
const char kDriveAppsScope[] = "https://www.googleapis.com/auth/drive.apps";

// The resource ID for the root directory for WAPI is defined in the spec:
// https://developers.google.com/google-apps/documents-list/
const char kWapiRootDirectoryResourceId[] = "folder:root";

// Parses the JSON value to ResourceList.
scoped_ptr<ResourceList> ParseResourceListOnBlockingPool(
    scoped_ptr<base::Value> value) {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(value);

  return ResourceList::ExtractAndParse(*value);
}

// Runs |callback| with |error| and |value|, but replace the error code with
// GDATA_PARSE_ERROR, if there was a parsing error.
void DidParseResourceListOnBlockingPool(
    const GetResourceListCallback& callback,
    GDataErrorCode error,
    scoped_ptr<ResourceList> resource_list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // resource_list being NULL indicates there was a parsing error.
  if (!resource_list)
    error = GDATA_PARSE_ERROR;

  callback.Run(error, resource_list.Pass());
}

// Parses the JSON value to ResourceList on the blocking pool and runs
// |callback| on the UI thread once parsing is done.
void ParseResourceListAndRun(const GetResourceListCallback& callback,
                             GDataErrorCode error,
                             scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, scoped_ptr<ResourceList>());
    return;
  }

  base::PostTaskAndReplyWithResult(
      BrowserThread::GetBlockingPool(),
      FROM_HERE,
      base::Bind(&ParseResourceListOnBlockingPool, base::Passed(&value)),
      base::Bind(&DidParseResourceListOnBlockingPool, callback, error));
}

// Parses the JSON value to ResourceEntry runs |callback|.
void ParseResourceEntryAndRun(const GetResourceEntryCallback& callback,
                              GDataErrorCode error,
                              scoped_ptr<base::Value> value) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!value) {
    callback.Run(error, scoped_ptr<ResourceEntry>());
    return;
  }

  // Parsing ResourceEntry is cheap enough to do on UI thread.
  scoped_ptr<ResourceEntry> entry =
      google_apis::ResourceEntry::ExtractAndParse(*value);
  if (!entry) {
    callback.Run(GDATA_PARSE_ERROR, scoped_ptr<ResourceEntry>());
    return;
  }

  callback.Run(error, entry.Pass());
}

// Extracts the open link url from the JSON Feed. Used by AuthorizeApp().
void ExtractOpenLinkAndRun(const std::string app_id,
                           const AuthorizeAppCallback& callback,
                           GDataErrorCode error,
                           scoped_ptr<ResourceEntry> entry) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Entry not found in the feed.
  if (!entry) {
    callback.Run(error, GURL());
    return;
  }

  const ScopedVector<google_apis::Link>& resource_links = entry->links();
  GURL open_link;
  for (size_t i = 0; i < resource_links.size(); ++i) {
    if (resource_links[i]->type() == google_apis::Link::LINK_OPEN_WITH &&
        resource_links[i]->app_id() == app_id) {
        open_link = resource_links[i]->href();
        break;
    }
  }

  callback.Run(error, open_link);
}

void ParseAboutResourceAndRun(
    const GetAboutResourceCallback& callback,
    GDataErrorCode error,
    scoped_ptr<AccountMetadata> account_metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<AboutResource> about_resource;
  if (account_metadata) {
    about_resource = AboutResource::CreateFromAccountMetadata(
        *account_metadata, kWapiRootDirectoryResourceId);
  }

  callback.Run(error, about_resource.Pass());
}

void ParseAppListAndRun(
    const GetAppListCallback& callback,
    GDataErrorCode error,
    scoped_ptr<AccountMetadata> account_metadata) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<AppList> app_list;
  if (account_metadata) {
    app_list = AppList::CreateFromAccountMetadata(*account_metadata);
  }

  callback.Run(error, app_list.Pass());
}

}  // namespace

GDataWapiService::GDataWapiService(
    net::URLRequestContextGetter* url_request_context_getter,
    const GURL& base_url,
    const std::string& custom_user_agent)
    : url_request_context_getter_(url_request_context_getter),
      runner_(NULL),
      url_generator_(base_url),
      custom_user_agent_(custom_user_agent) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataWapiService::~GDataWapiService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (runner_.get()) {
    runner_->operation_registry()->RemoveObserver(this);
    runner_->auth_service()->RemoveObserver(this);
  }
}

AuthService* GDataWapiService::auth_service_for_testing() {
  return runner_->auth_service();
}

void GDataWapiService::Initialize(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  std::vector<std::string> scopes;
  scopes.push_back(kDocsListScope);
  scopes.push_back(kSpreadsheetsScope);
  scopes.push_back(kUserContentScope);
  // Drive App scope is required for even WAPI v3 apps access.
  scopes.push_back(kDriveAppsScope);
  runner_.reset(new OperationRunner(profile,
                                    url_request_context_getter_,
                                    scopes,
                                    custom_user_agent_));
  runner_->Initialize();

  runner_->auth_service()->AddObserver(this);
  runner_->operation_registry()->AddObserver(this);
}

void GDataWapiService::AddObserver(DriveServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void GDataWapiService::RemoveObserver(DriveServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool GDataWapiService::CanStartOperation() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return HasRefreshToken();
}

void GDataWapiService::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  runner_->CancelAll();
}

bool GDataWapiService::CancelForFilePath(const base::FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return operation_registry()->CancelForFilePath(file_path);
}

OperationProgressStatusList GDataWapiService::GetProgressStatusList() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return operation_registry()->GetProgressStatusList();
}

std::string GDataWapiService::GetRootResourceId() const {
  return kWapiRootDirectoryResourceId;
}

void GDataWapiService::GetResourceList(
    const GURL& url,
    int64 start_changestamp,
    const std::string& search_query,
    bool shared_with_me,
    const std::string& directory_resource_id,
    const GetResourceListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Drive V2 API defines changestamp in int64, while DocumentsList API uses
  // int32. This narrowing should not cause any trouble.
  runner_->StartOperationWithRetry(
      new GetResourceListOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          url,
          static_cast<int>(start_changestamp),
          search_query,
          shared_with_me,
          directory_resource_id,
          base::Bind(&ParseResourceListAndRun, callback)));
}

void GDataWapiService::GetResourceEntry(
    const std::string& resource_id,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetResourceEntryOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          resource_id,
          base::Bind(&ParseResourceEntryAndRun, callback)));
}

void GDataWapiService::GetAccountMetadata(
    const GetAccountMetadataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetAccountMetadataOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          callback,
          true));  // Include installed apps.
}

void GDataWapiService::GetAboutResource(
    const GetAboutResourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetAccountMetadataOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::Bind(&ParseAboutResourceAndRun, callback),
          false));  // Exclude installed apps.
}

void GDataWapiService::GetAppList(const GetAppListCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetAccountMetadataOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::Bind(&ParseAppListAndRun, callback),
          true));  // Include installed apps.
}

void GDataWapiService::DownloadFile(
    const base::FilePath& virtual_path,
    const base::FilePath& local_cache_path,
    const GURL& download_url,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!download_action_callback.is_null());
  // get_content_callback may be null.

  runner_->StartOperationWithRetry(
      new DownloadFileOperation(operation_registry(),
                                url_request_context_getter_,
                                download_action_callback,
                                get_content_callback,
                                download_url,
                                virtual_path,
                                local_cache_path));
}

void GDataWapiService::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new DeleteResourceOperation(operation_registry(),
                                  url_request_context_getter_,
                                  url_generator_,
                                  callback,
                                  resource_id,
                                  etag));
}

void GDataWapiService::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new CreateDirectoryOperation(operation_registry(),
                                   url_request_context_getter_,
                                   url_generator_,
                                   base::Bind(&ParseResourceEntryAndRun,
                                              callback),
                                   parent_resource_id,
                                   directory_name));
}

void GDataWapiService::CopyHostedDocument(
    const std::string& resource_id,
    const std::string& new_name,
    const GetResourceEntryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new CopyHostedDocumentOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::Bind(&ParseResourceEntryAndRun, callback),
          resource_id,
          new_name));
}

void GDataWapiService::RenameResource(
    const std::string& resource_id,
    const std::string& new_name,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new RenameResourceOperation(operation_registry(),
                                  url_request_context_getter_,
                                  url_generator_,
                                  callback,
                                  resource_id,
                                  new_name));
}

void GDataWapiService::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new AddResourceToDirectoryOperation(operation_registry(),
                                          url_request_context_getter_,
                                          url_generator_,
                                          callback,
                                          parent_resource_id,
                                          resource_id));
}

void GDataWapiService::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new RemoveResourceFromDirectoryOperation(operation_registry(),
                                               url_request_context_getter_,
                                               url_generator_,
                                               callback,
                                               parent_resource_id,
                                               resource_id));
}

void GDataWapiService::InitiateUploadNewFile(
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!parent_resource_id.empty());

  runner_->StartOperationWithRetry(
      new InitiateUploadNewFileOperation(operation_registry(),
                                         url_request_context_getter_,
                                         url_generator_,
                                         callback,
                                         drive_file_path,
                                         content_type,
                                         content_length,
                                         parent_resource_id,
                                         title));
}

void GDataWapiService::InitiateUploadExistingFile(
    const base::FilePath& drive_file_path,
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const std::string& etag,
    const InitiateUploadCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(!resource_id.empty());

  runner_->StartOperationWithRetry(
      new InitiateUploadExistingFileOperation(operation_registry(),
                                              url_request_context_getter_,
                                              url_generator_,
                                              callback,
                                              drive_file_path,
                                              content_type,
                                              content_length,
                                              resource_id,
                                              etag));
}

void GDataWapiService::ResumeUpload(
    UploadMode upload_mode,
    const base::FilePath& drive_file_path,
    const GURL& upload_url,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    const scoped_refptr<net::IOBuffer>& buf,
    const UploadRangeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new ResumeUploadOperation(operation_registry(),
                                url_request_context_getter_,
                                callback,
                                upload_mode,
                                drive_file_path,
                                upload_url,
                                start_position,
                                end_position,
                                content_length,
                                content_type,
                                buf));
}

void GDataWapiService::GetUploadStatus(
    UploadMode upload_mode,
    const base::FilePath& drive_file_path,
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new GetUploadStatusOperation(operation_registry(),
                                   url_request_context_getter_,
                                   callback,
                                   upload_mode,
                                   drive_file_path,
                                   upload_url,
                                   content_length));
}

void GDataWapiService::AuthorizeApp(const std::string& resource_id,
                                    const std::string& app_id,
                                    const AuthorizeAppCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  runner_->StartOperationWithRetry(
      new AuthorizeAppOperation(
          operation_registry(),
          url_request_context_getter_,
          url_generator_,
          base::Bind(&ParseResourceEntryAndRun,
                     base::Bind(&ExtractOpenLinkAndRun, app_id, callback)),
          resource_id,
          app_id));
}

bool GDataWapiService::HasAccessToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasAccessToken();
}

bool GDataWapiService::HasRefreshToken() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return runner_->auth_service()->HasRefreshToken();
}

void GDataWapiService::ClearAccessToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return runner_->auth_service()->ClearAccessToken();
}

void GDataWapiService::ClearRefreshToken() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return runner_->auth_service()->ClearRefreshToken();
}

OperationRegistry* GDataWapiService::operation_registry() const {
  return runner_->operation_registry();
}

void GDataWapiService::OnOAuth2RefreshTokenChanged() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (CanStartOperation()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnReadyToPerformOperations());
  } else if (!HasRefreshToken()) {
    FOR_EACH_OBSERVER(
        DriveServiceObserver, observers_, OnRefreshTokenInvalid());
  }
}

void GDataWapiService::OnProgressUpdate(
    const OperationProgressStatusList& list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  FOR_EACH_OBSERVER(
      DriveServiceObserver, observers_, OnProgressUpdate(list));
}

}  // namespace google_apis
