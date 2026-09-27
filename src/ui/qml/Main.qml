import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Prism.Ui 1.0
import "Theme.js" as T

ApplicationWindow {
    id: root
    // Sized to fit smaller laptop screens; maximize gives the full workspace.
    width: 1280
    height: 780
    minimumWidth: 1000
    minimumHeight: 620
    title: qsTr("Prism — untitled")
    visible: true
    color: T.window

    Component.onCompleted: showMaximized()

    AppController { id: controller }

    // ---------------- shortcuts ----------------
    Shortcut { sequence: "Ctrl+Z"; onActivated: controller.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+Y"; onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+S"; onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+O"; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+N"; onActivated: newDocDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+N"; onActivated: controller.addLayer() }
    Shortcut { sequence: "Ctrl+E"; onActivated: controller.exportPng ? exportDialog.open() : {} }
    Shortcut { sequence: "B"; onActivated: controller.tool = AppController.BrushTool }
    Shortcut { sequence: "E"; onActivated: controller.tool = AppController.EraserTool }
    Shortcut { sequence: "F"; onActivated: controller.tool = AppController.FillTool }
    Shortcut { sequence: "I"; onActivated: controller.tool = AppController.EyedropperTool }
    Shortcut { sequence: "+"; onActivated: controller.brushSize = Math.min(200, controller.brushSize + 2) }
    Shortcut { sequence: "-"; onActivated: controller.brushSize = Math.max(1, controller.brushSize - 2) }
    Shortcut { sequence: "Delete"; onActivated: controller.clearCanvas() }

    // ---------------- menu bar ----------------
    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action { text: qsTr("&New…\tCtrl+N"); onTriggered: newDocDialog.open() }
            Action { text: qsTr("&Open…\tCtrl+O"); onTriggered: openDialog.open() }
            MenuSeparator {}
            Action { text: qsTr("&Save\tCtrl+S"); onTriggered: saveDialog.open() }
            Action { text: qsTr("&Export PNG…\tCtrl+E"); onTriggered: exportDialog.open() }
            MenuSeparator {}
            Action { text: qsTr("&Quit\tCtrl+Q"); onTriggered: Qt.quit() }
        }
        Menu {
            title: qsTr("&Edit")
            Action {
                text: qsTr("&Undo\tCtrl+Z")
                enabled: controller.canUndo
                onTriggered: controller.undo()
            }
            Action {
                text: qsTr("&Redo\tCtrl+Shift+Z")
                enabled: controller.canRedo
                onTriggered: controller.redo()
            }
            MenuSeparator {}
            Action { text: qsTr("Clear Layer\tDelete"); onTriggered: controller.clearCanvas() }
        }
        Menu {
            title: qsTr("&View")
            Action { text: qsTr("Zoom &In"); onTriggered: controller.zoom = Math.min(8, controller.zoom * 1.25) }
            Action { text: qsTr("Zoom &Out"); onTriggered: controller.zoom = Math.max(0.05, controller.zoom / 1.25) }
            Action { text: qsTr("&100%"); onTriggered: controller.zoom = 1.0 }
            Action { text: qsTr("&Fit to Window"); onTriggered: controller.zoom = 0.5 }
        }
        Menu {
            title: qsTr("&Image")
            Action { text: qsTr("Resize Canvas…"); enabled: false }
            Action { text: qsTr("Canvas Information"); enabled: false }
        }
        Menu {
            title: qsTr("&Layer")
            Action { text: qsTr("&New Layer\tCtrl+Shift+N"); onTriggered: controller.addLayer() }
            Action { text: qsTr("&Duplicate Layer"); onTriggered: controller.duplicateLayer() }
            Action { text: qsTr("&Delete Layer"); onTriggered: controller.deleteLayer() }
            MenuSeparator {}
            Action { text: qsTr("&Raise Layer"); onTriggered: controller.moveLayerUp() }
            Action { text: qsTr("&Lower Layer"); onTriggered: controller.moveLayerDown() }
        }
        Menu {
            title: qsTr("&Select")
            Action { text: qsTr("Select All"); enabled: false }
            Action { text: qsTr("Deselect"); enabled: false }
        }
        Menu {
            title: qsTr("Fi&lter")
            Action { text: qsTr("Blur"); enabled: false }
            Action { text: qsTr("Sharpen"); enabled: false }
        }
        Menu {
            title: qsTr("&Tools")
            Action { text: qsTr("Freehand Brush\tB"); onTriggered: controller.tool = AppController.BrushTool }
            Action { text: qsTr("Eraser\tE"); onTriggered: controller.tool = AppController.EraserTool }
            Action { text: qsTr("Fill\tF"); onTriggered: controller.tool = AppController.FillTool }
            Action { text: qsTr("Color Picker\tI"); onTriggered: controller.tool = AppController.EyedropperTool }
        }
        Menu {
            title: qsTr("&Settings")
            Action { text: qsTr("Preferences…"); enabled: false }
        }
        Menu {
            title: qsTr("&Window")
            Action { text: qsTr("Show Dockers"); enabled: false }
        }
        Menu {
            title: qsTr("&Help")
            Action { text: qsTr("About Prism"); enabled: false }
        }
    }

    // ---------------- toolbar ----------------
    header: ToolBar {
        height: 34
        background: Rectangle {
            color: T.window
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: T.border }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            spacing: 6

            // small file/edit action buttons
            Repeater {
                model: [
                    { g: "📄", tip: qsTr("New"),        act: "new" },
                    { g: "📂", tip: qsTr("Open"),       act: "open" },
                    { g: "💾", tip: qsTr("Save"),       act: "save" }
                ]
                delegate: Rectangle {
                    width: 24; height: 24
                    color: fh.hovered ? T.buttonHover : "transparent"
                    Text { anchors.centerIn: parent; text: modelData.g; font.pixelSize: 13 }
                    HoverHandler { id: fh }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (modelData.act === "new") newDocDialog.open()
                            else if (modelData.act === "open") openDialog.open()
                            else saveDialog.open()
                        }
                    }
                    ToolTip.visible: fh.hovered
                    ToolTip.text: modelData.tip
                }
            }

            Rectangle { width: 1; height: 20; color: T.border }

            Repeater {
                model: [
                    { g: "↶", tip: qsTr("Undo"),    act: "undo", on: controller.canUndo },
                    { g: "↷", tip: qsTr("Redo"),    act: "redo", on: controller.canRedo }
                ]
                delegate: Rectangle {
                    width: 24; height: 24
                    opacity: modelData.on ? 1.0 : 0.4
                    color: uh.hovered && modelData.on ? T.buttonHover : "transparent"
                    Text { anchors.centerIn: parent; text: modelData.g; color: T.text; font.pixelSize: 14 }
                    HoverHandler { id: uh }
                    MouseArea {
                        anchors.fill: parent
                        enabled: modelData.on
                        onClicked: modelData.act === "undo" ? controller.undo() : controller.redo()
                    }
                    ToolTip.visible: uh.hovered
                    ToolTip.text: modelData.tip
                }
            }

            Rectangle { width: 1; height: 20; color: T.border }

            // current brush preset button (opens the presets docker look)
            Rectangle {
                width: 168; height: 24
                color: T.windowAlt
                border.color: T.border
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    spacing: 6
                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        fillMode: Image.PreserveAspectFit
                        source: "image://preset/" + controller.activePresetIndex
                    }
                    Text {
                        Layout.fillWidth: true
                        text: controller.brushName
                        color: T.text
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: presetsPopup.open()
                }
            }

            Rectangle { width: 1; height: 20; color: T.border }

            // Opacity + Size combos, like Krita's toolbar
            Text { text: qsTr("Opacity:"); color: T.text; font.pixelSize: 11 }
            ComboBox {
                id: opacityCombo
                Layout.preferredWidth: 74
                height: 24
                font.pixelSize: 11
                model: ["100%", "75%", "50%", "25%", "10%"]
                onActivated: controller.brushFlow = parseInt(model[currentIndex]) / 100
            }

            Text { text: qsTr("Size:"); color: T.text; font.pixelSize: 11 }
            PSpinBox {
                Layout.preferredWidth: 96
                from: 1; to: 200
                value: Math.round(controller.brushSize)
                onValueModified: v => controller.brushSize = v
            }

            Text { text: qsTr("Hardness:"); color: T.text; font.pixelSize: 11 }
            PSlider {
                Layout.preferredWidth: 80
                from: 0; to: 1
                value: controller.brushHardness
                onMoved: controller.brushHardness = value
            }

            Item { Layout.fillWidth: true }

            // color swatch (opens the picker popup)
            Rectangle {
                width: 30; height: 24
                color: controller.brushColor
                border.color: T.border
                MouseArea {
                    anchors.fill: parent
                    onClicked: colorPopup.open()
                }
            }
        }
    }

    // ---------------- body ----------------
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // left toolbox
        ToolBox {
            controller: controller
            Layout.fillHeight: true
        }

        // center: canvas viewport with grey backdrop (Krita's canvas area)
        Rectangle {
            id: canvasArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: T.canvasBackdrop

            // Fit the canvas into the viewport once the layout is known.
            Component.onCompleted: controller.fitToViewport(width - 60, height - 60)

            Flickable {
                id: flick
                anchors.fill: parent
                contentWidth: Math.max(canvasSheet.width, flick.width)
                contentHeight: Math.max(canvasSheet.height, flick.height)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                interactive: controller.tool === AppController.PanTool

                Item {
                    width: flick.contentWidth
                    height: flick.contentHeight

                    // The "paper": white sheet behind the painted pixels, so
                    // transparency reads as blank paper rather than void.
                    Item {
                        id: canvasSheet
                        width: controller.canvasWidth * controller.zoom
                        height: controller.canvasHeight * controller.zoom
                        anchors.centerIn: parent

                        Rectangle {
                            anchors.fill: parent
                            color: "#ffffff"
                            border.color: "#1c1c1c"
                            border.width: 1
                        }

                        CanvasView {
                            id: canvasItem
                            anchors.fill: parent
                            controller: controller
                        }

                        MouseArea {
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                            cursorShape: controller.tool === AppController.PanTool
                                         ? Qt.OpenHandCursor : Qt.CrossCursor

                            property real lastX: 0
                            property real lastY: 0

                            function canvasX(mx) { return mx / controller.zoom }
                            function canvasY(my) { return my / controller.zoom }

                            onPressed: mouse => {
                                const cx = canvasX(mouse.x)
                                const cy = canvasY(mouse.y)
                                if (mouse.button === Qt.MiddleButton
                                        || controller.tool === AppController.PanTool) {
                                    lastX = mouse.x
                                    lastY = mouse.y
                                    return
                                }
                                if (controller.tool === AppController.FillTool) {
                                    controller.fillAt(cx, cy)
                                } else if (controller.tool === AppController.EyedropperTool) {
                                    controller.pickColorAt(cx, cy, true)
                                } else {
                                    controller.beginStroke(cx, cy)
                                }
                            }

                            onPositionChanged: mouse => {
                                const cx = canvasX(mouse.x)
                                const cy = canvasY(mouse.y)
                                if (mouse.buttons & Qt.MiddleButton
                                        || controller.tool === AppController.PanTool) {
                                    flick.contentX -= (mouse.x - lastX)
                                    flick.contentY -= (mouse.y - lastY)
                                    return
                                }
                                if (controller.tool === AppController.EyedropperTool) {
                                    if (mouse.buttons & Qt.LeftButton)
                                        controller.pickColorAt(cx, cy, true)
                                    return
                                }
                                if (pressed && (controller.tool === AppController.BrushTool
                                                || controller.tool === AppController.EraserTool))
                                    controller.extendStroke(cx, cy)
                            }

                            onReleased: {
                                if (controller.tool === AppController.BrushTool
                                        || controller.tool === AppController.EraserTool)
                                    controller.endStroke()
                            }
                        }
                    }
                }
            }
        }

        // right: dockers. Heights are proportional so all three always fit
        // the available column — each docker scrolls its own content.
        Rectangle {
            id: dockerArea
            Layout.preferredWidth: T.dockerWidth
            Layout.fillHeight: true
            color: T.window

            Column {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 2

                ColorSelector {
                    controller: controller
                    width: parent.width
                    height: dockerArea.height * 0.44
                }
                LayersDocker {
                    controller: controller
                    width: parent.width
                    height: dockerArea.height * 0.26 - 3
                }
                BrushPresetsDocker {
                    controller: controller
                    width: parent.width
                    height: dockerArea.height * 0.30 - 4
                }
            }
        }
    }

    // ---------------- footer / status bar ----------------
    footer: Rectangle {
        height: 24
        color: T.windowAlt

        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: T.border }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 12

            Text {
                text: controller.brushName
                color: T.text
                font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
            Text {
                text: controller.statusInfo
                color: T.textDim
                font.pixelSize: 11
            }
            Item { Layout.fillWidth: true }
            Text {
                text: qsTr("%1 × %2 px").arg(controller.canvasWidth).arg(controller.canvasHeight)
                color: T.textDim
                font.pixelSize: 11
            }
            Text {
                text: qsTr("0.00°")
                color: T.textDim
                font.pixelSize: 11
            }
            ComboBox {
                width: 108; height: 20
                font.pixelSize: 10
                model: ["Fit View (100%)", "25%", "50%", "100%", "200%", "400%", "800%"]
                onActivated: {
                    const z = [1.0, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0]
                    controller.zoom = z[currentIndex]
                }
            }
            PSlider {
                width: 90
                from: 0.05; to: 8
                value: controller.zoom
                onMoved: controller.zoom = value
            }
        }
    }

    // ---------------- dialogs & popups ----------------
    NewDocumentDialog { id: newDocDialog; controller: controller }

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
            const p = selectedFile.toString().toLowerCase()
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

    // brush preset dropdown from the toolbar button
    Popup {
        id: presetsPopup
        x: 200; y: 40
        width: 260; height: 300
        padding: 4
        background: Rectangle { color: T.windowAlt; border.color: T.border }

        GridView {
            anchors.fill: parent
            cellWidth: 50; cellHeight: 50
            clip: true
            model: controller.presetCount()
            currentIndex: controller.activePresetIndex
            delegate: Rectangle {
                width: 48; height: 48
                color: parent.currentIndex === index ? T.highlight : T.panelSunken
                border.color: T.border
                Image {
                    anchors.fill: parent
                    anchors.margins: 3
                    fillMode: Image.PreserveAspectFit
                    source: "image://preset/" + index
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        controller.selectPreset(index)
                        presetsPopup.close()
                    }
                }
                ToolTip.visible: ph.hovered
                ToolTip.text: controller.presetName(index)
                HoverHandler { id: ph }
            }
        }
    }

    // compact color popup from the toolbar swatch
    Popup {
        id: colorPopup
        x: root.width - 330; y: 40
        width: 300; height: 420
        padding: 0
        background: Rectangle { color: T.window; border.color: T.border }
        ColorSelector { controller: controller; anchors.fill: parent }
    }
}
