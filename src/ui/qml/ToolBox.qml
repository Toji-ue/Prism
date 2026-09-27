import QtQuick
import QtQuick.Controls.Basic
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style left toolbox: two columns of square tool buttons, then the
// foreground/background swatch pair at the bottom. All glyphs are vector
// icons drawn by Icon.qml — no emoji.
Rectangle {
    id: toolbox
    required property AppController controller

    implicitWidth: T.toolboxWidth
    color: T.panel
    border.color: T.border
    border.width: 1

    readonly property var tools: [
        { icon: "brush",       name: qsTr("Freehand Brush"),     tool: AppController.BrushTool,      key: "B" },
        { icon: "eraser",      name: qsTr("Eraser"),             tool: AppController.EraserTool,     key: "E" },
        { icon: "fill",        name: qsTr("Fill"),               tool: AppController.FillTool,       key: "F" },
        { icon: "picker",      name: qsTr("Color Picker"),       tool: AppController.EyedropperTool, key: "I" },
        { icon: "pan",         name: qsTr("Pan"),                tool: AppController.PanTool,        key: "" },
        { icon: "zoom",        name: qsTr("Zoom"),               tool: AppController.ZoomTool,       key: "" },
        { icon: "select-rect", name: qsTr("Rectangular Select"), tool: AppController.RectSelectTool, key: "M" },
        { icon: "move",        name: qsTr("Move Layer"),         tool: AppController.MoveTool,       key: "V" }
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
                    color: toolbox.controller.tool === modelData.tool
                           ? T.highlight
                           : (hover.hovered ? T.buttonHover : T.windowAlt)
                    border.color: T.border
                    border.width: 1

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
                        onClicked: toolbox.controller.tool = modelData.tool
                    }

                    ToolTip.visible: hover.hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.key.length > 0
                                  ? modelData.name + "  (" + modelData.key + ")"
                                  : modelData.name
                }
            }
        }
    }

    // Foreground / background swatches, offset like Krita's.
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
            color: "#ffffff"
            border.color: T.border
            border.width: 1
            MouseArea {
                anchors.fill: parent
                onClicked: toolbox.controller.brushColor = "#ffffff"
            }
        }
    }
}
