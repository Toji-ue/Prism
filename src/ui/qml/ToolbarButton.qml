import QtQuick
import QtQuick.Controls.Basic
import "Theme.js" as T

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
    property string tip: ""
    // Drawn in the highlight colour (the record button while recording).
    property bool active: false
    // Greyed out when false, and unclickable.
    property bool usable: true

    signal clicked()

    width: 24
    height: 24
    objectName: "toolbarButton_" + iconName
    color: hover.hovered && usable ? T.buttonHover : "transparent"
    opacity: usable ? 1.0 : 0.4

    Icon {
        anchors.centerIn: parent
        width: 15
        height: 15
        name: button.iconName
        color: button.active ? T.highlight : T.text
    }

    HoverHandler { id: hover }

    MouseArea {
        anchors.fill: parent
        enabled: button.usable
        onClicked: button.clicked()
    }

    ToolTip.visible: hover.hovered
    ToolTip.delay: 500
    ToolTip.text: button.tip
}
