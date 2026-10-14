import QtQuick
import QtQuick.Controls.Basic

import Prism

// Shared themed menu for both studio palettes.
Menu {
    id: control

    implicitWidth: 200

    delegate: MenuItem {
        id: item
        objectName: item.action ? item.action.objectName : ""

        // Separators arrive as MenuItems with empty text: render them as a gap
        // plus a hairline instead of a (wrongly styled) label.
        readonly property bool isSeparator: item.text.length === 0

        implicitHeight: isSeparator ? 8 : ThemeManager.themeColor("controlHeight")
        implicitWidth: control.width

        background: Rectangle {
            color: !isSeparator && item.highlighted && item.enabled ? ThemeManager.themeColor("selection")
                                                                    : "transparent"
            radius: ThemeManager.themeColor("radiusSmall")
            Rectangle {
                visible: item.isSeparator
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 12
                height: 1
                color: ThemeManager.themeColor("popupBorder")
            }
        }

        contentItem: Text {
            visible: !item.isSeparator
            leftPadding: 10
            rightPadding: 10
            // Qt texts use '&' to mark a keyboard mnemonic (e.g. "&New"). QML
            // Text renders it literally, so strip it: the marker must never
            // show up in the menu, and the toolbar/menu labels stay clean.
            text: item.text.replace(/&/g, "")
            font.pixelSize: ThemeManager.themeColor("fontBody")
            color: !item.enabled ? ThemeManager.themeColor("disabledText")
                                 : (item.highlighted ? ThemeManager.themeColor("selectionText") : ThemeManager.themeColor("text"))
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    background: Rectangle {
        implicitWidth: control.implicitWidth
        color: ThemeManager.themeColor("popup")
        border.color: ThemeManager.themeColor("popupBorder")
        border.width: 1
        radius: ThemeManager.themeColor("radiusPanel")
    }
}

