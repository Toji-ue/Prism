import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as T

// Compact Krita-style spin box: bordered field with two small stepper
// buttons. Controls Basic's SpinBox draws oversized +/− buttons that break
// the dense toolbar layout, so the app uses this instead.
Item {
    id: root

    property int from: 0
    property int to: 100
    property int value: 0
    property string suffix: ""
    property int decimals: 0

    signal valueModified(int v)

    implicitHeight: 22
    implicitWidth: 84

    function commit(v) {
        var clamped = Math.max(root.from, Math.min(root.to, v))
        if (clamped !== root.value) {
            root.value = clamped
            root.valueModified(clamped)
        } else {
            field.text = root.display()
        }
    }

    function display() {
        return root.decimals > 0
            ? root.value.toFixed(root.decimals)
            : String(root.value)
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
            if (!isNaN(n)) root.commit(Math.round(n))
            else field.text = root.display()
        }
    }

    Column {
        id: steppers
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.verticalCenter: parent.verticalCenter
        spacing: 0

        Repeater {
            model: ["▲", "▼"]
            delegate: Rectangle {
                width: 16; height: 10
                color: sh.hovered ? T.buttonHover : T.panelRaised
                Text {
                    anchors.centerIn: parent
                    text: modelData
                    color: T.text
                    font.pixelSize: 6
                }
                HoverHandler { id: sh }
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.commit(root.value + (index === 0 ? 1 : -1))
                }
            }
        }
    }
}
