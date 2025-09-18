import QtQuick
import QtQuick.Controls

Button {
    id: button

    property color backgroundColor: "steelblue"
    property color hoverColor: Qt.lighter(backgroundColor, 1.2)
    property color pressedColor: Qt.darker(backgroundColor, 1.3)
    property color disabledColor: Qt.darker(backgroundColor, 1.4)

    property color textColor: "white"
    property color disabledTextColor: "#AAAAAA"

    background: Rectangle {
        radius: 2
        border.color: Qt.darker(button.backgroundColor, 2)
        border.width: 1

        color: !button.enabled ? button.disabledColor : button.down ? button.pressedColor : button.hovered ? button.hoverColor : button.backgroundColor

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }

        opacity: button.enabled ? 1.0 : 0.6
    }

    contentItem: Text {
        text: button.text
        color: button.enabled ? button.textColor : button.disabledTextColor
        font: button.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        anchors.fill: parent
    }
}
