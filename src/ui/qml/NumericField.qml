import QtQuick
import QtQuick.Controls.Basic
import Prism

PTextField {
    id: control
    property real numberValue: 0
    property real from: 0
    property real to: 100
    property int decimals: 0
    property real stepSize: decimals > 0 ? 0.1 : 1
    signal committed(real value)
    horizontalAlignment: Text.AlignRight
    inputMethodHints: Qt.ImhFormattedNumbersOnly
    validator: DoubleValidator {
        bottom: control.from
        top: control.to
        decimals: control.decimals
        notation: DoubleValidator.StandardNotation
        locale: "C"
    }
    function restore() {
        text = Number(numberValue).toFixed(decimals);
        invalid = false;
    }
    function commit() {
        const n = Number(text);
        if (text.trim().length === 0 || !acceptableInput || !isFinite(n) || n < from || n > to) {
            invalid = true;
            return false;
        }
        invalid = false;
        committed(n);
        restore();
        return true;
    }
    function nudge(direction) {
        committed(Math.max(from, Math.min(to, numberValue + direction * stepSize)));
        restore();
    }
    Component.onCompleted: restore()
    onNumberValueChanged: if (!activeFocus)
        restore()
    onEditingFinished: commit()
    onTextEdited: invalid = false
    Keys.onEscapePressed: event => {
        restore();
        event.accepted = true;
    }
    Keys.onUpPressed: nudge(1)
    Keys.onDownPressed: nudge(-1)
}
