import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as T

// Dark dropdown menu. Controls Basic draws menu popups with a light frame and
// unthemed item text, which broke the two-colour look (this is separate from
// ComboBox popups, which PComboBox styles). Every Menu in Prism uses this.
Menu {
    id: control

    implicitWidth: 200

    delegate: MenuItem {
        id: item

        // Separators arrive as MenuItems with empty text: render them as a gap
        // plus a hairline instead of a (wrongly styled) label.
        readonly property bool isSeparator: item.text.length === 0

        implicitHeight: isSeparator ? 7 : 24
        implicitWidth: control.width

        background: Rectangle {
            color: !isSeparator && item.highlighted && item.enabled ? T.highlight
                                                                    : "transparent"
            Rectangle {
                visible: item.isSeparator
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 12
                height: 1
                color: T.popupBorder
            }
        }

        contentItem: Text {
            visible: !item.isSeparator
            leftPadding: 10
            rightPadding: 10
            text: item.text
            font.pixelSize: 11
            color: !item.enabled ? T.disabledText
                                 : (item.highlighted ? T.highlightText : T.text)
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        implicitWidth: control.implicitWidth
        color: T.popup
        border.color: T.popupBorder
        border.width: 1
    }
}
