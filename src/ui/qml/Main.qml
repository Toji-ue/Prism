import QtQuick
import QtQuick.Layouts

Window {
    id: root
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    title: qsTr("Prism — untitled")

    color: "#1e1e22"

    // Placeholder shell: dark canvas viewport + layer-panel stub.
    // Real canvas arrives with the canvas module (Qt RHI).
    Rectangle {
        anchors.fill: parent
        anchors.margins: 12
        color: "#26262b"
        radius: 6

        Text {
            anchors.centerIn: parent
            text: qsTr("Prism 0.1 — canvas module not yet wired")
            color: "#9a9aa5"
            font.pixelSize: 18
        }
    }
}
