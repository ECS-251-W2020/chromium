// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_loader_x11.h"

#include <float.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "skia/ext/image_operations.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/cursor_util.h"
#include "ui/base/cursor/cursors_aura.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"

namespace ui {

namespace {

// Load a cursor with a list of css names or shapes in order of decreasing
// priority.
::Cursor LoadFontCursor() {
  return x11::None;
}

template <typename... Ts>
::Cursor LoadFontCursor(int shape, Ts... ts);

template <typename... Ts>
::Cursor LoadFontCursor(const char* name, Ts... ts) {
  ::Cursor cursor = XcursorLibraryLoadCursor(gfx::GetXDisplay(), name);
  if (cursor != x11::None)
    return cursor;
  return LoadFontCursor(ts...);
}

template <typename... Ts>
::Cursor LoadFontCursor(int shape, Ts... ts) {
  ::Cursor cursor = XCreateFontCursor(gfx::GetXDisplay(), shape);
  if (cursor != x11::None)
    return cursor;
  return LoadFontCursor(ts...);
}

::Cursor LoadFontCursorForCursorType(CursorType id) {
  switch (id) {
    case CursorType::kMiddlePanning:
      return LoadFontCursor("all-scroll", XC_fleur);
    case CursorType::kMiddlePanningVertical:
      return LoadFontCursor("v-scroll");
    case CursorType::kMiddlePanningHorizontal:
      return LoadFontCursor("h-scroll");
    case CursorType::kNone:
      return LoadFontCursor("none");
    case CursorType::kGrab:
      return LoadFontCursor("openhand", "grab");
    case CursorType::kGrabbing:
      return LoadFontCursor("closedhand", "grabbing", XC_hand2);
    case CursorType::kNull:
    case CursorType::kPointer:
      return LoadFontCursor("left_ptr", XC_left_ptr);
    case CursorType::kMove:
      return LoadFontCursor("move", XC_fleur);
    case CursorType::kCross:
      return LoadFontCursor("crosshair", XC_cross);
    case CursorType::kHand:
      return LoadFontCursor("pointer", "hand", XC_hand2);
    case CursorType::kIBeam:
      return LoadFontCursor("text", XC_xterm);
    case CursorType::kProgress:
      return LoadFontCursor("progress", "left_ptr_watch", XC_watch);
    case CursorType::kWait:
      return LoadFontCursor("wait", XC_watch);
    case CursorType::kHelp:
      return LoadFontCursor("help");
    case CursorType::kEastResize:
    case CursorType::kEastPanning:
      return LoadFontCursor("e-resize", XC_right_side);
    case CursorType::kNorthResize:
    case CursorType::kNorthPanning:
      return LoadFontCursor("n-resize", XC_top_side);
    case CursorType::kNorthEastResize:
    case CursorType::kNorthEastPanning:
      return LoadFontCursor("ne-resize", XC_top_right_corner);
    case CursorType::kNorthWestResize:
    case CursorType::kNorthWestPanning:
      return LoadFontCursor("nw-resize", XC_top_left_corner);
    case CursorType::kSouthResize:
    case CursorType::kSouthPanning:
      return LoadFontCursor("s-resize", XC_bottom_side);
    case CursorType::kSouthEastResize:
    case CursorType::kSouthEastPanning:
      return LoadFontCursor("se-resize", XC_bottom_right_corner);
    case CursorType::kSouthWestResize:
    case CursorType::kSouthWestPanning:
      return LoadFontCursor("sw-resize", XC_bottom_left_corner);
    case CursorType::kWestResize:
    case CursorType::kWestPanning:
      return LoadFontCursor("w-resize", XC_right_side);
    case CursorType::kNorthSouthResize:
      return LoadFontCursor(XC_sb_v_double_arrow, "ns-resize");
    case CursorType::kEastWestResize:
      return LoadFontCursor(XC_sb_h_double_arrow, "ew-resize");
    case CursorType::kColumnResize:
      return LoadFontCursor("col-resize", XC_sb_h_double_arrow);
    case CursorType::kRowResize:
      return LoadFontCursor("row-resize", XC_sb_v_double_arrow);
    case CursorType::kNorthEastSouthWestResize:
      return LoadFontCursor("size_bdiag", "nesw-resize", "fd_double_arrow");
    case CursorType::kNorthWestSouthEastResize:
      return LoadFontCursor("size_fdiag", "nwse-resize", "bd_double_arrow");
    case CursorType::kVerticalText:
      return LoadFontCursor("vertical-text");
    case CursorType::kZoomIn:
      return LoadFontCursor("zoom-in");
    case CursorType::kZoomOut:
      return LoadFontCursor("zoom-out");
    case CursorType::kCell:
      return LoadFontCursor("cell", XC_plus);
    case CursorType::kContextMenu:
      return LoadFontCursor("context-menu");
    case CursorType::kAlias:
      return LoadFontCursor("alias");
    case CursorType::kNoDrop:
      return LoadFontCursor("no-drop");
    case CursorType::kCopy:
      return LoadFontCursor("copy");
    case CursorType::kNotAllowed:
      return LoadFontCursor("not-allowed", "crossed_circle");
    case CursorType::kDndNone:
      return LoadFontCursor("dnd-none", XC_hand2);
    case CursorType::kDndMove:
      return LoadFontCursor("dnd-move", XC_hand2);
    case CursorType::kDndCopy:
      return LoadFontCursor("dnd-copy", XC_hand2);
    case CursorType::kDndLink:
      return LoadFontCursor("dnd-link", XC_hand2);
    case CursorType::kCustom:
      NOTREACHED();
      return LoadFontCursor();
  }
  NOTREACHED() << "Case not handled for " << static_cast<int>(id);
  return LoadFontCursor();
}

}  // namespace

CursorLoader* CursorLoader::Create() {
  return new CursorLoaderX11;
}

CursorLoaderX11::ImageCursor::ImageCursor(XcursorImage* x_image,
                                          float scale,
                                          display::Display::Rotation rotation)
    : scale(scale), rotation(rotation) {
  cursor = CreateReffedCustomXCursor(x_image);
}

CursorLoaderX11::ImageCursor::~ImageCursor() {
  UnrefCustomXCursor(cursor);
}

CursorLoaderX11::CursorLoaderX11()
    : display_(gfx::GetXDisplay()),
      invisible_cursor_(CreateInvisibleCursor(), gfx::GetXDisplay()) {
  auto* cursor_theme_manager = CursorThemeManagerLinux::GetInstance();
  if (cursor_theme_manager)
    cursor_theme_observer_.Add(cursor_theme_manager);
}

CursorLoaderX11::~CursorLoaderX11() {
  UnloadAll();
}

void CursorLoaderX11::LoadImageCursor(CursorType id,
                                      int resource_id,
                                      const gfx::Point& hot) {
  SkBitmap bitmap;
  gfx::Point hotspot = hot;

  GetImageCursorBitmap(resource_id, scale(), rotation(), &hotspot, &bitmap);
  XcursorImage* x_image = SkBitmapToXcursorImage(&bitmap, hotspot);
  image_cursors_[id] =
      std::make_unique<ImageCursor>(x_image, scale(), rotation());
}

void CursorLoaderX11::LoadAnimatedCursor(CursorType id,
                                         int resource_id,
                                         const gfx::Point& hot,
                                         int frame_delay_ms) {
  std::vector<SkBitmap> bitmaps;
  gfx::Point hotspot = hot;

  GetAnimatedCursorBitmaps(resource_id, scale(), rotation(), &hotspot,
                           &bitmaps);

  XcursorImages* x_images = XcursorImagesCreate(bitmaps.size());
  x_images->nimage = bitmaps.size();

  for (unsigned int frame = 0; frame < bitmaps.size(); ++frame) {
    XcursorImage* x_image = SkBitmapToXcursorImage(&bitmaps[frame], hotspot);
    x_image->delay = frame_delay_ms;
    x_images->images[frame] = x_image;
  }

  animated_cursors_[id] = std::make_pair(
      XcursorImagesLoadCursor(gfx::GetXDisplay(), x_images), x_images);
}

void CursorLoaderX11::UnloadAll() {
  image_cursors_.clear();

  // Free animated cursors and images.
  for (const auto& cursor : animated_cursors_) {
    XcursorImagesDestroy(
        cursor.second.second);  // also frees individual frames.
    XFreeCursor(gfx::GetXDisplay(), cursor.second.first);
  }
}

void CursorLoaderX11::SetPlatformCursor(gfx::NativeCursor* cursor) {
  DCHECK(cursor);

  if (*cursor == CursorType::kNone) {
    cursor->SetPlatformCursor(invisible_cursor_.get());
    return;
  }

  if (*cursor == CursorType::kCustom)
    return;

  cursor->set_device_scale_factor(scale());
  cursor->SetPlatformCursor(CursorFromId(cursor->native_type()));
}

const XcursorImage* CursorLoaderX11::GetXcursorImageForTest(CursorType id) {
  return test::GetCachedXcursorImage(image_cursors_[id]->cursor);
}

void CursorLoaderX11::OnCursorThemeNameChanged(
    const std::string& cursor_theme_name) {
  XcursorSetTheme(display_, cursor_theme_name.c_str());
  ClearThemeCursors();
}

void CursorLoaderX11::OnCursorThemeSizeChanged(int cursor_theme_size) {
  XcursorSetDefaultSize(display_, cursor_theme_size);
  ClearThemeCursors();
}

bool CursorLoaderX11::IsImageCursor(gfx::NativeCursor native_cursor) {
  CursorType type = native_cursor.native_type();
  return image_cursors_.count(type) || animated_cursors_.count(type);
}

::Cursor CursorLoaderX11::CursorFromId(CursorType id) {
  auto font_it = font_cursors_.find(id);
  if (font_it != font_cursors_.end())
    return font_it->second;
  auto image_it = image_cursors_.find(id);
  if (image_it != image_cursors_.end()) {
    if (image_it->second->scale == scale() &&
        image_it->second->rotation == rotation()) {
      return image_it->second->cursor;
    } else {
      image_cursors_.erase(image_it);
    }
  }

  // First try to load the cursor directly.
  ::Cursor cursor = LoadFontCursorForCursorType(id);
  if (cursor != x11::None) {
    font_cursors_[id] = cursor;
    return cursor;
  }

  // If the theme is missing the desired cursor, use a chromium-supplied
  // fallback icon.
  int resource_id;
  gfx::Point point;
  if (ui::GetCursorDataFor(ui::CursorSize::kNormal, id, scale(), &resource_id,
                           &point)) {
    LoadImageCursor(id, resource_id, point);
    return image_cursors_[id]->cursor;
  }

  // As a last resort, return a left pointer.
  cursor = XCreateFontCursor(display_, XC_left_ptr);
  DCHECK(cursor);
  font_cursors_[id] = cursor;
  return cursor;
}

void CursorLoaderX11::ClearThemeCursors() {
  font_cursors_.clear();
  image_cursors_.clear();
}

}  // namespace ui
