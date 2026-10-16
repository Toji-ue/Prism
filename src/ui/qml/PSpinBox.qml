import QtQuick
import Prism

// Value is inbound: editing emits a change without breaking the caller's binding.
NumericField {
    id: control
    property int value: 0
    signal valueModified(int v)
    implicitWidth: 84
    numberValue: value
    onCommitted: n => valueModified(Math.round(n))
}
