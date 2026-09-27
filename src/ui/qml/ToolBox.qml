import QtQuick
import QtQuick.Controls.Basic
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style left toolbox: two columns of square tool buttons grouped the way
// Krita groups them (paint / shapes / view / selection / edit). Tools that are
// not implemented yet appear greyed out and cannot be activated, so nothing in
// the UI silently does nothing.
Rectangle {
    id: toolbox
    required property AppController controller

    implicitWidth: T.toolboxWidth
    color: T.panel
    border.color: T.border
    border.width: 1

    // ready: true  -> working tool
    // ready: false -> shown disabled with an explanatory tooltip
    readonly property var tools: [
        { icon: "brush",       name: qsTr("Freehand Brush"),         key: "B", tool: AppController.BrushTool,             ready: true },
        { icon: "eraser",      name: qsTr("Eraser"),                 key: "E", tool: AppController.EraserTool,            ready: true },
        { icon: "line",        name: qsTr("Line"),                   key: "L", tool: AppController.LineTool,              ready: true },
        { icon: "select-rect", name: qsTr("Rectangle"),              key: "R", tool: AppController.RectangleTool,         ready: true },
        { icon: "ellipse",     name: qsTr("Ellipse"),                key: "O", tool: AppController.EllipseTool,           ready: true },
        { icon: "gradient",    name: qsTr("Gradient"),               key: "G", tool: AppController.GradientTool,          ready: true },
        { icon: "fill",        name: qsTr("Fill"),                   key: "F", tool: AppController.FillTool,              ready: true },
        { icon: "picker",      name: qsTr("Color Picker"),           key: "I", tool: AppController.EyedropperTool,        ready: true },
        { icon: "pan",         name: qsTr("Pan"),                    key: "",  tool: AppController.PanTool,               ready: true },
        { icon: "zoom",        name: qsTr("Zoom"),                   key: "Z", tool: AppController.ZoomTool,              ready: true },
        { icon: "select-rect", name: qsTr("Rectangular Selection"),  key: "M", tool: AppController.RectSelectTool,        ready: true },
        { icon: "ellipse",     name: qsTr("Elliptical Selection"),   key: "",  tool: AppController.EllipseSelectTool,     ready: true },
        { icon: "lasso",       name: qsTr("Freehand Selection"),     key: "",  tool: AppController.LassoSelectTool,       ready: true },
        { icon: "polygon",     name: qsTr("Polygon Selection"),      key: "",  tool: AppController.PolygonSelectTool,     ready: true },
        { icon: "wand",        name: qsTr("Contiguous Selection"),   key: "W", tool: AppController.MagicWandTool,         ready: true },
        { icon: "similar",     name: qsTr("Similar Color Selection"), key: "", tool: AppController.SimilarColorSelectTool, ready: true },
        { icon: "move",        name: qsTr("Move Layer"),             key: "V", tool: AppController.MoveTool,              ready: true },
        { icon: "text",        name: qsTr("Text"),                   key: "",  tool: AppController.TextTool,              ready: true },
        { icon: "polyline",    name: qsTr("Polyline"),               key: "",  tool: AppController.PolylineTool,          ready: true },
        { icon: "bezier",      name: qsTr("Bezier Curve"),           key: "",  tool: AppController.BezierTool,            ready: true },
        { icon: "path",        name: qsTr("Freehand Path"),          key: "",  tool: AppController.FreehandPathTool,      ready: true },
        { icon: "arc",         name: qsTr("Arc"),                    key: "",  tool: AppController.ArcTool,               ready: true },
        { icon: "multibrush",  name: qsTr("Multibrush"),             key: "",  tool: AppController.MultiBrushTool,        ready: true },
        { icon: "dynamic",     name: qsTr("Dynamic Brush"),          key: "",  tool: AppController.DynamicBrushTool,      ready: true },
        { icon: "bezier",      name: qsTr("Bezier Selection"),       key: "",  tool: AppController.BezierSelectTool,      ready: true },
        { icon: "magnetic",    name: qsTr("Magnetic Selection"),     key: "",  tool: AppController.MagneticSelectTool,    ready: true },
        { icon: "outline",     name: qsTr("Outline Selection"),      key: "",  tool: AppController.OutlineSelectTool,     ready: true },
        { icon: "transform",   name: qsTr("Transform"),              key: "",  tool: AppController.TransformTool,         ready: true },
        { icon: "crop",        name: qsTr("Crop"),                   key: "",  tool: AppController.CropTool,              ready: true },
        { icon: "pattern",     name: qsTr("Pattern"),                key: "",  tool: AppController.PatternTool,           ready: true }
    ]

    // Current tool, always visible at the top so it is obvious which tool is
    // active and how to get back to the brush (Freehand Brush, key B).
    Rectangle {
        id: currentToolBar
        width: parent.width
        height: 30
        color: T.windowAlt
        Text {
            anchors.centerIn: parent
            width: parent.width - 8
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: toolbox.currentToolName
            color: T.text
            font.pixelSize: 9
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: T.border }
    }

    // Name of the active tool, derived from the same table the buttons use.
    readonly property string currentToolName: {
        for (let i = 0; i < tools.length; ++i)
            if (tools[i].tool === controller.tool)
                return tools[i].name
        return qsTr("(no tool)")
    }

    Flickable {
        anchors.top: currentToolBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: width
        contentHeight: grid.height + colors.y + colors.height + 12
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Grid {
            id: grid
            y: 4
            anchors.horizontalCenter: parent.horizontalCenter
            columns: 2
            spacing: 2

            Repeater {
                model: toolbox.tools

                delegate: Rectangle {
                    width: 26; height: 26
                    color: toolbox.controller.tool === modelData.tool
                           ? T.highlight
                           : (hover.hovered && modelData.ready ? T.buttonHover : T.windowAlt)
                    border.color: T.border
                    border.width: 1
                    // Unimplemented tools are visibly inert, never silently dead.
                    opacity: modelData.ready ? 1.0 : 0.35

                    Icon {
                        anchors.centerIn: parent
                        width: 16; height: 16
                        name: modelData.icon
                        color: toolbox.controller.tool === modelData.tool
                               ? T.highlightText : T.text
                    }

                    HoverHandler { id: hover }

                    MouseArea {
                        anchors.fill: parent
                        enabled: modelData.ready
                        onClicked: toolbox.controller.tool = modelData.tool
                    }

                    ToolTip.visible: hover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.ready
                                  ? (modelData.key.length > 0
                                     ? modelData.name + "  (" + modelData.key + ")"
                                     : modelData.name)
                                  : modelData.name + qsTr(" - not implemented yet")
                }
            }
        }

        // Foreground / background swatches, offset like Krita's.
        Item {
            id: colors
            y: 4 + grid.height + 10
            anchors.horizontalCenter: parent.horizontalCenter
            width: 40; height: 40

            Rectangle {
                width: 24; height: 24
                x: 14; y: 14
                color: toolbox.controller.brushColor
                border.color: T.border
                border.width: 1
                MouseArea {
                    anchors.fill: parent
                    onClicked: colorPopup.open()
                }
                Popup {
                    id: colorPopup
                    width: 300; height: 420
                    x: toolbox.width
                    y: -200
                    padding: 0
                    background: Rectangle { color: T.window; border.color: T.border }
                    ColorSelector {
                        anchors.fill: parent
                        controller: toolbox.controller
                    }
                }
            }
            Rectangle {
                width: 24; height: 24
                x: 0; y: 0
                color: T.tint(0.9)
                border.color: T.border
                border.width: 1
                MouseArea {
                    anchors.fill: parent
                    onClicked: toolbox.controller.brushColor = "#ffffff"
                }
            }
        }
    }
}
