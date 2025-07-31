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

            Button {
                text: "Refresh"
                Layout.alignment: Qt.AlignRight
                onClicked: {
                    AppImageManager.loadAppImageList()
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
                    property color backgroundColor: palette.base
                    property color hoverColor: Qt.lighter(backgroundColor, 1.2)
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

                        Item { Layout.preferredHeight: 5 }
                    }
                }
            }
        }
    }
}
