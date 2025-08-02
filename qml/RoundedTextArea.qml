import QtQuick
import QtQuick.Controls

TextArea {
    property bool singleLine: false

    Keys.onReturnPressed: event => {
        if(singleLine)
            event.accepted = true;
    }

    background: Rectangle {
        radius: 4
        border.width: 1
        border.color: activeFocus ? palette.highlight : palette.mid
        color: palette.base
    }
}
