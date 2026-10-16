import QtQuick
import QtQuick.Controls.Basic

import Prism

// Full-height hit area with a quiet track; scrolling changes only a focused slider.
Slider {
    id: control

    implicitHeight: 32

    // Wheel and arrow keys move the handle. Dragging a 10px handle with a pen
    // is imprecise, and every slider in the app gets this for free.
    readonly property real nudgeStep: control.stepSize > 0 ? control.stepSize
                                                           : (control.to - control.from) / 100

    function nudge(direction) {
        control.value = Math.max(control.from, Math.min(control.to,
                                                        control.value + direction * control.nudgeStep))
        // Behave like a drag: the controls that preview live listen to onMoved.
        control.moved()
    }

    activeFocusOnTab: true
    // Clicking the groove must let the arrows work straight afterwards.
    onPressedChanged: if (pressed) forceActiveFocus()

    Keys.onLeftPressed: control.nudge(-1)
    Keys.onDownPressed: control.nudge(-1)
    Keys.onRightPressed: control.nudge(1)
    Keys.onUpPressed: control.nudge(1)

    WheelHandler {
        enabled: control.activeFocus
        onWheel: wheel => {
            control.nudge(wheel.angleDelta.y > 0 ? 1 : -1)
            wheel.accepted = true
        }
    }

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4
        radius: ThemeManager.themeColor("radiusSmall")
        color: ThemeManager.themeColor("sliderGroove")
        border.color: ThemeManager.themeColor("border")

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: ThemeManager.themeColor("radiusSmall")
            color: ThemeManager.themeColor("textDim")
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 10
        height: 14
        radius: ThemeManager.themeColor("radiusChip")
        color: control.pressed ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("text")
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
    }
}

