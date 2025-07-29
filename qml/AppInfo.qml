import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    QtObject {
        id: utils

        function folderFromFilePath(path) {
            var lastSlash = path.lastIndexOf("/")
            if (lastSlash === -1)
                return ""
            return path.substring(0, lastSlash)
        }

        function openPathFolder(path) {
            var folderPath = folderFromFilePath(path)
            var urlPath = Qt.resolvedUrl("file://" + folderPath)
            Qt.openUrlExternally(urlPath)
        }
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 10
        anchors.topMargin: -20

        ColumnLayout {
            width: scrollView.width
            spacing: 5

            Image {
                source: AppImageManager.appImageMetadata?.icon
                width: 96
                height: 96
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectFit
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: AppImageManager.appImageMetadata?.name
                font.pixelSize: 24
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: AppImageManager.appImageMetadata?.version
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: AppImageManager.appImageMetadata?.comment
                Layout.alignment: Qt.AlignHCenter
            }

            Item {
                Layout.preferredHeight: 10
            }

            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter

                Timer {
                    id: launchTimer
                    interval: 2000
                    repeat: false
                }

                ColorButton {
                    text: launchTimer.running ? qsTr("Launching") : qsTr(
                                                    "Launch")
                    backgroundColor: "#4E7A6A"
                    font.pixelSize: 16
                    Layout.preferredWidth: 100
                    enabled: !launchTimer.running
                    onClicked: {
                        if (!AppImageManager.appImageMetadata?.integrated) {
                            AppImageManager.launchAppImage(
                                        AppImageManager.appImageMetadata?.path)
                        } else {
                            AppImageManager.launchAppImage(
                                        AppImageManager.appImageMetadata?.desktopFilePath)
                        }
                        launchTimer.start()
                    }
                }

                ColorButton {
                    text: qsTr("Register")
                    font.pixelSize: 16
                    Layout.preferredWidth: 100
                    visible: AppImageManager.appImageMetadata?.integration
                             === AppImageMetadata.None
                    onClicked: AppImageManager.registerAppImage(
                                   AppImageManager.appImageMetadata?.path)
                }

                ColorButton {
                    text: qsTr("Unregister")
                    backgroundColor: "#C43D3D"
                    font.pixelSize: 16
                    Layout.preferredWidth: 100
                    visible: AppImageManager.appImageMetadata?.integration
                             === AppImageMetadata.Internal
                    onClicked: AppImageManager.unregisterAppImage(
                                   AppImageManager.appImageMetadata?.path)
                }
            }

            Label {
                text: qsTr("This AppImage appears to have been integrated by another application.")
                color: "#C43D3D"
                Layout.alignment: Qt.AlignHCenter
                visible: AppImageManager.appImageMetadata?.integration === AppImageMetadata.External
            }

            Item {
                Layout.preferredHeight: 10
            }

            Label {
                text: qsTr("General")
                font.bold: true
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }
            GroupBox {
                Layout.alignment: Qt.AlignHCenter
                Layout.minimumWidth: 460
                Layout.maximumWidth: 460

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    Label {
                        text: qsTr("Package type")
                        font.bold: true
                    }

                    Label {
                        text: qsTr("AppImage Type ") + AppImageManager.appImageMetadata?.type
                    }

                    Item {
                        Layout.preferredHeight: 10
                    }

                    Label {
                        text: qsTr("Path")
                        font.bold: true
                    }

                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        TextArea {
                            text: AppImageManager.appImageMetadata?.path
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
                                            AppImageManager.appImageMetadata?.path)
                            }
                        }
                        IconButton {
                            text: "\uf07c"
                            width: 55
                            Layout.preferredWidth: 55
                            onClicked: utils.openPathFolder(
                                           AppImageManager.appImageMetadata?.path)
                        }
                    }
                }
            }

            Item {
                Layout.preferredHeight: 10
            }

            Label {
                text: qsTr("Desktop Integration")
                font.bold: true
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }
            GroupBox {
                Layout.alignment: Qt.AlignHCenter
                Layout.minimumWidth: 460
                Layout.maximumWidth: 460

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    Label {
                        text: qsTr("Desktop Path")
                        font.bold: true
                    }

                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        TextArea {
                            text: AppImageManager.appImageMetadata?.desktopFilePath
                            placeholderText: qsTr("AppImage has not been integrated...")
                            enabled: AppImageManager.appImageMetadata?.desktopFilePath
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            Layout.fillWidth: true
                        }
                        IconButton {
                            text: "\uf0c5"
                            width: 55
                            Layout.preferredWidth: 55
                            enabled: AppImageManager.appImageMetadata?.desktopFilePath
                            onClicked: {
                                ClipboardManager.copyToClipboard(
                                            AppImageManager.appImageMetadata?.desktopFilePath)
                            }
                        }
                        IconButton {
                            text: "\uf07c"
                            width: 55
                            Layout.preferredWidth: 55
                            enabled: AppImageManager.appImageMetadata?.desktopFilePath
                            onClicked: utils.openPathFolder(
                                           AppImageManager.appImageMetadata?.desktopFilePath)
                        }
                    }

                    Item {
                        Layout.preferredHeight: 10
                    }

                    Label {
                        text: qsTr("Categories")
                        font.bold: true
                    }

                    Label {
                        text: AppImageManager.appImageMetadata?.categories
                    }
                }
            }
        }
    }
}
