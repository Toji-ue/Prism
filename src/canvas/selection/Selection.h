#pragma once

#include "geometry/Types.h"
#include "raster/RasterBuffer.h"

namespace prism::canvas {

using prism::core::geometry::Rect;
using prism::core::raster::RasterBuffer;

/// Constrains an edit to a selection rectangle.
///
/// With \p restoreFrom == nullptr, pixels outside \p selection are cleared
/// (use this on a scratch buffer before committing a stroke). When
/// \p restoreFrom is given (the pre-edit copy of the same buffer), pixels
/// outside the selection are copied back from it — that is how a flood fill
/// or a filter is prevented from leaking outside the selection.
///
/// An empty selection means "no constraint" and leaves the buffer alone.
void clipToSelection(RasterBuffer& target, const Rect& selection,
                     const RasterBuffer* restoreFrom = nullptr);

} // namespace prism::canvas
