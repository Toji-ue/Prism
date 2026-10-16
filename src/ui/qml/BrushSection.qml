import QtQuick
import QtQuick.Layouts
import Prism

Column {
    id: section
    property string title: ""
    property bool expanded: false
    default property alias controls: body.data
    spacing: 6
    PButton {
        objectName: section.objectName.length ? section.objectName + "Toggle" : ""
        width: parent.width
        text: section.title
        checked: section.expanded
        onClicked: section.expanded = !section.expanded
        Accessible.description: expanded ? qsTr("Collapse section") : qsTr("Expand section")
        Icon {
            anchors.right: parent.right
            anchors.rightMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 14
            name: section.expanded ? "chevron-up" : "chevron-down"
        }
    }
    Column {
        id: body
        width: parent.width
        spacing: 8
        visible: section.expanded
    }
}
