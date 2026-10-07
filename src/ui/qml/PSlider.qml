import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as T

// Compact Krita-style slider: thin groove, small square handle. The default
// Controls Basic slider renders a large round knob that looks wrong in a
// dense tool UI, so every slider in the app uses this instead.
Slider {
    id: control

    implicitHeight: 18

    // Wheel and arrow keys move the handle. Dragging a 10px handle with a pen
    // is imprecise, and every slider in the app gets this for free.
    readonly property real nudgeStep: control.stepSize > 0 ? control.stepSize
                                                           : Math.max((control.to - control.from) / 100, 1)

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
        radius: T.radiusSmall
        color: T.sliderGroove
        border.color: T.border

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: T.radiusSmall
            color: T.highlight
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 10
        height: 14
        radius: 2
        color: control.pressed ? T.highlight : T.text
        border.color: T.border
    }
}
