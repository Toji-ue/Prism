import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

Popup {
    id: panel
    required property AppController controller
    required property Item backdrop
    signal requestBrushes
    signal requestEdit
    objectName: "drawingQuickControls"
    width: 336
    height: 420
    padding: 10
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    function openNear(px, py, viewportWidth, viewportHeight) {
        const gap = 24;
        width = Math.min(336, viewportWidth - 16);
        height = Math.min(420, viewportHeight - 16);
        x = Math.max(8, Math.min(viewportWidth - width - 8, px + gap + width <= viewportWidth - 8 ? px + gap : px - width - gap));
        y = Math.max(8, Math.min(viewportHeight - height - 8, py + gap + height <= viewportHeight - 8 ? py + gap : py - height - gap));
        open();
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
    contentItem: Flickable {
        id: scroll
        FocusScroll {
            viewport: scroll
        }
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}
        ColumnLayout {
            id: column
            width: scroll.width - 8
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: panel.controller.activePresetName
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
                ToolbarButton {
                    iconName: "undo"
                    tip: qsTr("Undo")
                    usable: panel.controller.canUndo
                    onClicked: panel.controller.undo()
                }
            }
            BrushLabSlider {
                Layout.fillWidth: true
                objectName: "quickSize"
                caption: qsTr("Size (px)")
                from: 1
                to: 200
                displayScale: 1
                suffix: "px"
                controllerValue: panel.controller.brushSize
                onMoved: value => panel.controller.brushSize = value
            }
            BrushLabSlider {
                Layout.fillWidth: true
                objectName: "quickFlow"
                caption: qsTr("Flow (%)")
                from: 0.01
                controllerValue: panel.controller.brushFlow
                onMoved: value => panel.controller.brushFlow = value
            }
            Text {
                text: qsTr("Recent colours")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 12
            }
            Flow {
                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                spacing: 6
                Repeater {
                    model: panel.controller.recentColors.length ? panel.controller.recentColors : [String(panel.controller.brushColor)]
                    PButton {
                        required property string modelData
                        width: 32
                        height: 32
                        Accessible.name: qsTr("Use recent colour %1").arg(modelData)
                        onClicked: panel.controller.brushColor = modelData
                        background: Rectangle {
                            color: modelData
                            radius: ThemeManager.themeColor("radiusChip")
                            border.width: parent.visualFocus ? 2 : 1
                            border.color: parent.visualFocus ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
                        }
                    }
                }
            }
            Text {
                text: qsTr("Pinned & recent brushes")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 12
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 6
                rowSpacing: 6
                Repeater {
                    model: panel.controller.quickPresets
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 0
                        PButton {
                            objectName: "quickBrush_" + modelData.index
                            Layout.fillWidth: true
                            implicitWidth: 94
                            implicitHeight: 40
                            text: modelData.name
                            checked: panel.controller.activePresetId === modelData.id
                            ToolTip.visible: hovered
                            ToolTip.text: modelData.name
                            onClicked: panel.controller.selectPreset(modelData.index)
                        }
                        ToolbarButton {
                            iconName: "pin"
                            active: modelData.pinned
                            tip: modelData.pinned ? qsTr("Unpin brush") : qsTr("Pin brush")
                            onClicked: panel.controller.togglePinnedPreset(modelData.index)
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                PButton {
                    objectName: "quickAllBrushes"
                    Layout.fillWidth: true
                    text: qsTr("All brushes")
                    onClicked: panel.requestBrushes()
                }
                PButton {
                    objectName: "quickEditBrush"
                    Layout.fillWidth: true
                    text: qsTr("Edit brush")
                    onClicked: panel.requestEdit()
                }
            }
        }
    }
}
