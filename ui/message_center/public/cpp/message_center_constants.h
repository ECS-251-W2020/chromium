// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_PUBLIC_CPP_MESSAGE_CENTER_CONSTANTS_H_
#define UI_MESSAGE_CENTER_PUBLIC_CPP_MESSAGE_CENTER_CONSTANTS_H_

#include <stddef.h>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

// TODO(estade): many of these constants could be internalized.
namespace message_center {

// Exported values /////////////////////////////////////////////////////////////

// Square image sizes in DIPs.
const int kNotificationButtonIconSize = 16;
const int kNotificationIconSize = 80;
// A border is applied to images that have a non-preferred aspect ratio.
const int kNotificationImageBorderSize = 10;
const int kNotificationPreferredImageWidth = 360;
const int kNotificationPreferredImageHeight = 240;
const int kSmallImageSize = 16;
const int kSmallImageSizeMD = 18;
const int kSmallImagePadding = 4;

// Limits.
const size_t kMaxVisibleMessageCenterNotifications = 100;
const size_t kMaxVisiblePopupNotifications = 3;

// DIP dimension; H size of the whole card.
const int kNotificationWidth = 360;

// Colors.
constexpr SkColor kMessageCenterBorderColor = SkColorSetRGB(0xC7, 0xCA, 0xCE);
constexpr SkColor kMessageCenterShadowColor =
    SkColorSetA(SK_ColorBLACK, 0.5 * 255);

// Within a notification ///////////////////////////////////////////////////////

// DIP dimensions (H = horizontal, V = vertical).

const int kIconToTextPadding = 16;  // H space between icon & title/message.
const int kTextTopPadding = 12;     // V space between text elements.
const int kIconBottomPadding = 16;  // Minimum non-zero V space between icon
                                    // and frame.
// H space between the context message and the end of the card.
const int kTextRightPadding = 23;
const int kTextLeftPadding = kNotificationIconSize + kIconToTextPadding;
const int kContextMessageViewWidth =
    kNotificationWidth - kTextLeftPadding - kTextRightPadding;
// space between buttons and frame.
const int kControlButtonPadding = 2;
const int kControlButtonBorderSize = 6;

// Text sizes.
const int kTitleFontSize = 14;        // For title only.
const int kEmptyCenterFontSize = 13;  // For empty message only.
const int kTitleLineHeight = 20;      // In DIPs.
const int kMessageFontSize = 12;      // For everything but title.
const int kMessageLineHeight = 18;    // In DIPs.

// Colors.
// Background of the card.
constexpr SkColor kNotificationBackgroundColor = SK_ColorWHITE;
// Background of the image.
constexpr SkColor kImageBackgroundColor = kNotificationBackgroundColor;
// Title, message, ...
constexpr SkColor kRegularTextColorMD = SkColorSetRGB(0x21, 0x21, 0x21);
constexpr SkColor kDimTextColorMD = SkColorSetRGB(0x75, 0x75, 0x75);
// The focus border.
constexpr SkColor kFocusBorderColor = SkColorSetRGB(64, 128, 250);
// Foreground of small icon image.
constexpr SkColor kSmallImageMaskForegroundColor = SK_ColorWHITE;
// Background of small icon image.
constexpr SkColor kSmallImageMaskBackgroundColor =
    SkColorSetRGB(0xa3, 0xa3, 0xa3);
// Background of the close button and the settings button.
#if defined(OS_CHROMEOS)
constexpr SkColor kControlButtonBackgroundColor =
    SkColorSetA(SK_ColorWHITE, 0.9 * 0xff);
#else
constexpr SkColor kControlButtonBackgroundColor = SK_ColorTRANSPARENT;
#endif

// Default accent color of notifications that are not generated by system.
constexpr SkColor kNotificationDefaultAccentColor = gfx::kChromeIconGrey;

// For list notifications.
// Not used when --enabled-new-style-notification is set.
const size_t kNotificationMaximumItems = 5;

// Timing. Web Notifications always use high-priority timings except on
// Chrome OS. Given the absence of a notification center on non-Chrome OS
// platforms, this improves users' ability to interact with the toasts.
const int kAutocloseDefaultDelaySeconds = 8;
const int kAutocloseHighPriorityDelaySeconds = 25;

// Buttons.
const int kButtonHeight = 38;              // In DIPs.
const int kButtonHorizontalPadding = 16;   // In DIPs.
const int kButtonIconTopPadding = 11;      // In DIPs.
const int kButtonIconToTitlePadding = 16;  // In DIPs.

#if !defined(OS_LINUX) || defined(USE_AURA)
constexpr SkColor kButtonSeparatorColor = SkColorSetRGB(234, 234, 234);
constexpr SkColor kHoveredButtonBackgroundColor = SkColorSetRGB(243, 243, 243);
#endif

// Progress bar.
const int kProgressBarTopPadding = 16;
#if defined(OS_MACOSX)
const int kProgressBarThickness = 5;
const int kProgressBarCornerRadius = 3;
constexpr SkColor kProgressBarBackgroundColor = SkColorSetARGB(26, 0, 0, 0);
constexpr SkColor kProgressBarSliceColor = SkColorSetRGB(26, 194, 34);
#endif

// Line limits.
const int kMaxTitleLines = 2;
const int kMessageCollapsedLineLimit = 2;
const int kMessageExpandedLineLimit = 5;
const int kContextMessageLineLimit = 1;

// Around notifications ////////////////////////////////////////////////////////

// Horizontal & vertical thickness of the border around the notifications in the
// notification list.
constexpr int kNotificationBorderThickness = 1;
// Horizontal & vertical space around & between notifications in the
// notification list.
constexpr int kMarginBetweenItemsInList = 8;

// Horizontal & vertical space around & between popup notifications.
constexpr int kMarginBetweenPopups = 10;

// Radius of the rounded corners of a notification.
// The corners are only rounded in Chrome OS.
constexpr int kNotificationCornerRadius = 2;

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_PUBLIC_CPP_MESSAGE_CENTER_CONSTANTS_H_
