import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ApplicationWindow {
    id: mainWindow

    width: 640
    height: 640
    visible: true

    QtObject {
        id: modalManager
        property var preferencesModal: null
    }

    Connections {
        target: ErrorManager
        function onMessageOccurred(message, messageType) {
            errorDialog.text = message
            switch (messageType) {
            case ErrorManager.Error:
                errorDialog.title = qsTr("Error")
                break
            case ErrorManager.Warning:
                errorDialog.title = qsTr("Warning")
                break
            default:
                errorDialog.title = qsTr("Message")
                break
            }
            errorDialog.open()
        }
    }

    Connections {
        target: AppImageManager
        function onBusyChanged(newValue) {
            busyIndicator.running = newValue
        }
        function onStateChanged(newValue) {
            switch (newValue) {
            case AppImageManager.AppInfo:
                pageContent.source = "AppInfo.qml"
                break
            default:
                pageContent.source = "AppList.qml"
                break
            }
        }
    }

    MessageDialog {
        id: errorDialog
        title: ""
        text: ""
        buttons: MessageDialog.Ok
    }

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
                    if (modalManager.preferencesModal == null) {
                        modalManager.preferencesModal = Qt.createComponent(
                                    "Preferences.qml").createObject(null)
                        modalManager.preferencesModal.transientParent = mainWindow

                        modalManager.preferencesModal.closing.connect(
                                    function () {
                                        modalManager.preferencesModal.destroy()
                                        modalManager.preferencesModal = null
                                    })
                    }

                    modalManager.preferencesModal.show()
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
        currentFolder: StandardPaths.writableLocation(
                           StandardPaths.HomeLocation)
        nameFilters: ["AppImage Files (*.AppImage *.appimage)"]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            AppImageManager.loadAppImageMetadata(fileDialog.selectedFile)
        }
    }

    Loader {
        id: pageContent
        anchors.fill: parent
        source: "AppList.qml"
    }

    BusyIndicator {
        id: busyIndicator
        anchors.centerIn: parent
        running: false
    }
}
