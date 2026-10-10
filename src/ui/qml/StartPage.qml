import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

// Krita-style start page: Start actions on the left, Recent Images in the
// middle, News/links on the right, and a community/about footer.
Rectangle {
    id: page
    required property AppController controller

    color: ThemeManager.themeColor("window")

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
                color: ThemeManager.themeColor("text")
                font.pixelSize: 46
                font.letterSpacing: -1.5
            }
            Text {
                text: qsTr("Layered painting for digital artists")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 13
            }

            Item { Layout.preferredHeight: 26 }

            // Primary action: accent-filled. One obvious next step beats two
            // identical text rows.
            Rectangle {
                Layout.fillWidth: true
                height: 46
                radius: ThemeManager.themeColor("radius")
                color: se.hovered ? Qt.lighter(ThemeManager.themeColor("highlight"), 1.08) : ThemeManager.themeColor("highlight")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    Icon {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        name: "file-new"
                        color: ThemeManager.themeColor("highlightText")
                    }
                    Text {
                        text: qsTr("New Document")
                        color: ThemeManager.themeColor("highlightText")
                        font.pixelSize: 15
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Ctrl+N"
                        color: ThemeManager.themeColor("highlightText")
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
                radius: ThemeManager.themeColor("radius")
                color: so.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
                border.color: ThemeManager.themeColor("border")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 10
                    Icon {
                        Layout.preferredWidth: 18
                        Layout.preferredHeight: 18
                        name: "file-open"
                        color: ThemeManager.themeColor("text")
                    }
                    Text {
                        text: qsTr("Open Image")
                        color: ThemeManager.themeColor("text")
                        font.pixelSize: 15
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "Ctrl+O"
                        color: ThemeManager.themeColor("textDim")
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
                color: ThemeManager.themeColor("text")
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
                        color: ThemeManager.themeColor("textDim")
                    }
                    Text {
                        text: modelData.label
                        color: ThemeManager.themeColor("textDim")
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
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 21
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    visible: page.controller.recentFiles.length > 0
                    width: 90; height: 24
                    color: ch.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
                    border.color: ThemeManager.themeColor("border")
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("Clear list")
                        color: ThemeManager.themeColor("textDim")
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
                color: ThemeManager.themeColor("windowAlt")
                border.color: ThemeManager.themeColor("border")

                // empty state, like Krita's
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    visible: page.controller.recentFiles.length === 0
                    text: qsTr("You don't seem to have any recently opened documents")
                    color: ThemeManager.themeColor("textDim")
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
                        color: ThemeManager.themeColor("panelRaised")
                        radius: ThemeManager.themeColor("radiusSmall")
                        border.color: rh.hovered ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 6

                            Rectangle {
                                width: parent.width
                                height: 62
                                radius: ThemeManager.themeColor("radiusSmall")
                                color: ThemeManager.themeColor("panelSunken")
                                // Show the file's own picture. Qt decodes any
                                // raster format itself, so no provider is
                                // needed; .prism projects (and anything that
                                // fails to load) keep the format icon.
                                Image {
                                    id: recentThumb
                                    anchors.fill: parent
                                    anchors.margins: 3
                                    asynchronous: true
                                    fillMode: Image.PreserveAspectFit
                                    sourceSize.width: 100
                                    sourceSize.height: 56
                                    source: modelData.name.endsWith(".prism") ? "" : modelData.url
                                    visible: status === Image.Ready
                                }
                                Icon {
                                    anchors.centerIn: parent
                                    width: 28; height: 28
                                    visible: recentThumb.status !== Image.Ready
                                    name: modelData.name.endsWith(".prism")
                                          ? "file-save" : "file-open"
                                    color: rh.hovered ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("textDim")
                                }
                            }
                            Text {
                                width: parent.width
                                text: modelData.name
                                color: ThemeManager.themeColor("text")
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
            color: ThemeManager.themeColor("textDim")
            font.pixelSize: 11
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("Prism %1 Â· canvas painting, brush engine, layered documents")
                  .arg(page.controller.versionString())
            color: ThemeManager.themeColor("textDim")
            font.pixelSize: 10
        }
    }
}

