import QtQuick
import QtQuick.Controls.Basic
import Prism

CheckBox {
    id: control
    readonly property bool keyboardButton: true
    implicitHeight: 32
    leftPadding: 0
    spacing: 8
    Accessible.name: text
    indicator: Rectangle {
        x: 2
        y: (control.height - height) / 2
        width: 22
        height: 22
        radius: ThemeManager.themeColor("radiusChip")
        color: control.checked ? ThemeManager.themeColor("selection") : ThemeManager.themeColor("panelSunken")
        border.width: control.visualFocus ? 2 : 1
        border.color: control.checked || control.visualFocus ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
        Icon {
            anchors.centerIn: parent
            width: 14
            height: 14
            name: "check"
            visible: control.checked
            color: ThemeManager.themeColor("text")
        }
    }
    contentItem: Text {
        leftPadding: 32
        text: control.text
        verticalAlignment: Text.AlignVCenter
        font.pixelSize: ThemeManager.themeColor("fontBody")
        color: control.enabled ? ThemeManager.themeColor("text") : ThemeManager.themeColor("disabledText")
        elide: Text.ElideRight
    }
}
