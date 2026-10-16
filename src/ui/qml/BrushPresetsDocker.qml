import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

Rectangle {
    id: docker
    required property AppController controller
    property bool embedded: false
    property bool editing: false
    property string query: ""
    property int filterMode: 0
    property var filteredPresets: []
    color: ThemeManager.themeColor("panel")
    radius: ThemeManager.themeColor("radiusPanel")
    function browse() {
        editing = false;
        searchField.clear();
        filterMode = 0;
        rebuildFilter();
    }
    function editBrush() {
        editing = true;
        basic.expanded = true;
        editor.contentY = 0;
    }
    function rebuildFilter() {
        const hadFocus = presets.activeFocus;
        const focusedId = presets.currentItem ? presets.currentItem.modelData.id : "";
        const pinFocused = presets.currentItem ? presets.currentItem.pinControl.activeFocus : false;
        const q = query.toLowerCase().trim();
        let entries = controller.brushPresets.filter(p => (!q.length || p.name.toLowerCase().indexOf(q) >= 0) && (filterMode === 0 || (filterMode === 1 ? p.pinned : p.recent >= 0)));
        if (filterMode === 2)
            entries.sort((a, b) => a.recent - b.recent);
        filteredPresets = entries;
        if (hadFocus)
            Qt.callLater(() => {
                if (presets.visible)
                    presets.focusEntry(entries.findIndex(p => p.id === focusedId), pinFocused);
            });
    }
    Component.onCompleted: rebuildFilter()
    onQueryChanged: rebuildFilter()
    onFilterModeChanged: rebuildFilter()
    Connections {
        target: docker.controller
        function onQuickPresetsChanged() {
            docker.rebuildFilter();
        }
        function onPresetsChanged() {
            docker.rebuildFilter();
        }
    }
    component Setting: BrushLabSlider {
        required property var modelData
        objectName: modelData.id
        caption: modelData.label
        from: modelData.min === undefined ? 0 : modelData.min
        to: modelData.max === undefined ? 1 : modelData.max
        displayScale: modelData.scale === undefined ? 100 : modelData.scale
        suffix: modelData.unit === undefined ? "%" : modelData.unit
        controllerValue: docker.controller[modelData.prop]
        enabled: !(modelData.solidOnly && docker.controller.brushGraphite)
        onMoved: value => docker.controller[modelData.prop] = value
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            PButton {
                objectName: "brushBrowseBack"
                visible: docker.editing
                text: qsTr("Back")
                onClicked: docker.editing = false
            }
            Text {
                Layout.fillWidth: true
                text: docker.editing ? docker.controller.activePresetName : qsTr("Choose a brush")
                color: ThemeManager.themeColor("text")
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            ToolbarButton {
                objectName: "brushManage"
                iconName: "more"
                tip: qsTr("Manage brushes")
                onClicked: management.open()
            }
        }
        Text {
            visible: docker.controller.brushModified
            text: qsTr("Modified · temporary changes")
            color: ThemeManager.themeColor("textDim")
            font.pixelSize: 11
        }
        PTextField {
            id: searchField
            objectName: "brushSearchInput"
            visible: !docker.editing
            Layout.fillWidth: true
            placeholderText: qsTr("Search brushes")
            Accessible.name: qsTr("Search brushes")
            onTextChanged: docker.query = text
        }
        RowLayout {
            visible: !docker.editing
            Layout.fillWidth: true
            Repeater {
                model: [qsTr("All"), qsTr("Pinned"), qsTr("Recent")]
                PButton {
                    required property int index
                    required property string modelData
                    objectName: "brushFilter_" + index
                    Layout.fillWidth: true
                    text: modelData
                    checked: docker.filterMode === index
                    onClicked: docker.filterMode = index
                }
            }
        }
        ListView {
            id: presets
            FocusScroll {
                viewport: presets
            }
            objectName: "brushBrowseList"
            visible: !docker.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: docker.filteredPresets
            ScrollBar.vertical: ScrollBar {}
            activeFocusOnTab: true
            function focusEntry(next, pin) {
                if (!count)
                    return;
                currentIndex = Math.max(0, Math.min(count - 1, next));
                positionViewAtIndex(currentIndex, ListView.Contain);
                Qt.callLater(() => {
                    if (!currentItem || !visible)
                        return;
                    if (pin)
                        currentItem.pinControl.forceActiveFocus();
                    else
                        currentItem.forceActiveFocus();
                });
            }
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Down)
                    focusEntry(currentIndex + 1);
                else if (event.key === Qt.Key_Up)
                    focusEntry(currentIndex - 1);
                else if (event.key === Qt.Key_Home)
                    focusEntry(0);
                else if (event.key === Qt.Key_End)
                    focusEntry(count - 1);
                else
                    return;
                event.accepted = true;
            }
            delegate: FocusScope {
                required property var modelData
                required property int index
                property alias pinControl: pinButton
                width: presets.width
                height: 54
                focus: ListView.isCurrentItem
                onActiveFocusChanged: if (activeFocus)
                    presets.currentIndex = index
                RowLayout {
                    anchors.fill: parent
                    spacing: 4
                    PButton {
                        focus: true
                        objectName: "brushPreset_" + modelData.index
                        Layout.fillWidth: true
                        implicitHeight: 54
                        checked: docker.controller.activePresetId === modelData.id
                        text: modelData.name
                        Accessible.name: qsTr("Select %1").arg(modelData.name)
                        onClicked: docker.controller.selectPreset(modelData.index)
                        contentItem: RowLayout {
                            spacing: 10
                            Image {
                                Layout.preferredWidth: 64
                                Layout.preferredHeight: 34
                                source: "image://preset/" + modelData.index + "/" + modelData.id
                                fillMode: Image.PreserveAspectFit
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: ThemeManager.themeColor("text")
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                    }
                    ToolbarButton {
                        id: pinButton
                        objectName: "brushPin_" + modelData.index
                        iconName: "pin"
                        active: modelData.pinned
                        tip: modelData.pinned ? qsTr("Unpin %1").arg(modelData.name) : qsTr("Pin %1").arg(modelData.name)
                        onClicked: docker.controller.togglePinnedPreset(modelData.index)
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                width: parent.width - 16
                visible: presets.count === 0
                text: docker.query.length ? qsTr("No matching brushes. Try another name.") : docker.filterMode === 1 ? qsTr("Pin a brush to keep it here.") : qsTr("Choose a brush from All to start your recent list.")
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 12
            }
        }
        Rectangle {
            visible: docker.editing
            Layout.fillWidth: true
            Layout.preferredHeight: 106
            radius: ThemeManager.themeColor("radiusControl")
            color: ThemeManager.themeColor("panelSunken")
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                Text {
                    text: docker.controller.brushGraphite ? qsTr("Light / firm / tilt · live preview") : qsTr("Live stroke preview")
                    color: ThemeManager.themeColor("textDim")
                    font.pixelSize: 11
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "white"
                    radius: 4
                    Image {
                        anchors.fill: parent
                        anchors.margins: 3
                        source: "image://preset/active/" + docker.controller.brushPreviewRevision
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        sourceSize.width: 300
                        sourceSize.height: 84
                    }
                }
            }
        }
        Flickable {
            id: editor
            objectName: "brushEditScroll"
            FocusScroll {
                viewport: editor
            }
            visible: docker.editing
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: sections.height
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            ScrollBar.vertical: ScrollBar {}
            Column {
                id: sections
                width: editor.width - 10
                spacing: 12
                BrushSection {
                    id: basic
                    objectName: "brushBasic"
                    title: qsTr("Basic")
                    width: parent.width
                    expanded: true
                    Repeater {
                        model: [
                            {
                                id: "labSize",
                                label: qsTr("Size (px)"),
                                prop: "brushSize",
                                min: 1,
                                max: 200,
                                scale: 1,
                                unit: "px"
                            },
                            {
                                id: "labFlow",
                                label: qsTr("Flow (%)"),
                                prop: "brushFlow",
                                min: 0.01
                            },
                            {
                                id: "labHardness",
                                label: qsTr("Hardness (%)"),
                                prop: "brushHardness"
                            },
                            {
                                id: "labStabilization",
                                label: qsTr("Stabilize (%)"),
                                prop: "brushStabilization"
                            }
                        ]
                        Setting {}
                    }
                }
                BrushSection {
                    objectName: "brushTip"
                    title: qsTr("Tip")
                    width: parent.width
                    Row {
                        spacing: 6
                        Repeater {
                            model: [qsTr("Round"), qsTr("Square"), qsTr("Image")]
                            PButton {
                                required property int index
                                required property string modelData
                                text: modelData
                                checked: index === 2 ? docker.controller.tipIsImage : !docker.controller.tipIsImage && docker.controller.brushTipSquare === (index === 1)
                                onClicked: {
                                    if (index === 2)
                                        tipImageDialog.open();
                                    else {
                                        if (docker.controller.tipIsImage)
                                            docker.controller.clearTipImage();
                                        docker.controller.brushTipSquare = index === 1;
                                    }
                                }
                            }
                        }
                    }
                    PButton {
                        visible: docker.controller.tipIsImage
                        text: qsTr("Remove tip image")
                        onClicked: docker.controller.clearTipImage()
                    }
                    Repeater {
                        model: [
                            {
                                id: "tipRoundness",
                                label: qsTr("Roundness (%)"),
                                prop: "brushTipRoundness",
                                min: 0.05
                            },
                            {
                                id: "tipAngle",
                                label: qsTr("Angle (deg)"),
                                prop: "brushTipAngle",
                                min: 0,
                                max: 359,
                                scale: 1,
                                unit: "deg"
                            }
                        ]
                        Setting {}
                    }
                    PCheckBox {
                        text: qsTr("Graphite material")
                        checked: docker.controller.brushGraphite
                        onToggled: docker.controller.brushGraphite = checked
                    }
                }
                BrushSection {
                    objectName: "brushDynamics"
                    title: qsTr("Dynamics")
                    width: parent.width
                    PCheckBox {
                        objectName: "pressureOpacityBox"
                        text: qsTr("Pressure affects deposit")
                        checked: docker.controller.pressureOpacity
                        onToggled: docker.controller.pressureOpacity = checked
                    }
                    Repeater {
                        model: [
                            {
                                id: "pressureCurve",
                                label: qsTr("Pressure curve"),
                                prop: "pressureCurve",
                                min: 0.3,
                                max: 3,
                                scale: 1,
                                unit: "gamma"
                            },
                            {
                                id: "labSpacing",
                                label: qsTr("Spacing (%)"),
                                prop: "brushSpacing",
                                min: 0.02
                            },
                            {
                                id: "labMinSize",
                                label: qsTr("Light size (%)"),
                                prop: "pressureSizeMin",
                                min: 0.05,
                                solidOnly: true
                            },
                            {
                                id: "labEndTaper",
                                label: qsTr("End taper"),
                                prop: "brushEndTaper",
                                min: 0,
                                max: 6,
                                scale: 1,
                                unit: "diameters",
                                solidOnly: true
                            }
                        ]
                        Setting {}
                    }
                    Text {
                        width: parent.width
                        visible: docker.controller.brushGraphite
                        text: qsTr("Graphite keeps rounded ends and at least 75% width at light pressure; light size and end taper do not apply.")
                        wrapMode: Text.WordWrap
                        color: ThemeManager.themeColor("textDim")
                        font.pixelSize: 11
                    }
                }
                BrushSection {
                    objectName: "brushGraphiteSection"
                    title: qsTr("Graphite")
                    width: parent.width
                    visible: docker.controller.brushGraphite
                    Repeater {
                        model: [
                            {
                                id: "labGrain",
                                label: qsTr("Grain (%)"),
                                prop: "brushGrain"
                            },
                            {
                                id: "labGrainScale",
                                label: qsTr("Grain scale"),
                                prop: "brushGrainScale",
                                min: 0.5,
                                max: 4,
                                scale: 1,
                                unit: "scale"
                            },
                            {
                                id: "labTiltShading",
                                label: qsTr("Tilt shade (%)"),
                                prop: "brushTiltShading"
                            }
                        ]
                        Setting {}
                    }
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            PButton {
                objectName: "brushEdit"
                visible: !docker.editing
                Layout.fillWidth: true
                text: qsTr("Edit brush")
                onClicked: docker.editBrush()
            }
            PButton {
                objectName: "brushSaveCopy"
                visible: docker.editing
                Layout.fillWidth: true
                text: qsTr("Save copy")
                onClicked: docker.controller.addPreset()
            }
            PButton {
                objectName: "brushReset"
                visible: docker.editing
                text: qsTr("Reset")
                enabled: docker.controller.brushModified
                onClicked: docker.controller.resetBrushToPreset()
            }
        }
        Text {
            visible: docker.editing
            Layout.fillWidth: true
            text: qsTr("Switching presets loads their saved settings. Save a copy to keep edits.")
            wrapMode: Text.WordWrap
            color: ThemeManager.themeColor("textDim")
            font.pixelSize: 11
        }
    }
    PMenu {
        id: management
        x: docker.width - width - 8
        y: 40
        Action {
            text: qsTr("Import brush…")
            onTriggered: brushImportDialog.open()
        }
        Action {
            text: qsTr("Export brush…")
            onTriggered: brushExportDialog.open()
        }
        Action {
            text: qsTr("Move preset up")
            enabled: docker.controller.activePresetIndex >= docker.controller.builtinPresetCount && docker.controller.activePresetIndex < docker.controller.presetCount() - 1
            onTriggered: docker.controller.movePreset(docker.controller.activePresetIndex, 1)
        }
        Action {
            text: qsTr("Move preset down")
            enabled: docker.controller.activePresetIndex > docker.controller.builtinPresetCount
            onTriggered: docker.controller.movePreset(docker.controller.activePresetIndex, -1)
        }
        Action {
            objectName: "brushDeleteAction"
            text: qsTr("Delete preset…")
            enabled: docker.controller.activePresetIndex >= docker.controller.builtinPresetCount
            onTriggered: {
                deletePreset.presetId = docker.controller.activePresetId;
                deletePreset.open();
            }
        }
    }
    Dialog {
        id: deletePreset
        objectName: "brushDeleteDialog"
        property string presetId: ""
        width: 292
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete preset?")
        contentItem: Column {
            spacing: 12
            Text {
                width: 260
                wrapMode: Text.WordWrap
                text: qsTr("This removes the saved preset from your brushes.")
                color: ThemeManager.themeColor("text")
                font.pixelSize: 12
            }
            Row {
                spacing: 8
                PButton {
                    text: qsTr("Cancel")
                    onClicked: deletePreset.close()
                }
                PButton {
                    objectName: "brushDeleteConfirm"
                    text: qsTr("Delete")
                    onClicked: {
                        if (deletePreset.presetId === docker.controller.activePresetId)
                            docker.controller.removePreset(docker.controller.activePresetIndex);
                        deletePreset.close();
                    }
                }
            }
        }
        background: Rectangle {
            color: ThemeManager.themeColor("popup")
            radius: ThemeManager.themeColor("radiusPanel")
            border.color: ThemeManager.themeColor("border")
        }
    }
    FileDialog {
        id: tipImageDialog
        title: qsTr("Select tip image (max 1024×1024)")
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.bmp)")]
        onAccepted: docker.controller.loadTipImage(selectedFile)
    }
    FileDialog {
        id: brushImportDialog
        title: qsTr("Import brush")
        nameFilters: [qsTr("Prism brush (*.prismbrush)")]
        onAccepted: docker.controller.importBrush(selectedFile)
    }
    FileDialog {
        id: brushExportDialog
        title: qsTr("Export brush")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("Prism brush (*.prismbrush)")]
        defaultSuffix: "prismbrush"
        onAccepted: docker.controller.exportBrush(selectedFile, docker.controller.activePresetIndex)
    }
}
