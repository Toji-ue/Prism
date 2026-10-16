import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

Rectangle {
    id: dock
    required property AppController controller
    signal openFullPicker
    implicitHeight: colourColumn.implicitHeight + 20
    color: ThemeManager.themeColor("panel")
    radius: ThemeManager.themeColor("radiusPanel")

    component TextAction: PButton {
        id: action
        implicitHeight: 32
        leftPadding: 8
        rightPadding: 8
        Accessible.name: text
        contentItem: Text {
            text: action.text
            color: ThemeManager.themeColor("text")
            font.pixelSize: ThemeManager.themeColor("fontBody")
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: action.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
            radius: ThemeManager.themeColor("radiusControl")
            border.width: action.visualFocus ? 1 : 0
            border.color: ThemeManager.themeColor("highlight")
        }
    }
    ColumnLayout {
        id: colourColumn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 7
        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            Text {
                Layout.fillWidth: true
                text: qsTr("Colour")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: ThemeManager.themeColor("fontBody")
                font.weight: Font.DemiBold
            }
            TextAction {
                objectName: "colourPickerPin"
                text: dock.controller.colourPickerPinned ? qsTr("Unpin") : qsTr("Pin")
                onClicked: dock.controller.colourPickerPinned = !dock.controller.colourPickerPinned
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Keep the full colour picker open")
            }
            TextAction {
                objectName: "colourPickerMore"
                text: qsTr("More…")
                onClicked: dock.openFullPicker()
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Full colour picker (Ctrl+3)")
            }
        }
        Rectangle {
            id: currentColour
            objectName: "currentColourSwatch"
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            radius: ThemeManager.themeColor("radiusControl")
            color: dock.controller.brushColor
            border.color: activeFocus ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
            border.width: activeFocus ? 2 : 1
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Current brush colour %1. Open colour picker").arg(dock.controller.brushColor)
            Accessible.onPressAction: dock.openFullPicker()
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                    dock.openFullPicker();
                    event.accepted = true;
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: dock.openFullPicker()
            }
        }
        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            spacing: 5
            Repeater {
                model: dock.controller.recentColors
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    objectName: "recentColour_" + index
                    width: 32
                    height: 32
                    radius: ThemeManager.themeColor("radiusChip")
                    color: modelData
                    border.color: activeFocus ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
                    border.width: activeFocus ? 2 : 1
                    activeFocusOnTab: true
                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Use recent colour %1").arg(modelData)
                    Accessible.onPressAction: dock.controller.brushColor = modelData
                    Keys.onPressed: event => {
                        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                            dock.controller.brushColor = modelData;
                            event.accepted = true;
                        }
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: dock.controller.brushColor = modelData
                    }
                }
            }
        }
    }
}
