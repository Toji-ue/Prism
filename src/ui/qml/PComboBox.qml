import QtQuick
import QtQuick.Controls.Basic
import Prism

// ComboBox with the app's dark dropdown. Controls Basic's default popup is a
// light list on a light frame, which broke the two-colour look, so every
// dropdown in Prism uses this instead: dark grey list, dark grey frame, and
// clearly dimmed entries when an option is unavailable.
ComboBox {
    id: control

    implicitHeight: 22
    font.pixelSize: 11

    delegate: ItemDelegate {
        required property var model
        required property int index
        width: control.width
        height: 22
        highlighted: control.highlightedIndex === index
        contentItem: Text {
            text: control.textRole ? model[control.textRole] : model[control.textRole]
            color: highlighted ? ThemeManager.themeColor("highlightText") : ThemeManager.themeColor("text")
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            leftPadding: 6
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: ThemeManager.themeColor("radiusSmall")
            color: highlighted ? ThemeManager.themeColor("highlight") : "transparent"
        }
    }

    indicator: Icon {
        x: control.width - width - 4
        y: (control.height - height) / 2
        width: 12
        height: 12
        name: "chevron-down"
        color: control.enabled ? ThemeManager.themeColor("text") : ThemeManager.themeColor("disabledText")
    }

    contentItem: Text {
        leftPadding: 6
        rightPadding: control.indicator.width + 4
        text: control.displayText
        font.pixelSize: 11
        color: control.enabled ? ThemeManager.themeColor("text") : ThemeManager.themeColor("disabledText")
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        color: ThemeManager.themeColor("panelRaised")
        border.color: ThemeManager.themeColor("border")
        border.width: 1
        radius: ThemeManager.themeColor("radiusSmall")
    }

    popup: Popup {
        y: control.height - 1
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 2, 240)
        padding: 1

        background: Rectangle {
            color: ThemeManager.themeColor("popup")
            border.color: ThemeManager.themeColor("popupBorder")
            border.width: 1
            radius: ThemeManager.themeColor("radiusSmall")
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
    }
}

