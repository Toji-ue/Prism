#pragma once

#include "document/Document.h"
#include "history/Command.h"
#include "layer/Layer.h"
#include "raster/RasterBuffer.h"

#include <memory>
#include <string>

namespace prism::core {

/// Adds a raster layer on redo, removes it on undo.
class AddLayerCommand final : public Command {
public:
    AddLayerCommand(Document& doc, std::string layerName);
    /// Pre-built layer (e.g. an imported image): redo inserts it as-is.
    AddLayerCommand(Document& doc, std::string layerName, std::unique_ptr<Layer> layer);

    const char* description() const override { return "Add layer"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::string m_layerName;
    std::unique_ptr<Layer> m_layer;
    std::size_t m_index = 0;
};

/// Replaces a rectangular patch of a raster layer; restores the previous
/// pixels on undo. Snapshots only the patch's bounding box.
///
/// Two ways to build it: the stroke constructor snapshots `before` from the
/// layer itself (call it BEFORE painting); the explicit constructor takes
/// both snapshots, for operations that mutate the layer first (flood fill).
class StrokeCommand final : public Command {
public:
    StrokeCommand(Document& doc, std::size_t layerIndex, raster::RasterBuffer paintedPixels,
                  int rectX, int rectY);
    StrokeCommand(Document& doc, std::size_t layerIndex, raster::RasterBuffer before,
                  raster::RasterBuffer after, int rectX, int rectY, std::string label);

    const char* description() const override { return m_label.c_str(); }

    void undo() override;
    void redo() override;

private:
    Layer& layer() const;

    Document& m_doc;
    std::size_t m_layerIndex;
    int m_rectX;
    int m_rectY;
    std::string m_label = "Brush stroke";
    /// true = the patch replaces the rect (erase/filter/shape/fill);
    /// false = the patch is painted over it (brush stroke).
    bool m_replace = false;
    raster::RasterBuffer m_before;
    raster::RasterBuffer m_after;
};

/// Lifts a rectangle of pixels out of a layer onto a new layer above it, in one
/// undo step — the Crop tool's cut-and-drag. The piece is a normal layer, so the
/// Move tool drags it and the compositor draws it with no new code.
class CutRegionToLayerCommand final : public Command {
public:
    CutRegionToLayerCommand(Document& doc, std::size_t layerIndex, std::string name, int x, int y,
                            int width, int height);

    const char* description() const override { return "Cut to layer"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::size_t m_index;
    std::string m_name;
    int m_x;
    int m_y;
    int m_width;
    int m_height;
    /// Source patch as it was, and as it becomes (transparent).
    raster::RasterBuffer m_before;
    raster::RasterBuffer m_after;
    /// Owned while the command sits undone.
    std::unique_ptr<Layer> m_layer;
    std::size_t m_pieceIndex = 0;
};

/// Removes a layer on redo (returns ownership to the command), re-inserts
/// it at the same index on undo.
class RemoveLayerCommand final : public Command {
public:
    RemoveLayerCommand(Document& doc, std::size_t index);

    const char* description() const override { return "Delete layer"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::size_t m_index;
    std::unique_ptr<Layer> m_layer;
    std::vector<std::uint64_t> m_childIds;
};

/// Moves a layer within the stack; undo reverses the move.
class MoveLayerCommand final : public Command {
public:
    MoveLayerCommand(Document& doc, std::size_t fromIndex, std::size_t toIndex);

    const char* description() const override { return "Reorder layer"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::size_t m_from;
    std::size_t m_to;
};

/// Duplicates a layer and, for a group, its full nested subtree.
class DuplicateLayerCommand final : public Command {
public:
    DuplicateLayerCommand(Document& doc, std::size_t index);

    const char* description() const override { return "Duplicate layer"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::size_t m_newIndex = 0;
    std::vector<std::unique_ptr<Layer>> m_copies;
};

/// Clears a raster layer's pixels (fills transparent); undo restores them.
class ClearLayerCommand final : public Command {
public:
    ClearLayerCommand(Document& doc, std::size_t index);

    const char* description() const override { return "Clear layer"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::size_t m_index;
    raster::RasterBuffer m_before;
    raster::RasterBuffer m_after;
};

/// Merges a layer into the one below it (Krita's "Merge with Layer Below"):
/// the upper layer is composited onto the lower with its blend mode and
/// opacity, then removed. Undo restores both layers exactly.
class MergeLayerDownCommand final : public Command {
public:
    /// \p upperIndex must have a layer below it.
    MergeLayerDownCommand(Document& doc, std::size_t upperIndex);

    const char* description() const override { return "Merge with layer below"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::size_t m_upperIndex;
    raster::RasterBuffer m_lowerBefore;
    /// The merged result, computed once so redo is deterministic.
    raster::RasterBuffer m_merged;
    std::unique_ptr<Layer> m_upper;
};

/// Crops by moving the canvas window: the canvas shrinks to the rectangle and
/// the origin moves to its top-left. NO layer pixel is touched, so the content
/// on every side stays in the layer and comes back when the layer is moved —
/// Photoshop's crop with "delete cropped pixels" off. Undo is free.
class CropCommand final : public Command {
public:
    CropCommand(Document& doc, int x, int y, int width, int height);

    const char* description() const override { return "Crop"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    int m_x;
    int m_y;
    int m_width;
    int m_height;
    CanvasSize m_beforeSize;
    geometry::Point m_beforeOrigin;
};

/// Destructive top-left canvas resize, with the original raster buffers held
/// for undo. Unlike CropCommand, pixels outside the old canvas are discarded.
class ResizeCanvasCommand final : public Command {
public:
    ResizeCanvasCommand(Document& doc, CanvasSize size);
    const char* description() const override { return "Resize canvas"; }
    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    CanvasSize m_newSize;
    CanvasSize m_oldSize;
    geometry::Point m_oldOrigin;
    std::vector<raster::RasterBuffer> m_oldPixels;
};

/// Wraps one layer (or an existing group) in a new group, retaining the
/// selected layer's old parent so nested groups undo without changing pixels.
class GroupLayerCommand final : public Command {
public:
    GroupLayerCommand(Document& doc, std::size_t index, std::string name);
    const char* description() const override { return "Group layer"; }
    void undo() override;
    void redo() override;
private:
    Document& m_doc;
    std::size_t m_index;
    std::uint64_t m_oldParent;
    std::unique_ptr<Layer> m_group;
    std::uint64_t m_groupId;
};

class SetLayerParentCommand final : public Command {
public:
    SetLayerParentCommand(Document& doc, std::size_t index, std::uint64_t parentId);
    const char* description() const override { return "Move layer into group"; }
    void undo() override;
    void redo() override;
private:
    Document& m_doc;
    std::size_t m_index;
    std::uint64_t m_oldParent;
    std::uint64_t m_newParent;
};

/// Moves a raster layer's whole content by (dx, dy) pixels. Undo restores
/// the pre-move pixels exactly (one full-buffer snapshot).
class MoveLayerContentCommand final : public Command {
public:
    MoveLayerContentCommand(Document& doc, std::size_t index, int dx, int dy);
    /// For a live drag: the layer has already been previewed at (dx, dy), so the
    /// pre-drag pixels come from the caller (the command is idempotent).
    MoveLayerContentCommand(Document& doc, std::size_t index, int dx, int dy,
                            raster::RasterBuffer before);

    const char* description() const override { return "Move layer content"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::size_t m_index;
    int m_dx;
    int m_dy;
    raster::RasterBuffer m_before;
};

/// Sets a single layer property (name, visibility, opacity, lock, or blend
/// mode). Stores before/after so undo restores exactly.
class SetLayerPropertyCommand final : public Command {
public:
    enum class Property {
        Name,
        Visible,
        Opacity,
        BlendMode,
        Locked,
        Clipped,
        AlphaLocked,
    };

    SetLayerPropertyCommand(Document& doc, std::size_t index, Property prop,
                            std::string value, std::string prevValue);
    SetLayerPropertyCommand(Document& doc, std::size_t index, Property prop, float value,
                            float prevValue);
    SetLayerPropertyCommand(Document& doc, std::size_t index, Property prop, bool value,
                            bool prevValue);
    SetLayerPropertyCommand(Document& doc, std::size_t index, Property prop,
                            BlendMode value, BlendMode prevValue);

    const char* description() const override { return "Layer property"; }

    void undo() override;
    void redo() override;

private:
    struct Value {
        enum class Kind { Str, Float, Bool, Blend };
        Kind kind = Kind::Str;
        std::string s;
        float f = 0.0F;
        bool b = false;
        BlendMode blend = BlendMode::Normal;
    };

    void apply(const Value& v);

    Document& m_doc;
    std::size_t m_index;
    Property m_prop;
    Value m_newValue;
    Value m_oldValue;
};

} // namespace prism::core
