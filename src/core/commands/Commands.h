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
    raster::RasterBuffer m_before;
    raster::RasterBuffer m_after;
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

/// Duplicates a layer (pixels + properties); undo removes the copy.
class DuplicateLayerCommand final : public Command {
public:
    DuplicateLayerCommand(Document& doc, std::size_t index);

    const char* description() const override { return "Duplicate layer"; }

    void undo() override;
    void redo() override;

private:
    Document& m_doc;
    std::size_t m_sourceIndex;
    std::size_t m_newIndex = 0;
    std::unique_ptr<Layer> m_copy;
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

/// Sets a single layer property (name, visibility, opacity, or blend mode).
/// Stores before/after so undo restores exactly.
class SetLayerPropertyCommand final : public Command {
public:
    enum class Property {
        Name,
        Visible,
        Opacity,
        BlendMode,
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
