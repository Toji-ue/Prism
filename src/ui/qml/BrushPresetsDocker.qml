import QtQuick
import QtQuick.Controls.Basic
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style Brush Presets docker: header actions, a filter row, then a
// thumbnail grid. Thumbnails are rendered by the real brush engine in C++,
// so each preset looks like the stroke it will actually paint.
Rectangle {
    id: docker
    required property AppController controller

    color: T.panel
    height: 300

    // ---- header ----
    Rectangle {
        id: header
        width: parent.width
        height: T.dockerHeaderHeight
        color: T.windowAlt

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Brush Presets")
            color: T.text
            font.pixelSize: 11
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1
            Repeater {
                model: ["＋", "▴", "▾", "✕"]
                delegate: Rectangle {
                    width: 20; height: 20
                    color: bh.hovered ? T.buttonHover : "transparent"
                    opacity: 0.5 // preset management is not wired to storage yet
                    Text {
                        anchors.centerIn: parent
                        text: modelData
                        color: T.text
                        font.pixelSize: 11
                    }
                    HoverHandler { id: bh }
                }
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: T.border }
    }

    // ---- filter row ----
    Row {
        id: filterRow
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 4
        spacing: 4

        ComboBox {
            width: 54; height: 20
            font.pixelSize: 10
            model: ["All"]
        }
        ComboBox {
            width: 54; height: 20
            font.pixelSize: 10
            model: ["Tag", "Ink", "Paint", "Eraser"]
        }
        ComboBox {
            width: 54; height: 20
            font.pixelSize: 10
            model: ["All", "Favorites"]
        }
    }

    // ---- thumbnail grid ----
    GridView {
        id: grid
        anchors.top: filterRow.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: searchRow.top
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        clip: true
        cellWidth: 50
        cellHeight: 50
        model: controller.presetCount()
        currentIndex: controller.activePresetIndex

        delegate: Rectangle {
            width: 48; height: 48
            color: grid.currentIndex === index ? T.highlight : T.panelSunken
            border.color: grid.currentIndex === index ? T.highlight : T.border
            border.width: 1

            Image {
                id: thumb
                anchors.fill: parent
                anchors.margins: 3
                fillMode: Image.PreserveAspectFit
                cache: true
                sourceSize.width: 44
                sourceSize.height: 44
                // Rendered by the real brush engine in C++ (image provider).
                source: "image://preset/" + index
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    docker.controller.selectPreset(index)
                    grid.currentIndex = index
                }
            }

            ToolTip.visible: thumbHover.hovered
            ToolTip.text: docker.controller.presetName(index)
            HoverHandler { id: thumbHover }
        }
    }

    // ---- search row ----
    Row {
        id: searchRow
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 4
        spacing: 4

        TextField {
            width: parent.width - 60
            height: 20
            font.pixelSize: 10
            placeholderText: qsTr("Search…")
        }
        CheckBox {
            text: qsTr("Filter")
            font.pixelSize: 10
            height: 20
        }
    }
}
