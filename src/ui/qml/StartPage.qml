import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

Rectangle {
    id: page
    required property AppController controller
    readonly property bool bothRecoveryTypes: controller.recoverableArtworks.length > 0
                                              && controller.recoverableRecordings.length > 0
    color: ThemeManager.themeColor("window")
    signal requestNew()
    signal requestOpen()
    signal requestRecordExport()

    MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton; cursorShape: Qt.ArrowCursor; hoverEnabled: true }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Math.max(32, (page.width - 1040) / 2)
        anchors.rightMargin: Math.max(32, (page.width - 1040) / 2)
        anchors.topMargin: Math.max(24, (page.height - 610) / 3)
        anchors.bottomMargin: 16
        spacing: 0

        Text {
            text: qsTr("PRISM")
            color: ThemeManager.themeColor("highlight")
            font.pixelSize: 12; font.weight: Font.DemiBold; font.letterSpacing: 3
        }
        Text {
            Layout.topMargin: 12
            text: qsTr("Make space for the work.")
            color: ThemeManager.themeColor("text")
            font.pixelSize: 33; font.weight: Font.Medium; font.letterSpacing: -0.8
        }
        Text {
            Layout.topMargin: 7
            text: qsTr("Start a canvas or continue where you left off.")
            color: ThemeManager.themeColor("textDim")
            font.pixelSize: 13
        }
        RowLayout {
            Layout.fillWidth: true; Layout.topMargin: 32; spacing: 10
            Rectangle {
                Layout.preferredWidth: 194; Layout.preferredHeight: 44
                radius: ThemeManager.themeColor("radiusControl")
                color: newHover.hovered ? Qt.lighter(ThemeManager.themeColor("highlight"), 1.08) : ThemeManager.themeColor("highlight")
                Row {
                    anchors.centerIn: parent; spacing: 10
                    Icon { width: 17; height: 17; name: "file-new"; color: ThemeManager.themeColor("highlightText") }
                    Text { text: qsTr("New Document"); color: ThemeManager.themeColor("highlightText"); font.pixelSize: 13; font.weight: Font.DemiBold }
                }
                HoverHandler { id: newHover }
                MouseArea { anchors.fill: parent; onClicked: page.requestNew() }
            }
            Rectangle {
                Layout.preferredWidth: 144; Layout.preferredHeight: 44
                radius: ThemeManager.themeColor("radiusControl")
                color: openHover.hovered ? ThemeManager.themeColor("buttonHover") : ThemeManager.themeColor("panelRaised")
                border.color: ThemeManager.themeColor("border")
                Row {
                    anchors.centerIn: parent; spacing: 10
                    Icon { width: 17; height: 17; name: "file-open"; color: ThemeManager.themeColor("text") }
                    Text { text: qsTr("Open"); color: ThemeManager.themeColor("text"); font.pixelSize: 13 }
                }
                HoverHandler { id: openHover }
                MouseArea { anchors.fill: parent; onClicked: page.requestOpen() }
            }
            Item { Layout.fillWidth: true }
        }

        ColumnLayout {
            Layout.fillWidth: true; Layout.topMargin: 24
            visible: page.controller.recoverableArtworks.length > 0
            spacing: 8
            Text { text: qsTr("Recover Artwork"); color: ThemeManager.themeColor("text"); font.pixelSize: 15; font.weight: Font.Medium }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(page.height < 700
                                                  ? (page.bothRecoveryTypes ? 72 : 108) : 142,
                                                  count * 50)
                clip: true
                spacing: 8
                model: page.controller.recoverableArtworks
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: 42
                    color: recoveryHover.hovered ? ThemeManager.themeColor("buttonHover") : ThemeManager.themeColor("panelRaised")
                    radius: ThemeManager.themeColor("radiusControl")
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                        Text { text: modelData.name; color: ThemeManager.themeColor("text"); font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
                        Text { text: modelData.when; color: ThemeManager.themeColor("textDim"); font.pixelSize: 11 }
                        Text { text: qsTr("Recover"); color: ThemeManager.themeColor("highlight"); font.pixelSize: 12 }
                    }
                    HoverHandler { id: recoveryHover }
                    MouseArea { anchors.fill: parent; onClicked: page.controller.requestDocumentAction("recover", {url: modelData.url}) }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.topMargin: 15
            visible: page.controller.recoverableRecordings.length > 0
            spacing: 8
            Text { text: qsTr("Recover Recording"); color: ThemeManager.themeColor("text"); font.pixelSize: 15; font.weight: Font.Medium }
            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(page.height < 700
                                                  ? (page.bothRecoveryTypes ? 72 : 108) : 142,
                                                  count * 50)
                clip: true
                spacing: 8
                model: page.controller.recoverableRecordings
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: 42
                    color: recordingHover.hovered ? ThemeManager.themeColor("buttonHover") : ThemeManager.themeColor("panelRaised")
                    radius: ThemeManager.themeColor("radiusControl")
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 12; anchors.rightMargin: 12
                        Text { text: modelData.name; color: ThemeManager.themeColor("text"); font.pixelSize: 12; Layout.fillWidth: true }
                        Text { text: qsTr("Export timelapse"); color: ThemeManager.themeColor("highlight"); font.pixelSize: 12 }
                    }
                    HoverHandler { id: recordingHover }
                    MouseArea { anchors.fill: parent; enabled: !page.controller.recordingRepairing; onClicked: page.controller.recoverRecordingAsync(modelData.url) }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: page.controller.recoverableArtworks.length > 0
                              || page.controller.recoverableRecordings.length > 0 ? 20 : 47
            Text { text: qsTr("Recent Files"); color: ThemeManager.themeColor("text"); font.pixelSize: 17; font.weight: Font.Medium }
            Item { Layout.fillWidth: true }
            Text {
                visible: page.controller.recentFiles.length > 0
                text: qsTr("Clear list")
                color: clearHover.hovered ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("textDim")
                font.pixelSize: 12
                HoverHandler { id: clearHover }
                MouseArea { anchors.fill: parent; onClicked: page.controller.clearRecentFiles() }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 100
            Layout.topMargin: 12
            radius: ThemeManager.themeColor("radiusPanel")
            color: ThemeManager.themeColor("panel")

            Text {
                visible: page.controller.recentFiles.length === 0
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 18
                text: qsTr("No recent files yet. Your work will appear here.")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 13
            }
            ListView {
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                visible: page.controller.recentFiles.length > 0
                model: page.controller.recentFiles
                delegate: Rectangle {
                    required property var modelData
                    width: ListView.view.width; height: 62
                    radius: ThemeManager.themeColor("radiusControl")
                    color: recentHover.hovered ? ThemeManager.themeColor("panelRaised") : "transparent"
                    Rectangle {
                        x: 9; y: 10; width: 42; height: 42
                        radius: ThemeManager.themeColor("radiusChip")
                        color: ThemeManager.themeColor("panelSunken")
                        Image {
                            id: thumbnail
                            anchors.fill: parent; anchors.margins: 2
                            asynchronous: true; fillMode: Image.PreserveAspectFit
                            sourceSize: Qt.size(42, 42)
                            source: modelData.exists ? modelData.previewUrl : ""
                            visible: status === Image.Ready
                        }
                        Icon { anchors.centerIn: parent; width: 20; height: 20; visible: thumbnail.status !== Image.Ready; name: "file-open"; color: ThemeManager.themeColor("textDim") }
                    }
                    Column {
                        x: 66; y: 11; width: parent.width - 82; spacing: 5
                        Text { width: parent.width; text: modelData.name; color: modelData.exists ? ThemeManager.themeColor("text") : ThemeManager.themeColor("textDim"); font.pixelSize: 13; elide: Text.ElideMiddle }
                        Text { width: parent.width; text: modelData.exists ? modelData.path : qsTr("File no longer exists"); color: ThemeManager.themeColor("textDim"); font.pixelSize: 11; elide: Text.ElideMiddle }
                    }
                    HoverHandler { id: recentHover }
                    MouseArea {
                        anchors.fill: parent; enabled: modelData.exists
                        onClicked: {
                            page.controller.requestDocumentAction("open", {url: modelData.url})
                        }
                    }
                    ToolTip.visible: recentHover.hovered; ToolTip.delay: 500
                    ToolTip.text: modelData.exists ? modelData.path : qsTr("File no longer exists: %1").arg(modelData.path)
                }
            }
        }
        Text { Layout.topMargin: 15; text: qsTr("Prism %1").arg(page.controller.versionString()); color: ThemeManager.themeColor("textDim"); font.pixelSize: 11 }
    }
}
