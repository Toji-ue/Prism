import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Prism.Ui 1.0

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    title: qsTr("Prism — untitled")

    visible: true
    color: "#1e1e22"

    AppController {
        id: controller
    }

    header: ToolBar {
        height: 44
        background: Rectangle { color: "#26262b" }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            Label {
                text: "Prism"
                color: "#e8e8ee"
                font.bold: true
            }
            Item { Layout.fillWidth: true }

            Button {
                text: qsTr("+ Layer")
                onClicked: controller.addLayer()
            }
            Button {
                text: controller.canUndo ? qsTr("Undo ") + controller.undoLabel
                                         : qsTr("Undo")
                enabled: controller.canUndo
                onClicked: controller.undo()
            }
            Button {
                text: qsTr("Redo")
                enabled: controller.canRedo
                onClicked: controller.redo()
            }
            Button {
                text: qsTr("Export PNG")
                onClicked: {
                    exportDialog.open()
                }
            }
            Item { Layout.fillWidth: true }
            Label { text: qsTr("Brush"); color: "#9a9aa5" }
            Slider {
                id: sizeSlider
                from: 1; to: 60; value: controller.brushSize
                onMoved: controller.brushSize = value
            }
            Label { text: sizeSlider.value.toFixed(0) + " px"; color: "#9a9aa5" }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 0
        spacing: 0

        // Layer panel
        Rectangle {
            Layout.preferredWidth: 200
            Layout.fillHeight: true
            color: "#222226"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4

                Label {
                    text: qsTr("LAYERS")
                    color: "#7a7a85"
                    font.pixelSize: 11
                    font.bold: true
                }
                ListView {
                    id: layerList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: controller.layerCount
                    delegate: Rectangle {
                        width: layerList.width
                        height: 34
                        color: controller.activeLayerIndex === index ? "#3a3a44" : "transparent"
                        radius: 4
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            text: controller.layerName(index)
                            color: "#c8c8d0"
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: controller.activeLayerIndex = index
                        }
                    }
                }
            }
        }

        // Canvas area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#19191d"

            Flickable {
                id: flick
                anchors.fill: parent
                anchors.margins: 24
                contentWidth: canvasItem.width
                contentHeight: canvasItem.height
                clip: true

                CanvasView {
                    id: canvasItem
                    width: controller.canvasWidth
                    height: controller.canvasHeight
                    controller: controller

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        onPressed: mouse => controller.beginStroke(mouse.x, mouse.y)
                        onPositionChanged: mouse => {
                            if (pressed)
                                controller.extendStroke(mouse.x, mouse.y)
                        }
                        onReleased: controller.endStroke()
                    }
                }
            }
        }
    }

    FileDialog {
        id: exportDialog
        title: qsTr("Export PNG")
        nameFilters: ["PNG image (*.png)"]
        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (!controller.exportPng(exportDialog.selectedFile))
                console.warn("export failed:", exportDialog.selectedFile)
        }
    }
}
