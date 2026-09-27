.pragma library

// Two-colour palette.
//
//   light = #ffffff   (foreground: text, icons, highlights)
//   dark  = #1f1f1f   (background: window, panels, canvas backdrop)
//
// Every other shade is mixed from those two, so the whole UI is literally two
// colours plus automatic tints. That constraint is deliberate: it keeps the
// interface calm, and it means adding a screen never means inventing a colour.
var light = "#ffffff"
var dark = "#1f1f1f"

/// Mix two #rrggbb colours; t = 0 returns a, t = 1 returns b.
function mix(a, b, t) {
    function channel(hex, i) { return parseInt(hex.substr(1 + i * 2, 2), 16) }
    var out = "#"
    for (var i = 0; i < 3; ++i) {
        var v = Math.round(channel(a, i) * (1 - t) + channel(b, i) * t)
        out += ("0" + v.toString(16)).slice(-2)
    }
    return out
}

/// A translucent version of a colour (used for overlays and dim text).
function fade(c, alpha) {
    return Qt.rgba(parseInt(c.substr(1, 2), 16) / 255,
                   parseInt(c.substr(3, 2), 16) / 255,
                   parseInt(c.substr(5, 2), 16) / 255,
                   alpha)
}

function tint(t) { return mix(dark, light, t) }

// ---- surfaces ----
var window = dark
var windowAlt = tint(0.04)
var base = dark
var panel = dark
var panelRaised = tint(0.09)
var panelSunken = tint(0.02)
var canvasBackdrop = tint(0.55)   // light grey desk behind the drawing area

// ---- lines and text ----
var border = tint(0.18)
var text = light
var textDim = tint(0.62)

// ---- interaction ----
// Selection is the foreground colour with inverted text, so "selected" reads
// instantly without a second hue.
var highlight = tint(0.88)
var highlightText = dark
var buttonHover = tint(0.11)
var sliderGroove = tint(0.06)
var danger = light

// ---- brush cursor (grey ring, per the drawing-tools convention) ----
var cursorRing = tint(0.55)
var cursorRingInner = fade(dark, 0.35)

// ---- dropdowns / menus ----
// The popup list is a touch darker than the panels so it reads as a layer
// above them, and disabled entries stay legible but obviously inert.
var popup = tint(0.07)
var popupBorder = tint(0.22)
var disabledText = tint(0.34)

// ---- metrics ----
var rowHeight = 22
var dockerHeaderHeight = 24
var toolboxWidth = 60
var dockerWidth = 212
