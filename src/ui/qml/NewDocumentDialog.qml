import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style "Create new document" dialog: preset categories on the left,
// Dimensions/Content tabs, Image Size and Color groups, a live summary line,
// and Create/Cancel.
Dialog {
    id: dialog
    required property AppController controller

    title: qsTr("Create new document - Prism")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 660
    height: 520
    padding: 0

    readonly property var categories: [
        { icon: "📄", name: qsTr("Custom Document") },
        { icon: "🖼", name: qsTr("Create from Clipboard") },
        { icon: "🎬", name: qsTr("Animation Templates") },
        { icon: "💬", name: qsTr("Comic Templates") },
        { icon: "16:10", name: qsTr("Design Templates") },
        { icon: "📷", name: qsTr("DSLR Templates") },
        { icon: "▦", name: qsTr("Texture Templates") }
    ]

    background: Rectangle {
        color: T.window
        border.color: T.border
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        // -------- body --------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // left: preset categories
            Rectangle {
                Layout.preferredWidth: 190
                Layout.fillHeight: true
                color: T.window

                ListView {
                    id: catList
                    anchors.fill: parent
                    anchors.margins: 4
                    currentIndex: 0
                    model: dialog.categories

                    delegate: Rectangle {
                        width: catList.width
                        height: 34
                        color: catList.currentIndex === index ? T.highlight : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            spacing: 8
                            Text {
                                text: modelData.icon
                                font.pixelSize: 13
                                color: catList.currentIndex === index
                                       ? T.highlightText : T.text
                                Layout.preferredWidth: 24
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                elide: Text.ElideRight
                                font.pixelSize: 12
                                color: catList.currentIndex === index
                                       ? T.highlightText : T.text
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: catList.currentIndex = index
                        }
                    }
                }
            }

            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: T.border }

            // right: settings
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: T.windowAlt

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    // tabs (Dimensions active; Content is informational)
                    RowLayout {
                        spacing: 4
                        Repeater {
                            model: [qsTr("Dimensions"), qsTr("Content")]
                            delegate: Rectangle {
                                width: 92; height: 24
                                color: index === dimTab.currentIndex ? T.windowAlt : T.panel
                                border.color: T.border
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: 11
                                    color: index === dimTab.currentIndex ? T.text : T.textDim
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: dimTab.currentIndex = index
                                }
                            }
                        }
                        QtObject { id: dimTab; property int currentIndex: 0 }
                        Item { Layout.fillWidth: true }
                    }

                    // -------- Image Size group --------
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 150
                        color: "transparent"
                        border.color: T.border
                        border.width: 1

                        Text {
                            x: 10; y: -8
                            text: qsTr("Image Size")
                            color: T.text
                            font.pixelSize: 11
                            Rectangle { anchors.fill: parent; anchors.margins: -2; color: T.windowAlt; z: -1 }
                        }

                        GridLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            columns: 4
                            columnSpacing: 8
                            rowSpacing: 8

                            Text { text: qsTr("Predefined:"); color: T.text; font.pixelSize: 11 }
                            ComboBox {
                                Layout.columnSpan: 2
                                Layout.fillWidth: true
                                height: 24
                                font.pixelSize: 11
                                model: ["Custom", "A4 300dpi", "A4 72dpi", "HD 1920×1080",
                                        "Full HD 2480×3508", "Square 2048",
                                        "Instagram 1080×1080", "Comic page 2480×3508"]
                                onActivated: {
                                    var sizes = [[0,0],[2480,3508],[842,595],[1920,1080],
                                                 [2480,3508],[2048,2048],[1080,1080],[2480,3508]]
                                    var s = sizes[currentIndex]
                                    if (s[0] > 0) {
                                        wField.text = s[0]
                                        hField.text = s[1]
                                    }
                                }
                            }
                            RowLayout {
                                spacing: 2
                                Rectangle {
                                    width: 22; height: 22; color: T.panel; border.color: T.border
                                    Text { anchors.centerIn: parent; text: "▭"; color: T.text; font.pixelSize: 12 }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: { var t = wField.text; wField.text = hField.text; hField.text = t }
                                    }
                                }
                                Rectangle {
                                    width: 22; height: 22
                                    color: T.highlight
                                    border.color: T.border
                                    Text { anchors.centerIn: parent; text: "▯"; color: T.highlightText; font.pixelSize: 12 }
                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: { var t = wField.text; wField.text = hField.text; hField.text = t }
                                    }
                                }
                            }

                            Text { text: qsTr("Width:"); color: T.text; font.pixelSize: 11 }
                            PSpinBox {
                                id: wField
                                Layout.fillWidth: true
                                from: 1; to: 20000
                                value: 2480
                                MouseArea { anchors.fill: parent; acceptedButtons: Qt.NoButton }
                            }
                            ComboBox {
                                Layout.fillWidth: true
                                height: 24
                                font.pixelSize: 11
                                model: [qsTr("Pixels (px)"), qsTr("Inches"), qsTr("Millimeters")]
                            }
                            Item {}

                            Text { text: qsTr("Height:"); color: T.text; font.pixelSize: 11 }
                            PSpinBox {
                                id: hField
                                Layout.fillWidth: true
                                from: 1; to: 20000
                                value: 3508
                            }
                            ComboBox {
                                Layout.fillWidth: true
                                height: 24
                                font.pixelSize: 11
                                model: [qsTr("Pixels (px)"), qsTr("Inches"), qsTr("Millimeters")]
                            }
                            Item {}

                            Text { text: qsTr("Resolution:"); color: T.text; font.pixelSize: 11 }
                            PSpinBox {
                                Layout.fillWidth: true
                                from: 1; to: 4800
                                value: 300
                            }
                            ComboBox {
                                Layout.fillWidth: true
                                height: 24
                                font.pixelSize: 11
                                model: [qsTr("Pixels/Inch"), qsTr("Pixels/Centimeter")]
                            }
                            Item {}
                        }
                    }

                    // -------- Color group --------
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 118
                        color: "transparent"
                        border.color: T.border
                        border.width: 1

                        Text {
                            x: 10; y: -8
                            text: qsTr("Color")
                            color: T.text
                            font.pixelSize: 11
                            Rectangle { anchors.fill: parent; anchors.margins: -2; color: T.windowAlt; z: -1 }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 8

                            Repeater {
                                model: [
                                    { label: qsTr("Model:"),  model: [qsTr("RGB/Alpha"), qsTr("Grayscale/Alpha")], idx: 0 },
                                    { label: qsTr("Depth:"),  model: [qsTr("8-bit integer/channel"), qsTr("16-bit integer/channel")], idx: 0 },
                                    { label: qsTr("Profile:"), model: [qsTr("sRGB-elle-V2-srgbtrc.icc (Default)"), qsTr("sRGB built-in")], idx: 0 }
                                ]
                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Text {
                                        text: modelData.label
                                        color: T.text
                                        font.pixelSize: 11
                                        Layout.preferredWidth: 60
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        height: 24
                                        font.pixelSize: 11
                                        model: modelData.model
                                        currentIndex: modelData.idx
                                    }
                                }
                            }
                        }
                    }

                    // -------- summary --------
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: T.text
                        font.pixelSize: 11
                        text: qsTr("This document will be %1 pixels by %2 pixels in RGB/Alpha (8-bit integer/channel). The pixel size is 32 bit. A single paint layer will use %3 MiB of RAM.")
                              .arg(wField.value).arg(hField.value)
                              .arg(((wField.value * hField.value * 4) / (1024 * 1024)).toFixed(1))
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }

        // -------- footer --------
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            color: T.window
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: T.border }

            RowLayout {
                anchors.fill: parent
                anchors.rightMargin: 12
                spacing: 8
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Create")
                    Layout.preferredWidth: 90
                    onClicked: {
                        dialog.controller.newDocument(wField.value, hField.value)
                        dialog.close()
                    }
                }
                Button {
                    text: qsTr("Cancel")
                    Layout.preferredWidth: 90
                    onClicked: dialog.close()
                }
            }
        }
    }
}
