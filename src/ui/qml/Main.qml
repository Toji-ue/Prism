import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Prism.Ui 1.0
import Prism

ApplicationWindow {
    id: root
    // Sized to fit smaller laptop screens; maximize gives the full workspace.
    width: 1280
    height: 780
    minimumWidth: 1000
    minimumHeight: 620
    title: controller.windowTitle
    visible: true
    color: ThemeManager.themeColor("window")

    // Injected from C++ so the image providers share this instance.
    required property AppController controller
    property bool closeApproved: false
    onClosing: function(event) {
        if (!closeApproved) {
            event.accepted = false
            controller.requestDocumentAction("close")
        }
    }

    readonly property bool focusMode: controller.focusMode
    // Panel IDs stay stable when docked panels are omitted from the floating tabs.
    readonly property var floatingPanels: focusMode
            ? [{ panelId: 0, title: qsTr("Layers") },
               { panelId: 1, title: qsTr("Brush Lab") },
               { panelId: 2, title: qsTr("Colour") }]
            : controller.colourPickerPinned
                ? [{ panelId: 1, title: qsTr("Brush Lab") }]
                : [{ panelId: 1, title: qsTr("Brush Lab") },
                   { panelId: 2, title: qsTr("Colour") }]
    readonly property bool textEditing: root.activeFocusItem instanceof TextInput
                                        || root.activeFocusItem instanceof TextField
                                        || root.activeFocusItem instanceof TextArea
    readonly property bool canvasKeyFocus: !root.activeFocusItem || root.activeFocusItem === canvasMouse
                                          || root.activeFocusItem === canvasItem || root.activeFocusItem === flick
                                          || root.activeFocusItem === root.contentItem
    property string toastText: ""
    property bool toastVisible: false
    property var pendingCanvasCentre: null
    property bool pendingViewportWidth: false
    property bool pendingViewportHeight: false
    readonly property bool pointerDown: canvasMouse.pressed || controller.strokeActive
    readonly property point pointerInWindow: canvasMouse.containsMouse
            ? canvasMouse.mapToItem(root.contentItem, canvasMouse.mouseX, canvasMouse.mouseY)
            : Qt.point(root.width / 2, root.height / 2)
    onPointerDownChanged: if (pointerDown) {
        quickPanel.close()
        drawingQuickControls.close()
    } else if (toastVisible) toastTimer.restart()
    onFocusModeChanged: {
        closeWorkspacePopups()
        canvasMouse.forceActiveFocus()
    }

    function closeWorkspacePopups() {
        quickPanel.close()
        drawingQuickControls.close()
    }

    function restorePendingCanvasCentre(changedAxis) {
        if (!pendingCanvasCentre) return
        if (changedAxis === "width") pendingViewportWidth = true
        if (changedAxis === "height") pendingViewportHeight = true
        const mapped = canvasSheet.mapToItem(flick, pendingCanvasCentre.x,
                                              pendingCanvasCentre.y)
        const maxX = Math.max(0, flick.contentWidth - flick.width)
        const maxY = Math.max(0, flick.contentHeight - flick.height)
        flick.contentX = Math.max(0, Math.min(maxX,
                                               flick.contentX + mapped.x - flick.width / 2))
        flick.contentY = Math.max(0, Math.min(maxY,
                                               flick.contentY + mapped.y - flick.height / 2))
        if (pendingViewportWidth && pendingViewportHeight)
            pendingCanvasCentre = null
    }

    function setDrawingChromeCollapsed(collapsed) {
        if (controller.startPageVisible || pointerDown || controller.focusMode === collapsed)
            return
        // Preserve the canvas point at the viewport centre when the side rails
        // and header change the available area. Zoom is deliberately untouched.
        pendingCanvasCentre = flick.mapToItem(canvasSheet, flick.width / 2, flick.height / 2)
        pendingViewportWidth = false
        pendingViewportHeight = false
        controller.focusMode = collapsed
    }

    function toggleDrawingChrome() {
        setDrawingChromeCollapsed(!controller.focusMode)
    }

    function showWorkspacePanel(index) {
        if (pointerDown || controller.startPageVisible) return
        if (index === 0 && !focusMode) {
            closeWorkspacePopups()
            mainLayersDocker.focusActiveLayer()
            return
        }
        if (index === 2 && !focusMode && controller.colourPickerPinned) {
            closeWorkspacePopups()
            pinnedColourPicker.forceActiveFocus()
            return
        }
        drawingQuickControls.close()
        quickPanel.page = index
        if (index === 1) brushLab.browse()
        quickPanel.open()
    }

    function openDrawingQuickControls() {
        if (pointerDown || controller.startPageVisible || root.textEditing)
            return
        if (drawingQuickControls.opened) { drawingQuickControls.close(); return }
        quickPanel.close()
        const point = root.pointerInWindow
        drawingQuickControls.openNear(point.x, point.y, root.contentItem.width, root.contentItem.height)
    }

    function showToast(message) {
        if (!message) return
        toastText = message
        toastVisible = true
        if (!pointerDown) toastTimer.restart()
    }

    Timer { id: toastTimer; interval: 3500; onTriggered: root.toastVisible = false }
    Connections {
        target: controller
        function onUnsavedChangesDecisionRequested() { unsavedDialog.open() }
        function onSaveAsRequested() { saveDialog.open() }
        function onCloseReady() { root.closeApproved = true; root.close() }
        function onCloseRecordingError(message) { recordingCloseDialog.detail = message; recordingCloseDialog.open() }
        function onRecordingRecovered(success) {
            if (!success) return
            if (controller.recordingHasGaps) partialRecordDialog.open()
            else { recordDialog.partial = false; recordDialog.open() }
        }
        function onDocumentReplaced() {
            root.closeWorkspacePopups()
            Qt.callLater(() => controller.fitToViewport(canvasArea.width - 40, canvasArea.height - 40))
        }
        function onStartPageVisibleChanged() {
            if (controller.startPageVisible) root.closeWorkspacePopups()
        }
        function onColourPickerPinnedChanged() {
            Qt.callLater(() => {
                if (controller.colourPickerPinned && !root.focusMode
                        && quickPanel.visible && quickPanel.page === 2)
                    root.showWorkspacePanel(2)
            })
        }
        function onLastErrorChanged() { if (controller.lastError) root.toastVisible = false }
        function onStatusMessageChanged() { root.showToast(controller.statusMessage) }
    }

    Component.onCompleted: {
        showMaximized()
        ThemeManager.setTheme(controller.theme)
    }

    /// Switches the palette and persists the choice. A short fade overlay
    /// (below) covers the switch so the whole window reads as one crossfade.
    function toggleTheme() {
        ThemeManager.toggleTheme()
        controller.theme = ThemeManager.activeThemeName
    }

    // Opens the New Document dialog. Also used by the UI verification hook
    // (prism --demo-new-dialog --screenshot=...).
    function openNewDocumentDialog() { newDocDialog.open() }

    /// Theme crossfade: a full-window veil in the NEW theme's window colour
    /// fades out over 180 ms, covering the instant palette swap underneath.
    /// One mechanism for every widget - per-binding colour animations would
    /// need property-per-colour tokens (rejected).
    /// ponytail: fixed 180 ms; respect a system reduce-motion setting when Qt
    /// exposes one to QML.
    Rectangle {
        id: themeFade
        anchors.fill: parent
        z: 9999
        color: ThemeManager.themeColor("window")
        opacity: 0
        visible: opacity > 0
        Behavior on opacity {
            id: themeFadeBehavior
            NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
        }
        Connections {
            target: ThemeManager
            function onThemeSwitched() {
                // Snap opaque (behaviour off), then let the Behaviour fade out.
                themeFadeBehavior.enabled = false
                themeFade.opacity = 1
                themeFadeBehavior.enabled = true
                themeFade.opacity = 0
            }
        }
    }

    // Start page is shown until a document exists (or File ? Start Page).
    StartPage {
        id: startPage
        anchors.fill: parent
        z: 100
        visible: root.controller.startPageVisible
        controller: root.controller
        onRequestNew: newDocDialog.open()
        onRequestOpen: openDialog.open()
        onRequestRecordExport: { recordDialog.partial = false; recordDialog.open() }
    }

    // Switches the Advanced Color Selector docker to its Tool Options tab.
    function showToolOptions() {
        showWorkspacePanel(1)
        if (quickPanel.visible) brushLab.editBrush()
    }

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
    function demoQuickControls(x, y) {
        controller.startPageVisible = false
        closeWorkspacePopups()
        drawingQuickControls.openNear(x, y, root.contentItem.width, root.contentItem.height)
    }

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
    Shortcut { sequence: "Ctrl+Z"; enabled: controller.canUndo && !root.textEditing && !root.pointerDown; onActivated: controller.undo() }
    Shortcut {
        sequence: "Ctrl+Shift+Z"
        enabled: controller.canRedo && !root.textEditing && !root.pointerDown
        onActivated: controller.redo()
    }
    Shortcut { sequence: "Ctrl+Y"; enabled: controller.canRedo && !root.textEditing && !root.pointerDown; onActivated: controller.redo() }
    Shortcut { sequence: "Ctrl+S"; enabled: !controller.saving && !root.pointerDown; onActivated: controller.saveCurrentProject() }
    Shortcut { sequence: "Ctrl+Shift+S"; enabled: !controller.saving && !root.pointerDown; onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+O"; enabled: !controller.documentEditing && !controller.documentActionPending; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+N"; enabled: !controller.documentEditing && !controller.documentActionPending; onActivated: newDocDialog.open() }
    Shortcut { sequence: "F1"; enabled: !root.pointerDown; onActivated: helpDialog.open() }
    Shortcut { sequence: "Ctrl+Shift+N"; enabled: !root.textEditing && !root.pointerDown && !controller.startPageVisible; onActivated: controller.addLayer() }
    Shortcut { sequence: "Ctrl+E"; onActivated: controller.exportPng ? exportDialog.open() : {} }
    Shortcut { sequence: "Ctrl+A"; enabled: !root.textEditing && !root.pointerDown && !controller.startPageVisible; onActivated: controller.selectAll() }
    Shortcut { sequence: "Ctrl+D"; enabled: !root.textEditing && !root.pointerDown && !controller.startPageVisible; onActivated: controller.clearSelection() }
    Shortcut { sequence: "Ctrl+Shift+I"; enabled: !root.textEditing && !root.pointerDown && !controller.startPageVisible; onActivated: controller.invertSelection() }
    Shortcut { sequence: "Ctrl+Shift+M"; enabled: !root.textEditing && !root.pointerDown && !controller.startPageVisible; onActivated: controller.mirrorHorizontal = !controller.mirrorHorizontal }
    // Finish an in-progress click path (polyline / bezier / selection paths).
    Shortcut {
        sequence: "Return"
        enabled: !root.textEditing && !root.pointerDown && (!root.activeFocusItem || !root.activeFocusItem.keyboardButton)
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
        enabled: !root.textEditing && !root.pointerDown && !quickPanel.visible && !drawingQuickControls.visible
        onActivated: {
            if (canvasMouse.cropRect) {
                canvasMouse.cropRect = null
                controller.clearSelection()
            }
            controller.cancelPath()
        }
    }
    Shortcut { sequence: "+"; enabled: !root.textEditing && !root.pointerDown && !controller.startPageVisible; onActivated: controller.brushSize = Math.min(200, controller.brushSize + 2) }
    Shortcut { sequence: "-"; enabled: !root.textEditing && !root.pointerDown && !controller.startPageVisible; onActivated: controller.brushSize = Math.max(1, controller.brushSize - 2) }
    Shortcut { sequence: "Delete"; enabled: !root.textEditing && !root.pointerDown && !controller.startPageVisible; onActivated: controller.clearCanvas() }
    Shortcut {
        sequence: "Tab"
        context: Qt.ApplicationShortcut
        enabled: !controller.startPageVisible && root.canvasKeyFocus && !root.pointerDown
        onActivated: root.toggleDrawingChrome()
    }
    Shortcut { sequence: "Ctrl+1"; context: Qt.ApplicationShortcut; enabled: !controller.startPageVisible && !root.textEditing && !root.pointerDown; onActivated: showWorkspacePanel(0) }
    Shortcut { sequence: "Ctrl+2"; context: Qt.ApplicationShortcut; enabled: !controller.startPageVisible && !root.textEditing && !root.pointerDown; onActivated: showWorkspacePanel(1) }
    Shortcut { sequence: "Ctrl+3"; context: Qt.ApplicationShortcut; enabled: !controller.startPageVisible && !root.textEditing && !root.pointerDown; onActivated: showWorkspacePanel(2) }

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
            enabled: modelData.key.length > 0 && !root.textEditing && !root.pointerDown
            sequence: modelData.key
            context: Qt.ApplicationShortcut
            onActivated: {
                if (modelData.tool === -1) root.openDrawingQuickControls()
                else root.controller.tool = modelData.tool
            }
        }
    }

    // ---------------- menu bar ----------------
    menuBar: MenuBar {
        visible: !root.focusMode && !controller.startPageVisible
        background: Rectangle {
            color: ThemeManager.themeColor("window")
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: ThemeManager.themeColor("border")
            }
        }
        delegate: MenuBarItem {
            id: menuItem
            objectName: menuItem.menu ? menuItem.menu.objectName + "Button" : ""
            implicitWidth: label.implicitWidth + 24
            implicitHeight: 32
            contentItem: Text {
                id: label
                text: menuItem.text.replace(/&/g, "")
                color: ThemeManager.themeColor("text")
                font.pixelSize: ThemeManager.themeColor("fontBody")
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
            }
            background: Rectangle {
                color: menuItem.highlighted ? ThemeManager.themeColor("buttonHover") : "transparent"
            }
        }
        // Everything except the ways to open a document is inert while the
        // start page is up, so menu entries that need a canvas are disabled.
        PMenu {
            id: fileMenu
            title: qsTr("&File")
            Action { text: qsTr("&New...\tCtrl+N"); onTriggered: newDocDialog.open() }
            Action { text: qsTr("&Open...\tCtrl+O"); onTriggered: openDialog.open() }
            Action { text: qsTr("&Save\tCtrl+S"); enabled: !controller.saving && !root.pointerDown; onTriggered: controller.saveCurrentProject() }
            Action { text: qsTr("Save &As...\tCtrl+Shift+S"); enabled: !controller.saving && !root.pointerDown; onTriggered: saveDialog.open() }
            Action { text: qsTr("&Export PNG...\tCtrl+E"); onTriggered: exportDialog.open() }
            Action {
                // Proof of process: the session's timelapse + hash chain, for
                // whoever asks whether the artwork is hand-drawn.
                text: qsTr("Export Drawing &Record (MP4 / GIF)...")
                enabled: controller.recordedFrames > 0 && !controller.recordingExporting
                onTriggered: { recordDialog.partial = false; recordDialog.open() }
            }
            Action { text: qsTr("Recording Storage…"); onTriggered: storageDialog.open() }
            Action { text: controller.recordingRepairing ? qsTr("Repairing recording…") : qsTr("Retry recording"); enabled: controller.recordingError.length > 0 && !controller.recordingRepairing; onTriggered: controller.retryRecording() }
            Action { text: qsTr("Export partial recording…"); enabled: controller.recordedFrames > 0 && !controller.recordingExporting; onTriggered: partialRecordDialog.open() }
            MenuSeparator {}
            Action {
                text: qsTr("&Start Page")
                onTriggered: controller.startPageVisible = true
            }
            MenuSeparator {}
            Action { text: qsTr("&Quit\tCtrl+Q"); onTriggered: root.close() }
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
                enabled: controller.canRedo && !root.textEditing && !root.pointerDown
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
            Action { text: qsTr("&Fit to Window"); onTriggered: controller.fitToViewport(canvasArea.width - 40, canvasArea.height - 40) }
            MenuSeparator {}
            Action { text: qsTr("Mirror Horizontally\tCtrl+Shift+M"); checkable: true; checked: controller.mirrorHorizontal; onTriggered: controller.mirrorHorizontal = !controller.mirrorHorizontal }
            Action { text: qsTr("Mirror Vertically"); checkable: true; checked: controller.mirrorVertical; onTriggered: controller.mirrorVertical = !controller.mirrorVertical }
        }
        PMenu {
            objectName: "workspaceImageMenu"
            title: qsTr("&Image")
            enabled: !controller.startPageVisible
            Action { objectName: "resizeCanvasAction"; text: qsTr("Resize Canvas..."); onTriggered: resizeDialog.open() }
            Action { text: qsTr("Canvas Information"); onTriggered: canvasInfoDialog.open() }
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
            Action { text: qsTr("Switch Light / Dark Theme"); onTriggered: root.toggleTheme() }
        }
        PMenu {
            title: qsTr("&Window")
            objectName: "workspaceWindowMenu"
            enabled: !controller.startPageVisible
            Action {
                text: qsTr("Focus Mode\tTab")
                checkable: true
                checked: controller.focusMode
                onTriggered: root.setDrawingChromeCollapsed(checked)
            }
            Action { text: qsTr("Layers\tCtrl+1"); onTriggered: showWorkspacePanel(0) }
            Action { text: qsTr("Brush Lab\tCtrl+2"); onTriggered: showWorkspacePanel(1) }
            Action { objectName: "workspaceColourMenuItem"; text: qsTr("Colour\tCtrl+3"); onTriggered: showWorkspacePanel(2) }
        }
        PMenu {
            title: qsTr("&Help")
            Action { text: qsTr("Drawing with Prism\tF1"); onTriggered: helpDialog.open() }
            Action { text: qsTr("About Prism"); onTriggered: aboutDialog.open() }
        }
    }

    // ---------------- toolbar ----------------
    header: ToolBar {
        visible: !controller.startPageVisible && !root.focusMode
        height: 40
        background: Rectangle {
            color: ThemeManager.themeColor("panel")
            radius: ThemeManager.themeColor("radiusPanel")
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 8

            ToolbarButton {
                iconName: "file-save"
                tip: qsTr("Save (Ctrl+S)")
                onClicked: controller.saveCurrentProject()
            }
            Rectangle { width: 1; height: 18; color: ThemeManager.themeColor("border") }
            ToolbarButton {
                iconName: "undo"
                tip: qsTr("Undo (Ctrl+Z)")
                usable: controller.canUndo
                onClicked: controller.undo()
            }
            ToolbarButton {
                iconName: "redo"
                tip: qsTr("Redo (Ctrl+Shift+Z)")
                usable: controller.canRedo
                onClicked: controller.redo()
            }
            Rectangle { width: 1; height: 18; color: ThemeManager.themeColor("border") }
            Rectangle {
                width: 172; height: 32
                radius: ThemeManager.themeColor("radiusControl")
                color: ThemeManager.themeColor("panelRaised")
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 7
                    anchors.rightMargin: 7
                    spacing: 6
                    Image {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        fillMode: Image.PreserveAspectFit
                        cache: false
                        source: "image://preset/" + controller.activePresetIndex + "/" + controller.activePresetId
                    }
                    Text {
                        Layout.fillWidth: true
                        text: (controller.eraser ? qsTr("Eraser · ") : "") + controller.activePresetName + (controller.brushModified ? " ·" : "")
                        color: ThemeManager.themeColor("text")
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Icon { Layout.preferredWidth: 12; Layout.preferredHeight: 12; name: "chevron-down"; color: ThemeManager.themeColor("textDim") }
                }
                MouseArea { anchors.fill: parent; onClicked: root.showWorkspacePanel(1) }
            }
            Rectangle {
                objectName: "toolbarColourSwatch"
                width: 32; height: 32
                radius: ThemeManager.themeColor("radiusControl")
                color: controller.brushColor
                border.color: ThemeManager.themeColor("border")
                MouseArea { anchors.fill: parent; onClicked: root.showWorkspacePanel(2) }
            }
            Item { Layout.fillWidth: true }
            Text {
                objectName: "recordingIndicator"
                visible: true
                text: controller.recordingRepairing ? qsTr("REC REPAIR") : controller.recordingError.length ? qsTr("REC ERROR") : controller.recordingHasGaps ? qsTr("REC · GAP") : controller.recording ? qsTr("REC") : qsTr("ARCHIVE")
                color: controller.recordingError.length ? ThemeManager.themeColor("danger") : ThemeManager.themeColor("highlight")
                font.pixelSize: 11
                HoverHandler { id: recordingHover }
                ToolTip.visible: recordingHover.hovered
                ToolTip.text: controller.recordingError.length ? controller.recordingError : controller.recordingHasGaps ? qsTr("Recording resumed with gaps. Only partial export is available.") : qsTr("Recording is automatic. Click to open Storage.")
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Open recording Storage")
                Accessible.onPressAction: storageDialog.open()
                Keys.onReturnPressed: storageDialog.open()
                Keys.onSpacePressed: storageDialog.open()
                MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: storageDialog.open() }
            }
            ToolbarButton {
                objectName: "workspaceCollapseButton"
                width: 32; height: 32
                iconName: "focus"
                iconSize: 18
                tip: qsTr("Focus mode (Tab)")
                onClicked: root.setDrawingChromeCollapsed(true)
            }
        }
    }

    Rectangle {
        id: workspaceRestoreSurface
        objectName: "workspaceRestoreSurface"
        z: 96
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 8
        width: 32; height: 32
        visible: root.focusMode && !controller.startPageVisible && !root.pointerDown
        radius: ThemeManager.themeColor("radiusControl")
        color: ThemeManager.themeColor("popup")
        border.color: ThemeManager.themeColor("popupBorder")

        ToolbarButton {
            anchors.fill: parent
            objectName: "workspaceRestoreButton"
            iconName: "workspace"
            iconSize: 18
            tip: qsTr("Show interface (Tab)")
            onClicked: root.setDrawingChromeCollapsed(false)
        }
    }

    // ---------------- body ----------------
    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8

        // left toolbox
        ToolBox {
            controller: root.controller
            Layout.fillHeight: true
            visible: !root.focusMode
        }

        // center: canvas viewport with grey backdrop (Krita's canvas area)
        Rectangle {
            id: canvasArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: ThemeManager.themeColor("canvasBackdrop")
            radius: ThemeManager.themeColor("radiusPanel")

            // Fit the canvas into the viewport once the layout is known.
            Component.onCompleted: controller.fitToViewport(width - 40, height - 40)

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
                visible: !root.focusMode
                color: ThemeManager.themeColor("popup")
                border.color: ThemeManager.themeColor("popupBorder")
                border.width: 1
                Text {
                    id: zoomLabel
                    anchors.centerIn: parent
                    text: Math.round(controller.zoom * 100) + "%"
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 11
                }
            }

            Flickable {
                id: flick
                objectName: "canvasFlick"
                anchors.fill: parent
                onWidthChanged: root.restorePendingCanvasCentre("width")
                onHeightChanged: root.restorePendingCanvasCentre("height")
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
                            color: "#ffffff"
                            // Soft desk edge around the paper instead of a hard
                            // black hairline: reads as a sheet on a desk.
                            border.color: ThemeManager.themeColor("canvasBackdrop")
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
                            color: ThemeManager.themeColor("textDim")
                            opacity: 0.6
                        }
                        Rectangle {
                            visible: root.controller.mirrorVertical
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width; height: 1
                            color: ThemeManager.themeColor("textDim")
                            opacity: 0.6
                        }

                        MouseArea {
                            id: canvasMouse
                            objectName: "canvasMouseArea"
                            // Keep the current pointer grab until its committed release.
                            enabled: !controller.documentActionPending || controller.documentEditing
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                            // Clicking the canvas takes focus back from whatever
                            // text field was last used. Window shortcuts (Ctrl+Z,
                            // Ctrl+S...) are swallowed by a focused TextInput Ã¢â‚¬â€
                            // Qt sends it a ShortcutOverride that it accepts Ã¢â‚¬â€ so
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
                                border.color: ThemeManager.themeColor("cursorRing")
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
                                    border.color: ThemeManager.themeColor("cursorRingInner")
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
                            border.color: "#ffffff"
                            border.width: 1

                            // dark inner line keeps the selection readable on
                            // white and on dark artwork alike
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                color: "transparent"
                                border.color: "#000000"
                                border.width: 1
                            }
                        }
                    }
                }
            }
        }

        // Layers stay visible above a small colour shelf while drawing.
        Rectangle {
            id: dockerArea
            Layout.preferredWidth: ThemeManager.themeColor("dockerWidth")
            Layout.fillHeight: true
            visible: !root.focusMode
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                spacing: 8
                LayersDocker {
                    id: mainLayersDocker
                    objectName: "workspaceLayers"
                    controller: root.controller
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                CompactColour {
                    controller: root.controller
                    visible: !controller.colourPickerPinned
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    onOpenFullPicker: root.showWorkspacePanel(2)
                }
                Rectangle {
                    visible: controller.colourPickerPinned
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(320, root.height * 0.42)
                    color: ThemeManager.themeColor("panel")
                    radius: 12
                    PButton { id: unpinColour; objectName: "unpinColourPicker"; text: qsTr("Compact colour"); height: 32; anchors.right: parent.right; onClicked: controller.colourPickerPinned = false }
                    ColorSelector {
                        id: pinnedColourPicker
                        objectName: "pinnedColourPicker"
                        controller: root.controller
                        anchors.top: unpinColour.bottom
                        anchors.bottom: parent.bottom
                        width: parent.width
                    }
                }
            }
        }
    }


    // One contextual strip instead of two rows of persistent chrome.
    footer: Item {
        visible: !controller.startPageVisible && !root.focusMode
        implicitHeight: 42
        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            color: ThemeManager.themeColor("panel")
            radius: ThemeManager.themeColor("radiusPanel")
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 7
                Text { text: qsTr("Size (px)"); color: ThemeManager.themeColor("textDim"); font.pixelSize: 11 }
                PSlider {
                    id: sizeSlider
                    objectName: "sizeSlider"
                    Layout.preferredWidth: 100
                    from: 1; to: 200
                    Component.onCompleted: value = controller.brushSize
                    Connections {
                        target: controller
                        function onBrushChanged() { if (!sizeSlider.pressed) sizeSlider.value = controller.brushSize }
                    }
                    onMoved: controller.brushSize = Math.round(value)
                }
                NumericField { objectName: "workspaceSizeField"; Layout.preferredWidth: 54; from: 1; to: 200; numberValue: controller.brushSize; Accessible.name: qsTr("Brush size in pixels"); onCommitted: value => controller.brushSize = value }
                Text { text: qsTr("Flow (%)"); color: ThemeManager.themeColor("textDim"); font.pixelSize: 11 }
                PSlider {
                    id: flowSlider
                    objectName: "flowSlider"
                    Layout.preferredWidth: 90
                    from: 0.01; to: 1
                    Component.onCompleted: value = controller.brushFlow
                    Connections {
                        target: controller
                        function onBrushChanged() { if (!flowSlider.pressed) flowSlider.value = controller.brushFlow }
                    }
                    onMoved: controller.brushFlow = value
                }
                NumericField { objectName: "workspaceFlowField"; Layout.preferredWidth: 54; from: 1; to: 100; numberValue: controller.brushFlow * 100; Accessible.name: qsTr("Brush flow percent"); onCommitted: value => controller.brushFlow = value / 100 }
                Rectangle { width: 1; height: 18; color: ThemeManager.themeColor("border") }
                Text {
                    Layout.preferredWidth: 110
                    text: controller.activeLayerInfo.name
                    elide: Text.ElideRight
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 11
                }
                Item {
                    visible: controller.activeLayerInfo.locked; width: 24; height: 32
                    Icon { anchors.centerIn: parent; width: 16; height: 16; name: "lock"; color: ThemeManager.themeColor("text") }
                    HoverHandler { id: stateHoverlocked }
                    ToolTip.visible: stateHoverlocked.hovered; ToolTip.text: qsTr("Locked layer")
                    Accessible.name: qsTr("Locked layer")
                }
                Item {
                    visible: controller.activeLayerInfo.alphaLocked; width: 24; height: 32
                    Icon { anchors.centerIn: parent; width: 16; height: 16; name: "alpha-lock"; color: ThemeManager.themeColor("text") }
                    HoverHandler { id: stateHoveralphaLocked }
                    ToolTip.visible: stateHoveralphaLocked.hovered; ToolTip.text: qsTr("Alpha lock")
                    Accessible.name: qsTr("Alpha lock")
                }
                Item {
                    visible: controller.activeLayerInfo.clipped; width: 24; height: 32
                    Icon { anchors.centerIn: parent; width: 16; height: 16; name: "clipping"; color: ThemeManager.themeColor("text") }
                    HoverHandler { id: stateHoverclipped }
                    ToolTip.visible: stateHoverclipped.hovered; ToolTip.text: qsTr("Clipped to layer below")
                    Accessible.name: qsTr("Clipped to layer below")
                }
                Text {
                    visible: false
                    text: ""
                    color: ThemeManager.themeColor("highlight")
                    font.pixelSize: 11
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: controller.saving ? qsTr("Saving…") : controller.saveStatus
                    Layout.maximumWidth: 140
                    elide: Text.ElideRight
                    color: ThemeManager.themeColor("textDim")
                    font.pixelSize: 11
                    HoverHandler { id: protectionHover }
                    ToolTip.visible: protectionHover.hovered
                    ToolTip.text: qsTr("Project: %1\nRecovery: %2\nLast protected: %3").arg(controller.saveStatus).arg(controller.autosaveStatus).arg(controller.lastProtectedTime || qsTr("Not yet"))
                }
                Text {
                    text: Math.round(controller.zoom * 100) + "%"
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 11
                }
                PButton { text: qsTr("Fit"); onClicked: controller.fitToViewport(canvasArea.width - 40, canvasArea.height - 40) }
            }
        }
    }


    QuickControls {
        id: drawingQuickControls
        controller: root.controller
        backdrop: canvasArea
        onRequestBrushes: root.showWorkspacePanel(1)
        onRequestEdit: root.showToolOptions()
    }

    Rectangle {
        id: notice
        objectName: "workspaceNotice"
        property point position: Qt.point(12, 12)
        function place() {
            if (root.pointerDown) return
            const pointer = root.pointerInWindow
            position = Qt.point(pointer.x < root.contentItem.width / 2 ? root.contentItem.width - width - 12 : 12,
                                pointer.y < root.contentItem.height / 2 ? root.contentItem.height - height - 12 : 54)
        }
        onVisibleChanged: if (visible) place()
        onWidthChanged: if (visible) place()
        onHeightChanged: if (visible) place()
        z: 150
        visible: (controller.lastError.length > 0 || root.toastVisible || controller.recordingRepairing || controller.recordingExporting || controller.saving) && !root.pointerDown
        width: Math.min(480, root.width - 32)
        height: noticeContent.implicitHeight + 24
        x: Math.max(8, Math.min(position.x, root.contentItem.width - width - 8))
        y: Math.max(54, Math.min(position.y, root.contentItem.height - height - 8))
        radius: ThemeManager.themeColor("radiusPanel")
        color: ThemeManager.themeColor("popup")
        border.color: ThemeManager.themeColor("popupBorder")
        ColumnLayout {
            id: noticeContent
            anchors.fill: parent
            anchors.margins: 12
            Text {
                Layout.fillWidth: true
                text: controller.lastError.length ? controller.lastError
                      : controller.saving ? qsTr("Saving project…")
                      : controller.recordingRepairing ? qsTr("Repairing recording…")
                      : controller.recordingExporting ? controller.statusMessage : root.toastText
                wrapMode: Text.WordWrap
                color: controller.lastError.length ? ThemeManager.themeColor("danger") : ThemeManager.themeColor("text")
                font.pixelSize: 12
                Accessible.role: Accessible.AlertMessage
                Accessible.name: text
            }
            RowLayout {
                visible: controller.lastError.length > 0
                PButton { objectName: "noticeSaveAs"; visible: controller.errorContext === "artwork-save" || controller.errorContext === "autosave"; text: qsTr("Save As…"); enabled: !controller.saving; onClicked: saveDialog.open() }
                PButton { objectName: "noticeRetryAutosave"; visible: controller.errorContext === "autosave"; text: qsTr("Retry autosave"); onClicked: controller.retryAutosave() }
                PButton { objectName: "noticeRetryRecording"; visible: controller.errorContext === "recording" && controller.recordingError.length > 0; text: qsTr("Retry recording"); enabled: !controller.recordingRepairing; onClicked: controller.retryRecording() }
                PButton { objectName: "noticePartialExport"; visible: controller.errorContext === "recording" && controller.recordedFrames > 0; text: qsTr("Partial export…"); enabled: !controller.recordingExporting; onClicked: partialRecordDialog.open() }
                PButton { text: qsTr("Dismiss"); onClicked: controller.dismissError() }
            }
        }
    }

    // ---------------- dialogs & popups ----------------
    Dialog {
        id: unsavedDialog
        objectName: "unsavedChangesDialog"
        title: qsTr("Save changes to %1?").arg(controller.documentName)
        anchors.centerIn: Overlay.overlay
        width: Math.min(460, root.width - 40)
        modal: true
        closePolicy: Popup.NoAutoClose
        background: Rectangle { color: ThemeManager.themeColor("popup"); radius: 12; border.color: ThemeManager.themeColor("popupBorder") }
        contentItem: ColumnLayout {
            spacing: 16
            Label { Layout.fillWidth: true; text: qsTr("Save your project before continuing. Autosave is a recovery copy, not a saved project."); wrapMode: Text.WordWrap; color: ThemeManager.themeColor("text") }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                PButton { objectName: "unsavedCancel"; text: qsTr("Cancel"); implicitHeight: 36; onClicked: { unsavedDialog.close(); controller.resolveUnsavedChanges("cancel") } }
                PButton { objectName: "unsavedDiscard"; text: qsTr("Discard"); implicitHeight: 36; onClicked: { unsavedDialog.close(); controller.resolveUnsavedChanges("discard") } }
                PButton { objectName: "unsavedSave"; text: qsTr("Save"); implicitHeight: 36; onClicked: { unsavedDialog.close(); controller.resolveUnsavedChanges("save") } }
            }
        }
    }
    Dialog {
        id: recordingCloseDialog
        property string detail: ""
        title: qsTr("Recording could not be completed")
        anchors.centerIn: Overlay.overlay
        width: Math.min(460, root.width - 40)
        modal: true
        closePolicy: Popup.NoAutoClose
        background: Rectangle { color: ThemeManager.themeColor("popup"); radius: 12; border.color: ThemeManager.themeColor("popupBorder") }
        contentItem: ColumnLayout {
            Label { Layout.fillWidth: true; text: recordingCloseDialog.detail + "\n" + qsTr("Your saved artwork is separate. Closing keeps the valid recording prefix, but it may be incomplete."); wrapMode: Text.WordWrap; color: ThemeManager.themeColor("text") }
            RowLayout {
                PButton { text: qsTr("Keep drawing"); implicitHeight: 36; onClicked: { recordingCloseDialog.close(); controller.confirmCloseRecordingError(false) } }
                PButton { text: qsTr("Continue with incomplete recording"); implicitHeight: 36; onClicked: { recordingCloseDialog.close(); controller.confirmCloseRecordingError(true) } }
            }
        }
    }
    Dialog {
        id: helpDialog
        objectName: "helpDialog"
        title: qsTr("Drawing with Prism")
        anchors.centerIn: Overlay.overlay
        width: Math.min(620, root.width - 48)
        height: Math.min(560, root.height - 60)
        modal: true
        standardButtons: Dialog.Close
        background: Rectangle { color: ThemeManager.themeColor("popup"); radius: 12; border.color: ThemeManager.themeColor("popupBorder") }
        contentItem: ScrollView {
            clip: true
            Text {
                width: helpDialog.availableWidth - 24
                wrapMode: Text.WordWrap
                color: ThemeManager.themeColor("text")
                font.pixelSize: 14
                textFormat: Text.RichText
                text: qsTr("<h3>Draw</h3><p>B selects the brush, E the eraser. Use Size and Flow in the bottom strip. Q opens Quick Controls near your pen; change that shortcut in Configure Tool Shortcuts. Ctrl+2 opens Choose a brush. Use Edit brush for Basic, Tip, Dynamics and Graphite settings; Save copy keeps changes and Reset restores the selected preset. HB and 2B use graphite grain; Ink tapers at release.</p><h3>Workspace</h3><p>Tab or the four-corner icon hides the interface. The top-right layout icon restores it. In the full workspace, Ctrl+1 focuses docked Layers and Ctrl+2 opens Choose a brush. Use Edit brush for Basic, Tip, Dynamics and Graphite settings; Save copy keeps changes and Reset restores the selected preset. Ctrl+3, the toolbar colour swatch and More open Colour, or focus the pinned picker. Colour contains colour controls; Tool Options opens Basic brush settings. Floating tabs only offer panels that are not already docked. In Focus mode, Ctrl+1/2/3 opens Layers, Brush Lab and Colour as floating tabs. These shortcuts wait while drawing and leave text fields alone. Every document opens with the full interface.</p><h3>Save and recover artwork</h3><p>Ctrl+S saves the native .prism project. Ctrl+Shift+S is Save As. Export creates a flat image and does not save your layers. Changes are protected by autosave after 45 seconds, once a stroke is committed. Recover Artwork on the Start Page opens a separate recovery copy; save it as a project. Save/Discard/Cancel protects closing or replacing modified artwork. Discard removes that artwork’s recovery copies.</p><h3>Recordings</h3><p>Recording runs automatically. A storage error is shown and can be retried. Earlier gaps remain marked as incomplete. Normal export waits for the final frame; Partial export explicitly exports the available timeline. .prismrec keeps every stored frame; GIF/MP4 samples the timeline to about four minutes. The hash chain verifies file consistency, not the identity of the artist.</p><h3>Storage</h3><p>Recording Storage lists sessions and disk usage. Recordings remain until you confirm deletion. Active sessions cannot be deleted. Recover Recording is separate from recovering your artwork.</p><h3>History and layers</h3><p>Ctrl+Z undoes and Ctrl+Shift+Z redoes. Layers supports groups, clipping, alpha lock and locking. Resize Canvas is reversible. Fit View fits the available canvas area; 100% shows actual pixel scale.</p>")
            }
        }
    }
    Dialog {
        id: storageDialog
        objectName: "recordingStorageDialog"
        property var sessions: []
        property string exportSource: ""
        title: qsTr("Recording Storage")
        anchors.centerIn: Overlay.overlay
        width: Math.min(680, root.width - 48)
        height: Math.min(520, root.height - 60)
        modal: true
        standardButtons: Dialog.Close
        function refresh() { sessions = controller.recordingStorage() }
        onOpened: refresh()
        background: Rectangle { color: ThemeManager.themeColor("popup"); radius: 12; border.color: ThemeManager.themeColor("popupBorder") }
        contentItem: ColumnLayout {
            spacing: 12
            Label { Layout.fillWidth: true; text: qsTr("Recordings are kept until you delete them. Artwork recovery is stored separately."); wrapMode: Text.WordWrap; color: ThemeManager.themeColor("text") }
            PButton { text: qsTr("Open folder"); implicitHeight: 36; onClicked: controller.openRecordingFolder() }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: storageDialog.sessions
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width
                    height: 88
                    color: ThemeManager.themeColor("panelRaised")
                    radius: 8
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        Label { Layout.fillWidth: true; text: modelData.name; elide: Text.ElideRight; color: ThemeManager.themeColor("text") }
                        RowLayout {
                            Label { Layout.fillWidth: true; text: modelData.state + (modelData.hasGaps ? qsTr(" · incomplete") : "") + " · " + (modelData.bytes / 1048576).toFixed(1) + " MiB"; color: ThemeManager.themeColor("textDim") }
                            PButton { text: qsTr("Export"); implicitHeight: 32; enabled: !modelData.active && !controller.recordingExporting; onClicked: { storageDialog.exportSource = modelData.url; storedRecordDialog.open() } }
                            PButton { text: qsTr("Delete"); implicitHeight: 32; enabled: !modelData.active && !controller.recordingExporting; onClicked: { deleteRecordDialog.url = modelData.url; deleteRecordDialog.open() } }
                        }
                    }
                }
                Label { anchors.centerIn: parent; visible: storageDialog.sessions.length === 0; text: qsTr("No recordings stored yet."); color: ThemeManager.themeColor("textDim") }
            }
        }
        Connections { target: controller; function onRecoveryChanged() { if (storageDialog.opened) storageDialog.refresh() } }
    }
    Dialog {
        id: deleteRecordDialog
        property string url: ""
        title: qsTr("Delete this recording permanently?")
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: { controller.deleteStoredRecording(url); storageDialog.refresh() }
        background: Rectangle { color: ThemeManager.themeColor("popup"); radius: 12; border.color: ThemeManager.themeColor("popupBorder") }
        Label { text: qsTr("This removes its timelapse. Your artwork is kept."); color: ThemeManager.themeColor("text") }
    }
    FileDialog {
        id: storedRecordDialog
        title: qsTr("Export stored drawing record")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Prism drawing record (*.prismrec)")]
        onAccepted: controller.exportStoredRecording(storageDialog.exportSource, selectedFile)
    }
    Dialog {
        id: partialRecordDialog
        title: qsTr("Export an incomplete recording?")
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.Cancel
        onAccepted: { recordDialog.partial = true; recordDialog.open() }
        background: Rectangle { color: ThemeManager.themeColor("popup"); radius: 12; border.color: ThemeManager.themeColor("popupBorder") }
        Label { width: 360; wrapMode: Text.WordWrap; text: qsTr("Only stored frames will be exported. Missing time or an unstored final frame cannot be recovered. This export does not prove full coverage."); color: ThemeManager.themeColor("text") }
    }
    NewDocumentDialog { id: newDocDialog; controller: root.controller }

    Dialog {
        id: resizeDialog
        objectName: "resizeCanvasDialog"
        title: qsTr("Resize Canvas")
        anchors.centerIn: parent
        modal: true
        width: 340
        onOpened: { resizeWidth.value = controller.canvasWidth; resizeHeight.value = controller.canvasHeight }
        background: Rectangle { color: ThemeManager.themeColor("popup"); border.color: ThemeManager.themeColor("popupBorder"); radius: ThemeManager.themeColor("radius") }
        contentItem: ColumnLayout {
            spacing: 12
            Text { text: qsTr("Keep artwork anchored to the top left."); color: ThemeManager.themeColor("textDim"); font.pixelSize: 12 }
            RowLayout {
                Text { text: qsTr("Width"); color: ThemeManager.themeColor("text"); Layout.preferredWidth: 75 }
                PSpinBox { id: resizeWidth; objectName: "resizeCanvasWidth"; Accessible.name: qsTr("Canvas width"); Layout.fillWidth: true; from: 1; to: 32768; onValueModified: v => value = v }
                Text { text: "px"; color: ThemeManager.themeColor("textDim") }
            }
            RowLayout {
                Text { text: qsTr("Height"); color: ThemeManager.themeColor("text"); Layout.preferredWidth: 75 }
                PSpinBox { id: resizeHeight; objectName: "resizeCanvasHeight"; Accessible.name: qsTr("Canvas height"); Layout.fillWidth: true; from: 1; to: 32768; onValueModified: v => value = v }
                Text { text: "px"; color: ThemeManager.themeColor("textDim") }
            }
            RowLayout {
                Layout.alignment: Qt.AlignRight
                PButton { text: qsTr("Cancel"); onClicked: resizeDialog.close() }
                PButton { objectName: "resizeCanvasApply"; text: qsTr("Resize"); onClicked: {
                    if (resizeWidth.commit() && resizeHeight.commit()) {
                        controller.resizeCanvas(resizeWidth.value, resizeHeight.value)
                        resizeDialog.close()
                    }
                } }
            }
        }
    }
    Dialog {
        id: canvasInfoDialog
        title: qsTr("Canvas Information")
        anchors.centerIn: parent
        modal: true
        width: 360
        standardButtons: Dialog.Close
        background: Rectangle { color: ThemeManager.themeColor("popup"); border.color: ThemeManager.themeColor("popupBorder"); radius: ThemeManager.themeColor("radius") }
        contentItem: Column {
            spacing: 10
            Text { text: qsTr("Document  %1").arg(controller.documentName); color: ThemeManager.themeColor("text"); font.pixelSize: 13 }
            Text { text: qsTr("Canvas  %1 × %2 px").arg(controller.canvasWidth).arg(controller.canvasHeight); color: ThemeManager.themeColor("text"); font.pixelSize: 13 }
            Text { text: qsTr("Resolution  %1 DPI").arg(controller.canvasDpi); color: ThemeManager.themeColor("text"); font.pixelSize: 13 }
            Text { text: qsTr("Color  RGB/Alpha · 8-bit · sRGB"); color: ThemeManager.themeColor("text"); font.pixelSize: 13 }
            Text { text: qsTr("Layer memory  %1 MiB").arg(controller.documentMemoryMiB().toFixed(1)); color: ThemeManager.themeColor("textDim"); font.pixelSize: 12 }
        }
    }
    Dialog {
        id: aboutDialog
        title: qsTr("About Prism")
        anchors.centerIn: parent
        modal: true
        width: 370
        standardButtons: Dialog.Close
        background: Rectangle { color: ThemeManager.themeColor("popup"); border.color: ThemeManager.themeColor("popupBorder"); radius: ThemeManager.themeColor("radius") }
        contentItem: Column {
            spacing: 10
            Text { text: "Prism"; color: ThemeManager.themeColor("text"); font.pixelSize: 25; font.weight: Font.Medium }
            Text { text: qsTr("Version %1").arg(controller.versionString()); color: ThemeManager.themeColor("textDim"); font.pixelSize: 13 }
            Text { text: qsTr("A canvas-first drawing studio for digital artists."); color: ThemeManager.themeColor("text"); font.pixelSize: 12; wrapMode: Text.WordWrap; width: 320 }
            Text { text: qsTr("Artwork stays in local Prism project files."); color: ThemeManager.themeColor("textDim"); font.pixelSize: 12 }
        }
    }

    // Text tool input (Krita keeps text entry in a small popup).
    Popup {
        id: textPopup
        property real pendingX: 0
        property real pendingY: 0
        anchors.centerIn: parent
        width: 300; height: 96
        padding: 10
        background: Rectangle { color: ThemeManager.themeColor("popup"); border.color: ThemeManager.themeColor("popupBorder"); border.width: 1; radius: ThemeManager.themeColor("radiusPanel") }
        Column {
            anchors.fill: parent
            spacing: 6
            Text { text: qsTr("Text"); color: ThemeManager.themeColor("text"); font.pixelSize: 11 }
            Rectangle {
                width: parent.width; height: 24
                color: ThemeManager.themeColor("panelRaised")
                border.color: ThemeManager.themeColor("border")
                TextInput {
                    id: textField
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    verticalAlignment: TextInput.AlignVCenter
                    color: ThemeManager.themeColor("text")
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
                color: ThemeManager.themeColor("textDim")
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
        // actually write Ã¢â‚¬â€ offering an unsupported format made Save fail with
        // no explanation.
        nameFilters: [qsTr("Prism document (*.prism)")]
        fileMode: FileDialog.SaveFile
        onAccepted: if (!controller.saveProjectAsync(selectedFile)) controller.cancelDocumentAction()
        onRejected: controller.cancelDocumentAction()
    }
    FileDialog {
        id: openDialog
        title: qsTr("Open")
        nameFilters: [qsTr("Prism document (*.prism)")]
                      .concat(controller.imageOpenFilters())
                      .concat([qsTr("All files (*)")])
        fileMode: FileDialog.OpenFile
        onAccepted: controller.requestDocumentAction("open", {url: selectedFile.toString()})
    }
    FileDialog {
        id: recordDialog
        property bool partial: false
        title: qsTr("Export Drawing Record")
        // The suffix picks the format: .prismrec is the verifiable one (hash
        // chain, checked by --check-record), mp4/gif are the shareable timelapse.
        nameFilters: [
            qsTr("Prism drawing record - verifiable (*.prismrec)"),
            qsTr("Video (*.mp4)"),
            qsTr("Animated GIF (*.gif)")
        ]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "prismrec"
        onAccepted: if (!controller.exportRecordingAsync(selectedFile, partial))
                        console.warn("export record failed: " + controller.lastError)
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

        background: Rectangle { color: ThemeManager.themeColor("window"); border.color: ThemeManager.themeColor("border"); border.width: 1; radius: ThemeManager.themeColor("radiusPanel") }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Configure Tool Shortcuts")
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 14
                }
                Item { Layout.fillWidth: true }
                PButton {
                    text: qsTr("Close")
                    implicitHeight: 24
                    onClicked: shortcutDialog.close()
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Type one character per tool, or leave it empty to disable its shortcut. Assigning a key that another tool uses takes the key from that tool.")
                color: ThemeManager.themeColor("textDim")
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
                            color: ThemeManager.themeColor("text")
                            font.pixelSize: 11
                            elide: Text.ElideRight
                        }
                        Text {
                            text: modelData.key.length > 0 ? modelData.key : qsTr("(none)")
                            color: ThemeManager.themeColor("textDim")
                            font.pixelSize: 10
                            Layout.preferredWidth: 40
                        }
                        Rectangle {
                            width: 56; height: 22
                            // The key box flags a conflict while typing.
                            color: ThemeManager.themeColor("panelRaised")
                            border.color: shortcutRow.conflictWith.length > 0
                                          ? ThemeManager.themeColor("text") : ThemeManager.themeColor("border")
                            border.width: 1
                            TextInput {
                                id: keyField
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                verticalAlignment: TextInput.AlignVCenter
                                color: ThemeManager.themeColor("text")
                                font.pixelSize: 11
                                maximumLength: 1
                                clip: true
                                Text {
                                    visible: keyField.text.length === 0 && !keyField.activeFocus
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    verticalAlignment: Text.AlignVCenter
                                    text: qsTr("set...")
                                    color: ThemeManager.themeColor("textDim")
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
                        color: ThemeManager.themeColor("text")
                        font.pixelSize: 10
                        elide: Text.ElideMiddle
                    }
                }
            }
        }
    }

    Popup {
        id: quickPanel
        objectName: "focusQuickPanel"
        property int page: 0
        x: root.contentItem.width - width - 8
        y: root.focusMode ? 42 : 8
        width: Math.min(360, root.contentItem.width - 16)
        // Popup coordinates include the full workspace's menu/toolbar offset.
        height: Math.min(570, root.height - root.contentItem.y - y - 8)
        padding: 6
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: root.focusMode ? "transparent" : ThemeManager.themeColor("panel")
            radius: ThemeManager.themeColor("radiusPanel")
            border.width: root.focusMode ? 0 : 1
            border.color: ThemeManager.themeColor("border")
            FocusGlassPanel {
                anchors.fill: parent
                visible: root.focusMode
                backdrop: canvasArea
            }
        }
        contentItem: ColumnLayout {
            spacing: 4
            RowLayout {
                objectName: "workspacePanelTabs"
                Layout.fillWidth: true
                visible: root.floatingPanels.length > 1
                Repeater {
                    model: root.floatingPanels
                    delegate: PButton {
                        id: panelTab
                        required property var modelData
                        objectName: "workspacePanelTab_" + modelData.panelId
                        Layout.fillWidth: true
                        Layout.preferredHeight: 32
                        text: modelData.title
                        Accessible.name: text
                        onClicked: root.showWorkspacePanel(modelData.panelId)
                        background: Rectangle {
                            radius: ThemeManager.themeColor("radiusSmall")
                            color: quickPanel.page === panelTab.modelData.panelId
                                    ? ThemeManager.themeColor("selection")
                                    : panelTab.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
                            border.width: panelTab.visualFocus ? 1 : 0
                            border.color: ThemeManager.themeColor("highlight")
                        }
                        contentItem: Text {
                            text: panelTab.text
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            color: quickPanel.page === panelTab.modelData.panelId ? ThemeManager.themeColor("selectionText") : ThemeManager.themeColor("textDim")
                            font.pixelSize: ThemeManager.themeColor("fontBody")
                        }
                    }
                }
            }
            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: quickPanel.page
                LayersDocker { controller: root.controller; embedded: true }
                BrushPresetsDocker { id: brushLab; objectName: "workspaceBrushLab"; controller: root.controller; embedded: true }
                ColorSelector { controller: root.controller }
            }
        }
    }

}



