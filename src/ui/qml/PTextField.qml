import QtQuick
import QtQuick.Controls.Basic
import Prism

TextField {
    id: control
    property bool invalid: false
    implicitHeight: 32
    implicitWidth: 120
    leftPadding: 9
    rightPadding: 9
    color: enabled ? ThemeManager.themeColor("text") : ThemeManager.themeColor("disabledText")
    placeholderTextColor: ThemeManager.themeColor("textDim")
    selectionColor: ThemeManager.themeColor("highlight")
    selectedTextColor: ThemeManager.themeColor("highlightText")
    font.pixelSize: ThemeManager.themeColor("fontBody")
    selectByMouse: true
    background: Rectangle {
        radius: ThemeManager.themeColor("radiusControl")
        color: ThemeManager.themeColor("panelSunken")
        border.width: control.activeFocus ? 2 : 1
        border.color: control.invalid ? ThemeManager.themeColor("danger") : control.activeFocus ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
    }
}
