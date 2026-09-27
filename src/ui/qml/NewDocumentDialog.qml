import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import "Theme.js" as T

// Krita-style "Create new document" dialog: preset categories on the left,
// Dimensions / Content tabs, Image Size and Color groups, a live summary, and
// Create / Cancel. Icons are vector (Icon.qml) — no emoji.
Dialog {
    id: dialog
    required property AppController controller

    title: qsTr("Create new document - Prism")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 700
    height: 660
    padding: 0

    readonly property var categories: [
        { icon: "file-new",     name: qsTr("Custom Document") },
        { icon: "duplicate",    name: qsTr("Create from Clipboard") },
        { icon: "move",         name: qsTr("Animation Templates") },
        { icon: "brush",        name: qsTr("Comic Templates") },
        { icon: "select-rect",  name: qsTr("Design Templates") },
        { icon: "transparency", name: qsTr("DSLR Templates") },
        { icon: "swatch",       name: qsTr("Texture Templates") }
    ]

    // Image size presets: [width, height, dpi]
    readonly property var sizePresets: [
        [0, 0, 72],
        [2480, 3508, 300],   // A4 @300
        [842, 595, 72],      // A4 @72
        [1920, 1080, 72],    // HD
        [2480, 3508, 300],   // Full page
        [2048, 2048, 72],    // Square
        [1080, 1080, 72],    // Social square
        [2560, 1440, 72]     // QHD
    ]

    background: Rectangle {
        color: T.window
        border.color: T.border
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ---- left: preset categories ----
            Rectangle {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: T.window

                ListView {
                    id: catList
                    anchors.fill: parent
                    anchors.margins: 4
                    currentIndex: 0
                    model: dialog.categories

                    delegate: Rectangle {
                        width: catList.width
                        height: 36
                        color: catList.currentIndex === index ? T.highlight : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            spacing: 8
                            Icon {
                                Layout.preferredWidth: 18
                                Layout.preferredHeight: 18
                                name: modelData.icon
                                color: catList.currentIndex === index
                                       ? T.highlightText : T.text
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                elide: Text.ElideRight
                                font.pixelSize: 12
                                color: catList.currentIndex === index
                                       ? T.highlightText : T.text
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: catList.currentIndex = index
                        }
                    }
                }
            }

            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: T.border }

            // ---- right: settings ----
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: T.windowAlt

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    // tabs
                    RowLayout {
                        spacing: 2
                        Repeater {
                            model: [qsTr("Dimensions"), qsTr("Content")]
                            delegate: Rectangle {
                                width: 100; height: 26
                                color: tabIndex === index ? T.windowAlt : T.panel
                                border.color: T.border
                                border.width: 1
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: 12
                                    color: tabIndex === index ? T.text : T.textDim
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: tabIndex = index
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // ================= DIMENSIONS TAB =================
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        visible: tabIndex === 0

                        // ---- Image Size ----
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: sizeGrid.implicitHeight + 34
                            color: "transparent"
                            border.color: T.border
                            border.width: 1

                            Text {
                                x: 10; y: -8
                                text: qsTr("Image Size")
                                color: T.text
                                font.pixelSize: 11
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -2
                                    color: T.windowAlt
                                    z: -1
                                }
                            }

                            GridLayout {
                                id: sizeGrid
                                anchors.fill: parent
                                anchors.margins: 12
                                columns: 4
                                columnSpacing: 8
                                rowSpacing: 8

                                Text { text: qsTr("Predefined:"); color: T.text; font.pixelSize: 11 }
                                ComboBox {
                                    Layout.columnSpan: 2
                                    Layout.fillWidth: true
                                    height: 24
                                    font.pixelSize: 11
                                    model: dialog.presetModel
                                    onActivated: dialog.applyPreset(currentIndex)
                                }
                                RowLayout {
                                    spacing: 2
                                    Repeater {
                                        model: [qsTr("Portrait"), qsTr("Landscape")]
                                        delegate: Rectangle {
                                            width: 24; height: 24
                                            color: index === dialog.orientationMode
                                                   ? T.highlight : T.panel
                                            border.color: T.border
                                            Icon {
                                                anchors.centerIn: parent
                                                width: 14; height: 14
                                                name: "select-rect"
                                                color: index === dialog.orientationMode
                                                       ? T.highlightText : T.text
                                                rotation: index === 0 ? 0 : 90
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                onClicked: {
                                                    dialog.orientationMode = index
                                                    const portrait = index === 0
                                                    if (portrait && wField.value > hField.value) {
                                                        const t = wField.value
                                                        wField.value = hField.value
                                                        hField.value = t
                                                    } else if (!portrait && wField.value < hField.value) {
                                                        const t2 = wField.value
                                                        wField.value = hField.value
                                                        hField.value = t2
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                Text { text: qsTr("Width:"); color: T.text; font.pixelSize: 11 }
                                PSpinBox {
                                    id: wField
                                    Layout.fillWidth: true
                                    from: 1; to: 20000
                                    value: 2480
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    height: 24
                                    font.pixelSize: 11
                                    model: [qsTr("Pixels (px)"), qsTr("Inches"), qsTr("Millimeters")]
                                }
                                Item {}

                                Text { text: qsTr("Height:"); color: T.text; font.pixelSize: 11 }
                                PSpinBox {
                                    id: hField
                                    Layout.fillWidth: true
                                    from: 1; to: 20000
                                    value: 3508
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    height: 24
                                    font.pixelSize: 11
                                    model: [qsTr("Pixels (px)"), qsTr("Inches"), qsTr("Millimeters")]
                                }
                                Item {}

                                Text { text: qsTr("Resolution:"); color: T.text; font.pixelSize: 11 }
                                PSpinBox {
                                    id: resField
                                    Layout.fillWidth: true
                                    from: 1; to: 4800
                                    value: 300
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    height: 24
                                    font.pixelSize: 11
                                    model: [qsTr("Pixels/Inch"), qsTr("Pixels/Centimeter")]
                                }
                                Item {}
                            }
                        }

                        // ---- Save Image Size as ----
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: qsTr("Save Image Size as:")
                                color: T.text
                                font.pixelSize: 11
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 24
                                color: T.base
                                border.color: T.border
                                TextInput {
                                    id: sizeNameField
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    verticalAlignment: TextInput.AlignVCenter
                                    color: T.text
                                    font.pixelSize: 11
                                    clip: true
                                }
                            }
                            Button {
                                text: qsTr("Save")
                                implicitHeight: 24
                                implicitWidth: 70
                                font.pixelSize: 11
                                enabled: sizeNameField.text.length > 0
                                onClicked: {
                                    // Stored for this session; listed back in
                                    // the Predefined dropdown above.
                                    var arr = dialog.savedSizes.slice(); arr.push({ name: sizeNameField.text, w: wField.value, h: hField.value }); dialog.savedSizes = arr
                                    sizeNameField.text = ""
                                }
                            }
                        }

                        // ---- Color ----
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: colorColumn.implicitHeight + 34
                            Layout.topMargin: 12
                            color: "transparent"
                            border.color: T.border
                            border.width: 1

                            Text {
                                x: 10; y: -8
                                text: qsTr("Color")
                                color: T.text
                                font.pixelSize: 11
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.margins: -2
                                    color: T.windowAlt
                                    z: -1
                                }
                            }

                            ColumnLayout {
                                id: colorColumn
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 8

                                Repeater {
                                    model: [
                                        { label: qsTr("Model:"),   opts: [qsTr("RGB/Alpha"), qsTr("Grayscale/Alpha")] },
                                        { label: qsTr("Depth:"),   opts: [qsTr("8-bit integer/channel"), qsTr("16-bit integer/channel")] },
                                        { label: qsTr("Profile:"), opts: [qsTr("sRGB-elle-V2-srgbtrc.icc (Default)"), qsTr("sRGB built-in")] }
                                    ]
                                    delegate: RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Text {
                                            text: modelData.label
                                            color: T.text
                                            font.pixelSize: 11
                                            Layout.preferredWidth: 60
                                        }
                                        ComboBox {
                                            Layout.fillWidth: true
                                            height: 24
                                            font.pixelSize: 11
                                            model: modelData.opts
                                        }
                                    }
                                }
                                Button {
                                    Layout.fillWidth: true
                                    text: qsTr("Color Space Browser")
                                    implicitHeight: 26
                                    font.pixelSize: 11
                                    enabled: false // read-only until ICC plumbing lands
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    // ================= CONTENT TAB =================
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8
                        visible: tabIndex === 1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text { text: qsTr("Name:"); color: T.text; font.pixelSize: 11; Layout.preferredWidth: 110 }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 24
                                color: T.base
                                border.color: T.border
                                TextInput {
                                    id: nameField
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    anchors.rightMargin: 6
                                    verticalAlignment: TextInput.AlignVCenter
                                    text: qsTr("Unnamed")
                                    color: T.text
                                    font.pixelSize: 11
                                    clip: true
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text { text: qsTr("Layers:"); color: T.text; font.pixelSize: 11; Layout.preferredWidth: 110 }
                            PSpinBox {
                                id: layersField
                                Layout.fillWidth: true
                                from: 1; to: 64
                                value: 1
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text { text: qsTr("Background Color:"); color: T.text; font.pixelSize: 11; Layout.preferredWidth: 110 }
                            Rectangle {
                                Layout.fillWidth: true
                                height: 24
                                color: bgColor
                                border.color: T.border
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: dialog.pickBackground = !dialog.pickBackground
                                }
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: dialog.pickBackground
                            spacing: 4
                            Flow {
                                Layout.fillWidth: true
                                spacing: 3
                                Repeater {
                                    model: ["#ffffff", "#000000", "#7f7f7f", "#da4453",
                                            "#e67e22", "#f6c02b", "#2ecc71", "#3daee9",
                                            "#9b59b6", "#00b894", "#34495e", "#95a5a6"]
                                    delegate: Rectangle {
                                        width: 22; height: 22
                                        color: modelData
                                        border.color: T.border
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: bgColor = modelData
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text { text: qsTr("Background Opacity:"); color: T.text; font.pixelSize: 11; Layout.preferredWidth: 110 }
                            PSlider {
                                id: bgOpacity
                                Layout.fillWidth: true
                                from: 0; to: 100
                                value: 100
                            }
                            Text {
                                text: Math.round(bgOpacity.value) + "%"
                                color: T.text
                                font.pixelSize: 11
                                Layout.preferredWidth: 36
                            }
                        }

                        Text { text: qsTr("Background:"); color: T.text; font.pixelSize: 11 }

                        ColumnLayout {
                            spacing: 2
                            Repeater {
                                model: [qsTr("As raster layer"),
                                        qsTr("As canvas color"),
                                        qsTr("As fill layer")]
                                delegate: RowLayout {
                                    spacing: 6
                                    Rectangle {
                                        width: 13; height: 13
                                        radius: 7
                                        color: "transparent"
                                        border.color: T.textDim
                                        Rectangle {
                                            anchors.centerIn: parent
                                            width: 7; height: 7
                                            radius: 4
                                            visible: dialog.backgroundMode === index
                                            color: T.highlight
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: dialog.backgroundMode = index
                                        }
                                    }
                                    Text {
                                        text: modelData
                                        color: T.text
                                        font.pixelSize: 11
                                    }
                                }
                            }
                        }

                        Text { text: qsTr("Description:"); color: T.text; font.pixelSize: 11 }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumHeight: 60
                            color: T.base
                            border.color: T.border
                            TextEdit {
                                anchors.fill: parent
                                anchors.margins: 6
                                color: T.text
                                font.pixelSize: 11
                                wrapMode: TextEdit.Wrap
                                clip: true
                            }
                        }
                    }

                    // ---- summary ----
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        color: T.text
                        font.pixelSize: 11
                        text: {
                            const px = wField.value * hField.value
                            const mb = (px * 4 / (1024 * 1024)).toFixed(1)
                            return qsTr("This document will be %1 pixels by %2 pixels in RGB/Alpha (8-bit integer/channel). The pixel size is 32 bit. A single paint layer will use %3 MiB of RAM.")
                                   .arg(wField.value).arg(hField.value).arg(mb)
                        }
                    }
                }
            }
        }

        // ---- footer ----
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            color: T.window
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: T.border }

            RowLayout {
                anchors.fill: parent
                anchors.rightMargin: 12
                spacing: 8
                Item { Layout.fillWidth: true }
                Button {
                    text: qsTr("Create")
                    Layout.preferredWidth: 96
                    implicitHeight: 26
                    onClicked: {
                        dialog.controller.createDocument(
                            wField.value, hField.value, nameField.text,
                            layersField.value, bgColor, Math.round(bgOpacity.value),
                            dialog.backgroundMode)
                        dialog.close()
                    }
                }
                Button {
                    text: qsTr("Cancel")
                    Layout.preferredWidth: 96
                    implicitHeight: 26
                    onClicked: dialog.close()
                }
            }
        }
    }

    // ---- dialog state ----
    property int tabIndex: 0
    property color bgColor: "#ffffff"
    property bool pickBackground: false
    property int orientationMode: 0   // 0 = portrait, 1 = landscape
    property int backgroundMode: 0    // matches AppController.BackgroundMode
    /// Sizes the user saved with "Save Image Size as" this session.
    property var savedSizes: []

    readonly property var presetNames: [
        qsTr("Custom"), "A4 300dpi", "A4 72dpi", "HD 1920×1080",
        "Full page 2480×3508", "Square 2048", "Social 1080×1080", "QHD 2560×1440"
    ]
    readonly property var presetModel: presetNames.concat(
        savedSizes.map(function (s) { return s.name }))

    function applyPreset(index) {
        if (index < sizePresets.length) {
            const s = sizePresets[index]
            if (s[0] > 0) {
                wField.value = s[0]
                hField.value = s[1]
                resField.value = s[2]
            }
        } else {
            const s = savedSizes[index - sizePresets.length]
            wField.value = s.w
            hField.value = s.h
        }
    }
}
