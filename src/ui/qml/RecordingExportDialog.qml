import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
import Prism

Dialog {
    id: dialog
    objectName: "recordingExportDialog"
    required property var controller
    property string timelineId: ""
    property var sessions: []
    property bool partial: false
    modal: true
    title: qsTr("Export timelapse")
    width: Math.min(480, parent ? parent.width - 32 : 480)
    height: Math.min(680, parent ? parent.height - 32 : 680)
    anchors.centerIn: Overlay.overlay
    background: Rectangle {
        color: ThemeManager.themeColor("popup")
        radius: 12
        border.color: ThemeManager.themeColor("popupBorder")
    }
    function settings() {
        return {
            seconds: [60, 120, 240][durationChoice.currentIndex],
            fps: [12, 24, 30][fpsChoice.currentIndex],
            maxSide: resolutionChoice.currentIndex === 0 ? 1280 : 1920,
            endSeconds: Math.round(endField.numberValue),
            introSeconds: introCheck.checked ? Math.round(introField.numberValue) : 0,
            inputFps: customSpeed.checked ? inputField.numberValue : 0
        };
    }
    readonly property int sourceCount: controller.recordingPreviewTimeline !== timelineId ? 0 : sessions.length ? sessions.reduce((n, id) => n + (controller.recordingFrameCounts[id] || 0), 0) : controller.recordingPreviewFrames
    readonly property real estimatedSeconds: customSpeed.checked ? sourceCount / Math.max(0.1, inputField.numberValue) + endField.numberValue + (introCheck.checked ? introField.numberValue : 0) : [60, 120, 240][durationChoice.currentIndex]
    header: Label {
        text: dialog.title
        padding: 16
        bottomPadding: 8
        font.pixelSize: 16
        font.weight: Font.DemiBold
        color: ThemeManager.themeColor("text")
    }
    contentItem: Flickable {
        id: scroll
        clip: true
        contentWidth: width
        contentHeight: body.height
        ScrollBar.vertical: ScrollBar {}
        FocusScroll {
            viewport: scroll
        }
        ColumnLayout {
            id: body
            width: scroll.width
            spacing: 10
            Label {
                text: qsTr("Format")
                color: ThemeManager.themeColor("text")
            }
            PComboBox {
                id: formatChoice
                objectName: "recordExportFormat"
                model: [qsTr("MP4 · H.264 (Windows)"), qsTr("GIF"), qsTr("Prism record bundle · v3")]
                Layout.fillWidth: true
            }
            Label {
                text: qsTr("Target length (includes intro/end)")
                color: ThemeManager.themeColor("text")
            }
            PComboBox {
                id: durationChoice
                objectName: "recordExportDuration"
                model: [qsTr("1 minute"), qsTr("2 minutes"), qsTr("4 minutes")]
                currentIndex: 1
                Layout.fillWidth: true
                enabled: !customSpeed.checked
            }
            RowLayout {
                Layout.fillWidth: true
                PComboBox {
                    id: resolutionChoice
                    model: [qsTr("1280 px long edge"), qsTr("1920 px long edge")]
                    currentIndex: 1
                    Layout.fillWidth: true
                }
                PComboBox {
                    id: fpsChoice
                    model: ["12 FPS", "24 FPS", "30 FPS"]
                    currentIndex: 2
                    Layout.preferredWidth: 110
                }
            }
            PButton {
                text: advanced.visible ? qsTr("Hide advanced") : qsTr("Advanced…")
                onClicked: advanced.visible = !advanced.visible
            }
            ColumnLayout {
                id: advanced
                visible: false
                Layout.fillWidth: true
                PCheckBox {
                    id: customSpeed
                    text: qsTr("Custom speed using Input FPS")
                }
                NumericField {
                    id: inputField
                    from: 0.1
                    to: 1000
                    decimals: 1
                    numberValue: 12
                    enabled: customSpeed.checked
                    Layout.fillWidth: true
                    onCommitted: value => numberValue = value
                    Accessible.name: qsTr("Input FPS")
                }
                Label {
                    text: qsTr("Hold final image (seconds)")
                    color: ThemeManager.themeColor("text")
                }
                NumericField {
                    id: endField
                    from: 0
                    to: 60
                    numberValue: 5
                    Layout.fillWidth: true
                    onCommitted: value => numberValue = value
                }
                PCheckBox {
                    id: introCheck
                    text: qsTr("Show final image at beginning")
                }
                NumericField {
                    id: introField
                    from: 1
                    to: 30
                    numberValue: 3
                    enabled: introCheck.checked
                    Layout.fillWidth: true
                    onCommitted: value => numberValue = value
                }
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: dialog.estimatedSeconds > 240 ? ThemeManager.themeColor("danger") : ThemeManager.themeColor("textDim")
                text: customSpeed.checked && dialog.sourceCount === 0 ? qsTr("Refresh the timeline preview to estimate Custom speed.") : qsTr("Estimated video: %1 seconds · maximum 240 seconds. Low source frame counts produce held images, not additional drawing detail.").arg(dialog.estimatedSeconds.toFixed(1))
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: formatChoice.currentIndex === 2
                text: qsTr("A full bundle includes related Save As branches and original frames. Selected-session export contains only your selection. No four-minute limit applies to record data.")
                color: ThemeManager.themeColor("textDim")
            }
            PCheckBox {
                id: partialCheck
                checked: dialog.partial
                text: qsTr("Allow Partial export")
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                visible: partialCheck.checked
                text: qsTr("I understand missing frames cannot be recovered and this export may not cover the complete drawing.")
                color: ThemeManager.themeColor("danger")
            }
            RowLayout {
                Layout.fillWidth: true
                PButton {
                    text: qsTr("Cancel")
                    onClicked: dialog.close()
                }
                Item {
                    Layout.fillWidth: true
                }
                PButton {
                    objectName: "recordExportChooseFile"
                    text: qsTr("Export…")
                    primary: true
                    enabled: !controller.recordingExporting && (formatChoice.currentIndex === 2 || dialog.estimatedSeconds <= 240)
                    onClicked: {
                        if (!endField.commit() || !introField.commit() || (customSpeed.checked && !inputField.commit()))
                            return;
                        file.nameFilters = [formatChoice.currentIndex === 0 ? qsTr("MP4 video (*.mp4)") : formatChoice.currentIndex === 1 ? qsTr("GIF animation (*.gif)") : qsTr("Prism record bundle (*.prismrec)")];
                        file.open();
                    }
                }
            }
        }
    }
    FileDialog {
        id: file
        title: qsTr("Export recording")
        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (controller.exportTimelapse(selectedFile, dialog.settings(), dialog.timelineId, dialog.sessions, partialCheck.checked))
                dialog.close();
        }
    }
}
