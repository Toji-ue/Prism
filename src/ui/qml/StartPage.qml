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

    RowLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 24

        // ---------------- Start ----------------
        ColumnLayout {
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            spacing: 6

            Text {
                text: qsTr("Start")
                color: T.text
                font.pixelSize: 21
            }

            Item { Layout.preferredHeight: 10 }

            Repeater {
                model: [
                    { icon: "file-new",  text: qsTr("New Image"),  hint: "Ctrl+N", act: "new" },
                    { icon: "file-open", text: qsTr("Open Image"), hint: "Ctrl+O", act: "open" }
                ]
                delegate: Rectangle {
                    Layout.fillWidth: true
                    height: 34
                    color: se.hovered ? T.buttonHover : "transparent"
                    radius: 3

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        spacing: 10
                        Icon {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            name: modelData.icon
                            color: T.highlight
                        }
                        Text {
                            text: modelData.text
                            color: T.text
                            font.pixelSize: 14
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: modelData.hint
                            color: T.textDim
                            font.pixelSize: 11
                        }
                    }
                    HoverHandler { id: se }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: modelData.act === "new" ? page.requestNew()
                                                           : page.requestOpen()
                    }
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
                        color: rh.hovered ? T.buttonHover : "transparent"
                        radius: 3

                        Column {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 6

                            Rectangle {
                                width: parent.width
                                height: 62
                                color: T.panelSunken
                                border.color: T.border
                                Icon {
                                    anchors.centerIn: parent
                                    width: 28; height: 28
                                    name: modelData.name.endsWith(".prism")
                                          ? "file-save" : "file-open"
                                    color: T.textDim
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

        // ---------------- Info panel ----------------
        // (Krita's "News" column is intentionally dropped — see the request
        // to keep the start page to start/recent/community.)
        ColumnLayout {
            Layout.preferredWidth: 240
            Layout.fillHeight: true
            spacing: 10

            Text {
                text: qsTr("About")
                color: T.text
                font.pixelSize: 21
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: T.windowAlt
                border.color: T.border

                Column {
                    anchors.centerIn: parent
                    anchors.margins: 16
                    spacing: 10
                    Text {
                        width: 200
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("Prism %1").arg(page.controller.versionString())
                        color: T.text
                        font.pixelSize: 13
                    }
                    Text {
                        width: 200
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                        text: qsTr("A native drawing app built with Qt and a custom brush engine.")
                        color: T.textDim
                        font.pixelSize: 11
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
