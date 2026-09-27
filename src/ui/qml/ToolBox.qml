import QtQuick
import QtQuick.Controls.Basic
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style left toolbox: two columns of square tool buttons, then the
// foreground/background swatch pair at the bottom.
Rectangle {
    id: toolbox
    required property AppController controller

    implicitWidth: T.toolboxWidth
    color: T.panel
    border.color: T.border
    border.width: 1

    // Tool entries: label glyph, tooltip, tool enum, implemented?
    readonly property var tools: [
        { glyph: "B",  name: qsTr("Freehand Brush"),  tool: AppController.BrushTool,      ready: true },
        { glyph: "E",  name: qsTr("Eraser"),          tool: AppController.EraserTool,     ready: true },
        { glyph: "🪣", name: qsTr("Fill"),            tool: AppController.FillTool,       ready: true },
        { glyph: "I",  name: qsTr("Color Picker"),    tool: AppController.EyedropperTool, ready: true },
        { glyph: "✋", name: qsTr("Pan"),             tool: AppController.PanTool,        ready: true },
        { glyph: "🔍", name: qsTr("Zoom"),            tool: AppController.ZoomTool,       ready: false },
        { glyph: "▭",  name: qsTr("Rectangular Select"), tool: AppController.RectSelectTool, ready: false },
        { glyph: "✥",  name: qsTr("Move Layer"),      tool: AppController.MoveTool,       ready: false }
    ]

    Column {
        anchors.top: parent.top
        anchors.topMargin: 4
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 2

        Grid {
            columns: 2
            spacing: 2

            Repeater {
                model: toolbox.tools

                delegate: Rectangle {
                    width: 26; height: 26
                    color: {
                        if (toolbox.controller.tool === modelData.tool)
                            return T.highlight
                        return hover.hovered ? T.buttonHover : T.windowAlt
                    }
                    border.color: T.border
                    opacity: modelData.ready ? 1.0 : 0.45

                    Text {
                        anchors.centerIn: parent
                        text: modelData.glyph
                        color: toolbox.controller.tool === modelData.tool
                               ? T.highlightText : T.text
                        font.pixelSize: 13
                    }

                    HoverHandler { id: hover }

                    MouseArea {
                        anchors.fill: parent
                        enabled: modelData.ready
                        onClicked: toolbox.controller.tool = modelData.tool
                    }

                    ToolTip.visible: hover.hovered
                    ToolTip.text: modelData.ready
                                  ? modelData.name
                                  : modelData.name + qsTr(" — not implemented yet")
                }
            }
        }
    }

    // Foreground / background color swatches, offset like Krita's.
    Item {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
        width: 40; height: 40

        Rectangle {
            width: 24; height: 24
            x: 14; y: 14
            color: toolbox.controller.brushColor
            border.color: T.border
            MouseArea {
                anchors.fill: parent
                onClicked: toolbox.controller.brushColor = "#ffffff"
            }
        }
        Rectangle {
            width: 24; height: 24
            x: 0; y: 0
            color: "#ffffff"
            border.color: T.border
            MouseArea {
                anchors.fill: parent
                onClicked: toolbox.controller.brushColor = "#ffffff"
            }
        }
    }
}
