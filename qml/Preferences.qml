import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion
import QtQuick.Layouts

ApplicationWindow {
    width: 540
    minimumWidth: 270
    height: 540
    minimumHeight: 270
    title: qsTr("Preferences")
    flags: Qt.Dialog | Qt.WindowTitleHint

    ColumnLayout {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 10
        spacing: 10

        GroupBox {
            title: qsTr("General")
            Layout.fillWidth: true

            ColumnLayout {
                spacing: 5
                anchors.fill: parent

                Label {
                    text: qsTr("AppImage Default Location")
                }
                RowLayout {
                    spacing: 5
                    Layout.fillWidth: true

                    TextField {
                        placeholderText: qsTr("AppImage default location")
                        text: AppSettings.appImageDefaultLocation
                        readOnly: true
                        Layout.fillWidth: true
                    }
                    IconButton {
                        text: "\uf07c"
                        width: 65
                        Layout.preferredWidth: 65
                    }
                }
            }
        }
    }
}
