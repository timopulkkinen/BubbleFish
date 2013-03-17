// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_list.h"

namespace message_center {

namespace {
static MessageCenter* g_message_center;
}

// static
void MessageCenter::Initialize() {
  DCHECK(g_message_center == NULL);
  g_message_center = new MessageCenter();
}

// static
MessageCenter* MessageCenter::Get() {
  DCHECK(g_message_center);
  return g_message_center;
}

// static
void MessageCenter::Shutdown() {
  DCHECK(g_message_center);
  delete g_message_center;
  g_message_center = NULL;
}

//------------------------------------------------------------------------------

MessageCenter::Delegate::~Delegate() {
}

//------------------------------------------------------------------------------

MessageCenter::MessageCenter()
    : delegate_(NULL) {
  notification_list_.reset(new NotificationList(this));
}

MessageCenter::~MessageCenter() {
  notification_list_.reset();
}

void MessageCenter::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void MessageCenter::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void MessageCenter::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void MessageCenter::SetMessageCenterVisible(bool visible) {
  notification_list_->SetMessageCenterVisible(visible);
}

size_t MessageCenter::NotificationCount() const {
  return notification_list_->NotificationCount();
}

size_t MessageCenter::UnreadNotificationCount() const {
  return notification_list_->unread_count();
}

bool MessageCenter::HasPopupNotifications() const {
  return notification_list_->HasPopupNotifications();
}

//------------------------------------------------------------------------------
// Client code interface.

void MessageCenter::AddNotification(
    NotificationType type,
    const std::string& id,
    const string16& title,
    const string16& message,
    const string16& display_source,
    const std::string& extension_id,
    const base::DictionaryValue* optional_fields) {
  notification_list_->AddNotification(type, id, title, message, display_source,
                                      extension_id, optional_fields);
  NotifyMessageCenterChanged(true);
}

void MessageCenter::UpdateNotification(
    const std::string& old_id,
    const std::string& new_id,
    const string16& title,
    const string16& message,
    const base::DictionaryValue* optional_fields) {
  notification_list_->UpdateNotificationMessage(
      old_id, new_id, title, message, optional_fields);
  NotifyMessageCenterChanged(true);
}

void MessageCenter::RemoveNotification(const std::string& id) {
  notification_list_->RemoveNotification(id);
  NotifyMessageCenterChanged(false);
}

void MessageCenter::SetNotificationIcon(const std::string& notification_id,
                                        const gfx::Image& image) {
  if (notification_list_->SetNotificationIcon(notification_id, image))
    NotifyMessageCenterChanged(true);
}

void MessageCenter::SetNotificationImage(const std::string& notification_id,
                                         const gfx::Image& image) {
  if (notification_list_->SetNotificationImage(notification_id, image))
    NotifyMessageCenterChanged(true);
}

void MessageCenter::SetNotificationButtonIcon(
    const std::string& notification_id, int button_index,
    const gfx::Image& image) {
  if (notification_list_->SetNotificationButtonIcon(notification_id,
                                                    button_index, image))
    NotifyMessageCenterChanged(true);
}

//------------------------------------------------------------------------------
// Overridden from NotificationChangeObserver:

void MessageCenter::OnRemoveNotification(const std::string& id, bool by_user) {
  if (delegate_)
    delegate_->NotificationRemoved(id, by_user);
}

void MessageCenter::OnRemoveAllNotifications(bool by_user) {
  if (delegate_) {
    const NotificationList::Notifications& notifications =
        notification_list_->GetNotifications();
    for (NotificationList::Notifications::const_iterator loopiter =
             notifications.begin();
         loopiter != notifications.end(); ) {
      NotificationList::Notifications::const_iterator curiter = loopiter++;
      std::string notification_id = (*curiter)->id();
      // May call RemoveNotification and erase curiter.
      delegate_->NotificationRemoved(notification_id, by_user);
    }
  }
}

void MessageCenter::OnDisableNotificationsByExtension(
    const std::string& id) {
  if (delegate_)
    delegate_->DisableExtension(id);
  // When we disable notifications, we remove any existing matching
  // notifications to avoid adding complicated UI to re-enable the source.
  notification_list_->SendRemoveNotificationsByExtension(id);
}

void MessageCenter::OnDisableNotificationsByUrl(const std::string& id) {
  if (delegate_)
    delegate_->DisableNotificationsFromSource(id);
  notification_list_->SendRemoveNotificationsBySource(id);
}

void MessageCenter::OnShowNotificationSettings(const std::string& id) {
  if (delegate_)
    delegate_->ShowSettings(id);
}

void MessageCenter::OnShowNotificationSettingsDialog(gfx::NativeView context) {
  if (delegate_)
    delegate_->ShowSettingsDialog(context);
}

void MessageCenter::OnExpanded(const std::string& id) {
  notification_list_->MarkNotificationAsExpanded(id);
}

void MessageCenter::OnClicked(const std::string& id) {
  if (delegate_)
    delegate_->OnClicked(id);
  if (HasPopupNotifications()) {
    notification_list_->MarkSinglePopupAsShown(id, true);
    NotifyMessageCenterChanged(false);
  }
}

void MessageCenter::OnButtonClicked(const std::string& id, int button_index) {
  if (delegate_)
    delegate_->OnButtonClicked(id, button_index);
  if (HasPopupNotifications()) {
    notification_list_->MarkSinglePopupAsShown(id, true);
    NotifyMessageCenterChanged(false);
  }
}

//------------------------------------------------------------------------------
// Overridden from NotificationList::Delegate:

void MessageCenter::SendRemoveNotification(const std::string& id,
                                           bool by_user) {
  if (delegate_)
    delegate_->NotificationRemoved(id, by_user);
}

void MessageCenter::OnQuietModeChanged(bool quiet_mode) {
  NotifyMessageCenterChanged(true);
}

//------------------------------------------------------------------------------
// Private.

void MessageCenter::NotifyMessageCenterChanged(bool new_notification) {
  FOR_EACH_OBSERVER(Observer,
                    observer_list_,
                    OnMessageCenterChanged(new_notification));
}


}  // namespace message_center
