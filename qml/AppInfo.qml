import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 10

        ColumnLayout {
            width: scrollView.width
            spacing: 5

            Image {
                source: AppImageManager.appImageMetadata?.icon
                width: 128
                height: 128
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

                ColorButton {
                    text: qsTr("Launch")
                    backgroundColor: "#4E7A6A"
                    font.pixelSize: 16
                    Layout.preferredWidth: 100
                }

                ColorButton {
                    text: qsTr("Integrate")
                    font.pixelSize: 16
                    Layout.preferredWidth: 100
                    visible: AppImageManager.appImageMetadata?.integration === AppImageMetadata.None
                }

                ColorButton {
                    text: qsTr("Delete")
                    backgroundColor: "#C43D3D"
                    font.pixelSize: 16
                    Layout.preferredWidth: 100
                    visible: AppImageManager.appImageMetadata?.integration === AppImageMetadata.Internal
                }
            }
        }
    }
}
