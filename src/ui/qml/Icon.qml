import QtQuick
import "Theme.js" as T

// Monochrome vector icon set drawn with Canvas — no emoji, no image assets.
// Every glyph is stroked in a single colour so icons match the theme and stay
// crisp at any size, the way a real drawing app's toolbar looks.
Canvas {
    id: icon

    property string name: "brush"
    property color color: T.text
    property real strokeWidth: 1.4

    implicitWidth: 18
    implicitHeight: 18
    antialiasing: true
    renderStrategy: Canvas.Immediate

    onNameChanged: requestPaint()
    onColorChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d")
        var w = width, h = height
        ctx.reset()
        ctx.clearRect(0, 0, w, h)
        ctx.strokeStyle = icon.color
        ctx.fillStyle = icon.color
        ctx.lineWidth = icon.strokeWidth
        ctx.lineJoin = "round"
        ctx.lineCap = "round"

        // Everything is drawn on a 16x16 grid scaled to the item size.
        var s = Math.min(w, h) / 16
        ctx.save()
        ctx.translate((w - 16 * s) / 2, (h - 16 * s) / 2)
        ctx.scale(s, s)

        switch (icon.name) {
        case "brush":
            // angled brush: ferrule + handle
            ctx.beginPath()
            ctx.moveTo(3, 13)
            ctx.lineTo(6.5, 9.5)
            ctx.lineTo(11.5, 4.5)
            ctx.lineTo(13.5, 6.5)
            ctx.lineTo(8.5, 11.5)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(2.2, 13.8)
            ctx.lineTo(3.6, 11.4)
            ctx.stroke()
            break

        case "eraser":
            ctx.beginPath()
            ctx.moveTo(2.5, 11.5)
            ctx.lineTo(8, 6)
            ctx.lineTo(13.5, 11.5)
            ctx.lineTo(10, 15)
            ctx.lineTo(5, 15)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(6.5, 7.5)
            ctx.lineTo(12, 13)
            ctx.stroke()
            break

        case "fill":
            // paint bucket, tipped, with a drip
            ctx.beginPath()
            ctx.moveTo(6.5, 2.5)
            ctx.lineTo(13, 9)
            ctx.lineTo(7.5, 14.5)
            ctx.lineTo(1, 8)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(5.5, 1.5)
            ctx.lineTo(7.5, 3.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(14, 10.5)
            ctx.quadraticCurveTo(15.5, 12.5, 14, 14)
            ctx.stroke()
            break

        case "picker":
            ctx.beginPath()
            ctx.moveTo(2.5, 13.5)
            ctx.lineTo(9, 7)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(8, 6)
            ctx.lineTo(11.5, 2.5)
            ctx.lineTo(13.5, 4.5)
            ctx.lineTo(10, 8)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(11.5, 3.5, 2.6, 0, Math.PI * 2)
            ctx.stroke()
            break

        case "pan":
            // open hand
            ctx.beginPath()
            ctx.moveTo(5, 9)
            ctx.lineTo(5, 5.5)
            ctx.moveTo(7.5, 9)
            ctx.lineTo(7.5, 4)
            ctx.moveTo(10, 9)
            ctx.lineTo(10, 4.5)
            ctx.moveTo(12.5, 9)
            ctx.lineTo(12.5, 7)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4.5, 9.5)
            ctx.quadraticCurveTo(4, 14.5, 8.5, 14.5)
            ctx.quadraticCurveTo(13, 14.5, 12.8, 9)
            ctx.stroke()
            break

        case "zoom":
            ctx.beginPath()
            ctx.arc(7, 7, 4.6, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(10.5, 10.5)
            ctx.lineTo(14.5, 14.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4.8, 7)
            ctx.lineTo(9.2, 7)
            ctx.moveTo(7, 4.8)
            ctx.lineTo(7, 9.2)
            ctx.stroke()
            break

        case "select-rect":
            ctx.beginPath()
            ctx.rect(2.5, 3.5, 11, 9)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(2.5, 3.5)
            ctx.lineTo(4.5, 3.5)
            ctx.moveTo(11.5, 3.5)
            ctx.lineTo(13.5, 3.5)
            ctx.stroke()
            break

        case "move":
            ctx.beginPath()
            ctx.moveTo(8, 2)
            ctx.lineTo(8, 14)
            ctx.moveTo(2, 8)
            ctx.lineTo(14, 8)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(8, 2)
            ctx.lineTo(6, 4.2)
            ctx.moveTo(8, 2)
            ctx.lineTo(10, 4.2)
            ctx.moveTo(8, 14)
            ctx.lineTo(6, 11.8)
            ctx.moveTo(8, 14)
            ctx.lineTo(10, 11.8)
            ctx.moveTo(2, 8)
            ctx.lineTo(4.2, 6)
            ctx.moveTo(2, 8)
            ctx.lineTo(4.2, 10)
            ctx.moveTo(14, 8)
            ctx.lineTo(11.8, 6)
            ctx.moveTo(14, 8)
            ctx.lineTo(11.8, 10)
            ctx.stroke()
            break

        case "file-new":
            ctx.beginPath()
            ctx.moveTo(4, 1.5)
            ctx.lineTo(9.5, 1.5)
            ctx.lineTo(12.5, 4.5)
            ctx.lineTo(12.5, 14.5)
            ctx.lineTo(4, 14.5)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(9.5, 1.5)
            ctx.lineTo(9.5, 4.5)
            ctx.lineTo(12.5, 4.5)
            ctx.stroke()
            break

        case "file-open":
            ctx.beginPath()
            ctx.moveTo(2, 12.5)
            ctx.lineTo(2, 3.5)
            ctx.lineTo(6.5, 3.5)
            ctx.lineTo(8, 5.5)
            ctx.lineTo(13.5, 5.5)
            ctx.lineTo(13.5, 12.5)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(2, 7.5)
            ctx.lineTo(13.5, 7.5)
            ctx.stroke()
            break

        case "file-save":
            ctx.beginPath()
            ctx.rect(2.5, 2.5, 11, 11)
            ctx.stroke()
            ctx.beginPath()
            ctx.rect(5.5, 2.5, 5, 4)
            ctx.stroke()
            ctx.beginPath()
            ctx.rect(5, 9.5, 6, 4)
            ctx.stroke()
            break

        case "export":
            ctx.beginPath()
            ctx.moveTo(8, 10.5)
            ctx.lineTo(8, 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(5, 5)
            ctx.lineTo(8, 2)
            ctx.lineTo(11, 5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(3, 9)
            ctx.lineTo(3, 14)
            ctx.lineTo(13, 14)
            ctx.lineTo(13, 9)
            ctx.stroke()
            break

        case "undo":
            ctx.beginPath()
            ctx.moveTo(5.5, 5)
            ctx.lineTo(2.5, 8)
            ctx.lineTo(5.5, 11)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(2.5, 8)
            ctx.lineTo(9.5, 8)
            ctx.quadraticCurveTo(13.5, 8, 13.5, 11.5)
            ctx.stroke()
            break

        case "redo":
            ctx.beginPath()
            ctx.moveTo(10.5, 5)
            ctx.lineTo(13.5, 8)
            ctx.lineTo(10.5, 11)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(13.5, 8)
            ctx.lineTo(6.5, 8)
            ctx.quadraticCurveTo(2.5, 8, 2.5, 11.5)
            ctx.stroke()
            break

        case "trash":
            ctx.beginPath()
            ctx.moveTo(3, 4.5)
            ctx.lineTo(13, 4.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(6, 4.5)
            ctx.lineTo(6, 2.5)
            ctx.lineTo(10, 2.5)
            ctx.lineTo(10, 4.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4.5, 4.5)
            ctx.lineTo(5.2, 14)
            ctx.lineTo(10.8, 14)
            ctx.lineTo(11.5, 4.5)
            ctx.stroke()
            break

        case "plus":
            ctx.beginPath()
            ctx.moveTo(8, 3.5)
            ctx.lineTo(8, 12.5)
            ctx.moveTo(3.5, 8)
            ctx.lineTo(12.5, 8)
            ctx.stroke()
            break

        case "duplicate":
            ctx.beginPath()
            ctx.rect(5.5, 5.5, 8, 8)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(2.5, 10.5)
            ctx.lineTo(2.5, 2.5)
            ctx.lineTo(10.5, 2.5)
            ctx.stroke()
            break

        case "arrow-up":
            ctx.beginPath()
            ctx.moveTo(8, 12.5)
            ctx.lineTo(8, 3.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4.5, 7)
            ctx.lineTo(8, 3.5)
            ctx.lineTo(11.5, 7)
            ctx.stroke()
            break

        case "arrow-down":
            ctx.beginPath()
            ctx.moveTo(8, 3.5)
            ctx.lineTo(8, 12.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4.5, 9)
            ctx.lineTo(8, 12.5)
            ctx.lineTo(11.5, 9)
            ctx.stroke()
            break

        case "close":
            ctx.beginPath()
            ctx.moveTo(4, 4)
            ctx.lineTo(12, 12)
            ctx.moveTo(12, 4)
            ctx.lineTo(4, 12)
            ctx.stroke()
            break

        case "eye":
            ctx.beginPath()
            ctx.moveTo(1.5, 8)
            ctx.quadraticCurveTo(8, 2.5, 14.5, 8)
            ctx.quadraticCurveTo(8, 13.5, 1.5, 8)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(8, 8, 2, 0, Math.PI * 2)
            ctx.fill()
            break

        case "eye-off":
            ctx.beginPath()
            ctx.moveTo(1.5, 8)
            ctx.quadraticCurveTo(8, 3.5, 14.5, 8)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(3, 13)
            ctx.lineTo(13, 3)
            ctx.stroke()
            break

        case "lock":
            ctx.beginPath()
            ctx.rect(3.5, 7, 9, 6.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(5.5, 7)
            ctx.lineTo(5.5, 5)
            ctx.quadraticCurveTo(8, 2.5, 10.5, 5)
            ctx.lineTo(10.5, 7)
            ctx.stroke()
            break

        case "unlock":
            ctx.beginPath()
            ctx.rect(3.5, 7, 9, 6.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(5.5, 7)
            ctx.lineTo(5.5, 5)
            ctx.quadraticCurveTo(8, 2.5, 10.5, 5)
            ctx.stroke()
            break

        case "lasso":
            // freehand loop with a tail
            ctx.beginPath()
            ctx.moveTo(8, 3.5)
            ctx.quadraticCurveTo(13.5, 3.5, 13.5, 8)
            ctx.quadraticCurveTo(13.5, 12.5, 8, 12.5)
            ctx.quadraticCurveTo(2.5, 12.5, 2.5, 8)
            ctx.quadraticCurveTo(2.5, 5.5, 5, 4.4)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(5, 4.4)
            ctx.lineTo(5.5, 7)
            ctx.stroke()
            break

        case "wand":
            // magic wand over a sparkle
            ctx.beginPath()
            ctx.moveTo(3.5, 13)
            ctx.lineTo(10, 6.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(9, 5.5)
            ctx.lineTo(12.5, 2)
            ctx.lineTo(14, 3.5)
            ctx.lineTo(10.5, 7)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4, 3.5)
            ctx.lineTo(4, 6.5)
            ctx.moveTo(2.5, 5)
            ctx.lineTo(5.5, 5)
            ctx.moveTo(11.5, 10.5)
            ctx.lineTo(11.5, 13)
            ctx.moveTo(10.2, 11.7)
            ctx.lineTo(12.7, 11.7)
            ctx.stroke()
            break

        case "mirror-h":
            ctx.beginPath()
            ctx.moveTo(8, 2)
            ctx.lineTo(8, 14)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(6, 4.5)
            ctx.lineTo(2.5, 8)
            ctx.lineTo(6, 11.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(10, 4.5)
            ctx.lineTo(13.5, 8)
            ctx.lineTo(10, 11.5)
            ctx.stroke()
            break

        case "mirror-v":
            ctx.beginPath()
            ctx.moveTo(2, 8)
            ctx.lineTo(14, 8)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4.5, 6)
            ctx.lineTo(8, 2.5)
            ctx.lineTo(11.5, 6)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(4.5, 10)
            ctx.lineTo(8, 13.5)
            ctx.lineTo(11.5, 10)
            ctx.stroke()
            break

        case "invert":
            ctx.beginPath()
            ctx.arc(8, 8, 5.5, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(8, 8, 5.5, -Math.PI * 0.5, Math.PI * 0.5)
            ctx.fill()
            break

        case "line":
            ctx.beginPath()
            ctx.moveTo(2.5, 13.5)
            ctx.lineTo(13.5, 2.5)
            ctx.stroke()
            break

        case "ellipse":
            ctx.beginPath()
            ctx.ellipse(8, 8, 5.5, 4, 0, 0, Math.PI * 2)
            ctx.stroke()
            break

        case "gradient":
            for (var gi = 0; gi < 4; ++gi) {
                ctx.beginPath()
                ctx.rect(2.5 + gi * 3, 3.5, 2, 9)
                ctx.globalAlpha = 1 - gi * 0.25
                ctx.fill()
            }
            ctx.globalAlpha = 1
            break

        case "polygon":
            ctx.beginPath()
            ctx.moveTo(8, 2.5)
            ctx.lineTo(14, 7)
            ctx.lineTo(11.5, 14)
            ctx.lineTo(4.5, 14)
            ctx.lineTo(2, 7)
            ctx.closePath()
            ctx.stroke()
            break

        case "polyline":
            ctx.beginPath()
            ctx.moveTo(2.5, 11)
            ctx.lineTo(6, 5.5)
            ctx.lineTo(9.5, 10)
            ctx.lineTo(13.5, 4)
            ctx.stroke()
            break

        case "bezier":
            ctx.beginPath()
            ctx.moveTo(2, 12)
            ctx.bezierCurveTo(5, 2, 11, 14, 14, 4)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(2, 12, 1.6, 0, Math.PI * 2)
            ctx.fill()
            ctx.beginPath()
            ctx.arc(14, 4, 1.6, 0, Math.PI * 2)
            ctx.fill()
            break

        case "path":
            ctx.beginPath()
            ctx.moveTo(2.5, 13)
            ctx.quadraticCurveTo(5, 3, 8, 8)
            ctx.quadraticCurveTo(11, 13, 13.5, 3.5)
            ctx.stroke()
            break

        case "arc":
            ctx.beginPath()
            ctx.arc(8, 10, 5.5, Math.PI, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(2.5, 10)
            ctx.lineTo(13.5, 10)
            ctx.stroke()
            break

        case "multibrush":
            ctx.beginPath()
            ctx.moveTo(3, 13)
            ctx.lineTo(8, 3)
            ctx.moveTo(8, 13)
            ctx.lineTo(13, 3)
            ctx.moveTo(5.5, 8)
            ctx.lineTo(10.5, 8)
            ctx.stroke()
            break

        case "dynamic":
            ctx.beginPath()
            ctx.moveTo(2.5, 10)
            ctx.quadraticCurveTo(6, 2, 9, 9)
            ctx.quadraticCurveTo(11, 13, 13.5, 5)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(9, 9, 1.8, 0, Math.PI * 2)
            ctx.fill()
            break

        case "similar":
            ctx.beginPath()
            ctx.rect(2.5, 2.5, 6, 6)
            ctx.stroke()
            ctx.beginPath()
            ctx.rect(7.5, 7.5, 6, 6)
            ctx.stroke()
            break

        case "magnetic":
            ctx.beginPath()
            ctx.moveTo(3, 13)
            ctx.quadraticCurveTo(4, 4, 8, 4.5)
            ctx.quadraticCurveTo(12.5, 5, 13, 12)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(3, 13)
            ctx.lineTo(3.5, 9)
            ctx.moveTo(13, 12)
            ctx.lineTo(12.5, 8)
            ctx.stroke()
            break

        case "outline":
            ctx.beginPath()
            ctx.moveTo(8, 2)
            ctx.lineTo(8, 6)
            ctx.moveTo(8, 10)
            ctx.lineTo(8, 14)
            ctx.moveTo(2, 8)
            ctx.lineTo(6, 8)
            ctx.moveTo(10, 8)
            ctx.lineTo(14, 8)
            ctx.stroke()
            break

        case "transform":
            ctx.beginPath()
            ctx.rect(3.5, 3.5, 9, 9)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(3.5, 3.5)
            ctx.lineTo(6, 3.5)
            ctx.moveTo(3.5, 3.5)
            ctx.lineTo(3.5, 6)
            ctx.moveTo(12.5, 12.5)
            ctx.lineTo(10, 12.5)
            ctx.moveTo(12.5, 12.5)
            ctx.lineTo(12.5, 10)
            ctx.stroke()
            break

        case "crop":
            ctx.beginPath()
            ctx.moveTo(4.5, 1.5)
            ctx.lineTo(4.5, 11.5)
            ctx.lineTo(14.5, 11.5)
            ctx.moveTo(1.5, 4.5)
            ctx.lineTo(11.5, 4.5)
            ctx.lineTo(11.5, 14.5)
            ctx.stroke()
            break

        case "pattern":
            ctx.beginPath()
            ctx.rect(2.5, 2.5, 11, 11)
            ctx.stroke()
            for (var pi = 0; pi < 3; ++pi) {
                ctx.beginPath()
                ctx.arc(5 + pi * 3, 6, 1.1, 0, Math.PI * 2)
                ctx.fill()
                ctx.beginPath()
                ctx.arc(6.5 + pi * 3, 10, 1.1, 0, Math.PI * 2)
                ctx.fill()
            }
            break

        case "text":
            ctx.beginPath()
            ctx.moveTo(3, 3.5)
            ctx.lineTo(13, 3.5)
            ctx.moveTo(8, 3.5)
            ctx.lineTo(8, 13)
            ctx.stroke()
            break

        case "chevron-down":
            ctx.beginPath()
            ctx.moveTo(4.5, 6.5)
            ctx.lineTo(8, 10)
            ctx.lineTo(11.5, 6.5)
            ctx.stroke()
            break

        case "chevron-up":
            ctx.beginPath()
            ctx.moveTo(4.5, 9.5)
            ctx.lineTo(8, 6)
            ctx.lineTo(11.5, 9.5)
            ctx.stroke()
            break

        case "stepper-up":
            ctx.beginPath()
            ctx.moveTo(4, 9.5)
            ctx.lineTo(8, 5.5)
            ctx.lineTo(12, 9.5)
            ctx.closePath()
            ctx.fill()
            break

        case "stepper-down":
            ctx.beginPath()
            ctx.moveTo(4, 6.5)
            ctx.lineTo(8, 10.5)
            ctx.lineTo(12, 6.5)
            ctx.closePath()
            ctx.fill()
            break

        case "search":
            ctx.beginPath()
            ctx.arc(7, 7, 4.2, 0, Math.PI * 2)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(10, 10)
            ctx.lineTo(14, 14)
            ctx.stroke()
            break

        case "transparency":
            // checkerboard swatch used behind transparency
            ctx.strokeRect(2.5, 2.5, 11, 11)
            ctx.fillRect(2.5, 2.5, 5.5, 5.5)
            ctx.fillRect(8, 8, 5.5, 5.5)
            break

        case "swatch":
            ctx.beginPath()
            ctx.rect(3.5, 3.5, 9, 9)
            ctx.fill()
            break

        case "link":
            ctx.beginPath()
            ctx.arc(6, 8, 2.5, Math.PI * 0.5, Math.PI * 1.5)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(10, 8, 2.5, -Math.PI * 0.5, Math.PI * 0.5)
            ctx.stroke()
            break
        }

        ctx.restore()
    }
}
