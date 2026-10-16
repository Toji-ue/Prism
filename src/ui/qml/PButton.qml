import QtQuick
import QtQuick.Controls.Basic
import Prism

Button {
    id: control
    property bool primary: false
    readonly property bool keyboardButton: true
    implicitHeight: 32
    implicitWidth: Math.max(32, implicitContentWidth + 24)
    padding: 8
    Accessible.name: text
    contentItem: Text {
        text: control.text
        color: !control.enabled ? ThemeManager.themeColor("disabledText") : control.primary ? ThemeManager.themeColor("highlightText") : ThemeManager.themeColor("text")
        font.pixelSize: ThemeManager.themeColor("fontBody")
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: ThemeManager.themeColor("radiusControl")
        color: control.primary ? ThemeManager.themeColor("highlight") : control.down || control.checked ? ThemeManager.themeColor("selection") : control.hovered ? ThemeManager.themeColor("buttonHover") : ThemeManager.themeColor("panelRaised")
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus || control.checked ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
        opacity: control.enabled ? 1 : 0.55
    }
}
