import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

Rectangle {
    id: docker
    required property AppController controller
    readonly property color current: controller.brushColor
    color: ThemeManager.themeColor("panel")
    radius: ThemeManager.themeColor("radiusPanel")
    border.width: activeFocus ? 2 : 0
    border.color: ThemeManager.themeColor("highlight")
    function hsv(h, s, v) {
        controller.setBrushHsv(h, s, v);
    }
    Flickable {
        id: scroller
        objectName: "colourScroll"
        FocusScroll {
            viewport: scroller
        }
        anchors.fill: parent
        anchors.margins: 10
        contentWidth: width
        contentHeight: content.height
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.vertical: ScrollBar {}
        Column {
            id: content
            width: scroller.width - 8
            spacing: 12
            Text {
                text: qsTr("Colour")
                color: ThemeManager.themeColor("text")
                font.pixelSize: 15
                font.weight: Font.DemiBold
            }
            Item {
                id: wheelBox
                width: Math.min(216, parent.width)
                height: width
                anchors.horizontalCenter: parent.horizontalCenter
                Canvas {
                    id: wheel
                    anchors.fill: parent
                    onPaint: {
                        let ctx = getContext("2d");
                        ctx.reset();
                        ctx.clearRect(0, 0, width, height);
                        let r = width / 2 - 3, inner = r - 19;
                        for (let i = 0; i < 180; ++i) {
                            let a = i * Math.PI * 2 / 180, b = (i + 1.2) * Math.PI * 2 / 180;
                            ctx.beginPath();
                            ctx.arc(width / 2, height / 2, r, a, b);
                            ctx.arc(width / 2, height / 2, inner, b, a, true);
                            ctx.closePath();
                            ctx.fillStyle = Qt.hsva(i / 180, 1, 1, 1);
                            ctx.fill();
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    function updateHue(mx, my) {
                        const dx = mx - width / 2, dy = my - height / 2;
                        let h = (Math.atan2(dy, dx) / (Math.PI * 2) + 1) % 1;
                        docker.hsv(h, docker.current.hsvSaturation, docker.current.hsvValue);
                    }
                    onPressed: mouse => {
                        const r = Math.hypot(mouse.x - width / 2, mouse.y - height / 2);
                        if (r < width / 2 - 24 || r > width / 2) {
                            mouse.accepted = false;
                            return;
                        }
                        updateHue(mouse.x, mouse.y);
                    }
                    onPositionChanged: mouse => {
                        if (pressed)
                            updateHue(mouse.x, mouse.y);
                    }
                }
                Rectangle {
                    width: 12
                    height: 12
                    radius: 6
                    color: "transparent"
                    border.width: 2
                    border.color: "white"
                    x: wheelBox.width / 2 + Math.cos(docker.controller.brushHue * Math.PI * 2) * (wheelBox.width / 2 - 12) - 6
                    y: wheelBox.height / 2 + Math.sin(docker.controller.brushHue * Math.PI * 2) * (wheelBox.width / 2 - 12) - 6
                }
                Rectangle {
                    id: sv
                    objectName: "colourSV"
                    width: wheelBox.width * 0.55
                    height: width
                    anchors.centerIn: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop {
                            position: 0
                            color: "white"
                        }
                        GradientStop {
                            position: 1
                            color: Qt.hsva(docker.controller.brushHue, 1, 1, 1)
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        gradient: Gradient {
                            GradientStop {
                                position: 0
                                color: "transparent"
                            }
                            GradientStop {
                                position: 1
                                color: "black"
                            }
                        }
                    }
                    Rectangle {
                        width: 10
                        height: 10
                        radius: 5
                        color: "transparent"
                        border.width: 2
                        border.color: docker.current.hsvValue > 0.6 ? "black" : "white"
                        x: docker.current.hsvSaturation * sv.width - 5
                        y: (1 - docker.current.hsvValue) * sv.height - 5
                    }
                    MouseArea {
                        anchors.fill: parent
                        function updateSV(mx, my) {
                            docker.hsv(docker.controller.brushHue, Math.max(0, Math.min(1, mx / width)), Math.max(0, Math.min(1, 1 - my / height)));
                        }
                        onPressed: mouse => updateSV(mouse.x, mouse.y)
                        onPositionChanged: mouse => {
                            if (pressed)
                                updateSV(mouse.x, mouse.y);
                        }
                    }
                }
            }
            Repeater {
                model: [qsTr("Hue"), qsTr("Saturation"), qsTr("Value")]
                Column {
                    id: channel
                    required property int index
                    required property string modelData
                    width: content.width
                    spacing: 3
                    readonly property real number: index === 0 ? docker.controller.brushHue * 360 : (index === 1 ? docker.current.hsvSaturation : docker.current.hsvValue) * 100
                    readonly property real limit: index === 0 ? 360 : 100
                    function apply(n) {
                        docker.hsv(index === 0 ? n / 360 : docker.controller.brushHue, index === 1 ? n / 100 : docker.current.hsvSaturation, index === 2 ? n / 100 : docker.current.hsvValue);
                    }
                    Text {
                        text: channel.modelData
                        color: ThemeManager.themeColor("textDim")
                        font.pixelSize: 12
                    }
                    RowLayout {
                        width: parent.width
                        spacing: 8
                        PSlider {
                            id: slider
                            objectName: "colorSlider_" + ["H", "S", "V"][channel.index]
                            Layout.fillWidth: true
                            from: 0
                            to: channel.limit
                            Accessible.name: channel.modelData
                            Component.onCompleted: value = channel.number
                            Connections {
                                target: docker.controller
                                function onBrushChanged() {
                                    if (!slider.pressed)
                                        slider.value = channel.number;
                                }
                            }
                            onMoved: channel.apply(value)
                        }
                        NumericField {
                            objectName: "colourField_" + ["H", "S", "V"][channel.index]
                            Layout.preferredWidth: 64
                            from: 0
                            to: channel.limit
                            numberValue: channel.number
                            Accessible.name: channel.modelData
                            onCommitted: value => channel.apply(value)
                        }
                    }
                }
            }
            RowLayout {
                width: parent.width
                spacing: 8
                Text {
                    text: qsTr("Hex")
                    color: ThemeManager.themeColor("textDim")
                    font.pixelSize: 12
                }
                PTextField {
                    id: hexField
                    objectName: "colourHexField"
                    Layout.fillWidth: true
                    Accessible.name: qsTr("Hex colour")
                    function restore() {
                        text = docker.current.toString().toUpperCase();
                        invalid = false;
                    }
                    function commit() {
                        let t = text.trim();
                        if (/^[0-9a-fA-F]{6}$/.test(t))
                            t = "#" + t;
                        if (!/^#[0-9a-fA-F]{6}$/.test(t)) {
                            invalid = true;
                            return;
                        }
                        const c = Qt.color(t);
                        docker.controller.brushColor = Qt.rgba(c.r, c.g, c.b, docker.current.a);
                        restore();
                    }
                    Component.onCompleted: restore()
                    Connections {
                        target: docker.controller
                        function onBrushChanged() {
                            if (!hexField.activeFocus)
                                hexField.restore();
                        }
                    }
                    onEditingFinished: commit()
                    onTextEdited: invalid = false
                    Keys.onEscapePressed: event => {
                        restore();
                        event.accepted = true;
                    }
                }
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    color: docker.current
                    radius: ThemeManager.themeColor("radiusChip")
                    border.color: ThemeManager.themeColor("border")
                }
            }
            Text {
                text: qsTr("Palette")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 12
            }
            Flow {
                width: parent.width
                spacing: 6
                Repeater {
                    model: ["#000000", "#ffffff", "#bfbfbf", "#d64550", "#e67e22", "#f1c40f", "#2ecc71", "#3498db", "#8e44ad", "#e84393", "#00b894", "#636e72"]
                    PButton {
                        required property string modelData
                        width: 32
                        height: 32
                        Accessible.name: qsTr("Use colour %1").arg(modelData)
                        onClicked: docker.controller.brushColor = modelData
                        background: Rectangle {
                            color: modelData
                            radius: ThemeManager.themeColor("radiusChip")
                            border.width: parent.visualFocus ? 2 : 1
                            border.color: parent.visualFocus ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
                        }
                    }
                }
            }
        }
    }
}
