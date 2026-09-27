import QtQuick
import QtQuick.Controls.Basic
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style Layers docker: title bar with action buttons, blend-mode
// combo, opacity slider, then the layer rows (eye / lock / name).
Rectangle {
    id: docker
    required property AppController controller

    color: T.panel
    implicitHeight: header.height + controls.height + list.height + 12
    height: 260

    // ---- header ----
    Rectangle {
        id: header
        width: parent.width
        height: T.dockerHeaderHeight
        color: T.windowAlt

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            Text {
                text: qsTr("Layers")
                color: T.text
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Repeater {
                model: [
                    { g: "＋", tip: qsTr("New layer"),        act: "add" },
                    { g: "⧉", tip: qsTr("Duplicate layer"),   act: "dup" },
                    { g: "▴", tip: qsTr("Raise layer"),       act: "up" },
                    { g: "▾", tip: qsTr("Lower layer"),       act: "down" },
                    { g: "✕", tip: qsTr("Delete layer"),      act: "del" }
                ]
                delegate: Rectangle {
                    width: 20; height: 20
                    color: mh.hovered ? T.buttonHover : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: modelData.g
                        color: T.text
                        font.pixelSize: 11
                    }
                    HoverHandler { id: mh }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            switch (modelData.act) {
                            case "add":  controller.addLayer(); break
                            case "dup":  controller.duplicateLayer(); break
                            case "up":   controller.moveLayerUp(); break
                            case "down": controller.moveLayerDown(); break
                            case "del":  controller.deleteLayer(); break
                            }
                        }
                    }
                    ToolTip.visible: mh.hovered
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

        ComboBox {
            id: blendCombo
            width: parent.width
            height: 22
            font.pixelSize: 11
            model: ["Normal", "Multiply", "Screen", "Overlay"]
            currentIndex: {
                var idx = controller.activeLayerIndex
                var item = controller.layers.data(controller.layers.index(idx, 0),
                                                  LayerListModel.BlendModeRole)
                return item === undefined ? 0 : item
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
                onMoved: controller.setLayerOpacity(controller.activeLayerIndex, value)
            }
            Text {
                text: Math.round(opacitySlider.value) + "%"
                color: T.text
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
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
            width: list.width
            height: 26
            color: controller.activeLayerIndex === index ? T.highlight : "transparent"

            MouseArea {
                anchors.fill: parent
                onClicked: controller.activeLayerIndex = index
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                // visibility eye
                Rectangle {
                    width: 18; height: 18
                    color: "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: model.visible ? "👁" : "—"
                        color: controller.activeLayerIndex === index
                               ? T.highlightText : T.textDim
                        font.pixelSize: 11
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: controller.setLayerVisible(index, !model.visible)
                    }
                }

                // thumbnail of the layer's own pixels is not exposed yet;
                // a plain chip keeps the row readable until it is.
                Rectangle {
                    width: 20; height: 16
                    color: T.panelSunken
                    border.color: T.border
                }

                TextInput {
                    id: nameField
                    width: 96
                    text: model.name
                    color: controller.activeLayerIndex === index
                           ? T.highlightText : T.text
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    selectByMouse: true
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
