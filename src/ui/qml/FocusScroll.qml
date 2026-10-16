import QtQuick

// Keep keyboard focus visible without changing a canvas viewport.
QtObject {
    id: helper
    required property Flickable viewport
    function revealFocus() {
        const window = viewport.Window.window;
        const item = window ? window.activeFocusItem : null;
        if (!item || !item.visible)
            return;
        let ancestor = item.parent;
        while (ancestor && ancestor !== viewport)
            ancestor = ancestor.parent;
        if (!ancestor)
            return;
        // Reveal nested list content first, then its containing page.
        for (let view = item.parent; view; view = view.parent) {
            if (view instanceof Flickable) {
                const top = item.mapToItem(view.contentItem, 0, 0).y;
                const bottom = item.mapToItem(view.contentItem, 0, item.height).y;
                let next = view.contentY;
                if (top < next)
                    next = top;
                else if (bottom > next + view.height)
                    next = bottom - view.height;
                view.contentY = Math.max(view.originY, Math.min(view.originY + Math.max(0, view.contentHeight - view.height), next));
            }
            if (view === viewport)
                break;
        }
    }
    property Connections watcher: Connections {
        target: helper.viewport.Window.window
        function onActiveFocusItemChanged() {
            Qt.callLater(helper.revealFocus);
        }
    }
}
