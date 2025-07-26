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
            spacing: 10

            Image {
                source: AppImageManager.appImageMetadata?.icon
                width: 128
                height: 128
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectFit
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }
}
