import QtQuick
import QtQuick.Controls.Basic
import Prism.Ui 1.0
import Prism

// Krita-style "Advanced Color Selector": HSV wheel + value strip + HSV
// sliders + hex entry. All reads/writes go through AppController.brushColor.
Rectangle {
    id: docker
    required property AppController controller

    color: ThemeManager.themeColor("panel")
    radius: ThemeManager.themeColor("radius")
    border.width: activeFocus ? 1 : 0
    border.color: ThemeManager.themeColor("highlight")

    readonly property color current: controller.brushColor
    /// 0 = Advanced Color Selector, 1 = Tool Options (Krita's docker tabs).
    property int tab: 0

    // ---- docker header with tab look ----
    Rectangle {
        id: header
        width: parent.width
        height: ThemeManager.themeColor("dockerHeaderHeight")
        color: "transparent"

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Repeater {
                model: [qsTr("Advanced Color Selector"), qsTr("Tool Options")]
                delegate: Text {
                    text: modelData
                    color: docker.tab === index ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("textDim")
                    font.pixelSize: 11
                    anchors.verticalCenter: parent.verticalCenter
                    bottomPadding: 2
                    MouseArea {
                        anchors.fill: parent
                        onClicked: docker.tab = index
                    }
                }
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: ThemeManager.themeColor("border") }
    }

    // Content scrolls: the docker is height-constrained by the docker
    // column, and every control must stay reachable on short screens.
    Flickable {
        id: scroller
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: width
        contentHeight: content.height + 16
        clip: true
        visible: docker.tab === 0
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 8
            spacing: 8

        // ---- HSV wheel ----
        Item {
            id: wheelBox
            width: 160; height: 160
            anchors.horizontalCenter: parent.horizontalCenter

            Canvas {
                id: wheel
                anchors.fill: parent
                renderStrategy: Canvas.Immediate

                onPaint: {
                    var ctx = getContext("2d")
                    var w = width, h = height
                    ctx.reset()
                    ctx.clearRect(0, 0, w, h)

                    var cx = w / 2, cy = h / 2
                    var r = Math.min(cx, cy) - 1
                    var v = docker.current.hsvValue

                    // Hue ring: 360 thin wedges, brightness = value.
                    var steps = 90
                    for (var i = 0; i < steps; ++i) {
                        var a0 = (i / steps) * Math.PI * 2
                        var a1 = ((i + 1) / steps) * Math.PI * 2
                        ctx.beginPath()
                        ctx.moveTo(cx, cy)
                        ctx.arc(cx, cy, r, a0, a1)
                        ctx.closePath()
                        ctx.fillStyle = Qt.hsva(i / steps, 1, Math.max(v, 0.05), 1)
                        ctx.fill()
                    }

                    // Saturation: white in the middle fading out to the rim.
                    var g = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
                    g.addColorStop(0, "rgba(255,255,255,1)")
                    g.addColorStop(1, "rgba(255,255,255,0)")
                    ctx.globalCompositeOperation = "source-over"
                    ctx.beginPath()
                    ctx.arc(cx, cy, r, 0, Math.PI * 2)
                    ctx.closePath()
                    ctx.fillStyle = g
                    ctx.fill()
                }

                // repaint whenever the value (brightness) changes
                Connections {
                    target: docker.controller
                    function onBrushChanged() { wheel.requestPaint() }
                }
            }

            // hue/saturation cursor
            Rectangle {
                id: cursor
                width: 10; height: 10
                radius: 5
                color: "transparent"
                border.color: docker.current.hsvValue > 0.55 ? "#000000" : "#ffffff"
                border.width: 1.5
                visible: docker.current.hsvSaturation > 0.001
                x: wheelBox.width / 2 + Math.cos(docker.current.hsvHue * Math.PI * 2) * (r_) - width / 2
                y: wheelBox.height / 2 + Math.sin(docker.current.hsvHue * Math.PI * 2) * (r_) - height / 2
                readonly property real r_: (Math.min(wheelBox.width, wheelBox.height) / 2 - 1)
                                 * docker.current.hsvSaturation
            }

            MouseArea {
                anchors.fill: parent
                onPositionChanged: mouse => pick(mouse)
                onPressed: mouse => pick(mouse)
                function pick(mouse) {
                    var cx = wheelBox.width / 2, cy = wheelBox.height / 2
                    var dx = mouse.x - cx, dy = mouse.y - cy
                    var r = Math.min(cx, cy) - 1
                    var dist = Math.min(Math.sqrt(dx * dx + dy * dy), r)
                    var hue = (Math.atan2(dy, dx) / (Math.PI * 2) + 1) % 1
                    var sat = dist / r
                    var val = docker.current.hsvValue
                    docker.controller.brushColor = Qt.hsva(hue, sat, val, 1)
                }
            }
        }

        // ---- value strip ----
        Item {
            width: 160; height: 16
            anchors.horizontalCenter: parent.horizontalCenter

            Canvas {
                id: valueStrip
                anchors.fill: parent
                renderStrategy: Canvas.Immediate
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.reset()
                    var g = ctx.createLinearGradient(0, 0, width, 0)
                    g.addColorStop(0, "rgba(0,0,0,1)")
                    g.addColorStop(1, Qt.hsva(docker.current.hsvHue,
                                              docker.current.hsvSaturation, 1, 1))
                    ctx.fillStyle = g
                    ctx.fillRect(0, 0, width, height)
                }
                Connections {
                    target: docker.controller
                    function onBrushChanged() { valueStrip.requestPaint() }
                }
            }

            Rectangle {
                width: 2; height: parent.height
                color: docker.current.hsvValue > 0.55 ? "black" : "white"
                x: docker.current.hsvValue * (parent.width - 2)
            }

            MouseArea {
                anchors.fill: parent
                onPositionChanged: mouse => setV(mouse)
                onPressed: mouse => setV(mouse)
                function setV(mouse) {
                    var v = Math.max(0, Math.min(1, mouse.x / width))
                    docker.controller.brushColor = Qt.hsva(docker.current.hsvHue,
                                                           docker.current.hsvSaturation, v, 1)
                }
            }
        }

        // ---- H / S / V sliders ----
        Repeater {
            model: [
                { label: "H", max: 360, val: function(c) { return c.hsvHue * 360 } },
                { label: "S", max: 100, val: function(c) { return c.hsvSaturation * 100 } },
                { label: "V", max: 100, val: function(c) { return c.hsvValue * 100 } }
            ]
            delegate: Row {
                spacing: 6
                Text {
                    text: modelData.label
                    color: ThemeManager.themeColor("textDim")
                    font.pixelSize: 11
                    width: 12
                    anchors.verticalCenter: parent.verticalCenter
                }
                PSlider {
                    id: sl
                    objectName: "colorSlider_" + modelData.label
                    width: 118
                    from: 0; to: modelData.max
                    // Synced imperatively, not bound: any write to `value` (a
                    // drag, the wheel, the arrow keys) destroys the binding, so
                    // after the first touch the slider showed a stale number
                    // while the colour moved on.
                    Component.onCompleted: syncFromColor()
                    Connections {
                        target: docker.controller
                        function onBrushChanged() {
                            if (!sl.pressed)
                                sl.syncFromColor()
                        }
                    }
                    function syncFromColor() {
                        value = modelData.val(docker.current)
                    }
                    onMoved: {
                        var c = docker.current
                        var h = c.hsvHue, s = c.hsvSaturation, v = c.hsvValue
                        if (modelData.label === "H") h = value / 360
                        else if (modelData.label === "S") s = value / 100
                        else v = value / 100
                        docker.controller.brushColor = Qt.hsva(h, s, v, 1)
                    }
                }
                TextField {
                    width: 44
                    text: Math.round(modelData.val(docker.current))
                    font.pixelSize: 11
                    onEditingFinished: {
                        var n = parseFloat(text)
                        if (isNaN(n)) return
                        var c = docker.current
                        var h = c.hsvHue, s = c.hsvSaturation, v = c.hsvValue
                        if (modelData.label === "H") h = Math.max(0, Math.min(360, n)) / 360
                        else if (modelData.label === "S") s = Math.max(0, Math.min(100, n)) / 100
                        else v = Math.max(0, Math.min(100, n)) / 100
                        docker.controller.brushColor = Qt.hsva(h, s, v, 1)
                    }
                }
            }
        }

        // ---- hex ----
        Row {
            spacing: 6
            Text {
                text: qsTr("Hex")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 11
                width: 28
                anchors.verticalCenter: parent.verticalCenter
            }
            TextField {
                width: 96
                font.pixelSize: 11
                text: docker.current.toString().toUpperCase()
                onEditingFinished: {
                    var t = text.trim()
                    if (t.length === 6) t = "#" + t
                    if (/^#[0-9a-fA-F]{6}$/.test(t))
                        docker.controller.brushColor = t
                }
            }
            Rectangle {
                width: 46; height: 22
                color: docker.current
                border.color: ThemeManager.themeColor("border")
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // ---- preset swatches ----
        Flow {
            width: parent.width
            spacing: 3
            Repeater {
                model: ["#000000", "#ffffff", "#7f7f7f", "#da4453", "#e67e22",
                        "#f6c02b", "#2ecc71", "#3daee9", "#3b5998", "#9b59b6",
                        "#8e44ad", "#e84393", "#00b894", "#636e72", "#b2bec3"]
                delegate: Rectangle {
                    width: 20; height: 20
                    color: modelData
                    border.color: ThemeManager.themeColor("border")
                    MouseArea {
                        anchors.fill: parent
                        onClicked: docker.controller.brushColor = modelData
                    }
                }
            }
        }
        }
    }

    // ---- Tool Options tab (Krita keeps brush settings in this docker) ----
    Flickable {
        id: toolOptions
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        contentWidth: width
        contentHeight: toolColumn.height + 16
        clip: true
        visible: docker.tab === 1
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: toolColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 8
            spacing: 10

            Text {
                text: qsTr("BRUSH")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 10
                font.bold: true
            }

            // Size
            Column {
                width: parent.width
                spacing: 2
                Row {
                    width: parent.width
                    Text { text: qsTr("Size"); color: ThemeManager.themeColor("text"); font.pixelSize: 11 }
                    Item { width: parent.width - 80; height: 1 }
                    Text {
                        text: Math.round(controller.brushSize) + " px"
                        color: ThemeManager.themeColor("textDim")
                        font.pixelSize: 11
                    }
                }
                PSlider {
                    width: parent.width
                    from: 1; to: 200
                    value: controller.brushSize
                    onMoved: controller.brushSize = Math.round(value)
                }
            }

            // Opacity (flow)
            Column {
                width: parent.width
                spacing: 2
                Row {
                    width: parent.width
                    Text { text: qsTr("Opacity"); color: ThemeManager.themeColor("text"); font.pixelSize: 11 }
                    Item { width: parent.width - 80; height: 1 }
                    Text {
                        text: Math.round(controller.brushFlow * 100) + "%"
                        color: ThemeManager.themeColor("textDim")
                        font.pixelSize: 11
                    }
                }
                PSlider {
                    width: parent.width
                    from: 0.05; to: 1
                    value: controller.brushFlow
                    onMoved: controller.brushFlow = value
                }
            }

            // Hardness
            Column {
                width: parent.width
                spacing: 2
                Row {
                    width: parent.width
                    Text { text: qsTr("Hardness"); color: ThemeManager.themeColor("text"); font.pixelSize: 11 }
                    Item { width: parent.width - 80; height: 1 }
                    Text {
                        text: Math.round(controller.brushHardness * 100) + "%"
                        color: ThemeManager.themeColor("textDim")
                        font.pixelSize: 11
                    }
                }
                PSlider {
                    width: parent.width
                    from: 0; to: 1
                    value: controller.brushHardness
                    onMoved: controller.brushHardness = value
                }
            }

            // Eraser toggle
            Rectangle {
                width: parent.width
                height: 26
                color: controller.eraser ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("panelRaised")
                border.color: ThemeManager.themeColor("border")
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Icon {
                        width: 14; height: 14
                        name: "eraser"
                        color: controller.eraser ? ThemeManager.themeColor("highlightText") : ThemeManager.themeColor("text")
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        text: qsTr("Eraser mode")
                        color: controller.eraser ? ThemeManager.themeColor("highlightText") : ThemeManager.themeColor("text")
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: controller.eraser = !controller.eraser
                }
            }

            // Current preset summary
            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                text: qsTr("Preset: %1").arg(controller.brushName)
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 10
            }
        }
    }
}
