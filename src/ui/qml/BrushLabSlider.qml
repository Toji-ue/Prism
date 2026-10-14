import QtQuick
import QtQuick.Controls.Basic
import Prism

Row {
    id: control
    property string caption: ""
    property real controllerValue: 0
    property real from: 0
    property real to: 1
    property string suffix: "%"
    property real displayScale: 100
    signal moved(real value)

    width: parent.width
    height: ThemeManager.themeColor("controlHeight")
    spacing: 6

    Text {
        width: 86
        anchors.verticalCenter: parent.verticalCenter
        text: control.caption
        color: ThemeManager.themeColor("textDim")
        font.pixelSize: ThemeManager.themeColor("fontBody")
        elide: Text.ElideRight
    }
    PSlider {
        id: slider
        objectName: control.objectName.length ? control.objectName + "Slider" : ""
        width: Math.max(60, control.width - 145)
        anchors.verticalCenter: parent.verticalCenter
        from: control.from
        to: control.to
        Component.onCompleted: value = control.controllerValue
        onMoved: control.moved(value)
        onPressedChanged: if (!pressed) value = control.controllerValue
    }
    Text {
        width: 44
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignRight
        text: Math.round(control.controllerValue * control.displayScale) + control.suffix
        color: ThemeManager.themeColor("text")
        font.pixelSize: ThemeManager.themeColor("fontBody")
    }
    onControllerValueChanged: if (!slider.pressed) slider.value = controllerValue
}
