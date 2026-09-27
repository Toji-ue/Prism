import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Prism.Ui 1.0

ApplicationWindow {
    id: root
    width: 1440
    height: 900
    minimumWidth: 1024
    minimumHeight: 640
    title: qsTr("Prism — untitled")
    visible: true
    color: "#1c1c20"

    // ---------------- keyboard shortcuts ----------------
    Shortcut { sequence: "Ctrl+Z"; onActivated: controller.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+Y"; onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+S"; onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+O"; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+N"; onActivated: newDocDialog.open() }
    Shortcut { sequence: "Ctrl+E"; onActivated: controller.eraser = !controller.eraser }
    Shortcut { sequence: "B"; onActivated: controller.eraser = false }
    Shortcut { sequence: "E"; onActivated: controller.eraser = true }
    Shortcut { sequence: "+"; onActivated: controller.brushSize = Math.min(200, controller.brushSize + 2) }
    Shortcut { sequence: "-"; onActivated: controller.brushSize = Math.max(1, controller.brushSize - 2) }

    AppController { id: controller }

    // ---------------- toolbar ----------------
    header: ToolBar {
        height: 46
        background: Rectangle { color: "#26262b" }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 6

            Label { text: "Prism"; color: "#f0f0f5"; font.bold: true; font.pixelSize: 15 }
            Rectangle { width: 1; height: 26; color: "#3a3a42" }

            ToolButton {
                text: "＋"
                ToolTip.text: qsTr("New layer")
                onClicked: controller.addLayer()
            }
            ToolButton {
                text: "⧉"
                ToolTip.text: qsTr("Duplicate layer")
                onClicked: controller.duplicateLayer()
            }
            ToolButton {
                text: "🗑"
                ToolTip.text: qsTr("Delete layer")
                onClicked: controller.deleteLayer()
            }
            ToolButton {
                text: "▴"
                ToolTip.text: qsTr("Move layer up")
                onClicked: controller.moveLayerUp()
            }
            ToolButton {
                text: "▾"
                ToolTip.text: qsTr("Move layer down")
                onClicked: controller.moveLayerDown()
            }

            Rectangle { width: 1; height: 26; color: "#3a3a42" }

            ToolButton {
                text: "↶"
                enabled: controller.canUndo
                ToolTip.text: controller.canUndo ? qsTr("Undo ") + controller.undoLabel : qsTr("Undo")
                onClicked: controller.undo()
            }
            ToolButton {
                text: "↷"
                enabled: controller.canRedo
                ToolTip.text: qsTr("Redo")
                onClicked: controller.redo()
            }

            Rectangle { width: 1; height: 26; color: "#3a3a42" }

            ToolButton {
                text: "🆕"
                ToolTip.text: qsTr("New (Ctrl+N)")
                onClicked: newDocDialog.open()
            }
            ToolButton {
                text: "📂"
                ToolTip.text: qsTr("Open (Ctrl+O)")
                onClicked: openDialog.open()
            }
            ToolButton {
                text: "💾"
                ToolTip.text: qsTr("Save (Ctrl+S)")
                onClicked: saveDialog.open()
            }
            ToolButton {
                text: "📤"
                ToolTip.text: qsTr("Export PNG")
                onClicked: exportDialog.open()
            }
            ToolButton {
                text: "🗑"
                ToolTip.text: qsTr("Clear active layer")
                onClicked: controller.clearCanvas()
            }

            Item { Layout.fillWidth: true }

            // brush controls
            ToolButton {
                text: "🖌"
                checkable: true
                checked: !controller.eraser
                ToolTip.text: qsTr("Brush (B)")
                onClicked: controller.eraser = false
            }
            ToolButton {
                text: "◻"
                checkable: true
                checked: controller.eraser
                ToolTip.text: qsTr("Eraser (E)")
                onClicked: controller.eraser = true
            }

            Rectangle {
                width: 18; height: 18; radius: 9
                color: controller.eraser ? "transparent" : controller.brushColor
                border.color: "#888"
                border.width: 1
                Rectangle {
                    anchors.centerIn: parent
                    width: Math.max(2, controller.brushSize * 0.28)
                    height: width
                    radius: width / 2
                    color: "transparent"
                    visible: !controller.eraser
                }
            }
            Rectangle {
                width: 26; height: 26; radius: 6
                color: controller.brushColor
                border.color: "#666"
                border.width: 1
                MouseArea {
                    anchors.fill: parent
                    onClicked: colorPicker.open()
                }
            }

            Label { text: "Size"; color: "#9a9aa5"; font.pixelSize: 12 }
            Slider {
                from: 1; to: 80
                value: controller.brushSize
                onMoved: controller.brushSize = value
                Layout.preferredWidth: 110
            }
            Label { text: "Flow"; color: "#9a9aa5"; font.pixelSize: 12 }
            Slider {
                from: 0.05; to: 1
                value: controller.brushFlow
                onMoved: controller.brushFlow = value
                Layout.preferredWidth: 80
            }
            Label { text: "Hard"; color: "#9a9aa5"; font.pixelSize: 12 }
            Slider {
                from: 0; to: 1
                value: controller.brushHardness
                onMoved: controller.brushHardness = value
                Layout.preferredWidth: 80
            }

            Item { Layout.fillWidth: true }

            Label { text: Math.round(controller.zoom * 100) + "%"; color: "#9a9aa5" }
            Slider {
                from: 0.05; to: 8
                value: controller.zoom
                onMoved: controller.zoom = value
                Layout.preferredWidth: 100
            }
            ToolButton {
                text: "🗗"
                ToolTip.text: qsTr("Fit / 100%")
                onClicked: controller.zoom = 1.0
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---------------- layer panel ----------------
        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: "#222226"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                Label { text: "LAYERS"; color: "#7a7a85"; font.pixelSize: 11; font.bold: true }

                ListView {
                    id: layerList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: controller.layers
                    clip: true

                    delegate: Rectangle {
                        width: layerList.width
                        height: 40
                        radius: 4
                        color: controller.activeLayerIndex === index ? "#3a3a44" : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            // visibility eye
                            Rectangle {
                                width: 22; height: 22; radius: 4
                                color: model.visible ? "#4a4a55" : "#2a2a30"
                                Label {
                                    anchors.centerIn: parent
                                    text: model.visible ? "👁" : "·"
                                    color: "#c8c8d0"
                                    font.pixelSize: 12
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: controller.setLayerVisible(index, !model.visible)
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                TextInput {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: "#e0e0e8"
                                    font.pixelSize: 12
                                    onEditingFinished: controller.renameLayer(index, text)
                                }
                                Slider {
                                    Layout.fillWidth: true
                                    from: 0; to: 100
                                    value: model.opacity
                                    onMoved: controller.setLayerOpacity(index, value)
                                }
                            }

                            ComboBox {
                                Layout.preferredWidth: 74
                                model: ["Normal", "Multiply", "Screen", "Overlay"]
                                currentIndex: model.blendMode
                                onActivated: controller.setLayerBlendMode(index, currentIndex)
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: controller.activeLayerIndex = index
                        }
                    }
                }
            }
        }

        // ---------------- canvas ----------------
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#19191d"

            Flickable {
                id: flick
                anchors.fill: parent
                anchors.margins: 20
                contentWidth: canvasItem.width
                contentHeight: canvasItem.height
                interactive: canvasItem.width * controller.zoom > flick.width
                             || canvasItem.height * controller.zoom > flick.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                CanvasView {
                    id: canvasItem
                    width: controller.canvasWidth * controller.zoom
                    height: controller.canvasHeight * controller.zoom
                    controller: controller

                    // checkerboard backdrop for transparency
                    Rectangle {
                        anchors.fill: parent
                        z: -1
                        color: "transparent"
                        border.color: "#3a3a42"
                        border.width: 1
                        // subtle checker via gradient grid
                    }

                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton
                        hoverEnabled: true
                        cursorShape: Qt.CrossCursor
                        onPressed: mouse => {
                            const cx = mouse.x / controller.zoom
                            const cy = mouse.y / controller.zoom
                            controller.beginStroke(cx, cy)
                        }
                        onPositionChanged: mouse => {
                            if (pressed) {
                                const cx = mouse.x / controller.zoom
                                const cy = mouse.y / controller.zoom
                                controller.extendStroke(cx, cy)
                            }
                        }
                        onReleased: controller.endStroke()
                    }
                }
            }
        }
    }

    // ---------------- color picker ----------------
    Popup {
        id: colorPicker
        x: root.width - 300
        y: 70
        width: 280
        height: 300
        padding: 12
        background: Rectangle { color: "#2b2b31"; radius: 8; border.color: "#444" }

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Label { text: "COLOR"; color: "#7a7a85"; font.pixelSize: 11; font.bold: true }

            // large swatch + hex
            RowLayout {
                spacing: 8
                Rectangle {
                    Layout.preferredWidth: 120; Layout.preferredHeight: 120
                    radius: 6
                    color: controller.brushColor
                    border.color: "#666"
                }
                ColumnLayout {
                    spacing: 6
                    Label { text: "Hex"; color: "#9a9aa5"; font.pixelSize: 11 }
                    TextField {
                        id: hexField
                        Layout.preferredWidth: 110
                        text: controller.brushColor.toString().substring(1).toUpperCase()
                        onAccepted: {
                            var c = text.replace("#", "")
                            if (c.length === 6)
                                controller.brushColor = "#" + c
                        }
                    }
                    // preset swatches
                    Flow {
                        Layout.preferredWidth: 110
                        spacing: 4
                        Repeater {
                            model: ["#000000","#ffffff","#e74c3c","#e67e22","#f1c40f",
                                    "#2ecc71","#3498db","#9b59b6","#95a5a6","#8e44ad"]
                            delegate: Rectangle {
                                width: 20; height: 20; radius: 3
                                color: modelData
                                border.color: "#555"
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: controller.brushColor = modelData
                                }
                            }
                        }
                    }
                }
            }

            // RGB sliders
            ColumnLayout {
                spacing: 4
                Repeater {
                    model: ["R", "G", "B"]
                    delegate: RowLayout {
                        spacing: 6
                        Label { text: modelData; color: "#9a9aa5"; Layout.preferredWidth: 14 }
                        Slider {
                            Layout.fillWidth: true
                            from: 0; to: 255
                            value: {
                                var c = controller.brushColor
                                return modelData === "R" ? c.r * 255 : (modelData === "G" ? c.g * 255 : c.b * 255)
                            }
                            onMoved: {
                                var c = controller.brushColor
                                var r = modelData === "R" ? value / 255 : c.r
                                var g = modelData === "G" ? value / 255 : c.g
                                var b = modelData === "B" ? value / 255 : c.b
                                controller.brushColor = Qt.rgba(r, g, b, 1)
                            }
                        }
                    }
                }
            }
        }
    }

    // ---------------- dialogs ----------------
    FileDialog {
        id: saveDialog
        title: qsTr("Save project")
        nameFilters: ["Prism project (*.prism)"]
        fileMode: FileDialog.SaveFile
        onAccepted: if (!controller.saveProject(selectedFile)) console.warn("save failed")
    }
    FileDialog {
        id: openDialog
        title: qsTr("Open project or image")
        nameFilters: ["Prism project (*.prism)", "Images (*.png *.jpg *.jpeg *.bmp)"]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            var p = selectedFile.toString().toLowerCase()
            if (p.endsWith(".prism")) {
                if (!controller.openProject(selectedFile)) console.warn("open failed")
            } else {
                if (!controller.importImage(selectedFile)) console.warn("import failed")
            }
        }
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Export PNG")
        nameFilters: ["PNG image (*.png)"]
        fileMode: FileDialog.SaveFile
        onAccepted: if (!controller.exportPng(selectedFile)) console.warn("export failed")
    }

    Popup {
        id: newDocDialog
        anchors.centerIn: parent
        width: 280; height: 180
        padding: 14
        background: Rectangle { color: "#2b2b31"; radius: 8; border.color: "#444" }
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            Label { text: "NEW DOCUMENT"; color: "#7a7a85"; font.pixelSize: 11; font.bold: true }
            RowLayout {
                Label { text: "Width"; color: "#c8c8d0" }
                TextField { id: wField; text: "1280"; Layout.fillWidth: true }
            }
            RowLayout {
                Label { text: "Height"; color: "#c8c8d0" }
                TextField { id: hField; text: "800"; Layout.fillWidth: true }
            }
            Button {
                text: "Create"
                Layout.alignment: Qt.AlignRight
                onClicked: {
                    controller.newDocument(parseInt(wField.text), parseInt(hField.text))
                    newDocDialog.close()
                }
            }
        }
    }
}
