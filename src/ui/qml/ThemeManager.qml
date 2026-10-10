pragma Singleton
import QtQuick

/// Runtime theme switching. One shared instance for every QML file:
///     ThemeManager.themeColor("window"), .themeColor("text"), ...
/// Bindings re-evaluate on switch because themeColor() reads
/// activeThemeName inside the binding evaluation.
///
/// Dark values are the original Theme.js palette, verbatim (tints precomputed
/// from mix(#1f1f1f, #ffffff, t)). Light is "warm paper" - see the design
/// notes in the repo docs.
Item {
    id: themeManager

    property string activeThemeName: "dark"

    property var darkColors: ({
        "window": "#1f1f1f",
        "windowAlt": "#282828",
        "surface": "#1f1f1f",
        "base": "#1f1f1f",
        "panel": "#1f1f1f",
        "panelRaised": "#333333",
        "panelSunken": "#232323",
        "canvasBackdrop": "#9a9a9a",
        "text": "#ffffff",
        "textDim": "#aaaaaa",
        "border": "#474747",
        "highlight": "#ff5a4e",
        "highlightText": "#1f1f1f",
        "buttonHover": "rgba(255, 90, 78, 0.14)",
        "sliderGroove": "#2c2c2c",
        "popup": "#2f2f2f",
        "popupBorder": "#505050",
        "disabledText": "#6b6b6b",
        "danger": "#ffffff",
        "cursorRing": "#9a9a9a",
        "cursorRingInner": "rgba(31, 31, 31, 0.35)",
        "radius": 7,
        "radiusSmall": 5,
        "rowHeight": 22,
        "dockerHeaderHeight": 24,
        "toolboxWidth": 60,
        "dockerWidth": 212
    })

    property var lightColors: ({
        "window": "#f5f2ec",
        "windowAlt": "#ece7de",
        "surface": "#ffffff",
        "base": "#ffffff",
        "panel": "#ffffff",
        "panelRaised": "#f8f5f0",
        "panelSunken": "#efeae2",
        "canvasBackdrop": "#c9c2b6",
        "text": "#241f1a",
        "textDim": "#8a7f72",
        "border": "#ddd5c9",
        "highlight": "#ff5a4e",
        "highlightText": "#ffffff",
        "buttonHover": "rgba(255, 90, 78, 0.10)",
        "sliderGroove": "#e5ddd1",
        "popup": "#ffffff",
        "popupBorder": "#e5ddd1",
        "disabledText": "#b3a99a",
        "danger": "#241f1a",
        "cursorRing": "#8a8378",
        "cursorRingInner": "rgba(255, 255, 255, 0.35)",
        "radius": 7,
        "radiusSmall": 5,
        "rowHeight": 22,
        "dockerHeaderHeight": 24,
        "toolboxWidth": 60,
        "dockerWidth": 212
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
