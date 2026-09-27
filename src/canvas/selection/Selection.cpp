#include "selection/Selection.h"

namespace prism::canvas {

void clipToSelection(RasterBuffer& target, const Rect& selection,
                     const RasterBuffer* restoreFrom)
{
    if (selection.isEmpty())
        return;
    if (target.isEmpty())
        return;

    for (int y = 0; y < target.height(); ++y) {
        const bool rowInside = y >= selection.y && y < selection.y + selection.height;
        for (int x = 0; x < target.width(); ++x) {
            const bool inside = rowInside && x >= selection.x
                             && x < selection.x + selection.width;
            if (inside)
                continue;

            if (restoreFrom && x < restoreFrom->width() && y < restoreFrom->height())
                target.set(x, y, restoreFrom->at(x, y));
            else
                target.set(x, y, prism::core::raster::kTransparent);
        }
    }
}

} // namespace prism::canvas
