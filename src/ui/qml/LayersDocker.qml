import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style Layers docker: title bar with action buttons, blend-mode
// combo, opacity slider, then the layer rows (eye / lock / thumbnail / name).
Rectangle {
    id: docker
    required property AppController controller

    color: T.panel

    // Opacity is edited live (no history per pixel of drag); the history
    // entry is created once on release, seeded with the drag-start value.
    property int dragStartOpacity: 100

    // ---- header ----
    Rectangle {
        id: header
        width: parent.width
        height: T.dockerHeaderHeight
        color: T.windowAlt

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Layers")
            color: T.text
            font.pixelSize: 11
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Repeater {
                model: [
                    { icon: "plus",      tip: qsTr("New layer"),      act: "add" },
                    { icon: "duplicate", tip: qsTr("Duplicate layer"), act: "dup" },
                    { icon: "arrow-up",  tip: qsTr("Raise layer"),    act: "up" },
                    { icon: "arrow-down", tip: qsTr("Lower layer"),   act: "down" },
                    { icon: "trash",     tip: qsTr("Delete layer"),   act: "del" }
                ]
                delegate: Rectangle {
                    width: 20; height: 20
                    color: mh.hovered ? T.buttonHover : "transparent"
                    Icon {
                        anchors.centerIn: parent
                        width: 14; height: 14
                        name: modelData.icon
                        color: T.text
                    }
                    HoverHandler { id: mh }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            switch (modelData.act) {
                            case "add":  addLayerMenu.popup(); break
                            case "dup":  controller.duplicateLayer(); break
                            case "up":   controller.moveLayerUp(); break
                            case "down": controller.moveLayerDown(); break
                            case "del":  controller.deleteLayer(); break
                            }
                        }
                    }
                    ToolTip.visible: mh.hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.tip
                }
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: T.border }
    }

    // ---- blend mode + opacity ----
    Column {
        id: controls
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 6
        spacing: 4

        PComboBox {
            id: blendCombo
            width: parent.width
            height: 22
            font.pixelSize: 11
            model: ["Normal", "Multiply", "Screen", "Overlay"]
            // Bound to the active layer's mode so external changes (undo,
            // layer switch) are reflected instead of leaving a stale label.
            currentIndex: {
                var idx = controller.activeLayerIndex
                var v = controller.layers.data(controller.layers.index(idx, 0),
                                               LayerListModel.BlendModeRole)
                return v === undefined ? 0 : v
            }
            onActivated: controller.setLayerBlendMode(controller.activeLayerIndex, currentIndex)
        }

        Row {
            width: parent.width
            spacing: 6
            Text {
                text: qsTr("Opacity:")
                color: T.text
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            PSlider {
                id: opacitySlider
                width: parent.width - 74
                from: 0; to: 100
                value: {
                    var v = controller.layers.data(
                        controller.layers.index(controller.activeLayerIndex, 0),
                        LayerListModel.OpacityRole)
                    return v === undefined ? 100 : v
                }
                onPressedChanged: {
                    if (pressed)
                        docker.dragStartOpacity = value
                    else
                        controller.commitLayerOpacity(controller.activeLayerIndex,
                                                      Math.round(value),
                                                      docker.dragStartOpacity)
                }
                // Live preview without touching history.
                onMoved: controller.previewLayerOpacity(controller.activeLayerIndex,
                                                        Math.round(value))
            }
            Text {
                text: Math.round(opacitySlider.value) + "%"
                color: T.text
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // ---- "+" button dropdown ----
    PMenu {
        id: addLayerMenu

        MenuItem {
            text: qsTr("New Layer")
            onTriggered: controller.addLayer()
        }
        MenuItem {
            text: qsTr("Import Image as Layer...")
            onTriggered: {
                console.info("layers+: Import Image as Layer clicked")
                importImageDialog.open()
            }
        }
    }

    FileDialog {
        id: importImageDialog
        title: qsTr("Import Image as Layer")
        nameFilters: [
            qsTr("Images (*.png *.jpg *.jpeg *.webp *.tif *.tiff *.bmp *.gif *.ppm *.ico)"),
            qsTr("All files (*)")
        ]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            console.info("layers+: import image accepted: " + selectedFile)
            if (!controller.importImageAsLayer(selectedFile))
                console.warn("layers+: import as layer failed: " + controller.lastError)
        }
    }

    // ---- layer context menu (right click on a row) ----
    PMenu {
        id: layerMenu
        property int targetIndex: 0

        MenuItem {
            text: qsTr("New Layer")
            onTriggered: controller.addLayer()
        }
        MenuItem {
            text: qsTr("Duplicate Layer")
            onTriggered: controller.duplicateLayer()
        }
        MenuItem {
            text: qsTr("Merge with Layer Below")
            enabled: layerMenu.targetIndex > 0
            onTriggered: controller.mergeLayerDown()
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Move Layer Up")
            onTriggered: controller.moveLayerUp()
        }
        MenuItem {
            text: qsTr("Move Layer Down")
            onTriggered: controller.moveLayerDown()
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Toggle Visibility")
            onTriggered: {
                const rows = controller.layers
                const visible = rows.data(rows.index(layerMenu.targetIndex, 0),
                                          LayerListModel.VisibleRole)
                controller.setLayerVisible(layerMenu.targetIndex, !visible)
            }
        }
        MenuItem {
            text: qsTr("Toggle Lock")
            onTriggered: {
                const rows = controller.layers
                const locked = rows.data(rows.index(layerMenu.targetIndex, 0),
                                         LayerListModel.LockedRole)
                controller.setLayerLocked(layerMenu.targetIndex, !locked)
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Select Layer Content")
            onTriggered: {
                controller.activeLayerIndex = layerMenu.targetIndex
                controller.selectLayerContent()
            }
        }
        MenuItem {
            text: qsTr("Clear Layer")
            onTriggered: {
                controller.activeLayerIndex = layerMenu.targetIndex
                controller.clearCanvas()
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Delete Layer")
            enabled: controller.layers.rowCount() > 1
            onTriggered: {
                controller.activeLayerIndex = layerMenu.targetIndex
                controller.deleteLayer()
            }
        }
    }

    // ---- layer list ----
    ListView {
        id: list
        anchors.top: controls.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.bottomMargin: 4
        clip: true
        model: controller.layers

        delegate: Rectangle {
            id: layerRow
            width: list.width
            height: 28
            color: controller.activeLayerIndex === index ? T.highlight : "transparent"

            // Left click selects; right click opens the layer context menu
            // (Krita's layer menu: new / duplicate / merge / move / remove).
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
                    controller.activeLayerIndex = index
                    if (mouse.button === Qt.RightButton) {
                        layerMenu.targetIndex = index
                        layerMenu.parent = layerRow
                        layerMenu.open()
                    }
                }
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 3
                anchors.right: parent.right
                anchors.rightMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                // visibility
                Item {
                    width: 18; height: 18
                    Icon {
                        anchors.centerIn: parent
                        width: 13; height: 13
                        name: model.visible ? "eye" : "eye-off"
                        color: controller.activeLayerIndex === index
                               ? T.highlightText : (model.visible ? T.text : T.textDim)
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: controller.setLayerVisible(index, !model.visible)
                    }
                }

                // lock
                Item {
                    width: 18; height: 18
                    Icon {
                        anchors.centerIn: parent
                        width: 13; height: 13
                        name: model.locked ? "lock" : "unlock"
                        color: controller.activeLayerIndex === index
                               ? T.highlightText : (model.locked ? T.text : T.textDim)
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: controller.setLayerLocked(index, !model.locked)
                    }
                }

                // real thumbnail of this layer's pixels
                Rectangle {
                    width: 24; height: 20
                    color: T.panelSunken
                    border.color: T.border
                    border.width: 1
                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: Image.PreserveAspectFit
                        cache: false
                        source: "image://layerthumb/" + index + "?v=" + controller.layerRevision
                    }
                }

                TextInput {
                    width: Math.max(50, parent.width - 150)
                    text: model.name
                    color: controller.activeLayerIndex === index ? T.highlightText : T.text
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    selectByMouse: true
                    clip: true
                    onEditingFinished: controller.renameLayer(index, text)
                }

                Text {
                    text: model.opacity + "%"
                    color: controller.activeLayerIndex === index
                           ? T.highlightText : T.textDim
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
