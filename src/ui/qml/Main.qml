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

    /// Which docker the right-hand column shows: 0 Layers, 1 Brush, 2 Colour.
    property int dockerTab: 0

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

    // Verification hook (prism --demo-menu): opens a dropdown so its colours
    // can be checked from a screenshot.
    function demoOpenMenu() { fileMenu.open() }

    // Drives the exact call the import menu entries make, without a native
    // file dialog (used by --demo-import-layer for UI-path verification).
    function demoImportLayer(path) {
        console.info("demo: QML calling importImageAsLayer(" + path + ")")
        var ok = controller.importImageAsLayer(path)
        console.info("demo: importImageAsLayer returned " + ok)
    }

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
    // Guarded like the menu items: an undo with an empty history used to throw
    // out of the controller and take the app down, so a stray Ctrl+Shift+Z
    // crashed it.
    Shortcut { sequence: "Ctrl+Z"; enabled: controller.canUndo; onActivated: controller.undo() }
    Shortcut {
        sequence: "Ctrl+Shift+Z"
        enabled: controller.canRedo
        onActivated: controller.redo()
    }
    Shortcut { sequence: "Ctrl+Y"; enabled: controller.canRedo; onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+S"; onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+O"; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+N"; onActivated: newDocDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+N"; onActivated: controller.addLayer() }
    Shortcut { sequence: "Ctrl+E"; onActivated: controller.exportPng ? exportDialog.open() : {} }
    Shortcut { sequence: "Ctrl+A"; onActivated: controller.selectAll() }
    Shortcut { sequence: "Ctrl+D"; onActivated: controller.clearSelection() }
    Shortcut { sequence: "Ctrl+Shift+I"; onActivated: controller.invertSelection() }
    Shortcut { sequence: "Ctrl+Shift+M"; onActivated: controller.mirrorHorizontal = !controller.mirrorHorizontal }
    // Finish an in-progress click path (polyline / bezier / selection paths).
    Shortcut {
        sequence: "Return"
        onActivated: {
            // A crop frame is a pending edit: Enter applies what is on screen.
            if (controller.tool === AppController.CropTool && canvasMouse.cropRect) {
                const r = canvasMouse.cropRect
                canvasMouse.cropRect = null
                controller.clearSelection()
                controller.cutSelectionToLayer(r.x, r.y, r.width, r.height)
                return
            }
            const t = controller.tool
            if (t === AppController.PolylineTool) controller.finishPath(0)
            else if (t === AppController.BezierTool) controller.finishPath(1)
            else if (t === AppController.BezierSelectTool) controller.finishPath(2)
            else if (t === AppController.MagneticSelectTool) controller.finishPath(3)
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (canvasMouse.cropRect) {
                canvasMouse.cropRect = null
                controller.clearSelection()
            }
            controller.cancelPath()
        }
    }
    Shortcut { sequence: "+"; onActivated: controller.brushSize = Math.min(200, controller.brushSize + 2) }
    Shortcut { sequence: "-"; onActivated: controller.brushSize = Math.max(1, controller.brushSize - 2) }
    Shortcut { sequence: "Delete"; onActivated: controller.clearCanvas() }

    // Leaving the crop tool commits the pending frame, like Photoshop: picking
    // another tool is how most people finish a crop ("drag a frame, then grab
    // the Move tool to place the image"). Discarding it silently meant the crop
    // never happened at all.
    Connections {
        target: controller
        function onToolChanged() {
            if (controller.tool === AppController.CropTool)
                return
            if (!canvasMouse.cropRect)
                return
            const r = canvasMouse.cropRect
            canvasMouse.cropRect = null
            controller.clearSelection()
            controller.cutSelectionToLayer(r.x, r.y, r.width, r.height)
        }
    }

    // Tool hotkeys are user-configurable (Settings > Configure Tool
    // Shortcuts). The list comes from the controller; the Instantiator
    // rebuilds the Shortcut objects whenever an assignment changes.
    Instantiator {
        model: root.controller.toolShortcutList
        delegate: Shortcut {
            enabled: modelData.key.length > 0
            sequence: modelData.key
            context: Qt.ApplicationShortcut
            onActivated: root.controller.tool = modelData.tool
        }
    }

    // ---------------- menu bar ----------------
    menuBar: MenuBar {
        // Everything except the ways to open a document is inert while the
        // start page is up, so menu entries that need a canvas are disabled.
        PMenu {
            id: fileMenu
            title: qsTr("&File")
            Action { text: qsTr("&New...\tCtrl+N"); onTriggered: newDocDialog.open() }
            Action { text: qsTr("&Open...\tCtrl+O"); onTriggered: openDialog.open() }
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
        PMenu {
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
        PMenu {
            title: qsTr("&View")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Zoom &In"); onTriggered: controller.zoom = Math.min(8, controller.zoom * 1.25) }
            Action { text: qsTr("Zoom &Out"); onTriggered: controller.zoom = Math.max(0.05, controller.zoom / 1.25) }
            Action { text: qsTr("&100%"); onTriggered: controller.zoom = 1.0 }
            Action { text: qsTr("&Fit to Window"); onTriggered: controller.zoom = 0.5 }
        }
        PMenu {
            title: qsTr("&Image")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Resize Canvas..."); enabled: false }
            Action { text: qsTr("Canvas Information"); enabled: false }
        }
        PMenu {
            title: qsTr("&Select")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Select &All\tCtrl+A"); onTriggered: controller.selectAll() }
            Action { text: qsTr("&Deselect\tCtrl+D"); onTriggered: controller.clearSelection() }
            Action { text: qsTr("&Invert Selection\tCtrl+Shift+I"); onTriggered: controller.invertSelection() }
        }
        PMenu {
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
        PMenu {
            title: qsTr("&Tools")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Freehand Brush\tB"); onTriggered: controller.tool = AppController.BrushTool }
            Action { text: qsTr("Eraser\tE"); onTriggered: controller.tool = AppController.EraserTool }
            Action { text: qsTr("Fill\tF"); onTriggered: controller.tool = AppController.FillTool }
            Action { text: qsTr("Color Picker\tI"); onTriggered: controller.tool = AppController.EyedropperTool }
        }
        PMenu {
            title: qsTr("&Settings")
            enabled: !controller.startPageVisible
            Action {
                text: qsTr("Configure Tool Shortcuts...")
                onTriggered: shortcutDialog.open()
            }
        }
        PMenu {
            title: qsTr("&Window")
            enabled: !controller.startPageVisible
            Action { text: qsTr("Show Dockers"); enabled: false }
        }
        PMenu {
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
                    { icon: "undo", tip: qsTr("Undo (Ctrl+Z)"),    act: "undo", on: controller.canUndo },
                    { icon: "redo", tip: qsTr("Redo (Ctrl+Shift+Z or Ctrl+Y)"), act: "redo", on: controller.canRedo }
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

            // Wheel = zoom in/out (Ctrl+wheel also works), anchored on the
            // pointer so the point under the cursor stays put.
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: event => {
                    const factor = event.angleDelta.y > 0 ? 1.15 : 1 / 1.15
                    controller.zoomAt(0, 0, factor)
                    event.accepted = true
                }
            }

            // Zoom level readout, top-left of the drawing area (Krita shows it
            // near the canvas, not only in the status bar).
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 8
                width: zoomLabel.implicitWidth + 12
                height: 20
                color: T.popup
                border.color: T.popupBorder
                border.width: 1
                Text {
                    id: zoomLabel
                    anchors.centerIn: parent
                    text: Math.round(controller.zoom * 100) + "%"
                    color: T.text
                    font.pixelSize: 11
                }
            }

            Flickable {
                id: flick
                objectName: "canvasFlick"
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
                            objectName: "canvasItem"
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
                            objectName: "canvasMouseArea"
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                            // Clicking the canvas takes focus back from whatever
                            // text field was last used. Window shortcuts (Ctrl+Z,
                            // Ctrl+S...) are swallowed by a focused TextInput —
                            // Qt sends it a ShortcutOverride that it accepts — so
                            // without this, undo kept editing the last text field
                            // instead of the document, "working sometimes".
                            focus: true
                            /// Right button held = temporary pan (Krita-style).
                            property bool rightPanning: false
                            // The brush draws its own grey size ring; the
                            // system cursor is hidden while painting so the
                            // ring is the only pointer.
                            cursorShape: {
                                // Cursor-leak fix lives in StartPage.qml (its
                                // background MouseArea pins an arrow cursor).
                                if (canvasMouse.rightPanning)
                                    return Qt.ClosedHandCursor
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
                                         && !canvasMouse.rightPanning
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
                            /// Crop marquee in canvas coordinates (Photoshop
                            /// behaviour): the drag draws the frame, then it can
                            /// be dragged into place and Enter applies it.
                            /// Cropping on mouse-up made a misplaced frame
                            /// final the instant the button came up.
                            property var cropRect: null
                            property bool cropMoving: false
                            property real cropGrabX: 0
                            property real cropGrabY: 0
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
                                // Explicit, not left to activeFocusOnPress: a
                                // focused text field keeps active focus for the
                                // whole press otherwise, and every window
                                // shortcut stayed routed to that field.
                                forceActiveFocus()
                                // Right button alone is a temporary pan; it must
                                // not start a stroke on its way through here.
                                if (mouse.button === Qt.RightButton) {
                                    rightPanning = true
                                    startX = canvasX(mouse.x)
                                    startY = canvasY(mouse.y)
                                    lastX = mouse.x
                                    lastY = mouse.y
                                    return
                                }
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
                                case AppController.LassoFillTool:
                                    controller.beginLassoFill(cx, cy)
                                    break
                                case AppController.BlendTool:
                                    controller.beginStroke(cx, cy)
                                    break
                                case AppController.EyedropperTool:
                                    controller.pickColorAt(cx, cy, true)
                                    break
                                case AppController.ZoomTool:
                                    controller.zoomStep(mouse.modifiers & Qt.ControlModifier)
                                    break
                                case AppController.MoveTool:
                                    draggingLayer = true
                                    controller.beginLayerMove()
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
                                // Click-based paths: collect points, finish with Enter.
                                case AppController.PolylineTool:
                                case AppController.BezierTool:
                                    controller.addPathPoint(cx, cy)
                                    break
                                case AppController.FreehandPathTool:
                                    controller.beginFreePath(cx, cy)
                                    break
                                case AppController.CropTool: {
                                    // Inside the existing frame: drag it whole.
                                    const r = canvasMouse.cropRect
                                    if (r && cx >= r.x && cx <= r.x + r.width
                                            && cy >= r.y && cy <= r.y + r.height) {
                                        canvasMouse.cropMoving = true
                                        canvasMouse.cropGrabX = cx - r.x
                                        canvasMouse.cropGrabY = cy - r.y
                                    } else {
                                        canvasMouse.cropMoving = false
                                        canvasMouse.cropRect = Qt.rect(cx, cy, 0, 0)
                                        controller.setSelection(cx, cy, 0, 0)
                                    }
                                    break
                                }
                                case AppController.TextTool:
                                    textPopup.pendingX = cx
                                    textPopup.pendingY = cy
                                    textPopup.open()
                                    break
                                case AppController.TransformTool:
                                    draggingLayer = true
                                    break
                                case AppController.DynamicBrushTool:
                                    controller.beginStroke(cx, cy)
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
                                const panning = (mouse.buttons & (Qt.MiddleButton | Qt.RightButton))
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
                                case AppController.FreehandPathTool:
                                    controller.extendFreePath(cx, cy)
                                    break
                                case AppController.LassoFillTool:
                                    controller.extendLassoFill(cx, cy)
                                    break
                                case AppController.CropTool: {
                                    if (canvasMouse.cropMoving && canvasMouse.cropRect) {
                                        const r = canvasMouse.cropRect
                                        const nx = Math.max(0, Math.min(controller.canvasWidth
                                                                        - r.width,
                                                                        cx - canvasMouse.cropGrabX))
                                        const ny = Math.max(0, Math.min(controller.canvasHeight
                                                                        - r.height,
                                                                        cy - canvasMouse.cropGrabY))
                                        canvasMouse.cropRect = Qt.rect(nx, ny, r.width, r.height)
                                        controller.setSelection(nx, ny, r.width, r.height)
                                    } else {
                                        canvasMouse.cropRect =
                                            Qt.rect(Math.min(startX, cx), Math.min(startY, cy),
                                                    Math.abs(cx - startX), Math.abs(cy - startY))
                                        controller.setSelection(startX, startY, cx - startX,
                                                                cy - startY)
                                    }
                                    break
                                }
                                case AppController.MoveTool:
                                    // Live drag: the layer follows the pointer (it
                                    // used to jump only on release, which reads
                                    // as "this layer cannot be moved").
                                    controller.previewLayerMove(Math.round(cx - startX),
                                                                Math.round(cy - startY))
                                    break
                                case AppController.BrushTool:
                                case AppController.EraserTool:
                                case AppController.BlendTool:
                                case AppController.DynamicBrushTool:
                                    controller.extendStroke(cx, cy)
                                    break
                                }
                            }

                            onDoubleClicked: {
                                // Double-clicking inside the crop frame is the
                                // other way Photoshop finishes a crop.
                                if (controller.tool !== AppController.CropTool
                                        || !canvasMouse.cropRect)
                                    return
                                const r = canvasMouse.cropRect
                                canvasMouse.cropRect = null
                                controller.clearSelection()
                                controller.cutSelectionToLayer(r.x, r.y, r.width, r.height)
                            }

                            onReleased: mouse => {
                                const cx = canvasX(mouse.x)
                                const cy = canvasY(mouse.y)
                                // Ending a right-button pan must not fall through
                                // to the stroke handling below: no stroke was
                                // started, and "endStroke" there would end one
                                // that belongs to a different press.
                                if (mouse.button === Qt.RightButton) {
                                    canvasMouse.rightPanning = false
                                    return
                                }
                                // The picker is a detour, not a destination:
                                // releasing hands the canvas back to the brush
                                // the artist was drawing with (the picked colour
                                // is already applied).
                                if (controller.tool === AppController.EyedropperTool) {
                                    controller.endEyedropper()
                                    return
                                }
                                if (controller.tool === AppController.MoveTool && draggingLayer) {
                                    controller.endLayerMove()
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
                                if (controller.tool === AppController.FreehandPathTool) {
                                    controller.endFreePath()
                                    return
                                }
                                if (controller.tool === AppController.LassoFillTool) {
                                    controller.endLassoFill()
                                    return
                                }
                                if (controller.tool === AppController.CropTool) {
                                    // The frame stays: putting it in place is part
                                    // of the gesture, Enter applies it (Photoshop).
                                    canvasMouse.cropMoving = false
                                    controller.setStatusMessage(qsTr("Drag the frame to position it, Enter to crop, Esc to cancel."))
                                    return
                                }
                                if (controller.tool === AppController.TransformTool && draggingLayer) {
                                    // Horizontal drag rotates, vertical drag scales.
                                    const deg = (cx - startX) * 0.5
                                    const sc = Math.max(0.1, 1.0 + (cy - startY) / 200.0)
                                    controller.transformLayer(deg, sc)
                                    draggingLayer = false
                                    return
                                }
                                if (controller.tool === AppController.PolylineTool
                                        || controller.tool === AppController.BezierTool
                                        || controller.tool === AppController.BezierSelectTool
                                        || controller.tool === AppController.MagneticSelectTool) {
                                    return // click-based: waits for Enter / right click
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

                // One tab bar for the whole column. Three dockers used to stack
                // with fixed proportions, so the one used constantly (Layers)
                // always had the least room while the colour wheel held nearly
                // half the height. Tabs give the active docker the full column.
                Rectangle {
                    width: parent.width
                    height: 26
                    color: T.panel
                    radius: T.radius

                    Row {
                        anchors.fill: parent
                        anchors.margins: 2
                        spacing: 2

                        Repeater {
                            model: [qsTr("Layers"), qsTr("Brush"), qsTr("Colour")]

                            delegate: Rectangle {
                                width: (parent.width - 4) / 3
                                height: parent.height
                                radius: T.radiusSmall
                                color: root.dockerTab === index ? T.highlight
                                                                : (tabHover.hovered
                                                                   ? T.buttonHover : "transparent")
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: root.dockerTab === index ? T.highlightText : T.text
                                    font.pixelSize: 11
                                }
                                HoverHandler { id: tabHover }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: root.dockerTab = index
                                }
                            }
                        }
                    }
                }

                StackLayout {
                    width: parent.width
                    height: parent.height - 28
                    currentIndex: root.dockerTab

                    LayersDocker {
                        controller: root.controller
                        embedded: true
                    }
                    BrushPresetsDocker {
                        controller: root.controller
                        embedded: true
                    }
                    ColorSelector {
                        id: mainColorSelector
                        controller: root.controller
                    }
                }
            }
        }
    }

    // ---------------- footer / status bar ----------------
    // ---------------- footer: tool options + status ----------------
    footer: Column {
        visible: !controller.startPageVisible

        // Brush controls sit under the canvas, next to the hand that
        // uses them, instead of crowding the top bar.
        Rectangle {
            width: parent.width
            height: 30
            color: T.windowAlt
            objectName: "toolOptionsBar"
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: T.border }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 8

            Rectangle { width: 1; height: 20; color: T.border }

            // Opacity (brush flow) as a slider, with the value beside it.
            // The value is NOT bound to the controller: a binding plus a write
            // from onMoved fights the drag, and whichever side loses makes the
            // slider look dead. Sync explicitly instead, and never while the
            // user is holding the handle.
            Text { text: qsTr("Opacity:"); color: T.text; font.pixelSize: 11 }
            PSlider {
                id: flowSlider
                objectName: "flowSlider"
                Layout.preferredWidth: 96
                from: 0.05; to: 1
                Component.onCompleted: value = controller.brushFlow
                Connections {
                    target: controller
                    function onBrushChanged() {
                        if (!flowSlider.pressed)
                            flowSlider.value = controller.brushFlow
                    }
                }
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
                id: sizeBox
                objectName: "sizeBox"
                Layout.preferredWidth: 84
                from: 1; to: 200
                Component.onCompleted: value = Math.round(controller.brushSize)
                Connections {
                    target: controller
                    function onBrushChanged() {
                        if (!sizeBox.activeFocus)
                            sizeBox.value = Math.round(controller.brushSize)
                    }
                }
                onValueModified: v => controller.brushSize = v
            }
            // Direct size slider - the fastest way to size a brush.
            PSlider {
                id: sizeSlider
                objectName: "sizeSlider"
                Layout.preferredWidth: 110
                from: 1; to: 200
                Component.onCompleted: value = Math.min(200, controller.brushSize)
                Connections {
                    target: controller
                    function onBrushChanged() {
                        if (!sizeSlider.pressed)
                            sizeSlider.value = Math.min(200, controller.brushSize)
                    }
                }
                onMoved: controller.brushSize = Math.round(value)
            }

            Text { text: qsTr("Hardness:"); color: T.text; font.pixelSize: 11 }
            PSlider {
                id: hardnessSlider
                objectName: "hardnessSlider"
                Layout.preferredWidth: 80
                from: 0; to: 1
                Component.onCompleted: value = controller.brushHardness
                Connections {
                    target: controller
                    function onBrushChanged() {
                        if (!hardnessSlider.pressed)
                            hardnessSlider.value = controller.brushHardness
                    }
                }
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
            }
        }

        // Status bar
        Rectangle {
            width: parent.width
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
            // A failed open/save must be visible, never silent.
            Text {
                visible: controller.lastError.length > 0
                text: controller.lastError
                color: T.text
                font.pixelSize: 11
            }
            // Informational outcome of the last action (e.g. how much of an
            // imported image did not fit the canvas).
            Text {
                visible: controller.statusMessage.length > 0
                text: controller.statusMessage
                color: T.textDim
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
                id: zoomSlider
                objectName: "zoomSlider"
                width: 90
                from: 0.05; to: 8
                Component.onCompleted: value = controller.zoom
                Connections {
                    target: controller
                    function onViewChanged() {
                        if (!zoomSlider.pressed)
                            zoomSlider.value = controller.zoom
                    }
                }
                onMoved: controller.zoom = value
            }
        }
        }
    }


    // ---------------- dialogs & popups ----------------
    NewDocumentDialog { id: newDocDialog; controller: root.controller }

    // Text tool input (Krita keeps text entry in a small popup).
    Popup {
        id: textPopup
        property real pendingX: 0
        property real pendingY: 0
        anchors.centerIn: parent
        width: 300; height: 96
        padding: 10
        background: Rectangle { color: T.popup; border.color: T.popupBorder; border.width: 1 }
        Column {
            anchors.fill: parent
            spacing: 6
            Text { text: qsTr("Text"); color: T.text; font.pixelSize: 11 }
            Rectangle {
                width: parent.width; height: 24
                color: T.panelRaised
                border.color: T.border
                TextInput {
                    id: textField
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    verticalAlignment: TextInput.AlignVCenter
                    color: T.text
                    font.pixelSize: 12
                    clip: true
                    onAccepted: {
                        root.controller.renderText(textPopup.pendingX, textPopup.pendingY,
                                                   text, Math.round(root.controller.brushSize * 3))
                        textPopup.close()
                    }
                }
            }
            Text {
                text: qsTr("Enter to place it on the active layer")
                color: T.textDim
                font.pixelSize: 10
            }
        }
        onOpened: textField.forceActiveFocus()
    }

    // Pattern picker for the Pattern tool.
    PMenu {
        id: patternMenu
        property int index: 0
        MenuItem { text: qsTr("Checker"); onTriggered: { patternMenu.index = 0; root.controller.fillPattern(0, 16) } }
        MenuItem { text: qsTr("Stripes"); onTriggered: { patternMenu.index = 1; root.controller.fillPattern(1, 16) } }
        MenuItem { text: qsTr("Dots"); onTriggered: { patternMenu.index = 2; root.controller.fillPattern(2, 16) } }
        MenuItem { text: qsTr("Fine checker"); onTriggered: { patternMenu.index = 0; root.controller.fillPattern(0, 6) } }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save As")
        // Formats come from the controller, filtered to what this Qt build can
        // actually write — offering an unsupported format made Save fail with
        // no explanation.
        nameFilters: controller.imageSaveFilters().concat([qsTr("All files (*)")])
        fileMode: FileDialog.SaveFile
        onAccepted: if (!controller.saveFile(selectedFile)) console.warn("save failed")
    }
    FileDialog {
        id: openDialog
        title: qsTr("Open")
        nameFilters: [qsTr("Prism document (*.prism)")]
                      .concat(controller.imageOpenFilters())
                      .concat([qsTr("All files (*)")])
        fileMode: FileDialog.OpenFile
        onAccepted: if (!controller.openFile(selectedFile)) console.warn("open failed")
    }
    FileDialog {
        id: exportDialog
        title: qsTr("Export")
        nameFilters: [
            qsTr("PNG image (*.png)"),
            qsTr("JPEG image (*.jpg)"),
            qsTr("WebP image (*.webp)"),
            qsTr("All files (*)")
        ]
        fileMode: FileDialog.SaveFile
        onAccepted: if (!controller.saveFile(selectedFile)) console.warn("export failed")
    }

    // Tool-shortcut editor. Single-key shortcuts (like Krita's tool keys):
    // type one character, or leave blank to disable that tool's key.
    Popup {
        id: shortcutDialog
        anchors.centerIn: parent
        width: 420; height: 520
        padding: 0

        background: Rectangle { color: T.window; border.color: T.border; border.width: 1 }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Configure Tool Shortcuts")
                    color: T.text
                    font.pixelSize: 14
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Close")
                    implicitHeight: 24
                    onClicked: shortcutDialog.close()
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Type one character per tool, or leave it empty to disable its shortcut. Assigning a key that another tool uses takes the key from that tool.")
                color: T.textDim
                font.pixelSize: 10
            }

            ListView {
                id: shortcutList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.controller.toolShortcutList

                delegate: ColumnLayout {
                    id: shortcutRow
                    width: shortcutList.width
                    spacing: 1

                    /// Live conflict check: names the OTHER tool (if any) that
                    /// currently holds the typed key, so the consequence is
                    /// visible BEFORE the assignment is committed.
                    readonly property string conflictWith: {
                        const list = root.controller.toolShortcutList
                        const k = keyField.text.trim().toUpperCase()
                        if (k.length === 0)
                            return ""
                        for (let i = 0; i < list.length; ++i)
                            if (list[i].tool !== modelData.tool && list[i].key === k)
                                return list[i].name
                        return ""
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            Layout.fillWidth: true
                            text: modelData.name
                            color: T.text
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                        Text {
                            text: modelData.key.length > 0 ? modelData.key : qsTr("(none)")
                            color: T.textDim
                            font.pixelSize: 10
                            Layout.preferredWidth: 40
                        }
                        Rectangle {
                            width: 56; height: 22
                            // The key box flags a conflict while typing.
                            color: T.panelRaised
                            border.color: shortcutRow.conflictWith.length > 0
                                          ? T.text : T.border
                            border.width: 1
                            TextInput {
                                id: keyField
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                verticalAlignment: TextInput.AlignVCenter
                                color: T.text
                                font.pixelSize: 11
                                maximumLength: 1
                                clip: true
                                Text {
                                    visible: keyField.text.length === 0 && !keyField.activeFocus
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("set...")
                                    color: T.textDim
                                    font.pixelSize: 10
                                }
                                onEditingFinished:
                                    root.controller.setToolShortcut(modelData.tool, text)
                            }
                        }
                    }

                    // The warning itself: names the tool that would lose the
                    // key if this assignment is committed.
                    Text {
                        Layout.fillWidth: true
                        visible: conflictWith.length > 0
                        leftPadding: 4
                        text: qsTr("! '%1' is already used by %2 - assigning it here will clear it there")
                              .arg(keyField.text.trim().toUpperCase())
                              .arg(conflictWith)
                        color: T.text
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }
            }
        }
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
