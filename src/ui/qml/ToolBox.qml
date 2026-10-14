import QtQuick
import QtQuick.Controls.Basic
import Prism.Ui 1.0

import Prism

// A short rail keeps everyday drawing tools close to the canvas. The full
// tool list stays available in a flyout instead of filling the whole edge.
Rectangle {
    id: toolbox
    required property AppController controller
    property bool glass: false

    implicitWidth: ThemeManager.themeColor("toolboxWidth")
    color: glass ? "transparent" : ThemeManager.themeColor("panel")
    border.color: glass ? "transparent" : ThemeManager.themeColor("border")
    border.width: 1
    radius: ThemeManager.themeColor("radius")

    // ready: true  -> working tool
    // ready: false -> shown disabled with an explanatory tooltip
    //
    // Scoped to what a DRAWING app needs (Clip Studio Paint's set), not a
    // bitmap editor's. Dropped: Multibrush and Magnetic/Outline/Bezier/Similar
    // selection (photo-retouch selections), Pattern and Arc (material/photo
    // tools). The engine still implements them; only the product exposes a
    // smaller, drawing-first set.
    readonly property var tools: [
        {
            icon: "brush",
            name: qsTr("Freehand Brush"),
            key: "B",
            tool: AppController.BrushTool,
            ready: true
        },
        {
            icon: "eraser",
            name: qsTr("Eraser"),
            key: "E",
            tool: AppController.EraserTool,
            ready: true
        },
        {
            icon: "dynamic",
            name: qsTr("Stabilized Brush"),
            key: "",
            tool: AppController.DynamicBrushTool,
            ready: true
        },
        {
            icon: "line",
            name: qsTr("Line"),
            key: "L",
            tool: AppController.LineTool,
            ready: true
        },
        {
            icon: "polyline",
            name: qsTr("Polyline"),
            key: "",
            tool: AppController.PolylineTool,
            ready: true
        },
        {
            icon: "bezier",
            name: qsTr("Curve"),
            key: "",
            tool: AppController.BezierTool,
            ready: true
        },
        {
            icon: "path",
            name: qsTr("Freehand Curve"),
            key: "",
            tool: AppController.FreehandPathTool,
            ready: true
        },
        {
            icon: "select-rect",
            name: qsTr("Rectangle"),
            key: "R",
            tool: AppController.RectangleTool,
            ready: true
        },
        {
            icon: "ellipse",
            name: qsTr("Ellipse"),
            key: "O",
            tool: AppController.EllipseTool,
            ready: true
        },
        {
            icon: "smudge",
            name: qsTr("Blend"),
            key: "",
            tool: AppController.BlendTool,
            ready: true
        },
        {
            icon: "fill",
            name: qsTr("Fill"),
            key: "F",
            tool: AppController.FillTool,
            ready: true
        },
        {
            icon: "outline",
            name: qsTr("Lasso Fill"),
            key: "",
            tool: AppController.LassoFillTool,
            ready: true
        },
        {
            icon: "gradient",
            name: qsTr("Gradient"),
            key: "G",
            tool: AppController.GradientTool,
            ready: true
        },
        {
            icon: "picker",
            name: qsTr("Color Picker"),
            key: "I",
            tool: AppController.EyedropperTool,
            ready: true
        },
        {
            icon: "select-rect-dashed",
            name: qsTr("Rectangular Selection"),
            key: "M",
            tool: AppController.RectSelectTool,
            ready: true
        },
        {
            icon: "select-ellipse-dashed",
            name: qsTr("Elliptical Selection"),
            key: "",
            tool: AppController.EllipseSelectTool,
            ready: true
        },
        {
            icon: "lasso",
            name: qsTr("Freehand Selection"),
            key: "",
            tool: AppController.LassoSelectTool,
            ready: true
        },
        {
            icon: "polygon",
            name: qsTr("Polygon Selection"),
            key: "",
            tool: AppController.PolygonSelectTool,
            ready: true
        },
        {
            icon: "wand",
            name: qsTr("Contiguous Selection"),
            key: "W",
            tool: AppController.MagicWandTool,
            ready: true
        },
        {
            icon: "move",
            name: qsTr("Move Layer"),
            key: "V",
            tool: AppController.MoveTool,
            ready: true
        },
        {
            icon: "transform",
            name: qsTr("Transform"),
            key: "",
            tool: AppController.TransformTool,
            ready: true
        },
        {
            icon: "text",
            name: qsTr("Text"),
            key: "",
            tool: AppController.TextTool,
            ready: true
        },
        {
            icon: "crop",
            name: qsTr("Crop"),
            key: "",
            tool: AppController.CropTool,
            ready: true
        },
        {
            icon: "pan",
            name: qsTr("Pan"),
            key: "",
            tool: AppController.PanTool,
            ready: true
        },
        {
            icon: "zoom",
            name: qsTr("Zoom"),
            key: "Z",
            tool: AppController.ZoomTool,
            ready: true
        }
    ]
    readonly property var railIndices: [0, 1, 9, 10, 13, 14, 19, 23]
    readonly property var groupedTools: {
        let entries = [];
        for (let i = 0; i < tools.length; ++i) {
            if (i === 0)
                entries.push({
                    heading: true,
                    name: qsTr("Drawing")
                });
            if (i === 14)
                entries.push({
                    heading: true,
                    name: qsTr("Selection")
                });
            if (i === 19)
                entries.push({
                    heading: true,
                    name: qsTr("Arrange & view")
                });
            entries.push(tools[i]);
        }
        return entries;
    }

    Text {
        id: railTitle
        anchors.top: parent.top
        anchors.topMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        text: qsTr("TOOLS")
        color: ThemeManager.themeColor("textDim")
        font.pixelSize: ThemeManager.themeColor("fontCaption")
        font.letterSpacing: 0.7
    }

    Column {
        anchors.top: railTitle.bottom
        anchors.topMargin: 10
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 4

        Repeater {
            model: toolbox.railIndices
            delegate: Rectangle {
                required property int modelData
                readonly property var entry: toolbox.tools[modelData]
                objectName: "toolButton_" + entry.icon
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: entry.key ? entry.name + " (" + entry.key + ")" : entry.name
                Accessible.onPressAction: toolbox.controller.tool = entry.tool
                Keys.onPressed: event => {
                    if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                        toolbox.controller.tool = entry.tool;
                        event.accepted = true;
                    }
                }
                width: 38
                height: 38
                radius: ThemeManager.themeColor("radiusSmall")
                color: toolbox.controller.tool === entry.tool ? ThemeManager.themeColor("selection") : (railHover.hovered ? ThemeManager.themeColor("buttonHover") : "transparent")
                border.width: toolbox.controller.tool === entry.tool || activeFocus ? 1 : 0
                border.color: ThemeManager.themeColor("highlight")
                Icon {
                    anchors.centerIn: parent
                    width: 19
                    height: 19
                    name: entry.icon
                    color: toolbox.controller.tool === entry.tool ? ThemeManager.themeColor("selectionText") : ThemeManager.themeColor("text")
                }
                HoverHandler {
                    id: railHover
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: toolbox.controller.tool = entry.tool
                }
                ToolTip.visible: railHover.hovered
                ToolTip.delay: 500
                ToolTip.text: entry.key ? entry.name + " (" + entry.key + ")" : entry.name
            }
        }

        Rectangle {
            objectName: "toolAllButton"
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("All tools")
            Accessible.onPressAction: allTools.open()
            Keys.onPressed: event => {
                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                    allTools.open();
                    event.accepted = true;
                }
            }
            width: 38
            height: 38
            radius: ThemeManager.themeColor("radiusSmall")
            color: allHover.hovered || allTools.opened ? ThemeManager.themeColor("buttonHover") : "transparent"
            border.width: activeFocus ? 1 : 0
            border.color: ThemeManager.themeColor("highlight")
            Text {
                anchors.centerIn: parent
                text: "···"
                font.pixelSize: 20
                color: ThemeManager.themeColor("text")
            }
            HoverHandler {
                id: allHover
            }
            MouseArea {
                anchors.fill: parent
                onClicked: allTools.open()
            }
            ToolTip.visible: allHover.hovered
            ToolTip.text: qsTr("All tools")
        }
    }

    Rectangle {
        id: swatch
        objectName: "toolColourSwatch"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        width: 32
        height: 32
        radius: ThemeManager.themeColor("radiusControl")
        activeFocusOnTab: true
        Accessible.role: Accessible.Button
        Accessible.name: qsTr("Brush colour")
        Accessible.onPressAction: railColorPopup.open()
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                railColorPopup.open();
                event.accepted = true;
            }
        }
        color: toolbox.controller.brushColor
        border.color: activeFocus ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
        border.width: 2
        MouseArea {
            anchors.fill: parent
            onClicked: railColorPopup.open()
        }
        ToolTip.visible: swatchHover.hovered
        ToolTip.text: qsTr("Brush colour")
        HoverHandler {
            id: swatchHover
        }
    }

    Popup {
        id: allTools
        focus: true
        x: toolbox.width + 6
        y: 42
        width: 228
        height: Math.min(590, toolbox.height - 54)
        padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: ThemeManager.themeColor("popup")
            border.color: ThemeManager.themeColor("popupBorder")
            radius: ThemeManager.themeColor("radius")
        }
        Column {
            anchors.fill: parent
            spacing: 6
            Text {
                text: qsTr("ALL TOOLS")
                color: ThemeManager.themeColor("textDim")
                font.pixelSize: ThemeManager.themeColor("fontCaption")
                font.letterSpacing: 0.7
            }
            Flickable {
                width: parent.width
                height: parent.height - 24
                contentWidth: width
                contentHeight: toolList.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                Column {
                    id: toolList
                    width: parent.width
                    spacing: 2
                    Repeater {
                        model: toolbox.groupedTools
                        delegate: Rectangle {
                            required property var modelData
                            objectName: "toolFlyout_" + (modelData.heading ? modelData.name : modelData.icon)
                            activeFocusOnTab: !modelData.heading
                            Accessible.role: modelData.heading ? Accessible.Heading : Accessible.Button
                            Accessible.name: modelData.name
                            function chooseTool() {
                                if (modelData.heading)
                                    return;
                                toolbox.controller.tool = modelData.tool;
                                allTools.close();
                            }
                            Accessible.onPressAction: chooseTool()
                            Keys.onPressed: event => {
                                if (event.key === Qt.Key_Space || event.key === Qt.Key_Return) {
                                    chooseTool();
                                    event.accepted = true;
                                }
                            }
                            width: toolList.width
                            height: modelData.heading ? 28 : 36
                            radius: ThemeManager.themeColor("radiusSmall")
                            color: modelData.heading ? "transparent" : toolbox.controller.tool === modelData.tool ? ThemeManager.themeColor("selection") : (rowHover.hovered ? ThemeManager.themeColor("buttonHover") : "transparent")
                            border.width: activeFocus ? 1 : 0
                            border.color: ThemeManager.themeColor("highlight")
                            Row {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 8
                                spacing: 10
                                Icon {
                                    visible: !modelData.heading
                                    width: 16
                                    height: 16
                                    name: modelData.icon || ""
                                    color: toolbox.controller.tool === modelData.tool ? ThemeManager.themeColor("selectionText") : ThemeManager.themeColor("text")
                                }
                                Text {
                                    text: modelData.name
                                    color: modelData.heading ? ThemeManager.themeColor("textDim") : toolbox.controller.tool === modelData.tool ? ThemeManager.themeColor("selectionText") : ThemeManager.themeColor("text")
                                    font.pixelSize: ThemeManager.themeColor("fontBody")
                                    font.weight: modelData.heading ? Font.DemiBold : Font.Normal
                                }
                            }
                            HoverHandler {
                                id: rowHover
                            }
                            MouseArea {
                                anchors.fill: parent
                                enabled: !modelData.heading
                                onClicked: parent.chooseTool()
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: railColorPopup
        focus: true
        x: toolbox.width + 6
        y: Math.max(4, toolbox.height - 426)
        width: 300
        height: 420
        padding: 0
        background: Rectangle {
            color: ThemeManager.themeColor("popup")
            border.color: ThemeManager.themeColor("popupBorder")
            radius: ThemeManager.themeColor("radiusPanel")
        }
        ColorSelector {
            anchors.fill: parent
            controller: toolbox.controller
        }
    }
}
