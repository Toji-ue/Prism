import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as T

// Compact Krita-style spin box: bordered field with two small stepper
// buttons. Controls Basic's SpinBox draws oversized +/− buttons that break
// the dense toolbar layout, so the app uses this instead.
//
// IMPORTANT: value is treated as an in-bound property. The control never
// writes to it — it only emits valueModified(). Writing would destroy the
// caller's binding (that bug made the toolbar size control drift out of sync
// with the brush).
Item {
    id: root

    property int from: 0
    property int to: 100
    property int value: 0
    property int decimals: 0

    signal valueModified(int v)

    implicitHeight: 22
    implicitWidth: 84

    function display() {
        return root.decimals > 0 ? root.value.toFixed(root.decimals) : String(root.value)
    }

    Rectangle {
        anchors.fill: parent
        color: T.windowAlt
        border.color: T.border
        border.width: 1
    }

    TextInput {
        id: field
        anchors.left: parent.left
        anchors.leftMargin: 6
        anchors.right: steppers.left
        anchors.verticalCenter: parent.verticalCenter
        text: root.display()
        color: T.text
        font.pixelSize: 11
        selectByMouse: true
        onEditingFinished: {
            var n = parseFloat(text)
            if (!isNaN(n)) {
                var clamped = Math.max(root.from, Math.min(root.to, Math.round(n)))
                if (clamped !== root.value)
                    root.valueModified(clamped)
            }
            // Restore the displayed text from the (possibly unchanged) value.
            field.text = root.display()
        }
    }

    Column {
        id: steppers
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Repeater {
            model: ["stepper-up", "stepper-down"]
            delegate: Rectangle {
                width: 16; height: 11
                color: sh.hovered ? T.buttonHover : T.panelRaised
                Icon {
                    anchors.centerIn: parent
                    width: 10; height: 10
                    name: modelData
                    color: T.text
                    strokeWidth: 1
                }
                HoverHandler { id: sh }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        var next = root.value + (index === 0 ? 1 : -1)
                        next = Math.max(root.from, Math.min(root.to, next))
                        if (next !== root.value)
                            root.valueModified(next)
                    }
                }
            }
        }
    }
}
