import QtQuick
import QtQuick.Controls

Button {
    id: button
    property color backgroundColor: "steelblue"
    property color hoverColor: Qt.lighter(backgroundColor, 1.2)
    property color pressedColor: Qt.darker(backgroundColor, 1.3)
    property color textColor: "white"

    background: Rectangle {
        radius: 4
        border.color: Qt.darker(button.backgroundColor, 1.6)
        border.width: 1

        color: button.down
                 ? button.pressedColor
                 : button.hovered
                     ? button.hoverColor
                     : button.backgroundColor

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }
    }

    contentItem: Text {
        text: button.text
        color: button.textColor
        font: button.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        anchors.fill: parent
    }
}
