.pragma library

// Prism visual identity: a warm ink-on-paper look with one coral accent.
//
//   light = #ffffff   (foreground: text, icons)
//   dark  = #1f1f1f   (background: window, panels, canvas backdrop)
//   accent = #ff5a4e  (active tool, selection, sliders — the one hue)
//
// Every neutral shade is mixed from light/dark, and every "active" state uses
// accent, so the UI stays calm while reading as clearly NOT Krita (dark,
// angular, monochrome). Rounded corners replace the square, hairline look.
var light = "#ffffff"
var dark = "#1f1f1f"
var accent = "#ff5a4e"

// Corner radii, so every surface rounds the same way.
var radius = 7
var radiusSmall = 5

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
// Active/selected state is the accent hue with dark text, so "selected" reads
// instantly and gives the whole interface its signature colour.
var highlight = accent
var highlightText = dark
var buttonHover = fade(accent, 0.14)
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
