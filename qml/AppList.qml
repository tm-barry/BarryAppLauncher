import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    Component.onCompleted: {
        if (!AppImageManager.loadingAppImageList
                && AppImageManager.appImageList.count === 0) {
            AppImageManager.loadAppImageList()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        anchors.margins: 10

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.minimumWidth: 460
            Layout.maximumWidth: 460
            Layout.preferredWidth: 460

            Label {
                text: qsTr("Registered")
                font.pixelSize: 14
                font.bold: true
                Layout.alignment: Qt.AlignLeft
            }

            RowLayout {
                id: rowLayout
                Layout.alignment: Qt.AlignRight

                ColorButton {
                    id: checkForUpdatesBtn
                    text: "Check for Updates"
                    Layout.preferredWidth: 130
                    enabled: !AppImageManager.loadingAppImageList
                    visible: !AppImageManager.appImageList.hasAnyNewRelease
                    onClicked: {
                        AppImageManager.checkForAllUpdates()
                    }
                }

                ColorButton {
                    id: updateAllBtn
                    text: "Update All"
                    Layout.preferredWidth: 85
                    visible: AppImageManager.appImageList.hasAnyNewRelease
                    enabled: !AppImageManager.loadingAppImageList
                    onClicked: {
                        AppImageManager.updateAllAppImages();
                    }
                }

                IconButton {
                    text: "\uf021"
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: checkForUpdatesBtn.height || updateAllBtn.height
                    enabled: !AppImageManager.loadingAppImageList
                    onClicked: {
                        AppImageManager.loadAppImageList()
                    }
                }
            }
        }

        Item {
            Layout.preferredHeight: 50
            visible: !AppImageManager.loadingAppImageList
                     && AppImageManager.appImageList.count === 0
        }

        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            visible: !AppImageManager.loadingAppImageList
                     && AppImageManager.appImageList.count === 0
            Layout.minimumWidth: 460
            Layout.maximumWidth: 460
            Layout.preferredWidth: 460
            spacing: 20

            Label {
                text: qsTr("No registered appimages found. Change the default appimage location in preferences or open an appimage to register.")
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignCenter
            }

            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter

                Button {
                    text: qsTr("Preferences")
                    Layout.preferredWidth: 150
                    onClicked: AppImageManager.requestModal(
                                   AppImageManager.Preferences)
                }

                Button {
                    text: qsTr("Open AppImage")
                    Layout.preferredWidth: 150
                    onClicked: AppImageManager.requestModal(
                                   AppImageManager.OpenDialog)
                }
            }
        }

        ListView {
            Layout.fillHeight: true
            Layout.alignment: Qt.AlignHCenter
            Layout.minimumWidth: 460
            Layout.maximumWidth: 460
            spacing: 5
            clip: true
            model: AppImageManager.appImageList

            delegate: ItemDelegate {
                width: ListView.view.width

                onClicked: {
                    AppImageManager.loadAppImageMetadata(model.path)
                }

                background: Rectangle {
                    property color backgroundColor: Qt.lighter(palette.base,
                                                               2.2)
                    property color hoverColor: Qt.darker(backgroundColor, 1.1)
                    property color pressedColor: Qt.darker(backgroundColor, 1.3)
                    property color disabledColor: Qt.darker(backgroundColor,
                                                            1.4)

                    radius: 8
                    color: !parent.enabled ? disabledColor : parent.down ? pressedColor : parent.hovered ? hoverColor : backgroundColor
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    spacing: 10
                    anchors.margins: 10

                    Image {
                        source: model.icon
                        width: 48
                        height: 48
                        sourceSize.width: width
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectFit

                        IconButton {
                            id: updateOptionsBtn

                            property var hasSelected: updaterReleases.some((release) => release.isSelected)

                            visible: hasNewRelease
                            text: "\uf35b"
                            palette.buttonText: hasSelected ? "#28a745" : undefined
                            width: 24
                            height: 24
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            anchors.bottomMargin: -5
                            onClicked: {
                                updateOptionsMenu.y = updateOptionsBtn.y + 30
                                updateOptionsMenu.open()
                            }
                        }

                        Menu {
                            id: updateOptionsMenu

                            ListView {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                clip: true
                                model: updaterReleases
                                spacing: 0
                                height: Math.min(contentHeight, 210)

                                delegate: MenuItem {
                                    onTriggered: {
                                        const initialValue = isSelected;
                                        updaterReleases.forEach((release) => release.isSelected = false);
                                        isSelected = !initialValue;
                                    }

                                    contentItem: RowLayout {
                                        spacing: 10

                                        Text {
                                            text: isSelected ? "\uf00c" : ""
                                            color: parent.palette.buttonText
                                            Layout.preferredWidth: 16
                                            horizontalAlignment: Text.Left
                                            verticalAlignment: Text.AlignVCenter
                                            font.family: fontAwesome.name
                                            FontLoader {
                                                id: fontAwesome
                                                source: "/assets/fonts/fa-6-solid-900.otf"
                                            }
                                        }

                                        Text {
                                            text: isNew ? version + " *" : version
                                            color: parent.palette.buttonText
                                            font.bold: isNew
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.Left
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: 5
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter

                        Label {
                            text: name
                            font.pixelSize: 14
                            font.bold: true
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignLeft
                        }

                        Label {
                            text: comment
                            visible: comment
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignLeft
                        }

                        Label {
                            text: version
                            opacity: 0.6
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignLeft
                        }
                    }
                }
            }
        }
    }
}
