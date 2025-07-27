import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    width: 540
    minimumWidth: 270
    height: 540
    minimumHeight: 270
    title: qsTr("Preferences")
    flags: Qt.Dialog | Qt.WindowTitleHint

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 10

        ColumnLayout {
            width: scrollView.width
            spacing: 10

            GroupBox {
                title: qsTr("General")
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    Label {
                        text: qsTr("AppImage default location")
                    }
                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        TextField {
                            placeholderText: qsTr("AppImage default location")
                            text: Qt.resolvedUrl(
                                      AppSettings.appImageDefaultLocation).toString(
                                      ).replace("file://", "")
                            readOnly: true
                            Layout.fillWidth: true
                        }
                        IconButton {
                            text: "\uf0c5"
                            width: 55
                            Layout.preferredWidth: 55
                            onClicked: {
                                ClipboardManager.copyToClipboard(
                                            Qt.resolvedUrl(
                                                AppSettings.appImageDefaultLocation).toString(
                                                ).replace("file://", ""))
                            }
                        }
                        IconButton {
                            text: "\uf07c"
                            width: 55
                            Layout.preferredWidth: 55
                            onClicked: folderDialog.open()
                        }

                        FolderDialog {
                            id: folderDialog
                            title: qsTr("AppImage Default Location")
                            currentFolder: Qt.resolvedUrl(
                                               AppSettings.appImageDefaultLocation.substring(
                                                   0,
                                                   AppSettings.appImageDefaultLocation.lastIndexOf(
                                                       "/")))
                            onAccepted: {
                                AppSettings.appImageDefaultLocation = folderDialog.selectedFolder
                            }
                        }
                    }

                    Item {
                        Layout.preferredHeight: 10
                    }

                    Label {
                        text: qsTr("Move/Copy appimages")
                    }
                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        ComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("Move"), qsTr("Copy")]
                            currentIndex: AppSettings.appImageFileOperation
                            onCurrentIndexChanged: AppSettings.appImageFileOperation = currentIndex
                        }
                    }
                }
            }
        }
    }
}
