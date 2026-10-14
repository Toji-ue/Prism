import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

Popup {
    id: panel
    required property AppController controller
    required property Item backdrop
    objectName: "drawingQuickControls"
    width: 296
    height: 355
    padding: 10
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function openNear(px, py, viewportWidth, viewportHeight) {
        const gap = 24
        x = Math.max(8, Math.min(viewportWidth - width - 8,
                                 px + gap + width <= viewportWidth ? px + gap : px - width - gap))
        y = Math.max(8, Math.min(viewportHeight - height - 8,
                                 py + gap + height <= viewportHeight ? py + gap : py - height - gap))
        open()
    }

    background: Item {
        FocusGlassPanel {
            anchors.fill: parent
            backdrop: panel.backdrop
            visible: panel.controller.focusMode
        }
        Rectangle {
            anchors.fill: parent
            visible: !panel.controller.focusMode
            radius: ThemeManager.themeColor("radiusPanel")
            color: ThemeManager.themeColor("popup")
            border.color: ThemeManager.themeColor("popupBorder")
        }
    }

    contentItem: ColumnLayout {
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: qsTr("QUICK CONTROLS")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 10
                font.letterSpacing: 0.7
            }
            Text {
                text: panel.controller.brushName
                color: ThemeManager.themeColor("text")
                font.pixelSize: 11
                elide: Text.ElideRight
                Layout.maximumWidth: 130
            }
            ToolbarButton {
                iconName: "undo"
                tip: qsTr("Undo")
                usable: panel.controller.canUndo
                onClicked: panel.controller.undo()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Size"); color: ThemeManager.themeColor("text"); font.pixelSize: 11; Layout.preferredWidth: 36 }
            PSlider {
                id: sizeSlider
                objectName: "quickSizeSlider"
                Layout.fillWidth: true
                from: 1; to: 200
                Component.onCompleted: value = panel.controller.brushSize
                Connections {
                    target: panel.controller
                    function onBrushChanged() {
                        if (!sizeSlider.pressed) sizeSlider.value = panel.controller.brushSize
                    }
                }
                onMoved: panel.controller.brushSize = Math.round(value)
            }
            Text { text: Math.round(panel.controller.brushSize) + " px"; color: ThemeManager.themeColor("text"); font.pixelSize: 11; Layout.preferredWidth: 42 }
        }
        RowLayout {
            Layout.fillWidth: true
            Text { text: qsTr("Flow"); color: ThemeManager.themeColor("text"); font.pixelSize: 11; Layout.preferredWidth: 36 }
            PSlider {
                id: flowSlider
                objectName: "quickFlowSlider"
                Layout.fillWidth: true
                from: 0.01; to: 1
                Component.onCompleted: value = panel.controller.brushFlow
                Connections {
                    target: panel.controller
                    function onBrushChanged() {
                        if (!flowSlider.pressed) flowSlider.value = panel.controller.brushFlow
                    }
                }
                onMoved: panel.controller.brushFlow = value
            }
            Text { text: Math.round(panel.controller.brushFlow * 100) + "%"; color: ThemeManager.themeColor("text"); font.pixelSize: 11; Layout.preferredWidth: 42 }
        }

        Text { text: qsTr("RECENT COLOURS"); color: ThemeManager.themeColor("textDim"); font.pixelSize: 10; font.letterSpacing: 0.7 }
        Row {
            spacing: 4
            Rectangle {
                width: 24; height: 24
                radius: ThemeManager.themeColor("radiusChip")
                color: panel.controller.brushColor
                border.color: ThemeManager.themeColor("border")
                MouseArea { anchors.fill: parent; onClicked: panel.controller.brushColor = parent.color }
            }
            Repeater {
                model: panel.controller.recentColors
                delegate: Rectangle {
                    required property string modelData
                    width: 24; height: 24
                    radius: ThemeManager.themeColor("radiusChip")
                    color: modelData
                    border.color: ThemeManager.themeColor("border")
                    MouseArea { anchors.fill: parent; onClicked: panel.controller.brushColor = modelData }
                }
            }
        }

        Text { text: qsTr("PINNED & RECENT BRUSHES"); color: ThemeManager.themeColor("textDim"); font.pixelSize: 10; font.letterSpacing: 0.7 }
        Grid {
            columns: 2
            spacing: 5
            Repeater {
                model: panel.controller.quickPresets
                delegate: Rectangle {
                    required property var modelData
                    width: 132; height: 42
                    radius: ThemeManager.themeColor("radiusControl")
                    color: panel.controller.activePresetIndex === modelData.index
                           ? ThemeManager.themeColor("selection") : ThemeManager.themeColor("panelRaised")
                    Image {
                        x: 4; anchors.verticalCenter: parent.verticalCenter
                        width: 30; height: 30
                        source: "image://preset/" + modelData.index
                        fillMode: Image.PreserveAspectFit
                    }
                    Text {
                        x: 38; width: 70; anchors.verticalCenter: parent.verticalCenter
                        text: modelData.name
                        color: ThemeManager.themeColor("text")
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                    MouseArea { anchors.fill: parent; onClicked: panel.controller.selectPreset(modelData.index) }
                    Text {
                        anchors.right: parent.right; anchors.rightMargin: 5
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.pinned ? "★" : "☆"
                        color: modelData.pinned ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("textDim")
                        font.pixelSize: 15
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -3
                            onClicked: panel.controller.togglePinnedPreset(modelData.index)
                        }
                    }
                }
            }
        }
    }
}
