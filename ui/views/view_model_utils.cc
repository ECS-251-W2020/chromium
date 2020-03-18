// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/view_model_utils.h"

#include <algorithm>

#include "ui/views/view.h"
#include "ui/views/view_model.h"

namespace views {

namespace {

// Used in calculating ideal bounds.
int primary_axis_coordinate(bool is_horizontal, const gfx::Point& point) {
  return is_horizontal ? point.x() : point.y();
}

}  // namespace

// static
void ViewModelUtils::SetViewBoundsToIdealBounds(const ViewModelBase& model) {
  for (auto& entry : model.entries())
    entry.view->SetBoundsRect(entry.ideal_bounds);
}

// static
bool ViewModelUtils::IsAtIdealBounds(const ViewModelBase& model) {
  return std::all_of(model.entries().begin(), model.entries().end(),
                     [](const ViewModelBase::Entry& entry) {
                       return entry.view->bounds() == entry.ideal_bounds;
                     });
}

// static
int ViewModelUtils::DetermineMoveIndex(const ViewModelBase& model,
                                       View* view,
                                       bool is_horizontal,
                                       int x,
                                       int y) {
  const auto& entries = model.entries();
  const int value = primary_axis_coordinate(is_horizontal, gfx::Point(x, y));
  DCHECK_NE(-1, model.GetIndexOfView(view));

  auto iter = entries.begin();
  for (; iter->view != view; ++iter) {
    const int mid_point = primary_axis_coordinate(
        is_horizontal, iter->ideal_bounds.CenterPoint());
    if (value < mid_point)
      return std::distance(entries.begin(), iter);
  }

  if (std::next(iter) == entries.end())
    return std::distance(entries.begin(), iter);

  // For indices after the current index ignore the bounds of the view being
  // dragged. This keeps the view from bouncing around as moved.
  const int delta = primary_axis_coordinate(
      is_horizontal, std::next(iter)->ideal_bounds.origin() -
                         iter->ideal_bounds.origin().OffsetFromOrigin());

  for (++iter; iter != entries.end(); ++iter) {
    const int mid_point = primary_axis_coordinate(
                              is_horizontal, iter->ideal_bounds.CenterPoint()) -
                          delta;
    if (value < mid_point)
      return std::distance(entries.begin(), iter) - 1;
  }
  return entries.size() - 1;
}

}  // namespace views
