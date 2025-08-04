import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    width: 480
    minimumWidth: 270
    height: 380
    minimumHeight: 360
    title: qsTr("Preferences")
    flags: Qt.Dialog | Qt.WindowTitleHint

    onVisibleChanged: {
        terminalTextField.text = SettingsManager.terminal
    }

    Connections {
        target: SettingsManager
        function onTerminalChanged(newValue) {
            terminalTextField.text = newValue
        }
    }

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
            RoundedGroupBox {
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
                            wrapMode: TextEdit.Wrap
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
                            currentFolder: utils.getParentFolder(
                                               SettingsManager.appImageDefaultLocation)
                            onAccepted: {
                                SettingsManager.appImageDefaultLocation
                                        = folderDialog.selectedFolder
                                AppImageManager.loadAppImageList()
                            }
                        }
                    }

                    Rectangle {
                        height: 1
                        Layout.fillWidth: true
                        color: palette.mid
                        opacity: 0.6
                        Layout.topMargin: 10
                        Layout.bottomMargin: 5
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

                    Rectangle {
                        height: 1
                        Layout.fillWidth: true
                        color: palette.mid
                        opacity: 0.6
                        Layout.topMargin: 10
                        Layout.bottomMargin: 5
                    }

                    Label {
                        text: qsTr("Terminal Override")
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        RoundedTextArea {
                            id: terminalTextField
                            singleLine: true
                            placeholderText: qsTr("Detect default terminal...")
                            wrapMode: TextEdit.Wrap
                            Layout.fillWidth: true
                        }

                        IconButton {
                            text: "\uf0c7"
                            width: 55
                            Layout.preferredWidth: 55
                            enabled: SettingsManager.terminal !== terminalTextField.text
                            onClicked: {
                                if(!terminalTextField.textSettings || Manager.terminalExists(terminalTextField.text))
                                {
                                    SettingsManager.terminal = terminalTextField.text;
                                }
                                else
                                {
                                    ErrorManager.reportError(qsTr("Terminal does not found: ") + terminalTextField.text);
                                }
                            }
                        }
                    }

                    Rectangle {
                        height: 1
                        Layout.fillWidth: true
                        color: palette.mid
                        opacity: 0.6
                        Layout.topMargin: 10
                        Layout.bottomMargin: 5
                    }

                    Label {
                        text: qsTr("Register Self")
                        font.bold: true
                    }

                    Label {
                        text: qsTr("BarryAppLauncher can register itself into your system menu.")
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                        Layout.alignment: Qt.AlignCenter
                        Layout.fillWidth: true
                    }

                    ColorButton {
                        text: qsTr("Register")
                        Layout.preferredWidth: 100
                        onClicked: AppImageManager.registerSelf()
                        Layout.alignment: Qt.AlignCenter
                    }
                }
            }
        }
    }
}
