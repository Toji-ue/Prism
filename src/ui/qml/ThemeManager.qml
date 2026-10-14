pragma Singleton
import QtQuick

/// Runtime theme switching. One shared instance for every QML file:
///     ThemeManager.themeColor("window"), .themeColor("text"), ...
/// Bindings re-evaluate on switch because themeColor() reads
/// activeThemeName inside the binding evaluation.
///
/// Shared semantic tokens for the studio workspace. The canvas remains the
/// brightest surface in light mode; coral marks actions, not whole panels.
Item {
    id: themeManager

    property string activeThemeName: "dark"

    property var darkColors: ({
        "window": "#1c2025",
        "windowAlt": "#232930",
        "surface": "#20262c",
        "base": "#20262c",
        "panel": "#20262c",
        "panelRaised": "#2b3138",
        "panelSunken": "#252b32",
        "canvasBackdrop": "#818a94",
        "text": "#edf0f2",
        "textDim": "#aeb7bf",
        "border": "#3b444d",
        "highlight": "#ce4238",
        "highlightText": "#ffffff",
        "selection": "#353d45",
        "selectionText": "#ffffff",
        "buttonHover": "#303840",
        "sliderGroove": "#303840",
        "popup": "#292f36",
        "popupBorder": "#46505a",
        "disabledText": "#818b95",
        "danger": "#ff8b80",
        "cursorRing": "#aeb7bf",
        "cursorRingInner": "rgba(28, 32, 37, 0.45)",
        "radius": 12,
        "radiusSmall": 8,
        "radiusPanel": 12,
        "radiusControl": 8,
        "radiusChip": 6,
        "rowHeight": 26,
        "dockerHeaderHeight": 40,
        "toolboxWidth": 52,
        "dockerWidth": 292,
        "fontBody": 12,
        "fontCaption": 10,
        "fontTitle": 15,
        "controlHeight": 32
    })

    property var lightColors: ({
        "window": "#f3f4f5",
        "windowAlt": "#e9ecee",
        "surface": "#ffffff",
        "base": "#ffffff",
        "panel": "#ffffff",
        "panelRaised": "#f7f8f8",
        "panelSunken": "#edf0f2",
        "canvasBackdrop": "#c4cbd0",
        "text": "#20262c",
        "textDim": "#5e6973",
        "border": "#d5dce0",
        "highlight": "#ce4238",
        "highlightText": "#ffffff",
        "selection": "#e8edef",
        "selectionText": "#20262c",
        "buttonHover": "#eef1f3",
        "sliderGroove": "#dce2e5",
        "popup": "#ffffff",
        "popupBorder": "#cdd5da",
        "disabledText": "#8a939c",
        "danger": "#b8352b",
        "cursorRing": "#5e6973",
        "cursorRingInner": "rgba(255, 255, 255, 0.35)",
        "radius": 12,
        "radiusSmall": 8,
        "radiusPanel": 12,
        "radiusControl": 8,
        "radiusChip": 6,
        "rowHeight": 26,
        "dockerHeaderHeight": 40,
        "toolboxWidth": 52,
        "dockerWidth": 292,
        "fontBody": 12,
        "fontCaption": 10,
        "fontTitle": 15,
        "controlHeight": 32
    })

    function themeColor(name) {
        const colors = activeThemeName === "light" ? lightColors : darkColors
        return colors[name]
    }

    function setTheme(themeName) {
        if (themeName !== "light" && themeName !== "dark") {
            console.error("Invalid theme name:", themeName)
            return
        }
        if (activeThemeName === themeName)
            return
        activeThemeName = themeName
        emitSwitched()
    }

    function toggleTheme() {
        activeThemeName = activeThemeName === "light" ? "dark" : "light"
        emitSwitched()
    }

    /// Emitted only on an actual switch, so the fade overlay does not run
    /// at startup when the persisted theme is applied.
    signal themeSwitched()
    function emitSwitched() { themeSwitched() }
}
