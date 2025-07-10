import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Fusion
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    width: 540
    minimumWidth: 270
    height: 540
    minimumHeight: 270
    title: qsTr("Preferences")
    flags: Qt.Dialog | Qt.WindowTitleHint

    FileDialog {
        id: folderDialog
        currentFolder: StandardPaths.writableLocation(StandardPaths.HomeLocation)
        onAccepted: {
            AppSettings.appImageDefaultLocation = folderDialog.selectedFolder
        }
    }

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
                        text: qsTr("AppImage Default Location")
                    }
                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        TextField {
                            placeholderText: qsTr("AppImage default location")
                            text: AppSettings.appImageDefaultLocation
                            readOnly: true
                            Layout.fillWidth: true
                        }
                        IconButton {
                            text: "\uf07c"
                            width: 65
                            Layout.preferredWidth: 65
                            onClicked: folderDialog.open()
                        }
                    }
                }
            }
        }
    }
}
