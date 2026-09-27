import QtQuick
import QtQuick.Controls.Basic
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style Brush Presets docker: header actions, filter row, thumbnail
// grid, search field. Thumbnails come from the real brush engine in C++
// (image://preset/<i>), so each cell shows the stroke it will actually paint.
Rectangle {
    id: docker
    required property AppController controller

    color: T.panel

    // Filter state — the grid shows preset indices that survive the filter.
    property string query: ""
    property var visibleIndices: []

    function rebuildFilter() {
        var out = []
        var q = docker.query.toLowerCase()
        for (var i = 0; i < docker.controller.presetCount(); ++i) {
            var n = docker.controller.presetName(i)
            if (q.length === 0 || n.toLowerCase().indexOf(q) >= 0)
                out.push(i)
        }
        docker.visibleIndices = out
    }

    Component.onCompleted: rebuildFilter()
    Connections {
        target: docker.controller
        function onPresetsChanged() { docker.rebuildFilter() }
    }

    onQueryChanged: rebuildFilter()

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
                model: [
                    { icon: "plus",       tip: qsTr("Save current brush as preset"), act: "add" },
                    { icon: "arrow-up",   tip: qsTr("Move preset up"),   act: "up" },
                    { icon: "arrow-down", tip: qsTr("Move preset down"), act: "down" },
                    { icon: "trash",      tip: qsTr("Delete preset"),    act: "del" }
                ]
                delegate: Rectangle {
                    width: 20; height: 20
                    color: bh.hovered ? T.buttonHover : "transparent"
                    Icon {
                        anchors.centerIn: parent
                        width: 14; height: 14
                        name: modelData.icon
                        color: T.text
                    }
                    HoverHandler { id: bh }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            var i = docker.controller.activePresetIndex
                            switch (modelData.act) {
                            case "add":  docker.controller.addPreset(); break
                            case "up":   docker.controller.movePreset(i, 1); break
                            case "down": docker.controller.movePreset(i, -1); break
                            case "del":  docker.controller.removePreset(i); break
                            }
                        }
                    }
                    ToolTip.visible: bh.hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.tip
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

        Rectangle {
            width: 58; height: 20
            color: T.windowAlt
            border.color: T.border
            Text {
                anchors.centerIn: parent
                text: qsTr("All")
                color: T.text
                font.pixelSize: 10
            }
        }
        Rectangle {
            width: 58; height: 20
            color: T.windowAlt
            border.color: T.border
            Text {
                anchors.centerIn: parent
                text: qsTr("Tag")
                color: T.textDim
                font.pixelSize: 10
            }
        }
        Rectangle {
            width: 58; height: 20
            color: T.windowAlt
            border.color: T.border
            Text {
                anchors.centerIn: parent
                text: qsTr("All")
                color: T.textDim
                font.pixelSize: 10
            }
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
        model: docker.visibleIndices
        currentIndex: docker.visibleIndices.indexOf(docker.controller.activePresetIndex)

        delegate: Rectangle {
            required property int modelData
            readonly property int presetIndex: modelData

            width: 48; height: 48
            color: docker.controller.activePresetIndex === presetIndex
                   ? T.highlight : (hh.hovered ? T.buttonHover : T.panelSunken)
            border.color: docker.controller.activePresetIndex === presetIndex
                          ? T.highlight : T.border
            border.width: 1

            Image {
                anchors.fill: parent
                anchors.margins: 3
                fillMode: Image.PreserveAspectFit
                cache: false
                sourceSize.width: 44
                sourceSize.height: 44
                source: "image://preset/" + presetIndex
            }

            HoverHandler { id: hh }

            MouseArea {
                anchors.fill: parent
                onClicked: docker.controller.selectPreset(presetIndex)
            }

            ToolTip.visible: hh.hovered
            ToolTip.delay: 500
            ToolTip.text: docker.controller.presetName(presetIndex)
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

        Rectangle {
            width: parent.width - 22
            height: 20
            color: T.windowAlt
            border.color: T.border

            Row {
                anchors.fill: parent
                anchors.leftMargin: 4
                spacing: 4
                Icon {
                    width: 12; height: 12
                    name: "search"
                    color: T.textDim
                    anchors.verticalCenter: parent.verticalCenter
                }
                TextInput {
                    id: searchField
                    width: parent.width - 20
                    anchors.verticalCenter: parent.verticalCenter
                    color: T.text
                    font.pixelSize: 10
                    clip: true
                    onTextChanged: docker.query = text
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: searchField.text.length === 0
                        text: qsTr("Search…")
                        color: T.textDim
                        font.pixelSize: 10
                    }
                }
            }
        }
        Rectangle {
            width: 18; height: 20
            color: clearHover.hovered ? T.buttonHover : "transparent"
            Icon {
                anchors.centerIn: parent
                width: 12; height: 12
                name: "close"
                color: T.textDim
            }
            HoverHandler { id: clearHover }
            MouseArea {
                anchors.fill: parent
                onClicked: { searchField.text = ""; docker.query = "" }
            }
            ToolTip.visible: clearHover.hovered
            ToolTip.text: qsTr("Clear search")
        }
    }
}
