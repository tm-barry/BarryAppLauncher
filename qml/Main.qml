import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ApplicationWindow {
    id: mainWindow

    width: 640
    height: 640
    visible: true

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&Open AppImage...")
                onTriggered: fileDialog.open()
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

    FileDialog {
        id: fileDialog
        title: qsTr("Open AppImage")
        currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)
        nameFilters: ["AppImage Files (*.AppImage *.appimage)"]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            console.log(fileDialog.selectedFile);
            console.log(AppImageManager.isAppImageType2(fileDialog.selectedFile));
            console.log(AppImageManager.findDesktopFileForExecutable(fileDialog.selectedFile));
        }
    }

    Loader {
        id: pageContent
        anchors.fill: parent
    }
}
