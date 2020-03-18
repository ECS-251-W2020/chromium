// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_list.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"

namespace message_center {

namespace {

bool ShouldShowNotificationAsPopup(
    const Notification& notification,
    const NotificationBlockers& blockers) {
  for (auto* blocker : blockers) {
    if (!blocker->ShouldShowNotificationAsPopup(notification))
      return false;
  }
  return true;
}

}  // namespace

bool ComparePriorityTimestampSerial::operator()(Notification* n1,
                                                Notification* n2) const {
  if (n1->priority() > n2->priority())  // Higher pri go first.
    return true;
  if (n1->priority() < n2->priority())
    return false;
  return CompareTimestampSerial()(n1, n2);
}

bool CompareTimestampSerial::operator()(Notification* n1,
                                        Notification* n2) const {
  if (n1->timestamp() > n2->timestamp())  // Newer come first.
    return true;
  if (n1->timestamp() < n2->timestamp())
    return false;
  if (n1->serial_number() > n2->serial_number())  // Newer come first.
    return true;
  if (n1->serial_number() < n2->serial_number())
    return false;
  return false;
}

bool NotificationList::NotificationState::operator!=(
    const NotificationState& other) const {
  return shown_as_popup != other.shown_as_popup || is_read != other.is_read;
}

NotificationList::NotificationList(MessageCenter* message_center)
    : message_center_(message_center),
      quiet_mode_(false) {
}

NotificationList::~NotificationList() {
}

void NotificationList::SetNotificationsShown(
    const NotificationBlockers& blockers,
    std::set<std::string>* updated_ids) {
  Notifications notifications = GetVisibleNotifications(blockers);

  for (Notification* notification : notifications) {
    NotificationState* state = &GetNotification(notification->id())->second;
    const NotificationState original_state = *state;
    state->shown_as_popup = true;
    state->is_read = true;
    if (updated_ids && (original_state != *state))
      updated_ids->insert(notification->id());
  }
}

void NotificationList::AddNotification(
    std::unique_ptr<Notification> notification) {
  PushNotification(std::move(notification));
}

void NotificationList::UpdateNotificationMessage(
    const std::string& old_id,
    std::unique_ptr<Notification> new_notification) {
  auto iter = GetNotification(old_id);
  if (iter == notifications_.end())
    return;

  NotificationState state = iter->second;

  if ((new_notification->renotify() ||
       !message_center_->HasMessageCenterView()) &&
      !quiet_mode_) {
    state = NotificationState();
  }

  // Do not use EraseNotification and PushNotification, since we don't want to
  // change unread counts nor to update is_read/shown_as_popup states.
  notifications_.erase(iter);

  // We really don't want duplicate IDs.
  DCHECK(GetNotification(new_notification->id()) == notifications_.end());
  notifications_.emplace(std::move(new_notification), state);
}

void NotificationList::RemoveNotification(const std::string& id) {
  EraseNotification(GetNotification(id));
}

NotificationList::Notifications NotificationList::GetNotificationsByNotifierId(
        const NotifierId& notifier_id) {
  Notifications notifications;
  for (const auto& tuple : notifications_) {
    Notification* notification = tuple.first.get();
    if (notification->notifier_id() == notifier_id)
      notifications.insert(notification);
  }
  return notifications;
}

NotificationList::Notifications NotificationList::GetNotificationsByAppId(
    const std::string& app_id) {
  Notifications notifications;
  for (const auto& tuple : notifications_) {
    Notification* notification = tuple.first.get();
    if (notification->notifier_id().id == app_id)
      notifications.insert(notification);
  }
  return notifications;
}

bool NotificationList::SetNotificationIcon(const std::string& notification_id,
                                           const gfx::Image& image) {
  auto iter = GetNotification(notification_id);
  if (iter == notifications_.end())
    return false;
  iter->first->set_icon(image);
  return true;
}

bool NotificationList::SetNotificationImage(const std::string& notification_id,
                                            const gfx::Image& image) {
  auto iter = GetNotification(notification_id);
  if (iter == notifications_.end())
    return false;
  iter->first->set_image(image);
  return true;
}

bool NotificationList::HasNotificationOfType(const std::string& id,
                                             const NotificationType type) {
  auto iter = GetNotification(id);
  if (iter == notifications_.end())
    return false;

  return iter->first->type() == type;
}

bool NotificationList::HasPopupNotifications(
    const NotificationBlockers& blockers) {
  for (const auto& tuple : notifications_) {
    if (tuple.first->priority() < DEFAULT_PRIORITY)
      break;
    if (!tuple.second.shown_as_popup &&
        ShouldShowNotificationAsPopup(*tuple.first, blockers)) {
      return true;
    }
  }
  return false;
}

NotificationList::PopupNotifications
NotificationList::GetPopupNotifications(const NotificationBlockers& blockers,
                                        std::list<std::string>* blocked) {
  PopupNotifications result;
  size_t default_priority_popup_count = 0;

  // Collect notifications that should be shown as popups. Start from oldest.
  for (auto iter = notifications_.rbegin(); iter != notifications_.rend();
       iter++) {
    NotificationState* state = &iter->second;
    Notification* notification = iter->first.get();
    if (state->shown_as_popup)
      continue;

    // No popups for LOW/MIN priority.
    if (notification->priority() < DEFAULT_PRIORITY)
      continue;

    if (!ShouldShowNotificationAsPopup(*notification, blockers)) {
      if (state->is_read)
        state->shown_as_popup = true;
      if (blocked)
        blocked->push_back(notification->id());
      continue;
    }

    // Checking limits. No limits for HIGH/MAX priority. DEFAULT priority
    // will return at most kMaxVisiblePopupNotifications entries. If the
    // popup entries are more, older entries are used. see crbug.com/165768
    if (notification->priority() == DEFAULT_PRIORITY &&
        default_priority_popup_count++ >= kMaxVisiblePopupNotifications) {
      continue;
    }

    result.insert(notification);
  }
  return result;
}

void NotificationList::MarkSinglePopupAsShown(
    const std::string& id, bool mark_notification_as_read) {
  auto iter = GetNotification(id);
  DCHECK(iter != notifications_.end());

  NotificationState* state = &iter->second;
  const Notification& notification = *iter->first;

  if (iter->second.shown_as_popup)
    return;

  // System notification is marked as shown only when marked as read.
  if (notification.priority() != SYSTEM_PRIORITY || mark_notification_as_read)
    state->shown_as_popup = true;

  // The popup notification is already marked as read when it's displayed.
  // Set the is_read back to false if necessary.
  if (!mark_notification_as_read)
    state->is_read = false;
}

void NotificationList::MarkSinglePopupAsDisplayed(const std::string& id) {
  auto iter = GetNotification(id);
  if (iter == notifications_.end())
    return;

  NotificationState* state = &iter->second;

  if (state->shown_as_popup)
    return;

  state->is_read = true;
}

NotificationDelegate* NotificationList::GetNotificationDelegate(
    const std::string& id) {
  auto iter = GetNotification(id);
  if (iter == notifications_.end())
    return nullptr;
  return iter->first->delegate();
}

void NotificationList::SetQuietMode(bool quiet_mode) {
  quiet_mode_ = quiet_mode;
  if (quiet_mode_) {
    for (auto& tuple : notifications_)
      tuple.second.shown_as_popup = true;
  }
}

Notification* NotificationList::GetNotificationById(const std::string& id) {
  auto iter = GetNotification(id);
  if (iter != notifications_.end())
    return iter->first.get();
  return nullptr;
}

NotificationList::Notifications NotificationList::GetVisibleNotifications(
    const NotificationBlockers& blockers) const {
  Notifications result;
  for (const auto& tuple : notifications_) {
    bool should_show = true;
    for (size_t i = 0; i < blockers.size(); ++i) {
      if (!blockers[i]->ShouldShowNotification(*tuple.first)) {
        should_show = false;
        break;
      }
    }
    if (should_show)
      result.insert(tuple.first.get());
  }

  return result;
}

size_t NotificationList::NotificationCount(
    const NotificationBlockers& blockers) const {
  return GetVisibleNotifications(blockers).size();
}

NotificationList::OwnedNotifications::iterator
NotificationList::GetNotification(const std::string& id) {
  for (auto iter = notifications_.begin(); iter != notifications_.end();
       ++iter) {
    if (iter->first->id() == id)
      return iter;
  }
  return notifications_.end();
}

void NotificationList::EraseNotification(OwnedNotifications::iterator iter) {
  notifications_.erase(iter);
}

void NotificationList::PushNotification(
    std::unique_ptr<Notification> notification) {
  // Ensure that notification.id is unique by erasing any existing
  // notification with the same id (shouldn't normally happen).
  auto iter = GetNotification(notification->id());
  NotificationState state;
  if (iter != notifications_.end()) {
    state = iter->second;
    EraseNotification(iter);
  } else {
    // TODO(mukai): needs to distinguish if a notification is dismissed by
    // the quiet mode or user operation.
    state.shown_as_popup =
        message_center_->IsMessageCenterVisible() || quiet_mode_;
  }
  if (notification->priority() == MIN_PRIORITY)
    state.is_read = true;
  notifications_.emplace(std::move(notification), state);
}

}  // namespace message_center
