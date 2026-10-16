import QtQuick
import QtQuick.Effects
import Prism

Item {
    id: panel
    property Item backdrop
    default property alias panelContent: content.children
    signal pointerLeft()
    clip: true
    HoverHandler { onHoveredChanged: if (!hovered) panel.pointerLeft() }

    ShaderEffectSource {
        id: capture
        width: panel.width
        height: panel.height
        sourceItem: panel.backdrop
        sourceRect: {
            if (!panel.backdrop) return Qt.rect(0, 0, panel.width, panel.height)
            const p = panel.mapToItem(panel.backdrop, 0, 0)
            return Qt.rect(p.x, p.y, panel.width, panel.height)
        }
        live: panel.visible
        visible: false
    }
    MultiEffect {
        anchors.fill: parent
        visible: panel.visible
        source: capture
        blurEnabled: true
        blurMax: 16
        blur: 1
        autoPaddingEnabled: false
    }
    Rectangle {
        anchors.fill: parent
        color: ThemeManager.themeColor("panel")
        opacity: 0.82
        border.color: ThemeManager.themeColor("border")
        radius: ThemeManager.themeColor("radiusPanel")
    }
    Item { id: content; anchors.fill: parent }
}
