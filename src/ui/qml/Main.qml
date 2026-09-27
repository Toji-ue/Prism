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
    title: controller.windowTitle
    visible: true
    color: T.window

    // Injected from C++ so the image providers share this instance.
    required property AppController controller

    Component.onCompleted: showMaximized()

    // Opens the New Document dialog. Also used by the UI verification hook
    // (prism --demo-new-dialog --screenshot=...).
    function openNewDocumentDialog() { newDocDialog.open() }

    // Start page is shown until a document exists (or File ? Start Page).
    StartPage {
        id: startPage
        anchors.fill: parent
        z: 100
        visible: root.controller.startPageVisible
        controller: root.controller
        onRequestNew: newDocDialog.open()
        onRequestOpen: openDialog.open()
    }

    // Switches the Advanced Color Selector docker to its Tool Options tab.
    function showToolOptions() { mainColorSelector.tab = 1 }

    // Verification hook (prism --demo-cursor): pins the brush ring at a
    // canvas position so a screenshot can prove its size and colour, since
    // Windows will not deliver hover events to a non-foreground window.
    property var demoPointer: null
    function demoCursor(x, y) { demoPointer = Qt.point(x, y) }

    // The active tool's keyboard shortcut, for the status bar hint.
    readonly property string toolHint: {
        switch (controller.tool) {
        case AppController.BrushTool: return qsTr("Freehand Brush")
        case AppController.EraserTool: return qsTr("Eraser")
        case AppController.FillTool: return qsTr("Fill")
        case AppController.EyedropperTool: return qsTr("Color Picker")
        case AppController.PanTool: return qsTr("Pan")
        case AppController.ZoomTool: return qsTr("Zoom")
        case AppController.RectSelectTool: return qsTr("Rectangular Select")
        case AppController.MoveTool: return qsTr("Move Layer")
        }
        return ""
    }

    // ---------------- shortcuts ----------------
    Shortcut { sequence: "Ctrl+Z"; onActivated: controller.undo() }
    Shortcut { sequence: "Ctrl+Shift+Z"; onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+Y"; onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+S"; onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+O"; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+N"; onActivated: newDocDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+N"; onActivated: controller.addLayer() }
    Shortcut { sequence: "Ctrl+E"; onActivated: controller.exportPng ? exportDialog.open() : {} }
    Shortcut { sequence: "Ctrl+A"; onActivated: controller.selectAll() }
    Shortcut { sequence: "Ctrl+D"; onActivated: controller.clearSelection() }
    Shortcut { sequence: "Ctrl+Shift+I"; onActivated: controller.invertSelection() }
    Shortcut { sequence: "Ctrl+Shift+M"; onActivated: controller.mirrorHorizontal = !controller.mirrorHorizontal }
    Shortcut { sequence: "L"; onActivated: controller.tool = AppController.LassoSelectTool }
    Shortcut { sequence: "W"; onActivated: controller.tool = AppController.MagicWandTool }
    Shortcut { sequence: "M"; onActivated: controller.tool = AppController.RectSelectTool }
    Shortcut { sequence: "V"; onActivated: controller.tool = AppController.MoveTool }
    Shortcut { sequence: "Z"; onActivated: controller.tool = AppController.ZoomTool }
    Shortcut { sequence: "B"; onActivated: controller.tool = AppController.BrushTool }
    Shortcut { sequence: "E"; onActivated: controller.tool = AppController.EraserTool }
    Shortcut { sequence: "F"; onActivated: controller.tool = AppController.FillTool }
    Shortcut { sequence: "I"; onActivated: controller.tool = AppController.EyedropperTool }
    Shortcut { sequence: "+"; onActivated: controller.brushSize = Math.min(200, controller.brushSize + 2) }
    Shortcut { sequence: "-"; onActivated: controller.brushSize = Math.max(1, controller.brushSize - 2) }
    Shortcut { sequence: "Delete"; onActivated: controller.clearCanvas() }

    // ---------------- menu bar ----------------
    menuBar: MenuBar {
        // Everything except the ways to open a document is inert while the
        // start page is up, so menu entries that need a canvas are disabled.
        Menu {
            title: qsTr("&File")
            Action { text: qsTr("&New...\tCtrl+N"); onTriggered: newDocDialog.open() }
            Action { text: qsTr("&Open...\tCtrl+O"); onTriggered: openDialog.open() }
            MenuSeparator {}
            Action { text: qsTr("&Save\tCtrl+S"); onTriggered: saveDialog.open() }
            Action { text: qsTr("&Export PNG...\tCtrl+E"); onTriggered: exportDialog.open() }
            MenuSeparator {}
            Action {
                text: qsTr("&Start Page")
                onTriggered: controller.startPageVisible = true
            }
            MenuSeparator {}
            Action { text: qsTr("&Quit\tCtrl+Q"); onTriggered: Qt.quit() }
        }
        Menu {
            title: qsTr("&Edit")
            enabled: !controller.startPageVisible
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
            enabled: !controller.startPageVisible
            Action { text: qsTr("Zoom &In"); onTriggered: controller.zoom = Math.min(8, controller.zoom * 1.25) }
            Action { text: qsTr("Zoom &Out"); onTriggered: controller.zoom = Math.max(0.05, controller.zoom / 1.25) }
            Action { text: qsTr("&100%"); onTriggered: controller.zoom = 1.0 }
            Action { text: qsTr("&Fit to Window"); onTriggered: controller.zoom = 0.5 }
        }
        Menu {
            title: qsTr("&Image")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Resize Canvas..."); enabled: false }
            Action { text: qsTr("Canvas Information"); enabled: false }
        }
        Menu {
            title: qsTr("&Layer")
            enabled: !controller.startPageVisible
            Action { text: qsTr("&New Layer\tCtrl+Shift+N"); onTriggered: controller.addLayer() }
            Action { text: qsTr("&Duplicate Layer"); onTriggered: controller.duplicateLayer() }
            Action { text: qsTr("&Delete Layer"); onTriggered: controller.deleteLayer() }
            MenuSeparator {}
            Action { text: qsTr("&Raise Layer"); onTriggered: controller.moveLayerUp() }
            Action { text: qsTr("&Lower Layer"); onTriggered: controller.moveLayerDown() }
        }
        Menu {
            title: qsTr("&Select")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Select &All\tCtrl+A"); onTriggered: controller.selectAll() }
            Action { text: qsTr("&Deselect\tCtrl+D"); onTriggered: controller.clearSelection() }
            Action { text: qsTr("&Invert Selection\tCtrl+Shift+I"); onTriggered: controller.invertSelection() }
        }
        Menu {
            title: qsTr("Fi&lter")
            enabled: !controller.startPageVisible
            Action { text: qsTr("&Invert"); onTriggered: controller.applyFilter("invert") }
            Action { text: qsTr("&Desaturate"); onTriggered: controller.applyFilter("desaturate") }
            Action { text: qsTr("&Blur"); onTriggered: controller.applyFilter("blur") }
            Action {
                text: qsTr("&Brightness / Contrast")
                onTriggered: controller.applyFilter("brightness-contrast")
            }
        }
        Menu {
            title: qsTr("&Tools")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Freehand Brush\tB"); onTriggered: controller.tool = AppController.BrushTool }
            Action { text: qsTr("Eraser\tE"); onTriggered: controller.tool = AppController.EraserTool }
            Action { text: qsTr("Fill\tF"); onTriggered: controller.tool = AppController.FillTool }
            Action { text: qsTr("Color Picker\tI"); onTriggered: controller.tool = AppController.EyedropperTool }
        }
        Menu {
            title: qsTr("&Settings")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Preferences..."); enabled: false }
        }
        Menu {
            title: qsTr("&Window")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Show Dockers"); enabled: false }
        }
        Menu {
            title: qsTr("&Help")
            enabled: !controller.startPageVisible
            Action { text: qsTr("About Prism"); enabled: false }
        }
    }

    // ---------------- toolbar ----------------
    header: ToolBar {
        // Hidden on the start page: the welcome screen owns the whole window.
        visible: !controller.startPageVisible
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

            // small file action buttons
            Repeater {
                model: [
                    { icon: "file-new",  tip: qsTr("New (Ctrl+N)"),  act: "new" },
                    { icon: "file-open", tip: qsTr("Open (Ctrl+O)"), act: "open" },
                    { icon: "file-save", tip: qsTr("Save (Ctrl+S)"), act: "save" }
                ]
                delegate: Rectangle {
                    width: 24; height: 24
                    color: fh.hovered ? T.buttonHover : "transparent"
                    Icon {
                        anchors.centerIn: parent
                        width: 15; height: 15
                        name: modelData.icon
                        color: T.text
                    }
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
                    ToolTip.delay: 500
                    ToolTip.text: modelData.tip
                }
            }

            Rectangle { width: 1; height: 20; color: T.border }

            Repeater {
                model: [
                    { icon: "undo", tip: qsTr("Undo"),    act: "undo", on: controller.canUndo },
                    { icon: "redo", tip: qsTr("Redo"),    act: "redo", on: controller.canRedo }
                ]
                delegate: Rectangle {
                    width: 24; height: 24
                    opacity: modelData.on ? 1.0 : 0.4
                    color: uh.hovered && modelData.on ? T.buttonHover : "transparent"
                    Icon {
                        anchors.centerIn: parent
                        width: 15; height: 15
                        name: modelData.icon
                        color: T.text
                    }
                    HoverHandler { id: uh }
                    MouseArea {
                        anchors.fill: parent
                        enabled: modelData.on
                        onClicked: modelData.act === "undo" ? controller.undo() : controller.redo()
                    }
                    ToolTip.visible: uh.hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.tip
                }
            }

            Rectangle { width: 1; height: 20; color: T.border }

            // current brush preset button (opens the preset grid)
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
                        cache: false
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
                    Icon {
                        Layout.preferredWidth: 12
                        Layout.preferredHeight: 12
                        name: "chevron-down"
                        color: T.textDim
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: presetsPopup.open()
                }
            }

            Rectangle { width: 1; height: 20; color: T.border }

            // Opacity (brush flow) as a slider, with the value beside it.
            // Bound to brushFlow so presets and the docker stay in sync.
            Text { text: qsTr("Opacity:"); color: T.text; font.pixelSize: 11 }
            PSlider {
                Layout.preferredWidth: 96
                from: 0.05; to: 1
                value: controller.brushFlow
                onMoved: controller.brushFlow = value
            }
            Text {
                Layout.preferredWidth: 34
                text: Math.round(controller.brushFlow * 100) + "%"
                color: T.text
                font.pixelSize: 11
            }

            Text { text: qsTr("Size:"); color: T.text; font.pixelSize: 11 }
            PSpinBox {
                Layout.preferredWidth: 84
                from: 1; to: 200
                value: Math.round(controller.brushSize)
                onValueModified: v => controller.brushSize = v
            }
            // Direct size slider - the fastest way to size a brush.
            PSlider {
                Layout.preferredWidth: 110
                from: 1; to: 120
                value: Math.min(120, controller.brushSize)
                onMoved: controller.brushSize = Math.round(value)
            }

            Text { text: qsTr("Hardness:"); color: T.text; font.pixelSize: 11 }
            PSlider {
                Layout.preferredWidth: 80
                from: 0; to: 1
                value: controller.brushHardness
                onMoved: controller.brushHardness = value
            }

            Rectangle { width: 1; height: 20; color: T.border }

            // Mirror canvas (Krita's mirror tools)
            Repeater {
                model: [
                    { icon: "mirror-h", tip: qsTr("Mirror canvas horizontally (Ctrl+Shift+M)"), act: "h" },
                    { icon: "mirror-v", tip: qsTr("Mirror canvas vertically"), act: "v" },
                    { icon: "invert",   tip: qsTr("Invert selection (Ctrl+Shift+I)"), act: "inv" }
                ]
                delegate: Rectangle {
                    width: 24; height: 24
                    readonly property bool on: (modelData.act === "h" && root.controller.mirrorHorizontal)
                                               || (modelData.act === "v" && root.controller.mirrorVertical)
                    color: on ? T.highlight : (mh.hovered ? T.buttonHover : "transparent")
                    Icon {
                        anchors.centerIn: parent
                        width: 15; height: 15
                        name: modelData.icon
                        color: on ? T.highlightText : T.text
                    }
                    HoverHandler { id: mh }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (modelData.act === "h") root.controller.mirrorHorizontal = !root.controller.mirrorHorizontal
                            else if (modelData.act === "v") root.controller.mirrorVertical = !root.controller.mirrorVertical
                            else root.controller.invertSelection()
                        }
                    }
                    ToolTip.visible: mh.hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.tip
                }
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
            controller: root.controller
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
                // Panning is handled explicitly by the canvas MouseArea (and by
                // the middle mouse button with any tool). Leaving the Flickable
                // interactive made it steal drags and fight that logic, which
                // is why the Pan tool felt dead.
                interactive: false

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
                            color: T.light
                            border.color: T.dark
                            border.width: 1
                        }

                        CanvasView {
                            id: canvasItem
                            anchors.fill: parent
                            controller: root.controller
                        }

                        // Selection tint - works for any shape (rect, lasso,
                        // magic wand) because it renders the selection mask.
                        Image {
                            anchors.fill: parent
                            visible: root.controller.hasSelection
                            fillMode: Image.Stretch
                            cache: false
                            source: root.controller.hasSelection
                                    ? "image://selection/" + root.controller.selectionRevision
                                    : ""
                        }

                        // Mirror indicators, like Krita's mirror lines.
                        Rectangle {
                            visible: root.controller.mirrorHorizontal
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 1; height: parent.height
                            color: T.tint(0.8)
                            opacity: 0.6
                        }
                        Rectangle {
                            visible: root.controller.mirrorVertical
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width; height: 1
                            color: T.tint(0.8)
                            opacity: 0.6
                        }

                        MouseArea {
                            id: canvasMouse
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                            // The brush draws its own grey size ring; the
                            // system cursor is hidden while painting so the
                            // ring is the only pointer.
                            cursorShape: {
                                switch (controller.tool) {
                                case AppController.BrushTool:
                                case AppController.EraserTool:
                                    return Qt.BlankCursor
                                case AppController.PanTool: return Qt.OpenHandCursor
                                case AppController.ZoomTool: return Qt.PointingHandCursor
                                case AppController.MoveTool: return Qt.SizeAllCursor
                                }
                                return Qt.CrossCursor
                            }
                            hoverEnabled: true

                            // ---- grey brush cursor ----
                            Rectangle {
                                id: brushCursor
                                visible: (controller.tool === AppController.BrushTool
                                          || controller.tool === AppController.EraserTool)
                                         && (canvasMouse.containsMouse || root.demoPointer !== null)
                                width: Math.max(4, controller.brushSize * 2 * controller.zoom)
                                height: width
                                radius: width / 2
                                color: "transparent"
                                border.color: T.cursorRing
                                border.width: 1
                                readonly property real pointerX: root.demoPointer !== null
                                                                 ? root.demoPointer.x
                                                                 : canvasMouse.mouseX
                                readonly property real pointerY: root.demoPointer !== null
                                                                 ? root.demoPointer.y
                                                                 : canvasMouse.mouseY
                                x: pointerX - width / 2
                                y: pointerY - height / 2

                                // inner dark ring keeps it readable on white
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: parent.width - 2
                                    height: width
                                    radius: width / 2
                                    color: "transparent"
                                    border.color: T.cursorRingInner
                                    border.width: 1
                                }
                            }

                            property real lastX: 0
                            property real lastY: 0
                            property real startX: 0
                            property real startY: 0
                            property bool draggingLayer: false
                            /// Freehand lasso points collected during a drag.
                            property var lassoPoints: []

                            // Mirroring flips what the artist sees, so input
                            // must be flipped the same way to keep the brush
                            // under the cursor.
                            function canvasX(mx) {
                                const x = mx / controller.zoom
                                return controller.mirrorHorizontal
                                       ? controller.canvasWidth - x : x
                            }
                            function canvasY(my) {
                                const y = my / controller.zoom
                                return controller.mirrorVertical
                                       ? controller.canvasHeight - y : y
                            }

                            onPressed: mouse => {
                                const cx = canvasX(mouse.x)
                                const cy = canvasY(mouse.y)
                                startX = cx
                                startY = cy
                                lastX = mouse.x
                                lastY = mouse.y
                                draggingLayer = false

                                const panning = mouse.button === Qt.MiddleButton
                                                || controller.tool === AppController.PanTool
                                if (panning)
                                    return

                                switch (controller.tool) {
                                case AppController.FillTool:
                                    controller.fillAt(cx, cy)
                                    break
                                case AppController.EyedropperTool:
                                    controller.pickColorAt(cx, cy, true)
                                    break
                                case AppController.ZoomTool:
                                    controller.zoomStep(mouse.modifiers & Qt.ControlModifier)
                                    break
                                case AppController.MoveTool:
                                    draggingLayer = true
                                    break
                                case AppController.RectSelectTool:
                                    controller.setSelection(cx, cy, 0, 0)
                                    break
                                case AppController.EllipseSelectTool:
                                    controller.setEllipseSelection(cx, cy, 0, 0)
                                    break
                                case AppController.LassoSelectTool:
                                case AppController.PolygonSelectTool:
                                    lassoPoints = [Qt.point(cx, cy)]
                                    break
                                case AppController.MagicWandTool:
                                    controller.magicWandAt(cx, cy, 32)
                                    break
                                case AppController.SimilarColorSelectTool:
                                    controller.similarColorAt(cx, cy, 32)
                                    break
                                case AppController.LineTool:
                                case AppController.RectangleTool:
                                case AppController.EllipseTool:
                                case AppController.GradientTool:
                                    controller.beginShape(cx, cy)
                                    break
                                default:
                                    controller.beginStroke(cx, cy)
                                    break
                                }
                            }

                            onPositionChanged: mouse => {
                                const cx = canvasX(mouse.x)
                                const cy = canvasY(mouse.y)
                                const panning = (mouse.buttons & Qt.MiddleButton)
                                                || controller.tool === AppController.PanTool
                                if (panning) {
                                    flick.contentX -= (mouse.x - lastX)
                                    flick.contentY -= (mouse.y - lastY)
                                    return
                                }
                                if (controller.tool === AppController.EyedropperTool) {
                                    if (mouse.buttons & Qt.LeftButton)
                                        controller.pickColorAt(cx, cy, true)
                                    return
                                }
                                if (!pressed)
                                    return

                                switch (controller.tool) {
                                case AppController.RectSelectTool:
                                    controller.setSelection(startX, startY, cx - startX, cy - startY)
                                    break
                                case AppController.EllipseSelectTool:
                                    controller.setEllipseSelection(startX, startY, cx - startX,
                                                                   cy - startY)
                                    break
                                case AppController.LassoSelectTool:
                                case AppController.PolygonSelectTool: {
                                    // Sample sparsely: the polygon only needs
                                    // enough points to describe its outline.
                                    const pts = lassoPoints.slice()
                                    const last = pts[pts.length - 1]
                                    if (!last || Math.abs(last.x - cx) + Math.abs(last.y - cy) > 3) {
                                        pts.push(Qt.point(cx, cy))
                                        lassoPoints = pts
                                    }
                                    break
                                }
                                case AppController.LineTool:
                                case AppController.RectangleTool:
                                case AppController.EllipseTool:
                                case AppController.GradientTool:
                                    controller.updateShape(cx, cy)
                                    break
                                case AppController.BrushTool:
                                case AppController.EraserTool:
                                    controller.extendStroke(cx, cy)
                                    break
                                }
                            }

                            onReleased: mouse => {
                                const cx = canvasX(mouse.x)
                                const cy = canvasY(mouse.y)
                                if (controller.tool === AppController.MoveTool && draggingLayer) {
                                    const dx = Math.round(cx - startX)
                                    const dy = Math.round(cy - startY)
                                    if (dx !== 0 || dy !== 0)
                                        controller.moveActiveLayer(dx, dy)
                                    draggingLayer = false
                                    return
                                }
                                if (controller.tool === AppController.LineTool
                                        || controller.tool === AppController.RectangleTool
                                        || controller.tool === AppController.EllipseTool
                                        || controller.tool === AppController.GradientTool) {
                                    controller.endShape()
                                    return
                                }
                                if (controller.tool === AppController.LassoSelectTool
                                        || controller.tool === AppController.PolygonSelectTool) {
                                    if (lassoPoints.length >= 3)
                                        controller.setLassoSelection(lassoPoints)
                                    lassoPoints = []
                                    return
                                }
                                if (controller.tool === AppController.BrushTool
                                        || controller.tool === AppController.EraserTool)
                                    controller.endStroke()
                            }
                        }

                        // Selection outline (dashed, like a marching-ants frame).
                        Rectangle {
                            visible: controller.hasSelection
                            x: controller.selectionX * controller.zoom
                            y: controller.selectionY * controller.zoom
                            width: controller.selectionW * controller.zoom
                            height: controller.selectionH * controller.zoom
                            color: "transparent"
                            border.color: T.light
                            border.width: 1

                            // dark inner line keeps the selection readable on
                            // white and on dark artwork alike
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                color: "transparent"
                                border.color: T.dark
                                border.width: 1
                            }
                        }
                    }
                }
            }
        }

        // right: dockers. Heights are proportional so all three always fit
        // the available column - each docker scrolls its own content.
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
                    id: mainColorSelector
                    controller: root.controller
                    width: parent.width
                    height: dockerArea.height * 0.44
                }
                LayersDocker {
                    controller: root.controller
                    width: parent.width
                    height: dockerArea.height * 0.26 - 3
                }
                BrushPresetsDocker {
                    controller: root.controller
                    width: parent.width
                    height: dockerArea.height * 0.30 - 4
                }
            }
        }
    }

    // ---------------- footer / status bar ----------------
    footer: Rectangle {
        visible: !controller.startPageVisible
        height: 24
        color: T.windowAlt

        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: T.border }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 12

            Text {
                text: root.toolHint + "  -  " + controller.brushName
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
                text: qsTr("%1 ?- %2 px").arg(controller.canvasWidth).arg(controller.canvasHeight)
                color: T.textDim
                font.pixelSize: 11
            }
            Text {
                text: qsTr("Selection: %1 ?- %2").arg(controller.selectionW).arg(controller.selectionH)
                visible: controller.hasSelection
                color: T.highlight
                font.pixelSize: 11
            }
            Text {
                text: qsTr("0.00deg")
                color: T.textDim
                font.pixelSize: 11
            }
            PComboBox {
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
    NewDocumentDialog { id: newDocDialog; controller: root.controller }

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
        ColorSelector { controller: root.controller; anchors.fill: parent }
    }
}
