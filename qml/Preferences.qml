import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    width: 540
    minimumWidth: 270
    height: 270
    minimumHeight: 270
    title: qsTr("Preferences")
    flags: Qt.Dialog | Qt.WindowTitleHint

    QtObject {
        id: utils

        function getParentFolder(url) {
            const path = url.toString()
            const parent = path.substring(0, path.lastIndexOf("/"))
            return parent
        }
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 10

        ColumnLayout {
            width: scrollView.width
            spacing: 10

            Label {
                text: qsTr("General")
                font.bold: true
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }
            GroupBox {
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    Label {
                        text: qsTr("AppImage default location")
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        TextArea {
                            placeholderText: qsTr("AppImage default location")
                            text: Qt.resolvedUrl(
                                      SettingsManager.appImageDefaultLocation).toString(
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
                                                SettingsManager.appImageDefaultLocation).toString(
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
                            currentFolder: utils.getParentFolder(SettingsManager.appImageDefaultLocation)
                            onAccepted: {
                                SettingsManager.appImageDefaultLocation
                                        = folderDialog.selectedFolder
                            }
                        }
                    }

                    Item {
                        Layout.preferredHeight: 10
                    }

                    Label {
                        text: qsTr("Move/Copy appimages")
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        ComboBox {
                            Layout.fillWidth: true
                            model: [qsTr("Move"), qsTr("Copy")]
                            currentIndex: SettingsManager.appImageFileOperation
                            onCurrentIndexChanged: SettingsManager.appImageFileOperation
                                                   = currentIndex
                        }
                    }
                }
            }
        }
    }
}
