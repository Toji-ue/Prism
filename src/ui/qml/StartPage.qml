import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

Rectangle {
    id: page
    required property AppController controller
    signal requestNew
    signal requestOpen
    signal requestRecordExport
    color: ThemeManager.themeColor("window")
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.ArrowCursor
    }
    Flickable {
        id: scroll
        FocusScroll {
            viewport: scroll
        }
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: width
        contentHeight: contents.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}
        ColumnLayout {
            id: contents
            width: Math.min(1040, scroll.width - 16)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 16
            Text {
                text: qsTr("Prism")
                color: ThemeManager.themeColor("text")
                font.pixelSize: 28
                font.weight: Font.Medium
            }
            Text {
                text: qsTr("Start a canvas or continue where you left off.")
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 13
            }
            RowLayout {
                spacing: 10
                PButton {
                    objectName: "startNewDocument"
                    text: qsTr("New Document")
                    primary: true
                    implicitHeight: 40
                    implicitWidth: 180
                    onClicked: page.requestNew()
                }
                PButton {
                    objectName: "startOpen"
                    text: qsTr("Open")
                    implicitHeight: 40
                    implicitWidth: 120
                    onClicked: page.requestOpen()
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: page.controller.recoverableArtworks.length > 0
                spacing: 8
                Text {
                    text: qsTr("Recover Artwork")
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Text {
                    text: qsTr("Open a recovery copy, then save it as a project.")
                    color: ThemeManager.themeColor("textDim")
                    font.pixelSize: 12
                }
                ListView {
                    id: artworks
                    FocusScroll {
                        viewport: artworks
                    }
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(132, count * 44)
                    model: page.controller.recoverableArtworks
                    clip: true
                    spacing: 4
                    ScrollBar.vertical: ScrollBar {}
                    delegate: PButton {
                        required property var modelData
                        width: artworks.width
                        height: 40
                        text: modelData.name + " · " + modelData.when
                        Accessible.name: qsTr("Recover artwork %1").arg(modelData.name)
                        enabled: !page.controller.documentActionPending && !page.controller.saving
                        onClicked: page.controller.requestDocumentAction("recover", {
                            url: modelData.url
                        })
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                visible: page.controller.recoverableRecordings.length > 0
                spacing: 8
                Text {
                    text: qsTr("Recover Recording")
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                Text {
                    text: page.controller.recordingRepairing ? qsTr("Loading recording…") : qsTr("Export a timelapse. This does not restore artwork.")
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: ThemeManager.themeColor("textDim")
                    font.pixelSize: 12
                }
                ListView {
                    id: recordings
                    FocusScroll {
                        viewport: recordings
                    }
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(132, count * 44)
                    model: page.controller.recoverableRecordings
                    clip: true
                    spacing: 4
                    ScrollBar.vertical: ScrollBar {}
                    delegate: PButton {
                        required property var modelData
                        width: recordings.width
                        height: 40
                        text: qsTr("Export timelapse · %1").arg(modelData.name)
                        enabled: !page.controller.recordingRepairing && !page.controller.recordingExporting
                        onClicked: page.controller.recoverRecordingAsync(modelData.url)
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Recent Files")
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                PButton {
                    visible: page.controller.recentFiles.length > 0
                    text: qsTr("Clear list")
                    onClicked: page.controller.clearRecentFiles()
                }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: page.controller.recentFiles.length ? Math.min(320, page.controller.recentFiles.length * 78 + 16) : 92
                color: ThemeManager.themeColor("panel")
                radius: ThemeManager.themeColor("radiusPanel")
                Text {
                    anchors.fill: parent
                    anchors.margins: 16
                    visible: page.controller.recentFiles.length === 0
                    text: qsTr("Your recent projects will appear here.\nChoose New Document to start, or Open to continue a project.")
                    wrapMode: Text.WordWrap
                    color: ThemeManager.themeColor("textDim")
                    font.pixelSize: 13
                    verticalAlignment: Text.AlignVCenter
                }
                ListView {
                    id: recent
                    FocusScroll {
                        viewport: recent
                    }
                    anchors.fill: parent
                    anchors.margins: 8
                    visible: count > 0
                    clip: true
                    spacing: 4
                    model: page.controller.recentFiles
                    ScrollBar.vertical: ScrollBar {}
                    delegate: RowLayout {
                        required property var modelData
                        required property int index
                        width: recent.width
                        height: 74
                        spacing: 6
                        PButton {
                            objectName: "recentFile_" + index
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: modelData.name
                            enabled: modelData.exists && !page.controller.documentActionPending
                            Accessible.description: modelData.path
                            onClicked: page.controller.requestDocumentAction("open", {
                                url: modelData.url
                            })
                            ToolTip.visible: hovered
                            ToolTip.text: modelData.path
                            contentItem: RowLayout {
                                spacing: 12
                                Image {
                                    Layout.preferredWidth: 46
                                    Layout.preferredHeight: 46
                                    source: modelData.exists ? modelData.previewUrl : ""
                                    fillMode: Image.PreserveAspectFit
                                    asynchronous: true
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: ThemeManager.themeColor("text")
                                        font.pixelSize: 13
                                        elide: Text.ElideMiddle
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.exists ? modelData.path : qsTr("Missing file · %1").arg(modelData.path)
                                        color: ThemeManager.themeColor("textDim")
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                    }
                                }
                            }
                        }
                        PButton {
                            objectName: "removeRecent_" + index
                            visible: !modelData.exists
                            text: qsTr("Remove")
                            Accessible.name: qsTr("Remove %1 from recents").arg(modelData.name)
                            onClicked: page.controller.removeRecentFile(modelData.url)
                        }
                    }
                }
            }
            Text {
                text: qsTr("Prism %1").arg(page.controller.versionString())
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 11
            }
        }
    }
}
