import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Prism.Ui 1.0
import Prism

// The document form shares the workspace's colour, typography and corner tokens.
Dialog {
    id: dialog
    objectName: "newDocumentDialog"
    required property AppController controller
    title: qsTr("New document")
    modal: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(620, Overlay.overlay.width - 32)
    height: Math.min(650, Overlay.overlay.height - 32)
    padding: 20
    property real dimensionWidth: 1920
    property real dimensionHeight: 1080
    property int documentDpi: 72
    property int unitIndex: 0
    property bool fromClipboard: false
    property int clipboardWidth: 0
    property int clipboardHeight: 0
    property bool solidBackground: false
    property string errorMessage: ""
    property var savedSizes: []
    property bool showSavePreset: false
    readonly property real pixelsPerUnit: unitIndex === 1 ? documentDpi : unitIndex === 2 ? documentDpi / 25.4 : 1
    readonly property int pixelWidth: fromClipboard ? clipboardWidth : Math.round(dimensionWidth * pixelsPerUnit)
    readonly property int pixelHeight: fromClipboard ? clipboardHeight : Math.round(dimensionHeight * pixelsPerUnit)
    readonly property bool validDimensions: pixelWidth > 0 && pixelHeight > 0 && pixelWidth <= 16384 && pixelHeight <= 16384 && documentDpi > 0 && documentDpi <= 2400
    readonly property var builtInSizes: [
        {
            name: qsTr("Custom"),
            width: 0,
            height: 0,
            dpi: 72
        },
        {
            name: qsTr("A4 · 300 dpi"),
            width: 2480,
            height: 3508,
            dpi: 300
        },
        {
            name: qsTr("A4 · 72 dpi"),
            width: 595,
            height: 842,
            dpi: 72
        },
        {
            name: qsTr("HD · 1920 × 1080"),
            width: 1920,
            height: 1080,
            dpi: 72
        },
        {
            name: qsTr("Square · 2048 × 2048"),
            width: 2048,
            height: 2048,
            dpi: 72
        },
        {
            name: qsTr("Social · 1080 × 1080"),
            width: 1080,
            height: 1080,
            dpi: 72
        },
        {
            name: qsTr("QHD · 2560 × 1440"),
            width: 2560,
            height: 1440,
            dpi: 72
        }
    ]
    readonly property var presetModel: builtInSizes.concat(savedSizes)
    function changeUnits(next) {
        const w = pixelWidth, h = pixelHeight;
        unitIndex = next;
        dimensionWidth = w / pixelsPerUnit;
        dimensionHeight = h / pixelsPerUnit;
    }
    function applyPreset(index) {
        const preset = presetModel[index];
        if (!preset || preset.width <= 0 || preset.height <= 0)
            return;
        documentDpi = preset.dpi;
        dimensionWidth = preset.width / pixelsPerUnit;
        dimensionHeight = preset.height / pixelsPerUnit;
    }
    function chooseClipboard() {
        const size = controller.clipboardImageSize();
        if (size.width <= 0 || size.height <= 0) {
            errorMessage = qsTr("The clipboard has no image. Copy an image, then try again.");
            return;
        }
        errorMessage = "";
        clipboardWidth = size.width;
        clipboardHeight = size.height;
        fromClipboard = true;
        dimensionWidth = size.width / pixelsPerUnit;
        dimensionHeight = size.height / pixelsPerUnit;
    }
    function commitDimensions() {
        const widthValid = widthField.commitValue();
        const heightValid = heightField.commitValue();
        const dpiValid = dpiField.commitValue();
        return widthValid && heightValid && dpiValid;
    }
    function create() {
        const dimensionsValid = commitDimensions();
        const layersValid = layersField.commitValue();
        const opacityValid = !solidBackground || opacityField.commitValue();
        if (!dimensionsValid || !layersValid || !opacityValid) {
            errorMessage = qsTr("Complete the numeric fields with valid values before creating the document.");
            return;
        }
        if (!validDimensions) {
            errorMessage = qsTr("Use dimensions from 1 to 16,384 pixels and a DPI from 1 to 2,400.");
            return;
        }
        if (solidBackground && !backgroundColor.acceptableInput) {
            errorMessage = qsTr("Enter a background colour as #RRGGBB.");
            return;
        }
        const accepted = controller.requestDocumentAction("new", {
            width: pixelWidth,
            height: pixelHeight,
            dpi: documentDpi,
            name: documentName.text.trim() || qsTr("Untitled"),
            layerCount: Math.round(layersField.numericValue),
            background: solidBackground ? "solid" : "transparent",
            backgroundColor: backgroundColor.text,
            backgroundOpacity: Math.round(opacityField.numericValue),
            clipboard: fromClipboard
        });
        if (accepted)
            close();
        else
            errorMessage = controller.lastError || qsTr("The document could not be created. Review the settings and try again.");
    }
    onOpened: {
        savedSizes = controller.documentSizePresets();
        errorMessage = "";
        documentName.forceActiveFocus();
        documentName.selectAll();
    }
    onDocumentDpiChanged: {
        if (fromClipboard) {
            dimensionWidth = clipboardWidth / pixelsPerUnit;
            dimensionHeight = clipboardHeight / pixelsPerUnit;
        }
    }
    component Label: Text {
        color: ThemeManager.themeColor("text")
        font.pixelSize: ThemeManager.themeColor("fontBody")
        verticalAlignment: Text.AlignVCenter
    }
    component Field: PTextField {
        implicitHeight: 36
    }
    component NumberField: Field {
        id: number
        property real numericValue: 1
        property int decimals: 0
        property real maximum: 32768
        property real minimum: 1
        signal valueEdited(real next)
        text: decimals === 0 ? String(Math.round(numericValue)) : Number(numericValue.toFixed(decimals)).toString()
        validator: DoubleValidator {
            bottom: number.minimum
            top: number.maximum
            decimals: number.decimals
            locale: "C"
            notation: DoubleValidator.StandardNotation
        }
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        function commitValue() {
            const next = Number(text);
            if (!acceptableInput || !isFinite(next))
                return false;
            valueEdited(next);
            text = Qt.binding(function () {
                return number.decimals === 0 ? String(Math.round(number.numericValue)) : Number(number.numericValue.toFixed(number.decimals)).toString();
            });
            return true;
        }
        onEditingFinished: commitValue()
    }
    component Action: PButton {
        implicitHeight: 36
        leftPadding: 14
        rightPadding: 14
    }
    component Choice: ComboBox {
        id: choice
        implicitHeight: 36
        font.pixelSize: ThemeManager.themeColor("fontBody")
        contentItem: Text {
            text: choice.displayText
            color: ThemeManager.themeColor("text")
            font: choice.font
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 28
            elide: Text.ElideRight
        }
        indicator: Icon {
            x: choice.width - 23
            y: (choice.height - height) / 2
            width: 14
            height: 14
            name: "chevron-down"
            color: ThemeManager.themeColor("text")
        }
        background: Rectangle {
            radius: ThemeManager.themeColor("radiusControl")
            color: choice.hovered ? ThemeManager.themeColor("buttonHover") : ThemeManager.themeColor("panelRaised")
            border.width: choice.visualFocus ? 2 : 1
            border.color: choice.visualFocus ? ThemeManager.themeColor("highlight") : ThemeManager.themeColor("border")
        }
        delegate: ItemDelegate {
            width: choice.width
            height: 36
            highlighted: choice.highlightedIndex === index
            text: choice.textRole ? modelData[choice.textRole] : modelData
            contentItem: Text {
                text: parent.text
                color: parent.highlighted ? ThemeManager.themeColor("selectionText") : ThemeManager.themeColor("text")
                font: choice.font
                leftPadding: 10
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            background: Rectangle {
                radius: ThemeManager.themeColor("radiusControl")
                color: parent.highlighted ? ThemeManager.themeColor("selection") : "transparent"
            }
        }
        popup: Popup {
            y: choice.height + 4
            width: choice.width
            padding: 4
            implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
            background: Rectangle {
                radius: ThemeManager.themeColor("radiusPanel")
                color: ThemeManager.themeColor("popup")
                border.color: ThemeManager.themeColor("popupBorder")
            }
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: choice.popup.visible ? choice.delegateModel : null
                currentIndex: choice.highlightedIndex
                ScrollIndicator.vertical: ScrollIndicator {}
            }
        }
    }
    background: Rectangle {
        radius: ThemeManager.themeColor("radiusPanel")
        color: ThemeManager.themeColor("window")
        border.color: ThemeManager.themeColor("border")
    }
    header: Label {
        text: qsTr("New document")
        padding: 20
        bottomPadding: 4
        font.pixelSize: ThemeManager.themeColor("fontTitle")
        font.weight: Font.DemiBold
    }
    contentItem: ScrollView {
        id: scroll
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ColumnLayout {
            width: scroll.availableWidth
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Action {
                    objectName: "newDocumentBlank"
                    text: qsTr("Blank canvas")
                    primary: !dialog.fromClipboard
                    onClicked: {
                        dialog.fromClipboard = false;
                        dialog.errorMessage = "";
                    }
                }
                Action {
                    objectName: "newDocumentClipboard"
                    text: qsTr("From clipboard")
                    primary: dialog.fromClipboard
                    onClicked: dialog.chooseClipboard()
                }
                Item {
                    Layout.fillWidth: true
                }
            }
            Label {
                Layout.topMargin: 8
                text: qsTr("Name")
            }
            Field {
                id: documentName
                objectName: "newDocumentName"
                Layout.fillWidth: true
                text: qsTr("Untitled")
                Accessible.name: qsTr("Document name")
            }
            Label {
                Layout.topMargin: 8
                text: qsTr("Canvas size")
                font.weight: Font.DemiBold
            }
            Choice {
                objectName: "newDocumentSizePreset"
                Layout.fillWidth: true
                enabled: !dialog.fromClipboard
                model: dialog.presetModel
                textRole: "name"
                Accessible.name: qsTr("Canvas size preset")
                onActivated: dialog.applyPreset(currentIndex)
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 3
                columnSpacing: 10
                rowSpacing: 6
                Label {
                    text: qsTr("Width")
                }
                Label {
                    text: qsTr("Height")
                }
                Label {
                    text: qsTr("Units")
                }
                NumberField {
                    id: widthField
                    objectName: "newDocumentWidth"
                    Layout.fillWidth: true
                    enabled: !dialog.fromClipboard
                    numericValue: dialog.dimensionWidth
                    decimals: dialog.unitIndex === 0 ? 0 : 4
                    minimum: dialog.unitIndex === 0 ? 1 : 0.0001
                    Accessible.name: qsTr("Canvas width")
                    onValueEdited: next => dialog.dimensionWidth = next
                }
                NumberField {
                    id: heightField
                    objectName: "newDocumentHeight"
                    Layout.fillWidth: true
                    enabled: !dialog.fromClipboard
                    numericValue: dialog.dimensionHeight
                    decimals: dialog.unitIndex === 0 ? 0 : 4
                    minimum: dialog.unitIndex === 0 ? 1 : 0.0001
                    Accessible.name: qsTr("Canvas height")
                    onValueEdited: next => dialog.dimensionHeight = next
                }
                Choice {
                    objectName: "newDocumentUnits"
                    Layout.preferredWidth: 100
                    model: [qsTr("Pixels"), qsTr("Inches"), qsTr("mm")]
                    currentIndex: dialog.unitIndex
                    Accessible.name: qsTr("Dimension units")
                    onActivated: {
                        widthField.commitValue();
                        heightField.commitValue();
                        dialog.changeUnits(currentIndex);
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label {
                    text: qsTr("Resolution")
                }
                NumberField {
                    id: dpiField
                    objectName: "newDocumentDpi"
                    Layout.preferredWidth: 100
                    numericValue: dialog.documentDpi
                    maximum: 2400
                    Accessible.name: qsTr("Resolution in dots per inch")
                    onValueEdited: next => dialog.documentDpi = Math.round(next)
                }
                Label {
                    text: qsTr("dpi")
                    color: ThemeManager.themeColor("textDim")
                }
                Item {
                    Layout.fillWidth: true
                }
                Action {
                    objectName: "newDocumentSwapDimensions"
                    text: qsTr("Swap width / height")
                    enabled: !dialog.fromClipboard
                    onClicked: {
                        widthField.commitValue();
                        heightField.commitValue();
                        const previous = dialog.dimensionWidth;
                        dialog.dimensionWidth = dialog.dimensionHeight;
                        dialog.dimensionHeight = previous;
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: dialog.fromClipboard ? qsTr("Clipboard pixels and transparency are preserved.") : dialog.unitIndex === 0 ? qsTr("Changing DPI keeps the pixel dimensions.") : qsTr("Changing DPI keeps the physical size and changes the pixel dimensions.")
                color: ThemeManager.themeColor("textDim")
                wrapMode: Text.WordWrap
            }
            Action {
                text: dialog.showSavePreset ? qsTr("Hide preset options") : qsTr("Save this size as a preset…")
                enabled: !dialog.fromClipboard
                onClicked: dialog.showSavePreset = !dialog.showSavePreset
            }
            RowLayout {
                Layout.fillWidth: true
                visible: dialog.showSavePreset
                Field {
                    id: presetName
                    objectName: "newDocumentPresetName"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Preset name")
                    Accessible.name: qsTr("Preset name")
                }
                Action {
                    objectName: "newDocumentSavePreset"
                    text: qsTr("Save preset")
                    enabled: presetName.text.trim().length > 0 && dialog.validDimensions
                    onClicked: {
                        dialog.commitDimensions();
                        dialog.controller.saveDocumentSizePreset(presetName.text.trim(), dialog.pixelWidth, dialog.pixelHeight, dialog.documentDpi);
                        dialog.savedSizes = dialog.controller.documentSizePresets();
                        presetName.text = "";
                        dialog.showSavePreset = false;
                    }
                }
            }
            Label {
                Layout.topMargin: 10
                text: qsTr("Artwork")
                font.weight: Font.DemiBold
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Paint layers")
                }
                Item {
                    Layout.fillWidth: true
                }
                NumberField {
                    id: layersField
                    objectName: "newDocumentLayers"
                    Layout.preferredWidth: 100
                    maximum: 64
                    numericValue: 1
                    Accessible.name: qsTr("Number of paint layers, excluding background")
                    onValueEdited: next => numericValue = Math.round(next)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Background")
                }
                Choice {
                    objectName: "newDocumentBackground"
                    Layout.fillWidth: true
                    model: [qsTr("Transparent"), qsTr("Solid raster layer")]
                    currentIndex: dialog.solidBackground ? 1 : 0
                    Accessible.name: qsTr("Background type")
                    onActivated: dialog.solidBackground = currentIndex === 1
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: dialog.solidBackground
                spacing: 10
                Label {
                    text: qsTr("Colour")
                }
                Field {
                    id: backgroundColor
                    objectName: "newDocumentBackgroundColor"
                    Layout.fillWidth: true
                    text: "#ffffff"
                    validator: RegularExpressionValidator {
                        regularExpression: /^#[0-9a-fA-F]{6}$/
                    }
                    Accessible.name: qsTr("Background colour in hexadecimal")
                }
                Label {
                    text: qsTr("Opacity")
                }
                NumberField {
                    id: opacityField
                    objectName: "newDocumentBackgroundOpacity"
                    Layout.preferredWidth: 64
                    minimum: 0
                    maximum: 100
                    numericValue: 100
                    Accessible.name: qsTr("Background opacity percent")
                    onValueEdited: next => numericValue = Math.round(next)
                }
                Label {
                    text: "%"
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("RGB / Alpha · 8-bit · sRGB")
                color: ThemeManager.themeColor("textDim")
                Layout.topMargin: 8
            }
            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("%1 × %2 pixels · %3 dpi · %4 MiB per raster layer").arg(dialog.pixelWidth).arg(dialog.pixelHeight).arg(dialog.documentDpi).arg((dialog.pixelWidth * dialog.pixelHeight * 4 / (1024 * 1024)).toFixed(1))
                color: ThemeManager.themeColor("textDim")
            }
            Label {
                objectName: "newDocumentError"
                Layout.fillWidth: true
                visible: dialog.errorMessage.length > 0
                text: dialog.errorMessage
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
            }
        }
    }
    footer: RowLayout {
        spacing: 8
        Item {
            Layout.fillWidth: true
        }
        Action {
            objectName: "newDocumentCancel"
            text: qsTr("Cancel")
            Layout.bottomMargin: 20
            onClicked: dialog.close()
        }
        Action {
            objectName: "newDocumentCreate"
            text: qsTr("Create document")
            primary: true
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            enabled: dialog.validDimensions
            onClicked: dialog.create()
        }
    }
}
