import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    width: 360
    minimumWidth: 360
    height: 360
    minimumHeight: 360
    title: qsTr("About")
    flags: Qt.Dialog | Qt.WindowTitleHint

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 10
        anchors.topMargin: 20

        ColumnLayout {
            width: scrollView.width
            spacing: 10

            Image {
                source: "qrc:/assets/icons/barryapplauncher.svg"
                width: 128
                height: 128
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectFit
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: AppName
                font.pixelSize: 24
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: AppVersion
                font.pixelSize: 16
                font.bold: true
                opacity: 0.6
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: qsTr("BarryAppLauncher is a lightweight Qt app that seamlessly integrates AppImages into your desktop menu and lets you update them in-place.")
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.minimumWidth: 320
                Layout.maximumWidth: 320
                Layout.alignment: Qt.AlignCenter
            }
        }
    }
}
