import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism

Rectangle {
    id: panel
    required property var controller
    property string timelineId: controller.activeRecordingTimeline
    property var rows: []
    property var sessions: []
    property var selectedSessions: []
    property bool initialPreviewPending: false
    signal closeRequested
    signal exportRequested(string timeline, var sessions)
    signal manageRequested
    signal importRequested
    signal linkRequested(string timeline)
    color: ThemeManager.themeColor("panel")
    radius: 12
    function refresh() {
        rows = controller.recordingTimelines;
        let selected = rows.findIndex(row => row.id === timelineId);
        if (selected < 0)
            selected = rows.findIndex(row => row.id === controller.activeRecordingTimeline);
        timelineChoice.currentIndex = selected;
        if (selected >= 0)
            timelineId = rows[selected].id;
        sessions = controller.recordingSessions(timelineId);
    }
    function preview() {
        controller.refreshRecordingPreview(timelineId);
    }
    Component.onCompleted: refresh()
    Connections {
        target: controller
        function onDocumentReplaced() {
            panel.timelineId = controller.activeRecordingTimeline;
            panel.selectedSessions = [];
            panel.sessions = [];
        }
        function onRecordingChanged() {
            if (panel.visible) {
                panel.refresh();
                panel.loadInitialPreview();
            }
        }
    }
    function loadInitialPreview() {
        if (initialPreviewPending && !controller.recordingExporting && controller.recordedFrames > 0 && timelineId.length > 0) {
            initialPreviewPending = false;
            preview();
        }
    }
    onVisibleChanged: {
        initialPreviewPending = visible;
        if (visible) {
            refresh();
            Qt.callLater(loadInitialPreview);
        }
    }
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Recording")
                font.pixelSize: 15
                font.bold: true
                color: ThemeManager.themeColor("text")
                Layout.fillWidth: true
            }
            PButton {
                objectName: "recordingDockClose"
                text: "×"
                implicitWidth: 32
                Accessible.name: qsTr("Close Recording panel")
                onClicked: panel.closeRequested()
            }
        }
        Flickable {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: content.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            FocusScroll {
                viewport: scroll
            }
            ColumnLayout {
                id: content
                width: scroll.width
                spacing: 8
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    font.pixelSize: 12
                    text: controller.recordingError.length ? controller.recordingError : controller.recordingHasGaps ? qsTr("Recording has gaps · Partial export required") : qsTr("Automatic · canvas only · PNG")
                    color: controller.recordingError.length ? ThemeManager.themeColor("danger") : ThemeManager.themeColor("textDim")
                }
                PButton {
                    visible: controller.recordingError.length > 0
                    text: qsTr("Retry recording")
                    enabled: !controller.recordingRepairing
                    onClicked: controller.retryRecording()
                }
                Label {
                    visible: controller.recordingLinkConflict
                    text: qsTr("This artwork differs from its last recording link. Choose a timeline before continuing the link.")
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: ThemeManager.themeColor("text")
                }
                RowLayout {
                    visible: controller.recordingLinkConflict
                    PButton {
                        text: qsTr("Continue previous")
                        onClicked: controller.resolveRecordingLink(true)
                    }
                    PButton {
                        text: qsTr("New timeline")
                        onClicked: controller.resolveRecordingLink(false)
                    }
                }
                PComboBox {
                    id: timelineChoice
                    objectName: "recordingTimelineChoice"
                    enabled: !controller.recordingExporting
                    Layout.fillWidth: true
                    model: panel.rows.map(row => row.name + (row.active ? qsTr(" · current") : ""))
                    onActivated: {
                        panel.timelineId = panel.rows[currentIndex].id;
                        panel.selectedSessions = [];
                        panel.sessions = controller.recordingSessions(panel.timelineId);
                        panel.preview();
                    }
                    Accessible.name: qsTr("Recording timeline")
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 170
                    radius: 8
                    color: ThemeManager.themeColor("panelRaised")
                    Image {
                        anchors.fill: parent
                        anchors.margins: 4
                        source: controller.recordingPreview
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                    Label {
                        anchors.centerIn: parent
                        visible: controller.recordingPreview.length === 0
                        text: qsTr("Refresh to preview stored frames")
                        color: ThemeManager.themeColor("textDim")
                        font.pixelSize: 12
                    }
                }
                PSlider {
                    id: previewSlider
                    objectName: "recordingPreviewSlider"
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(0, controller.recordingPreviewFrames - 1)
                    stepSize: 1
                    enabled: controller.recordingPreviewFrames > 0
                    Accessible.name: qsTr("Recorded frame")
                    onMoved: seekDelay.restart()
                    onPressedChanged: if (!pressed)
                        seekDelay.restart()
                    Timer {
                        id: seekDelay
                        interval: 150
                        onTriggered: {
                            if (controller.recordingExporting)
                                restart();
                            else
                                controller.seekRecordingPreview(Math.round(previewSlider.value));
                        }
                    }
                }
                RowLayout {
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("%1 stored frames").arg(controller.recordingPreviewFrames)
                        color: ThemeManager.themeColor("textDim")
                        font.pixelSize: 12
                    }
                    PButton {
                        objectName: "recordingRefresh"
                        text: qsTr("Refresh")
                        enabled: !controller.recordingExporting
                        onClicked: panel.preview()
                    }
                }
                Label {
                    text: qsTr("Capture interval (seconds)")
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 12
                }
                NumericField {
                    objectName: "recordingIntervalField"
                    Layout.fillWidth: true
                    from: 0.5
                    to: 60
                    decimals: 1
                    stepSize: 0.5
                    numberValue: controller.captureInterval / 1000
                    onCommitted: value => controller.captureInterval = Math.round(value * 1000)
                    Accessible.name: qsTr("Capture interval in seconds")
                }
                Label {
                    text: qsTr("Capture resolution · long edge")
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 12
                }
                PComboBox {
                    objectName: "recordingResolutionChoice"
                    Layout.fillWidth: true
                    model: [qsTr("720 px · economical"), qsTr("1920 px · balanced"), qsTr("Original · larger files")]
                    currentIndex: controller.captureResolution === 720 ? 0 : controller.captureResolution === 0 ? 2 : 1
                    onActivated: controller.captureResolution = [720, 1920, 0][currentIndex]
                }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("New settings affect future frames. Older 720 px frames cannot gain detail when exported larger.")
                    color: ThemeManager.themeColor("textDim")
                    font.pixelSize: 12
                }
                Label {
                    text: qsTr("Sessions · none selected means all")
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 12
                }
                Repeater {
                    model: panel.sessions
                    delegate: PCheckBox {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.name + " · " + (modelData.bytes / 1048576).toFixed(1) + " MiB" + (modelData.active ? qsTr(" · live") : modelData.incomplete ? qsTr(" · incomplete") : "")
                        checked: panel.selectedSessions.indexOf(modelData.id) >= 0
                        onClicked: {
                            let next = panel.selectedSessions.slice();
                            let index = next.indexOf(modelData.id);
                            if (checked && index < 0)
                                next.push(modelData.id);
                            if (!checked && index >= 0)
                                next.splice(index, 1);
                            panel.selectedSessions = next;
                        }
                    }
                }
                PButton {
                    objectName: "recordingDockExport"
                    text: qsTr("Export…")
                    primary: true
                    Layout.fillWidth: true
                    enabled: !controller.recordingExporting && panel.timelineId.length > 0
                    onClicked: panel.exportRequested(panel.timelineId, panel.selectedSessions)
                }
                PButton {
                    text: qsTr("Link artwork…")
                    Layout.fillWidth: true
                    enabled: !controller.recordingExporting
                    onClicked: panel.linkRequested(panel.timelineId)
                }
                PButton {
                    text: qsTr("Import recording bundle…")
                    Layout.fillWidth: true
                    enabled: !controller.recordingExporting
                    onClicked: panel.importRequested()
                }
                PButton {
                    text: qsTr("Manage recordings…")
                    Layout.fillWidth: true
                    onClicked: panel.manageRequested()
                }
                Label {
                    visible: controller.recordingExporting
                    text: qsTr("Working… %1%").arg(controller.recordingExportProgress)
                    color: ThemeManager.themeColor("textDim")
                }
                PButton {
                    text: qsTr("Cancel operation")
                    visible: controller.recordingExporting
                    Layout.fillWidth: true
                    onClicked: controller.cancelRecordingExport()
                }
            }
        }
    }
}
