import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

// Brush Lab: preset shelf, live engine preview, and the active brush controls.
Rectangle {
    id: docker
    required property AppController controller
    /// True when hosted in the tabbed docker column (see LayersDocker).
    property bool embedded: false

    color: ThemeManager.themeColor("panel")
    radius: ThemeManager.themeColor("radius")

    // Filter state â€” the grid shows preset indices that survive the filter.
    property string query: ""
    property var visibleIndices: []

    function rebuildFilter() {
        var out = []
        var q = docker.query.toLowerCase()
        for (var i = 0; i < docker.controller.presetCount(); ++i) {
            var n = docker.controller.presetName(i)
            if (q.length === 0 || n.toLowerCase().indexOf(q) >= 0)
                out.push(i)
        }
        docker.visibleIndices = out
    }

    Component.onCompleted: rebuildFilter()
    Connections {
        target: docker.controller
        function onPresetsChanged() { docker.rebuildFilter() }
    }

    onQueryChanged: rebuildFilter()

    // ---- header ----
    Rectangle {
        id: header
        width: parent.width
        height: ThemeManager.themeColor("dockerHeaderHeight")
        color: "transparent"

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Brush Lab").toUpperCase()
            visible: !docker.embedded
            color: ThemeManager.themeColor("textDim")
            font.pixelSize: 10
            font.letterSpacing: 0.5
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1
            Repeater {
                model: [
                    { icon: "plus",       tip: qsTr("Save current brush as preset"), act: "add" },
                    { icon: "undo",       tip: qsTr("Reset brush to selected preset"), act: "reset" },
                    { icon: "file-open",  tip: qsTr("Import brush from .prismbrush file"), act: "import" },
                    { icon: "export",     tip: qsTr("Export active brush to .prismbrush file"), act: "export" },
                    { icon: "arrow-up",   tip: qsTr("Move preset up"),   act: "up" },
                    { icon: "arrow-down", tip: qsTr("Move preset down"), act: "down" },
                    { icon: "trash",      tip: qsTr("Delete preset"),    act: "del" }
                ]
                delegate: Rectangle {
                    width: 20; height: 20
                    visible: modelData.act !== "del" || docker.controller.activePresetIndex >= docker.controller.builtinPresetCount
                    color: bh.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
                    Icon {
                        anchors.centerIn: parent
                        width: 14; height: 14
                        name: modelData.icon
                        color: ThemeManager.themeColor("text")
                    }
                    HoverHandler { id: bh }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            var i = docker.controller.activePresetIndex
                            switch (modelData.act) {
                            case "add":    docker.controller.addPreset(); break
                            case "reset":  docker.controller.resetBrushToPreset(); break
                            case "import": brushImportDialog.open(); break
                            case "export": brushExportDialog.open(); break
                            case "up":     docker.controller.movePreset(i, 1); break
                            case "down":   docker.controller.movePreset(i, -1); break
                            case "del":    docker.controller.removePreset(i); break
                            }
                        }
                    }
                    ToolTip.visible: bh.hovered
                    ToolTip.delay: 500
                    ToolTip.text: modelData.tip
                }
            }
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: ThemeManager.themeColor("border") }
    }

    // ---- preset shelf ----
    Text {
        id: filterRow
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.leftMargin: 8
        height: 24
        verticalAlignment: Text.AlignVCenter
        text: qsTr("PRESETS") + "  ·  " + docker.visibleIndices.length
        color: ThemeManager.themeColor("textDim")
        font.pixelSize: ThemeManager.themeColor("fontCaption")
        font.letterSpacing: 0.8
    }

    // ---- thumbnail grid ----
    GridView {
        id: grid
        anchors.top: filterRow.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: previewPanel.top
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        clip: true
        cellWidth: 50
        cellHeight: 50
        model: docker.visibleIndices
        currentIndex: docker.visibleIndices.indexOf(docker.controller.activePresetIndex)

        delegate: Rectangle {
            required property int modelData
            readonly property int presetIndex: modelData

            width: 48; height: 48
            color: docker.controller.activePresetIndex === presetIndex
                   ? ThemeManager.themeColor("selection") : (hh.hovered ? ThemeManager.themeColor("buttonHover") : ThemeManager.themeColor("panelSunken"))
            border.color: docker.controller.activePresetIndex === presetIndex
                          ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
            border.width: 1

            Image {
                anchors.fill: parent
                anchors.margins: 3
                fillMode: Image.PreserveAspectFit
                cache: false
                sourceSize.width: 44
                sourceSize.height: 44
                source: "image://preset/" + presetIndex
            }

            HoverHandler { id: hh }

            MouseArea {
                anchors.fill: parent
                onClicked: docker.controller.selectPreset(presetIndex)
            }

            ToolTip.visible: hh.hovered
            ToolTip.delay: 500
            ToolTip.text: docker.controller.presetName(presetIndex)
        }
    }

    Rectangle {
        id: previewPanel
        anchors.bottom: editorScroll.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        height: 108
        radius: ThemeManager.themeColor("radiusSmall")
        color: ThemeManager.themeColor("panelSunken")

        Text {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 8
            text: docker.controller.brushGraphite ? qsTr("LIVE STROKE  ·  LIGHT / FIRM / TILT")
                                                  : qsTr("LIVE STROKE")
            color: ThemeManager.themeColor("textDim")
            font.pixelSize: ThemeManager.themeColor("fontCaption")
            font.letterSpacing: 0.8
        }
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 8
            height: 78
            radius: 4
            color: "#ffffff"
            Image {
                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectFit
                cache: false
                sourceSize.width: 220
                sourceSize.height: 84
                source: "image://preset/active/" + docker.controller.brushPreviewRevision
            }
        }
    }

    Flickable {
        id: editorScroll
        anchors.bottom: searchRow.top
        anchors.bottomMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        height: Math.min(282, Math.max(180, docker.height * 0.43))
        contentWidth: width
        contentHeight: tipEditor.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

    Column {
        id: tipEditor
        width: editorScroll.width - 8
        spacing: 3

        Rectangle { width: parent.width; height: 1; color: ThemeManager.themeColor("border") }

        Row {
            spacing: 4
            Text {
                text: qsTr("Tip:")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
            // Round / Square / Image selector.
            Repeater {
                model: [
                    { label: qsTr("Round"),  act: "round" },
                    { label: qsTr("Square"), act: "square" },
                    { label: qsTr("Image"),  act: "image" }
                ]
                delegate: Rectangle {
                    width: 44; height: 18
                    readonly property bool on:
                        modelData.act === "image" ? docker.controller.tipIsImage
                        : !docker.controller.tipIsImage && docker.controller.brushTipSquare === (modelData.act === "square")
                    color: on ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("windowAlt")
                    border.color: on ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        color: on ? ThemeManager.themeColor("highlightText") : ThemeManager.themeColor("text")
                        font.pixelSize: 9
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            if (modelData.act === "image")
                                tipImageDialog.open()
                            else
                                docker.controller.brushTipSquare = (modelData.act === "square")
                        }
                    }
                }
            }
            // Leave an image tip back to procedural defaults.
            Rectangle {
                visible: docker.controller.tipIsImage
                width: 18; height: 18
                color: clearTipHover.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
                Icon { anchors.centerIn: parent; width: 12; height: 12; name: "close"; color: ThemeManager.themeColor("textDim") }
                HoverHandler { id: clearTipHover }
                MouseArea { anchors.fill: parent; onClicked: docker.controller.clearTipImage() }
                ToolTip.visible: clearTipHover.hovered
                ToolTip.text: qsTr("Remove tip image")
            }
        }

        Row {
            spacing: 4
            Text {
                text: qsTr("Round:")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
            PSlider {
                id: roundnessSlider
                objectName: "tipRoundnessSlider"
                width: tipEditor.width - 110
                from: 0.05; to: 1
                value: docker.controller.brushTipRoundness
                onMoved: docker.controller.brushTipRoundness = value
                Connections {
                    target: docker.controller
                    function onBrushChanged() {
                        if (!roundnessSlider.pressed)
                            roundnessSlider.value = docker.controller.brushTipRoundness
                    }
                }
            }
            Text {
                text: Math.round(docker.controller.brushTipRoundness * 100) + "%"
                color: ThemeManager.themeColor("text")
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            spacing: 4
            Text {
                text: qsTr("Angle:")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
            PSlider {
                id: angleSlider
                objectName: "tipAngleSlider"
                width: tipEditor.width - 110
                from: 0; to: 359
                stepSize: 1
                value: docker.controller.brushTipAngle
                onMoved: docker.controller.brushTipAngle = value
                Connections {
                    target: docker.controller
                    function onBrushChanged() {
                        if (!angleSlider.pressed)
                            angleSlider.value = docker.controller.brushTipAngle
                    }
                }
            }
            Text {
                text: Math.round(docker.controller.brushTipAngle) + " deg"
                color: ThemeManager.themeColor("text")
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            spacing: 4
            Rectangle {
                objectName: "pressureOpacityBox"
                width: 104; height: 22
                radius: ThemeManager.themeColor("radiusSmall")
                color: docker.controller.pressureOpacity ? ThemeManager.themeColor("selection")
                                                         : ThemeManager.themeColor("panelSunken")
                border.color: docker.controller.pressureOpacity ? ThemeManager.themeColor("highlight")
                                                                : ThemeManager.themeColor("border")
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Pressure opacity")
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 10
                }
                MouseArea { anchors.fill: parent; onClicked: docker.controller.pressureOpacity = !docker.controller.pressureOpacity }
            }
            Text {
                text: qsTr("Curve:")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: 10
                anchors.verticalCenter: parent.verticalCenter
            }
            PSlider {
                id: curveSlider
                objectName: "pressureCurveSlider"
                width: tipEditor.width - 190
                from: 0.3; to: 3
                value: docker.controller.pressureCurve
                onMoved: docker.controller.pressureCurve = value
            }
        }

        BrushLabSlider {
            objectName: "labSize"
            caption: qsTr("Size")
            from: 1; to: 200
            displayScale: 1; suffix: " px"
            controllerValue: docker.controller.brushSize
            onMoved: value => docker.controller.brushSize = value
        }
        BrushLabSlider {
            objectName: "labHardness"
            caption: qsTr("Hardness")
            controllerValue: docker.controller.brushHardness
            onMoved: value => docker.controller.brushHardness = value
        }
        BrushLabSlider {
            objectName: "labFlow"
            caption: qsTr("Flow")
            from: 0.01
            controllerValue: docker.controller.brushFlow
            onMoved: value => docker.controller.brushFlow = value
        }
        BrushLabSlider {
            objectName: "labSpacing"
            caption: qsTr("Spacing")
            from: 0.02
            controllerValue: docker.controller.brushSpacing
            onMoved: value => docker.controller.brushSpacing = value
        }
        BrushLabSlider {
            objectName: "labMinSize"
            caption: qsTr("Light size")
            from: 0.05
            controllerValue: docker.controller.pressureSizeMin
            onMoved: value => docker.controller.pressureSizeMin = value
        }
        BrushLabSlider {
            objectName: "labStabilization"
            caption: qsTr("Stabilize")
            controllerValue: docker.controller.brushStabilization
            onMoved: value => docker.controller.brushStabilization = value
        }
        BrushLabSlider {
            objectName: "labEndTaper"
            caption: qsTr("End taper")
            from: 0; to: 6
            displayScale: 1; suffix: " diameters"
            controllerValue: docker.controller.brushEndTaper
            onMoved: value => docker.controller.brushEndTaper = value
        }
        Rectangle {
            width: tipEditor.width; height: 23
            color: docker.controller.brushGraphite ? ThemeManager.themeColor("selection")
                                                  : ThemeManager.themeColor("panelSunken")
            border.color: ThemeManager.themeColor("border")
            Text {
                anchors.centerIn: parent
                text: qsTr("Graphite material")
                color: ThemeManager.themeColor("text")
                font.pixelSize: 10
            }
            MouseArea { anchors.fill: parent; onClicked: docker.controller.brushGraphite = !docker.controller.brushGraphite }
        }
        BrushLabSlider {
            objectName: "labGrain"
            visible: docker.controller.brushGraphite
            caption: qsTr("Grain")
            controllerValue: docker.controller.brushGrain
            onMoved: value => docker.controller.brushGrain = value
        }
        BrushLabSlider {
            objectName: "labGrainScale"
            visible: docker.controller.brushGraphite
            caption: qsTr("Grain scale")
            from: 0.5; to: 4
            controllerValue: docker.controller.brushGrainScale
            onMoved: value => docker.controller.brushGrainScale = value
        }
        BrushLabSlider {
            objectName: "labTiltShading"
            visible: docker.controller.brushGraphite
            caption: qsTr("Tilt shading")
            controllerValue: docker.controller.brushTiltShading
            onMoved: value => docker.controller.brushTiltShading = value
        }
    }
    }

    FileDialog {
        id: tipImageDialog
        title: qsTr("Select a tip image (max 1024x1024)")
        nameFilters: [ qsTr("Images (*.png *.jpg *.jpeg *.bmp)") ]
        onAccepted: docker.controller.loadTipImage(selectedFile)
    }

    FileDialog {
        id: brushImportDialog
        title: qsTr("Import brush")
        nameFilters: [ qsTr("Prism brush (*.prismbrush)") ]
        onAccepted: docker.controller.importBrush(selectedFile)
    }

    FileDialog {
        id: brushExportDialog
        title: qsTr("Export brush")
        fileMode: FileDialog.SaveFile
        nameFilters: [ qsTr("Prism brush (*.prismbrush)") ]
        defaultSuffix: "prismbrush"
        onAccepted: docker.controller.exportBrush(selectedFile,
                                                 docker.controller.activePresetIndex)
    }

    // ---- search row ----
    Row {
        id: searchRow
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 4
        spacing: 4

        Rectangle {
            width: parent.width - 22
            height: 20
            color: ThemeManager.themeColor("windowAlt")
            border.color: ThemeManager.themeColor("border")

            Row {
                anchors.fill: parent
                anchors.leftMargin: 4
                spacing: 4
                Icon {
                    width: 12; height: 12
                    name: "search"
                    color: ThemeManager.themeColor("textDim")
                    anchors.verticalCenter: parent.verticalCenter
                }
                TextInput {
                    objectName: "brushSearchInput"
                    id: searchField
                    width: parent.width - 20
                    anchors.verticalCenter: parent.verticalCenter
                    color: ThemeManager.themeColor("text")
                    font.pixelSize: 10
                    clip: true
                    onTextChanged: docker.query = text
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: searchField.text.length === 0
                        text: qsTr("Search...")
                        color: ThemeManager.themeColor("textDim")
                        font.pixelSize: 10
                    }
                }
            }
        }
        Rectangle {
            width: 18; height: 20
            color: clearHover.hovered ? ThemeManager.themeColor("buttonHover") : "transparent"
            Icon {
                anchors.centerIn: parent
                width: 12; height: 12
                name: "close"
                color: ThemeManager.themeColor("textDim")
            }
            HoverHandler { id: clearHover }
            MouseArea {
                anchors.fill: parent
                onClicked: { searchField.text = ""; docker.query = "" }
            }
            ToolTip.visible: clearHover.hovered
            ToolTip.text: qsTr("Clear search")
        }
    }
}

