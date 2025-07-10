import QtQuick
import QtQuick.Controls.Fusion

ApplicationWindow {
    id: mainWindow

    palette: AppSettings.theme === "dark" ? palette.dark : palette.light

    width: 640
    height: 640
    visible: true

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&Open AppImage...")
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Preferences")
                onTriggered: {
                    var preferences = Qt.createComponent(
                                "Preferences.qml").createObject()
                    preferences.transientParent = mainWindow
                    preferences.show()
                }
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Quit")
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: qsTr("&Help")
            Action {
                text: qsTr("&About")
            }
        }
    }

    Loader {
        id: pageContent
        anchors.fill: parent
    }
}
