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
        Accessible.name: control.caption
        objectName: control.objectName.length ? control.objectName + "Slider" : ""
        width: Math.max(50, control.width - 176)
        anchors.verticalCenter: parent.verticalCenter
        from: control.from
        to: control.to
        Component.onCompleted: value = control.controllerValue
        onMoved: control.moved(value)
        onPressedChanged: if (!pressed) value = control.controllerValue
    }
    NumericField {
        objectName: control.objectName.length ? control.objectName + "Field" : ""
        width: 72
        anchors.verticalCenter: parent.verticalCenter
        Accessible.name: control.caption
        from: control.from * control.displayScale
        to: control.to * control.displayScale
        decimals: control.displayScale === 1 && control.to < 10 ? 2 : 0
        numberValue: control.controllerValue * control.displayScale
        onCommitted: value => control.moved(value / control.displayScale)
        ToolTip.visible: hovered
        ToolTip.text: control.caption + " (" + control.suffix.trim() + ")"
    }
    onControllerValueChanged: if (!slider.pressed) slider.value = controllerValue
}
