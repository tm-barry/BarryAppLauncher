import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    property int maximumWidth: 460

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

        function launchAppImage(terminal = false) {
            if (!AppImageManager.appImageMetadata?.integrated) {
                AppImageManager.launchAppImage(
                            AppImageManager.appImageMetadata?.path, terminal)
            } else {
                AppImageManager.launchAppImage(
                            AppImageManager.appImageMetadata?.desktopFilePath,
                            terminal)
            }
            launchTimer.start()
        }
    }

    MessageDialog {
        id: unregisterDialog
        title: qsTr("Unregister")
        text: qsTr("Would you like the AppImage file deleted also?")
        buttons: MessageDialog.Yes | MessageDialog.No | MessageDialog.Cancel

        onButtonClicked: button => {
                             switch (button) {
                                 case MessageDialog.Yes:
                                 AppImageManager.unregisterAppImage(
                                     AppImageManager.appImageMetadata?.path,
                                     true)
                                 break
                                 case MessageDialog.No:
                                 AppImageManager.unregisterAppImage(
                                     AppImageManager.appImageMetadata?.path,
                                     false)
                                 break
                             }
                         }
    }

    ScrollView {
        id: scrollView
        anchors.fill: parent
        anchors.margins: 10
        anchors.topMargin: -2

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
                visible: AppImageManager.appImageMetadata?.executable
            }

            Label {
                text: "\uf023"
                font.pixelSize: 96
                Layout.alignment: Qt.AlignHCenter
                visible: !AppImageManager.appImageMetadata?.executable
                FontLoader {
                    id: fontAwesome
                    source: "/assets/fonts/fa-6-solid-900.otf"
                }
            }

            Label {
                text: AppImageManager.appImageMetadata?.name
                font.pixelSize: 18
                font.bold: true
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: AppImageManager.appImageMetadata?.version
                opacity: 0.6
                Layout.alignment: Qt.AlignHCenter
                visible: AppImageManager.appImageMetadata?.executable
            }

            Label {
                text: AppImageManager.appImageMetadata?.comment
                visible: AppImageManager.appImageMetadata?.executable
                         && AppImageManager.appImageMetadata?.comment
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
                Layout.maximumWidth: maximumWidth
                wrapMode: TextEdit.Wrap
                Layout.fillWidth: true
            }

            Item {
                Layout.preferredHeight: 5
            }

            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter

                Timer {
                    id: launchTimer
                    interval: 2000
                    repeat: false
                }

                RowLayout {
                    spacing: 0
                    visible: AppImageManager.appImageMetadata?.executable

                    ColorButton {
                        id: launchBtn
                        property string bgColor: "#4E7A6A"

                        text: launchTimer.running ? qsTr("Launching") : qsTr(
                                                        "Launch")
                        backgroundColor: bgColor
                        Layout.preferredWidth: 100
                        enabled: !launchTimer.running
                                 && !AppImageManager.loadingAppImage
                        onClicked: utils.launchAppImage()
                    }

                    ColorButton {
                        id: launchOptionsBtn
                        text: "\u25BE"
                        backgroundColor: launchBtn.bgColor
                        Layout.preferredWidth: 30
                        enabled: launchBtn.enabled
                        onClicked: {
                            launchOptionsMenu.y = launchOptionsBtn.y + 30
                            launchOptionsMenu.open()
                        }
                    }

                    Menu {
                        id: launchOptionsMenu

                        MenuItem {
                            text: qsTr("Launch in Terminal")
                            onTriggered: utils.launchAppImage(true)
                        }
                    }
                }

                RowLayout {
                    spacing: 0
                    visible: AppImageManager.appImageMetadata.updateType

                    ColorButton {
                        text: qsTr("Check for Update")
                        Layout.preferredWidth: 130
                        enabled: !AppImageManager.loadingAppImage
                        visible: !AppImageManager.appImageMetadata.hasNewRelease
                        onClicked: AppImageManager.checkForUpdate()
                    }

                    ColorButton {
                        text: qsTr("Update")
                        Layout.preferredWidth: 100
                        enabled: !AppImageManager.loadingAppImage
                        visible: AppImageManager.appImageMetadata.hasNewRelease
                    }

                    ColorButton {
                        id: updateOptionsBtn
                        text: "\u25BE"
                        Layout.preferredWidth: 30
                        enabled: !AppImageManager.loadingAppImage
                        visible: AppImageManager.appImageMetadata.updaterReleases.length > 0
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
                            model: AppImageManager.appImageMetadata.updaterReleases
                            spacing: 0
                            height: Math.min(contentHeight, 210)

                            delegate: MenuItem {
                                text: isNew ? version + " *" : version
                                font.bold: isNew
                                onTriggered: console.log("Selected release:",
                                                         version, date,
                                                         download)
                            }
                        }
                    }
                }

                ColorButton {
                    text: qsTr("Register")
                    Layout.preferredWidth: 100
                    enabled: !AppImageManager.loadingAppImage
                    visible: AppImageManager.appImageMetadata?.executable
                             && AppImageManager.appImageMetadata?.integration
                             === AppImageMetadata.None
                    onClicked: AppImageManager.registerAppImage(
                                   AppImageManager.appImageMetadata?.path)
                }

                ColorButton {
                    text: qsTr("Unregister")
                    backgroundColor: "#C43D3D"
                    Layout.preferredWidth: 100
                    enabled: !AppImageManager.loadingAppImage
                    visible: AppImageManager.appImageMetadata?.executable
                             && AppImageManager.appImageMetadata?.integration
                             === AppImageMetadata.Internal
                    onClicked: unregisterDialog.open()
                }

                ColorButton {
                    text: qsTr("Unlock")
                    Layout.preferredWidth: 100
                    visible: !AppImageManager.appImageMetadata?.executable
                    onClicked: AppImageManager.unlockAppImage(
                                   AppImageManager.appImageMetadata?.path)
                }
            }

            Label {
                text: qsTr("This AppImage appears to have been integrated by another application.")
                color: "#C43D3D"
                Layout.alignment: Qt.AlignHCenter
                visible: AppImageManager.appImageMetadata?.integration === AppImageMetadata.External
                horizontalAlignment: Text.AlignHCenter
                Layout.maximumWidth: maximumWidth
                wrapMode: TextEdit.Wrap
                Layout.fillWidth: true
            }

            Label {
                text: qsTr("Verify the source of this appimage before unlocking it.")
                Layout.alignment: Qt.AlignHCenter
                visible: !AppImageManager.appImageMetadata?.executable
                horizontalAlignment: Text.AlignHCenter
                Layout.maximumWidth: maximumWidth
                wrapMode: TextEdit.Wrap
                Layout.fillWidth: true
            }

            Item {
                Layout.preferredHeight: 5
            }

            Label {
                text: qsTr("General")
                font.bold: true
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }
            RoundedGroupBox {
                Layout.alignment: Qt.AlignHCenter
                Layout.minimumWidth: maximumWidth
                Layout.maximumWidth: maximumWidth

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    Label {
                        text: qsTr("Package type")
                        font.bold: true
                    }

                    TextArea {
                        text: qsTr("AppImage Type ") + AppImageManager.appImageMetadata?.type
                        readOnly: true
                    }

                    Item {
                        Layout.preferredHeight: 5
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
                        CopyButton {
                            copyText: AppImageManager.appImageMetadata?.path
                            width: 55
                            Layout.preferredWidth: 55
                        }
                        IconButton {
                            text: "\uf07c"
                            width: 55
                            Layout.preferredWidth: 55
                            onClicked: utils.openPathFolder(
                                           AppImageManager.appImageMetadata?.path)
                        }
                    }

                    Item {
                        Layout.preferredHeight: 5
                        visible: !AppImageManager.appImageMetadata?.executable
                    }

                    Label {
                        text: qsTr("Checksum (sha256)")
                        font.bold: true
                        visible: !AppImageManager.appImageMetadata?.executable
                    }

                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true
                        visible: !AppImageManager.appImageMetadata?.executable

                        TextArea {
                            text: AppImageManager.appImageMetadata?.checksum
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            Layout.fillWidth: true
                        }
                        CopyButton {
                            copyText: AppImageManager.appImageMetadata?.checksum
                            width: 55
                            Layout.preferredWidth: 55
                        }
                    }
                }
            }

            Item {
                Layout.preferredHeight: 5
                visible: AppImageManager.appImageMetadata?.executable
            }

            Label {
                text: qsTr("Desktop Integration")
                font.bold: true
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
                visible: AppImageManager.appImageMetadata?.executable
            }
            RoundedGroupBox {
                Layout.alignment: Qt.AlignHCenter
                Layout.minimumWidth: maximumWidth
                Layout.maximumWidth: maximumWidth
                visible: AppImageManager.appImageMetadata?.executable

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
                        CopyButton {
                            copyText: AppImageManager.appImageMetadata?.desktopFilePath
                            width: 35
                            Layout.preferredWidth: 35
                            enabled: AppImageManager.appImageMetadata?.desktopFilePath
                        }
                        IconButton {
                            text: "\uf07c"
                            width: 35
                            Layout.preferredWidth: 35
                            enabled: AppImageManager.appImageMetadata?.desktopFilePath
                            onClicked: utils.openPathFolder(
                                           AppImageManager.appImageMetadata?.desktopFilePath)
                        }
                        IconButton {
                            text: "\uf044"
                            width: 35
                            Layout.preferredWidth: 35
                            enabled: AppImageManager.appImageMetadata?.desktopFilePath
                            onClicked: AppImageManager.openDesktopFileInTextEditor(
                                           AppImageManager.appImageMetadata?.desktopFilePath)
                        }
                    }

                    Item {
                        Layout.preferredHeight: 5
                    }

                    Label {
                        text: qsTr("Categories")
                        font.bold: true
                    }

                    TextArea {
                        text: AppImageManager.appImageMetadata?.categories
                        readOnly: true
                    }
                }
            }

            Item {
                Layout.preferredHeight: 5
                visible: AppImageManager.appImageMetadata?.desktopFilePath
            }

            Label {
                text: qsTr("Update")
                font.bold: true
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
                visible: AppImageManager.appImageMetadata?.desktopFilePath
            }
            RoundedGroupBox {
                id: updateGroupBox
                Layout.alignment: Qt.AlignHCenter
                Layout.minimumWidth: maximumWidth
                Layout.maximumWidth: maximumWidth
                visible: AppImageManager.appImageMetadata?.desktopFilePath

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    Label {
                        text: qsTr("Update Type")
                        font.bold: true
                    }

                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        ComboBox {
                            enabled: !AppImageManager.loadingAppImage
                            Layout.fillWidth: true
                            model: ListModel {
                                ListElement {
                                    text: "None"
                                    value: ""
                                }
                                ListElement {
                                    text: "Json"
                                    value: "json"
                                }
                            }
                            textRole: "text"
                            valueRole: "value"
                            currentIndex: {
                                if (!AppImageManager.appImageMetadata)
                                    return 0
                                for (var i = 0; i < model.count; i++) {
                                    if (model.get(i).value
                                            === AppImageManager.appImageMetadata.updateType)
                                        return i
                                }
                                return 0
                            }
                            onCurrentIndexChanged: {
                                if (AppImageManager.appImageMetadata) {
                                    AppImageManager.appImageMetadata.updateType = model.get(
                                                currentIndex).value

                                    scrollView.ScrollBar.vertical.position = updateGroupBox.y
                                }
                            }
                        }
                        IconButton {
                            text: "\uf0c7"
                            width: 55
                            Layout.preferredWidth: 55
                            enabled: AppImageManager.appImageMetadata?.desktopFilePath
                                     && AppImageManager.appImageMetadata?.updateDirty
                                     && !AppImageManager.loadingAppImage
                            onClicked: {
                                AppImageManager.saveUpdateSettings()
                            }
                        }
                    }

                    Item {
                        Layout.preferredHeight: 5
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Label {
                        text: qsTr("Url")
                        font.bold: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    RoundedTextArea {
                        text: AppImageManager.appImageMetadata?.updateUrl || ""
                        enabled: !AppImageManager.loadingAppImage
                        onTextChanged: {
                            if (AppImageManager.appImageMetadata)
                                AppImageManager.appImageMetadata.updateUrl = text
                        }
                        placeholderText: "ex: https://api.github.com/repos/dev/proj/releases/latest"
                        wrapMode: TextEdit.Wrap
                        Layout.fillWidth: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Item {
                        Layout.preferredHeight: 5
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Label {
                        text: qsTr("Download Field")
                        font.bold: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    RoundedTextArea {
                        text: AppImageManager.appImageMetadata?.updateDownloadField
                              || ""
                        enabled: !AppImageManager.loadingAppImage
                        onTextChanged: {
                            if (AppImageManager.appImageMetadata)
                                AppImageManager.appImageMetadata.updateDownloadField = text
                        }
                        placeholderText: "ex: assets[*].browser_download_url"
                        wrapMode: TextEdit.Wrap
                        Layout.fillWidth: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Item {
                        Layout.preferredHeight: 5
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Label {
                        text: qsTr("Download Pattern")
                        font.bold: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    RoundedTextArea {
                        text: AppImageManager.appImageMetadata?.updateDownloadPattern
                              || ""
                        enabled: !AppImageManager.loadingAppImage
                        onTextChanged: {
                            if (AppImageManager.appImageMetadata)
                                AppImageManager.appImageMetadata.updateDownloadPattern = text
                        }
                        placeholderText: "ex: appName-.*-x86_64\\.AppImage"
                        wrapMode: TextEdit.Wrap
                        Layout.fillWidth: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Item {
                        Layout.preferredHeight: 5
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Label {
                        text: qsTr("Version Field")
                        font.bold: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    RoundedTextArea {
                        text: AppImageManager.appImageMetadata?.updateVersionField
                              || ""
                        enabled: !AppImageManager.loadingAppImage
                        onTextChanged: {
                            if (AppImageManager.appImageMetadata)
                                AppImageManager.appImageMetadata.updateVersionField = text
                        }
                        placeholderText: "ex: tag_name"
                        wrapMode: TextEdit.Wrap
                        Layout.fillWidth: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Item {
                        Layout.preferredHeight: 5
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Label {
                        text: qsTr("Date Field")
                        font.bold: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    RoundedTextArea {
                        text: AppImageManager.appImageMetadata?.updateDateField
                              || ""
                        enabled: !AppImageManager.loadingAppImage
                        onTextChanged: {
                            if (AppImageManager.appImageMetadata)
                                AppImageManager.appImageMetadata.updateDateField = text
                        }
                        placeholderText: "ex: published_at"
                        wrapMode: TextEdit.Wrap
                        Layout.fillWidth: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Item {
                        Layout.preferredHeight: 5
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    Label {
                        text: qsTr("Filters")
                        font.bold: true
                        visible: AppImageManager.appImageMetadata?.updateType
                    }

                    ColumnLayout {
                        visible: AppImageManager.appImageMetadata?.updateType
                        spacing: 5

                        Repeater {
                            model: AppImageManager.appImageMetadata?.updateFilters
                            delegate: RowLayout {
                                spacing: 5
                                Layout.fillWidth: true

                                TextField {
                                    text: modelData.field
                                    enabled: !AppImageManager.loadingAppImage
                                    onTextChanged: modelData.field = text
                                    placeholderText: qsTr("Field...")
                                    Layout.fillWidth: true
                                }
                                TextField {
                                    text: modelData.pattern
                                    enabled: !AppImageManager.loadingAppImage
                                    onTextChanged: modelData.pattern = text
                                    placeholderText: qsTr("Pattern...")
                                    Layout.fillWidth: true
                                }
                                ColorButton {
                                    text: "x"
                                    enabled: !AppImageManager.loadingAppImage
                                    Layout.preferredWidth: 35
                                    backgroundColor: "#C43D3D"
                                    onClicked: AppImageManager.appImageMetadata?.removeUpdateFilter(
                                                   index)
                                }
                            }
                        }

                        ColorButton {
                            text: "Add Filter"
                            enabled: !AppImageManager.loadingAppImage
                            backgroundColor: "#4E7A6A"
                            onClicked: AppImageManager.appImageMetadata?.addUpdateFilterWithValues(
                                           "", "")
                        }
                    }
                }
            }
        }
    }
}
