import QtQuick
import QtQuick.Controls.Basic
import Prism

// A toolbar icon button.
//
// Deliberately written out instead of coming from a Repeater: a Canvas icon
// created by a Repeater inside the window's ToolBar never gets its texture, so
// those buttons laid out in the right place, hit-tested correctly, reported a
// sane size - and drew nothing at all. One button per line is the boring fix
// that works; the toolbox and layer docker Repeaters are unaffected.
Rectangle {
    id: button

    property string iconName: ""
    property real iconSize: 15
    property string tip: ""
    // Drawn in the highlight colour (the record button while recording).
    property bool active: false
    // Greyed out when false, and unclickable.
    property bool usable: true
    readonly property bool keyboardButton: true

    signal clicked

    width: 32
    height: 32
    objectName: "toolbarButton_" + iconName
    activeFocusOnTab: usable
    Accessible.role: Accessible.Button
    Accessible.name: tip.length > 0 ? tip : iconName
    Accessible.onPressAction: if (button.usable)
        button.clicked()
    color: hover.hovered && usable ? ThemeManager.themeColor("buttonHover") : "transparent"
    radius: ThemeManager.themeColor("radiusControl")
    border.color: activeFocus ? ThemeManager.themeColor("highlight") : "transparent"
    border.width: activeFocus ? 1 : 0
    opacity: usable ? 1.0 : 0.4

    Keys.onPressed: event => {
        if (button.usable && (event.key === Qt.Key_Space || event.key === Qt.Key_Return || event.key === Qt.Key_Enter)) {
            button.clicked();
            event.accepted = true;
        }
    }

    Icon {
        anchors.centerIn: parent
        width: button.iconSize
        height: button.iconSize
        name: button.iconName
        color: button.active ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("text")
    }

    HoverHandler {
        id: hover
    }

    MouseArea {
        anchors.fill: parent
        enabled: button.usable
        onClicked: button.clicked()
    }

    ToolTip.visible: hover.hovered
    ToolTip.delay: 500
    ToolTip.text: button.tip
}
