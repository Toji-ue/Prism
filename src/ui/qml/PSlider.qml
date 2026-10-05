import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as T

// Compact Krita-style slider: thin groove, small square handle. The default
// Controls Basic slider renders a large round knob that looks wrong in a
// dense tool UI, so every slider in the app uses this instead.
Slider {
    id: control

    implicitHeight: 18

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
