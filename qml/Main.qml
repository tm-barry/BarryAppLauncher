import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: mainWindow

    width: 540
    minimumWidth: 480
    height: 680
    minimumHeight: 480
    visible: true

    Component.onCompleted: {
        if (FileArg) {
            AppImageManager.loadAppImageMetadata(FileArg)
        }
    }

    QtObject {
        id: modalManager

        property var aboutModal: null
        property var preferencesModal: null

        function openAboutModal() {
            if (modalManager.aboutModal == null) {
                modalManager.aboutModal = Qt.createComponent(
                            "About.qml").createObject(null)
                modalManager.aboutModal.transientParent = mainWindow

                modalManager.aboutModal.closing.connect(function () {
                    modalManager.aboutModal.destroy()
                    modalManager.aboutModal = null
                })
            }

            modalManager.aboutModal.show()
        }

        function openPreferencesModal() {
            if (modalManager.preferencesModal == null) {
                modalManager.preferencesModal = Qt.createComponent(
                            "Preferences.qml").createObject(null)
                modalManager.preferencesModal.transientParent = mainWindow

                modalManager.preferencesModal.closing.connect(function () {
                    modalManager.preferencesModal.destroy()
                    modalManager.preferencesModal = null
                })
            }

            modalManager.preferencesModal.show()
        }
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

        function onModalRequested(modal) {
            switch (modal) {
            case AppImageManager.Preferences:
                modalManager.openPreferencesModal()
                break
            case AppImageManager.OpenDialog:
                fileDialog.open()
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

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action {
                text: qsTr("&Open AppImage...")
                onTriggered: fileDialog.open()
                enabled: !AppImageManager.loadingAppImage
                         && !AppImageManager.updating
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Preferences")
                onTriggered: modalManager.openPreferencesModal()
                enabled: !AppImageManager.updating
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
                onTriggered: modalManager.openAboutModal()
            }
        }
    }

    header: headerLoader.item

    Loader {
        id: headerLoader
        sourceComponent: AppImageManager.state
                         === AppImageManager.AppInfo ? appInfoHeaderComponent : null
    }

    Component {
        id: appInfoHeaderComponent

        ToolBar {
            background: null

            RowLayout {
                anchors.fill: parent

                ToolButton {
                    onClicked: AppImageManager.state = AppImageManager.AppList
                    Layout.alignment: Qt.AlignLeft

                    contentItem: Row {
                        anchors.centerIn: parent
                        spacing: 10

                        Label {
                            text: "\uf104"

                            FontLoader {
                                id: fontAwesome
                                source: "/assets/fonts/fa-6-solid-900.otf"
                            }
                        }
                        Label {
                            text: qsTr("App List")
                        }
                    }
                }
            }
        }
    }

    DropArea {
        anchors.fill: parent

        onDropped: function (dropEvent) {
            for (var i = 0; i < dropEvent.urls.length; i++) {
                var url = dropEvent.urls[i];
                var localPath = url.toString().replace("file://", "");
                if (localPath.toLowerCase().endsWith(".appimage")) {
                    Qt.callLater(function () {
                        AppImageManager.loadAppImageMetadata(localPath);
                    });
                }
            }
        }
    }

    Loader {
        id: pageContent
        anchors.fill: parent
        source: "AppList.qml"
    }

    footer: Rectangle {
        ProgressBar {
            id: busyIndicator
            indeterminate: true
            anchors.centerIn: parent
            width: parent.width
            visible: AppImageManager.loadingAppImage
                     || AppImageManager.loadingAppImageList
                     || AppImageManager.updating
        }
    }
}
