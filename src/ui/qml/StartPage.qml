import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style start page: Start actions on the left, Recent Images in the
// middle, News/links on the right, and a community/about footer.
Rectangle {
    id: page
    required property AppController controller

    color: T.window

    signal requestNew()
    signal requestOpen()

    // Cover the whole page with a plain-arrow cursor. Areas of the start page
    // that declare no cursor of their own used to inherit the canvas's blank
    // painting cursor through the overlay, so the pointer simply vanished.
    // NoButton keeps this from swallowing the real clicks underneath.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        cursorShape: Qt.ArrowCursor
        hoverEnabled: true
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 24

        // ---------------- Hero + primary actions ----------------
        ColumnLayout {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            spacing: 6

            Text {
                text: qsTr("Prism")
                color: T.text
                font.pixelSize: 46
                font.letterSpacing: -1.5
            }
            Text {
                text: qsTr("Layered painting for digital artists")
                color: T.textDim
                font.pixelSize: 13
            }

            Item { Layout.preferredHeight: 26 }

            // Primary action: accent-filled. One obvious next step beats two
            // identical text rows.
            Rectangle {
                Layout.fillWidth: true
                height: 46
                radius: T.radius
                color: se.hovered ? Qt.lighter(T.highlight, 1.08) : T.highlight

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    Icon {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        name: "file-new"
                        color: T.highlightText
                    }
                    Text {
                        text: qsTr("New Document")
                        color: T.highlightText
                        font.pixelSize: 15
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Ctrl+N"
                        color: T.highlightText
                        opacity: 0.65
                        font.pixelSize: 11
                    }
                }
                HoverHandler { id: se }
                MouseArea {
                    anchors.fill: parent
                    onClicked: page.requestNew()
                }
            }

            // Secondary action: outlined, same size and rhythm.
            Rectangle {
                Layout.fillWidth: true
                height: 46
                radius: T.radius
                color: so.hovered ? T.buttonHover : "transparent"
                border.color: T.border

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    Icon {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        name: "file-open"
                        color: T.text
                    }
                    Text {
                        text: qsTr("Open Image")
                        color: T.text
                        font.pixelSize: 15
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Ctrl+O"
                        color: T.textDim
                        font.pixelSize: 11
                    }
                }
                HoverHandler { id: so }
                MouseArea {
                    anchors.fill: parent
                    onClicked: page.requestOpen()
                }
            }

            Item { Layout.fillHeight: true }

            // ---------------- Community / about ----------------
            Text {
                text: qsTr("Community")
                color: T.text
                font.pixelSize: 21
            }
            Repeater {
                model: [
                    { icon: "swatch",       label: qsTr("Support Prism") },
                    { icon: "duplicate",    label: qsTr("Source Code") },
                    { icon: "transparency", label: qsTr("Website") }
                ]
                delegate: RowLayout {
                    spacing: 8
                    Icon {
                        Layout.preferredWidth: 14
                        Layout.preferredHeight: 14
                        name: modelData.icon
                        color: T.textDim
                    }
                    Text {
                        text: modelData.label
                        color: T.textDim
                        font.pixelSize: 12
                    }
                }
            }
        }

        // ---------------- Recent Images ----------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: qsTr("Recent Images")
                    color: T.text
                    font.pixelSize: 21
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    visible: page.controller.recentFiles.length > 0
                    width: 90; height: 24
                    color: ch.hovered ? T.buttonHover : "transparent"
                    border.color: T.border
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Clear list")
                        color: T.textDim
                        font.pixelSize: 11
                    }
                    HoverHandler { id: ch }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: page.controller.clearRecentFiles()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: T.windowAlt
                border.color: T.border

                // empty state, like Krita's
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: page.controller.recentFiles.length === 0
                    text: qsTr("You don't seem to have any recently opened documents")
                    color: T.textDim
                    font.pixelSize: 12
                }

                GridView {
                    id: recents
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    cellWidth: 120
                    cellHeight: 110
                    model: page.controller.recentFiles

                    delegate: Rectangle {
                        width: 112; height: 102
                        color: T.panelRaised
                        radius: T.radiusSmall
                        border.color: rh.hovered ? T.highlight : T.border
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 6

                            Rectangle {
                                width: parent.width
                                height: 62
                                radius: T.radiusSmall
                                color: T.panelSunken
                                Icon {
                                    anchors.centerIn: parent
                                    width: 28; height: 28
                                    name: modelData.name.endsWith(".prism")
                                          ? "file-save" : "file-open"
                                    color: rh.hovered ? T.highlight : T.textDim
                                }
                            }
                            Text {
                                width: parent.width
                                text: modelData.name
                                color: T.text
                                font.pixelSize: 11
                                elide: Text.ElideMiddle
                                wrapMode: Text.NoWrap
                            }
                        }

                        HoverHandler { id: rh }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                const url = modelData.url
                                if (modelData.name.toLowerCase().endsWith(".prism"))
                                    page.controller.openProject(url)
                                else
                                    page.controller.importImage(url)
                            }
                        }
                        ToolTip.visible: rh.hovered
                        ToolTip.delay: 500
                        ToolTip.text: modelData.path
                    }
                }
            }
        }
    }

    // ---------------- footer ----------------
    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        spacing: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Prism is an open drawing application for digital artists.")
            color: T.textDim
            font.pixelSize: 11
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Prism %1 · canvas painting, brush engine, layered documents")
                  .arg(page.controller.versionString())
            color: T.textDim
            font.pixelSize: 10
        }
    }
}
