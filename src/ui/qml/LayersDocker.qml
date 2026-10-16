import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import Prism.Ui 1.0
import Prism

// Krita-style Layers docker: title bar with action buttons, blend-mode
// combo, opacity slider, then the layer rows (eye / lock / thumbnail / name).
Rectangle {
    id: docker
    required property AppController controller
    /// True when hosted in the tabbed docker column: the tab already names the
    /// panel, so only its action buttons are kept.
    property bool embedded: false
    property bool glass: false

    function focusActiveLayer() {
        list.positionViewAtIndex(controller.activeLayerIndex, ListView.Contain)
        const row = list.itemAtIndex(controller.activeLayerIndex)
        if (row) row.forceActiveFocus()
        else list.forceActiveFocus()
    }

    color: glass ? "transparent" : ThemeManager.themeColor("panel")
    radius: ThemeManager.themeColor("radius")

    // Opacity is edited live (no history per pixel of drag); the history
    // entry is created once on release, seeded with the drag-start value.
    property int dragStartOpacity: 100

    // ---- header ----
    Rectangle {
        id: header
        width: parent.width
        height: ThemeManager.themeColor("dockerHeaderHeight")
        color: "transparent"
        radius: ThemeManager.themeColor("radius")

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Layers").toUpperCase()
            visible: !docker.embedded
            color: ThemeManager.themeColor("textDim")
            font.pixelSize: 10
            font.letterSpacing: 0.5
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Repeater {
                model: [
                    {
                        icon: "plus",
                        tip: qsTr("New layer"),
                        act: "add"
                    },
                    {
                        icon: "duplicate",
                        tip: qsTr("Duplicate layer"),
                        act: "dup"
                    },
                    {
                        icon: "arrow-up",
                        tip: qsTr("Raise layer"),
                        act: "up"
                    },
                    {
                        icon: "arrow-down",
                        tip: qsTr("Lower layer"),
                        act: "down"
                    },
                    {
                        icon: "trash",
                        tip: qsTr("Delete layer"),
                        act: "del"
                    }
                ]
                delegate: Rectangle {
                    width: 32
                    height: 32
                    radius: ThemeManager.themeColor("radiusControl")
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.tip
                    function trigger() {
                        switch (modelData.act) {
                        case "add":
                            addLayerMenu.popup();
                            break;
                        case "dup":
                            controller.duplicateLayer();
                            break;
                        case "up":
                            controller.moveLayerUp();
                            break;
                        case "down":
                            controller.moveLayerDown();
                            break;
                        case "del":
                            controller.deleteLayer();
                            break;
                        }
                    }
                    Accessible.onPressAction: trigger()
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                            trigger();
                            event.accepted = true;
                        }
                    }
                    border.width: activeFocus ? 1 : 0
                    border.color: ThemeManager.themeColor("highlight")
                    // Names the button for the --demo-click verification hook
                    // (e.g. "layerButton_up" is Raise layer).
                    objectName: "layerButton_" + modelData.act
                    color: mh.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
                    Icon {
                        anchors.centerIn: parent
                        width: 14
                        height: 14
                        name: modelData.icon
                        color: ThemeManager.themeColor("text")
                    }
                    HoverHandler {
                        id: mh
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: parent.trigger()
                    }
                    ToolTip.visible: mh.hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.tip
                }
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: ThemeManager.themeColor("border")
        }
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
            height: 32
            Accessible.name: qsTr("Layer blend mode")
            font.pixelSize: 11
            model: controller.blendModeNames()
            // NOT bound: a binding plus the write from onActivated fight each
            // other, leaving a stale label. Sync explicitly instead.
            Component.onCompleted: syncBlendCombo()
            Connections {
                target: controller
                function onActiveLayerIndexChanged() {
                    blendCombo.syncBlendCombo();
                }
                function onLayersChanged() {
                    blendCombo.syncBlendCombo();
                }
            }
            function syncBlendCombo() {
                const idx = controller.activeLayerIndex;
                const v = controller.layers.data(controller.layers.index(idx, 0), LayerListModel.BlendModeRole);
                currentIndex = v === undefined ? 0 : v;
            }
            onActivated: controller.setLayerBlendMode(controller.activeLayerIndex, currentIndex)
        }

        Row {
            width: parent.width
            spacing: 6
            Text {
                id: opacityLabel
                text: qsTr("Opacity (%)")
                color: ThemeManager.themeColor("text")
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            PSlider {
                id: opacitySlider
                objectName: "opacitySlider"
                height: 32
                Accessible.name: qsTr("Layer opacity")
                width: Math.max(32, parent.width - opacityLabel.width - 66)
                from: 0
                to: 100
                // Value is synced imperatively: while the user drags, the model
                // updates to follow the handle, and outside a drag the model
                // leads. Binding it here made the two overwrite each other, so
                // the handle and the number appeared frozen.
                Component.onCompleted: syncOpacity()
                Connections {
                    target: controller
                    function onActiveLayerIndexChanged() {
                        opacitySlider.syncOpacity();
                    }
                    function onLayersChanged() {
                        opacitySlider.syncOpacity();
                    }
                }
                function syncOpacity() {
                    if (pressed)
                        return;
                    const v = controller.layers.data(controller.layers.index(controller.activeLayerIndex, 0), LayerListModel.OpacityRole);
                    value = v === undefined ? 100 : v;
                }
                onPressedChanged: {
                    if (pressed)
                        docker.dragStartOpacity = value;
                    else
                        controller.commitLayerOpacity(controller.activeLayerIndex, Math.round(value), docker.dragStartOpacity);
                }
                // Live preview without touching history.
                onMoved: {
                    const index = controller.activeLayerIndex
                    if (pressed) controller.previewLayerOpacity(index, Math.round(value))
                    else {
                        const before = controller.layers.data(controller.layers.index(index, 0), LayerListModel.OpacityRole)
                        controller.commitLayerOpacity(index, Math.round(value), before === undefined ? 100 : before)
                    }
                }
            }
            NumericField {
                id: opacityValue
                objectName: "layerOpacityField"
                width: 54
                anchors.verticalCenter: parent.verticalCenter
                Accessible.name: qsTr("Layer opacity percent")
                from: 0
                to: 100
                numberValue: opacitySlider.value
                onCommitted: value => {
                    const index = controller.activeLayerIndex;
                    const before = controller.layers.data(controller.layers.index(index, 0), LayerListModel.OpacityRole);
                    controller.commitLayerOpacity(index, Math.round(value), before === undefined ? 100 : before);
                }
                Connections {
                    target: controller
                    function onActiveLayerIndexChanged() { Qt.callLater(opacityValue.restore); }
                }
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
            text: qsTr("Group selected layer")
            onTriggered: controller.groupLayer()
        }
        MenuItem {
            text: qsTr("Import Image as Layer...")
            onTriggered: {
                console.info("layers+: Import Image as Layer clicked");
                importImageDialog.open();
            }
        }
    }

    FileDialog {
        id: importImageDialog
        title: qsTr("Import Image as Layer")
        nameFilters: controller.imageOpenFilters().concat([qsTr("All files (*)")])
        fileMode: FileDialog.OpenFile
        onAccepted: {
            console.info("layers+: import image accepted: " + selectedFile);
            if (!controller.importImageAsLayer(selectedFile))
                console.warn("layers+: import as layer failed: " + controller.lastError);
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
            text: qsTr("Group selected layer")
            onTriggered: controller.groupLayer()
        }
        MenuItem {
            text: qsTr("Move into group above")
            onTriggered: controller.moveLayerIntoGroupAbove(layerMenu.targetIndex)
        }
        MenuItem {
            text: qsTr("Move out of group")
            onTriggered: controller.moveLayerOutOfGroup(layerMenu.targetIndex)
        }
        MenuItem {
            text: qsTr("Clip to layer below")
            checkable: true
            checked: controller.layers.data(controller.layers.index(layerMenu.targetIndex, 0), LayerListModel.ClippedRole) === true
            onTriggered: controller.setLayerClipped(layerMenu.targetIndex, checked)
        }
        MenuItem {
            text: qsTr("Alpha Lock")
            checkable: true
            checked: controller.layers.data(controller.layers.index(layerMenu.targetIndex, 0), LayerListModel.AlphaLockedRole) === true
            onTriggered: controller.setLayerAlphaLocked(layerMenu.targetIndex, checked)
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
                const rows = controller.layers;
                const visible = rows.data(rows.index(layerMenu.targetIndex, 0), LayerListModel.VisibleRole);
                controller.setLayerVisible(layerMenu.targetIndex, !visible);
            }
        }
        MenuItem {
            text: qsTr("Toggle Lock")
            onTriggered: {
                const rows = controller.layers;
                const locked = rows.data(rows.index(layerMenu.targetIndex, 0), LayerListModel.LockedRole);
                controller.setLayerLocked(layerMenu.targetIndex, !locked);
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Select Layer Content")
            onTriggered: {
                controller.activeLayerIndex = layerMenu.targetIndex;
                controller.selectLayerContent();
            }
        }
        MenuItem {
            text: qsTr("Clear Layer")
            onTriggered: {
                controller.activeLayerIndex = layerMenu.targetIndex;
                controller.clearCanvas();
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Delete Layer")
            enabled: controller.layers.rowCount() > 1
            onTriggered: {
                controller.activeLayerIndex = layerMenu.targetIndex;
                controller.deleteLayer();
            }
        }
    }

    // ---- layer list ----
    ListView {
        id: list
        objectName: "layerList"
        anchors.top: controls.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        height: Math.min(Math.max(36, docker.height - y - 4), count * 36)
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        clip: true
        model: controller.layers
        // Krita's layer docker lists the TOP layer first, while layer index 0
        // is the bottom of the stack (the compositor paints 0..n-1 upwards).
        // Without this the list was upside down: "Raise layer" pushed the row
        // visibly DOWN, which reads as "raise does the opposite".
        verticalLayoutDirection: ListView.BottomToTop
        // Drag a row to restack it (Krita's drag & drop layer reordering). The
        // dragged index is tracked here so the drop indicator can be drawn by
        // the view rather than by a delegate that scrolls away.
        property int dragFromIndex: -1
        property int dropToIndex: -1

        /// Layer index whose row occupies \a viewportY in this view. Asked of
        /// the delegates themselves (instead of dividing by the row height) so
        /// the maths does not depend on the layout direction: BottomToTop makes
        /// contentY grow the other way.
        function indexAtViewportY(viewportY) {
            for (let i = 0; i < count; ++i) {
                const item = itemAtIndex(i);
                if (!item)
                    continue;
                const top = item.y - contentY;
                if (viewportY >= top && viewportY < top + item.height)
                    return i;
            }
            return -1;
        }

        /// Viewport y of the top edge of row \a layerIndex's row.
        function viewportYOfRow(layerIndex) {
            const item = itemAtIndex(layerIndex);
            return item ? item.y - contentY : 0;
        }

        /// Centre of a row in this view's coordinates. Used by the
        /// --demo-drag-layer hook, which must not assume a layout direction.
        function demoRowY(layerIndex) {
            const item = itemAtIndex(layerIndex);
            return item ? item.y - contentY + item.height / 2 : -1;
        }

        // Drop indicator: a Krita-style line showing where the layer will land.
        Rectangle {
            id: dropIndicator
            visible: list.dragFromIndex >= 0 && list.dropToIndex >= 0
            x: 0
            width: list.width
            height: 2
            color: ThemeManager.themeColor("highlight")
            z: 10
            // Drawn on the edge the layer is moving TOWARDS, in list terms.
            y: list.dropToIndex > list.dragFromIndex ? list.viewportYOfRow(list.dropToIndex) + 35 : list.viewportYOfRow(list.dropToIndex)
        }

        delegate: Rectangle {
            id: layerRow
            width: ListView.view ? ListView.view.width : 0
            height: 36
            objectName: "layerRow_" + index
            activeFocusOnTab: !editingName
            Accessible.role: Accessible.ListItem
            Accessible.name: model.name
            Accessible.selected: controller.activeLayerIndex === index
            Accessible.onPressAction: controller.activeLayerIndex = index
            border.width: activeFocus ? 1 : 0
            border.color: ThemeManager.themeColor("highlight")
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                    controller.activeLayerIndex = index;
                    event.accepted = true;
                } else if (event.key === Qt.Key_F2) {
                    layerRow.editingName = true;
                    nameEdit.forceActiveFocus();
                    nameEdit.selectAll();
                    event.accepted = true;
                }
            }
            color: controller.activeLayerIndex === index ? ThemeManager.themeColor("highlight") : "transparent"
            // The row being dragged fades so the gap it will leave is visible.
            opacity: ListView.view && ListView.view.dragFromIndex === index ? 0.45 : 1.0
            // Renaming is entered by double-clicking the row, which keeps the
            // whole row available as a drag surface the rest of the time.
            property bool editingName: false
            // Attached `ListView.view` resolves on the delegate item, not inside
            // a pointer handler, so it is aliased here for the DragHandler.
            readonly property var listView: ListView.view

            // Select / context menu / rename. Dragging is handled by the
            // DragHandler below instead: a MouseArea loses its grab the moment
            // the ListView (a Flickable) decides the gesture is a scroll, which
            // made reordering silently do nothing.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: mouse => {
                    // A plain click makes the row active (Krita selects on
                    // click); the drag handler below only fires once the
                    // pointer actually moves, so without this a click on a
                    // layer did nothing and the user could not switch layers.
                    controller.activeLayerIndex = index;
                    if (mouse.button === Qt.RightButton) {
                        layerMenu.targetIndex = index;
                        layerMenu.parent = layerRow;
                        layerMenu.open();
                    }
                }
                onDoubleClicked: {
                    layerRow.editingName = true;
                    nameEdit.forceActiveFocus();
                    nameEdit.selectAll();
                }
            }

            // Drag to restack, like Krita's layer docker. A pointer handler
            // keeps the grab for the whole gesture, so the drop target is
            // computed from one continuous drag.
            DragHandler {
                id: rowDrag
                target: null
                xAxis.enabled: false
                yAxis.enabled: true
                // The Flickable would otherwise win the grab for a vertical drag.
                grabPermissions: PointerHandler.CanTakeOverFromAnything

                // layerRow.listView, not the outer `list` id and not the
                // attached property: this handler can fire once more while its
                // delegate is being destroyed after the reorder, and the
                // enclosing id is gone by then (ReferenceError).
                readonly property var view: layerRow.listView

                onActiveChanged: {
                    if (!view)
                        return;
                    if (active) {
                        controller.activeLayerIndex = index;
                        view.dragFromIndex = index;
                        view.dropToIndex = index;
                    } else if (view.dragFromIndex >= 0) {
                        console.info("layers: drag row " + view.dragFromIndex + " -> " + view.dropToIndex);
                        controller.moveLayerTo(view.dragFromIndex, view.dropToIndex);
                        view.dragFromIndex = -1;
                        view.dropToIndex = -1;
                    }
                }
                onCentroidChanged: {
                    if (!active || !view)
                        return;
                    // Which row is the pointer over? Ask the view rather than
                    // dividing by the row height: with BottomToTop layout the
                    // y-to-index mapping is flipped.
                    const p = view.mapFromItem(null, rowDrag.centroid.scenePosition.x, rowDrag.centroid.scenePosition.y);
                    const over = view.indexAtViewportY(p.y);
                    if (over < 0)
                        return;
                    view.dropToIndex = over;
                }
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 3
                anchors.right: parent.right
                anchors.rightMargin: 3
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Item {
                    width: Math.min(24, model.depth * 12)
                    height: 1
                }

                // visibility
                Rectangle {
                    id: visibilityButton
                    objectName: "layerVisibility_" + index
                    width: 32
                    height: 32
                    radius: ThemeManager.themeColor("radiusControl")
                    color: visibilityHover.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
                    activeFocusOnTab: true
                    border.width: activeFocus ? 1 : 0
                    border.color: ThemeManager.themeColor("highlight")
                    Accessible.role: Accessible.CheckBox
                    Accessible.name: qsTr("Show layer %1").arg(model.name)
                    Accessible.checked: model.visible
                    Accessible.onToggleAction: controller.setLayerVisible(index, !model.visible)
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                            controller.setLayerVisible(index, !model.visible);
                            event.accepted = true;
                        }
                    }
                    HoverHandler {
                        id: visibilityHover
                    }
                    ToolTip.visible: visibilityHover.hovered
                    ToolTip.text: model.visible ? qsTr("Hide layer") : qsTr("Show layer")
                    Icon {
                        anchors.centerIn: parent
                        width: 13
                        height: 13
                        name: model.visible ? "eye" : "eye-off"
                        color: controller.activeLayerIndex === index ? ThemeManager.themeColor("highlightText") : (model.visible ? ThemeManager.themeColor("text") : ThemeManager.themeColor("textDim"))
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: controller.setLayerVisible(index, !model.visible)
                    }
                }

                // lock
                Rectangle {
                    id: lockButton
                    objectName: "layerLock_" + index
                    width: 32
                    height: 32
                    radius: ThemeManager.themeColor("radiusControl")
                    color: lockHover.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
                    activeFocusOnTab: true
                    border.width: activeFocus ? 1 : 0
                    border.color: ThemeManager.themeColor("highlight")
                    Accessible.role: Accessible.CheckBox
                    Accessible.name: qsTr("Lock layer %1").arg(model.name)
                    Accessible.checked: model.locked
                    Accessible.onToggleAction: controller.setLayerLocked(index, !model.locked)
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                            controller.setLayerLocked(index, !model.locked);
                            event.accepted = true;
                        }
                    }
                    HoverHandler {
                        id: lockHover
                    }
                    ToolTip.visible: lockHover.hovered
                    ToolTip.text: model.locked ? qsTr("Unlock layer") : qsTr("Lock layer")
                    Icon {
                        anchors.centerIn: parent
                        width: 13
                        height: 13
                        name: model.locked ? "lock" : "unlock"
                        color: controller.activeLayerIndex === index ? ThemeManager.themeColor("highlightText") : (model.locked ? ThemeManager.themeColor("text") : ThemeManager.themeColor("textDim"))
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: controller.setLayerLocked(index, !model.locked)
                    }
                }

                // real thumbnail of this layer's pixels
                Rectangle {
                    width: 24
                    height: 20
                    color: ThemeManager.themeColor("panelSunken")
                    border.color: ThemeManager.themeColor("border")
                    border.width: 1
                    Image {
                        anchors.fill: parent
                        anchors.margins: 1
                        fillMode: Image.PreserveAspectFit
                        cache: false
                        source: "image://layerthumb/" + index + "?v=" + controller.layerRevision
                    }
                }

                // Name. A permanently-focusable TextInput swallowed the press
                // that starts a drag, so the row could not be restacked by
                // dragging it. Krita's behaviour instead: drag anywhere on the
                // row, double-click the name to rename.
                Text {
                    id: nameText
                    visible: !layerRow.editingName
                    width: Math.max(12, parent.width - Math.min(24, model.depth * 12) - 64 - 24 - opacityText.width - 18)
                    text: model.type === 2 ? "▣ " + model.name : model.name
                    color: controller.activeLayerIndex === index ? ThemeManager.themeColor("highlightText") : ThemeManager.themeColor("text")
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    clip: true
                }
                TextInput {
                    id: nameEdit
                    visible: layerRow.editingName
                    width: nameText.width
                    text: model.name
                    color: controller.activeLayerIndex === index ? ThemeManager.themeColor("highlightText") : ThemeManager.themeColor("text")
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    selectByMouse: true
                    clip: true
                    onEditingFinished: {
                        controller.renameLayer(index, text);
                        layerRow.editingName = false;
                    }
                    Keys.onEscapePressed: {
                        text = model.name;
                        layerRow.editingName = false;
                    }
                }

                Text {
                    id: opacityText
                    text: model.opacity + "%"
                    color: controller.activeLayerIndex === index ? ThemeManager.themeColor("highlightText") : ThemeManager.themeColor("textDim")
                    font.pixelSize: 10
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }
    }
}
