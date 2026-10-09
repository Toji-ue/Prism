import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Theme.js" as T

// LAN collab: host a room (Prism prints the address to share) or join one.
// Everything here is one shared canvas on one shared layer per document - see
// CollabSession for what is and is not synchronised.
Popup {
    id: dialog

    property var controller

    modal: true
    anchors.centerIn: parent
    width: 440
    padding: 0
    focus: true
    closePolicy: Popup.CloseOnEscape
    background: Rectangle {
        color: T.window
        border.color: T.border
        border.width: 1
    }

    component Input: TextField {
        color: T.text
        font.pixelSize: 11
        selectByMouse: true
        implicitHeight: 24
        background: Rectangle {
            color: T.windowAlt
            border.color: T.border
            border.width: 1
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        Text {
            text: qsTr("Collaborate over LAN")
            color: T.text
            font.pixelSize: 13
            font.bold: true
        }

        // ---- in a session -------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: dialog.controller.collabActive

            Text {
                text: qsTr("Room (share this address, everyone on the same Wi-Fi):")
                color: T.textDim
                font.pixelSize: 11
            }
            Input {
                id: roomField
                Layout.fillWidth: true
                readOnly: true
                text: dialog.controller.collabAddress
            }
            Text {
                text: qsTr("%1 other painter(s) connected - brush and eraser strokes are shared.")
                      .arg(dialog.controller.collabPeers)
                color: T.text
                font.pixelSize: 11
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Undo and the Blend tool stay local while you are in a room: a shared layer cannot un-paint someone else's stroke.")
                color: T.textDim
                font.pixelSize: 10
            }
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                Button {
                    objectName: "collabLeaveButton"
                    text: qsTr("Leave room")
                    implicitHeight: 24
                    implicitWidth: 96
                    font.pixelSize: 11
                    onClicked: dialog.controller.leaveCollab()
                }
            }
        }

        // ---- not in a session ---------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            visible: !dialog.controller.collabActive

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Text { text: qsTr("Your name"); color: T.text; font.pixelSize: 11 }
                Input {
                    id: hostNameField
                    Layout.fillWidth: true
                    text: qsTr("Host")
                }
                Button {
                    objectName: "collabHostButton"
                    text: qsTr("Create room")
                    implicitHeight: 24
                    implicitWidth: 96
                    font.pixelSize: 11
                    onClicked: dialog.controller.hostCollab(hostNameField.text)
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: T.border }

            Text {
                text: qsTr("Join a room")
                color: T.text
                font.pixelSize: 11
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Input {
                    id: addressField
                    objectName: "collabAddressField"
                    Layout.fillWidth: true
                    placeholderText: qsTr("192.168.1.5:48291")
                }
                Input {
                    id: guestNameField
                    Layout.preferredWidth: 90
                    text: qsTr("Guest")
                }
                Button {
                    objectName: "collabJoinButton"
                    text: qsTr("Join")
                    implicitHeight: 24
                    implicitWidth: 70
                    font.pixelSize: 11
                    enabled: addressField.text.length > 0
                    onClicked: dialog.controller.joinCollab(addressField.text, guestNameField.text)
                }
            }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Joining replaces the drawing open here with the host's. Same Wi-Fi only - no internet relay.")
                color: T.textDim
                font.pixelSize: 10
            }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            visible: dialog.controller.lastError.length > 0
            text: dialog.controller.lastError
            color: T.highlight
            font.pixelSize: 10
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Close")
                implicitHeight: 24
                implicitWidth: 70
                font.pixelSize: 11
                onClicked: dialog.close()
            }
        }
    }
}
