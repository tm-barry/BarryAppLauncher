import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    width: 480
    minimumWidth: 270
    height: 540
    minimumHeight: 360
    title: qsTr("Preferences")
    flags: Qt.Dialog | Qt.WindowTitleHint

    FontLoader {
        id: fontAwesome
        source: "/assets/fonts/fa-6-solid-900.otf"
    }

    onVisibleChanged: {
        terminalTextField.text = SettingsManager.terminal
        textEditorTextField.text = SettingsManager.textEditor

        loadUpdateHeaders()
    }

    function loadUpdateHeaders() {
        headersModel.clear()
        let headers = SettingsManager.getUpdateHeadersJson()
        for (var i = 0; i < headers.length; i++) {
            headersModel.append(headers[i])
        }
    }

    function saveUpdateHeaders() {
        let array = []
        for (let i = 0; i < headersModel.count; i++) {
            let item = headersModel.get(i)
            if (item.website && item.header) {
                array.push(item)
            }
        }
        SettingsManager.saveUpdateHeadersJson(array)
    }

    Connections {
        target: SettingsManager
        function onTerminalChanged(newValue) {
            terminalTextField.text = newValue
        }
        function onTextEditorChanged(newValue) {
            textEditorTextField.text = newValue
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
                        CopyButton {
                            copyText: Qt.resolvedUrl(
                                          SettingsManager.appImageDefaultLocation).toString(
                                          ).replace("file://", "")
                            width: 55
                            Layout.preferredWidth: 55
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
                                if (!terminalTextField.text
                                        || SettingsManager.terminalExists(
                                            terminalTextField.text)) {
                                    SettingsManager.terminal = terminalTextField.text
                                } else {
                                    ErrorManager.reportError(
                                                qsTr("Terminal not found: ")
                                                + terminalTextField.text)
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
                        text: qsTr("Text Editor Override")
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        RoundedTextArea {
                            id: textEditorTextField
                            singleLine: true
                            placeholderText: qsTr("Detect default text editor...")
                            wrapMode: TextEdit.Wrap
                            Layout.fillWidth: true
                        }

                        IconButton {
                            text: "\uf0c7"
                            width: 55
                            Layout.preferredWidth: 55
                            enabled: SettingsManager.textEditor !== textEditorTextField.text
                            onClicked: {
                                if (!textEditorTextField.text
                                        || SettingsManager.textEditorExists(
                                            textEditorTextField.text)) {
                                    SettingsManager.textEditor = textEditorTextField.text
                                } else {
                                    ErrorManager.reportError(
                                                qsTr("Text editor not found: ")
                                                + textEditorTextField.text)
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

            Label {
                text: qsTr("Updater")
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
                        text: qsTr("Keep Backup")
                        font.bold: true
                    }
                    RowLayout {
                        spacing: 5
                        Layout.fillWidth: true

                        Label {
                            text: qsTr("Keep backup of previous appimage when updating?")
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }

                        CheckBox {
                            checked: SettingsManager.keepBackup
                            onCheckedChanged: {
                                SettingsManager.keepBackup = checked
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
                        text: qsTr("Update Headers")
                        font.bold: true
                    }

                    RowLayout {
                        spacing: 5
                        Layout.alignment: Qt.AlignHCenter

                        ColorButton {
                            text: qsTr("Add Header")
                            Layout.preferredWidth: 100
                            onClicked: {
                                headersModel.append({
                                                        "website": "",
                                                        "header": "",
                                                        "value": ""
                                                    })

                                scrollView.ScrollBar.vertical.position = headersContainer.y + headersContainer.height
                            }
                        }

                        IconButton {
                            id: saveHeaderButton
                            text: "\uf0c7"
                            width: 55
                            Layout.preferredWidth: 55
                            onClicked: {
                                saveUpdateHeaders();
                                loadUpdateHeaders();
                                text = "\uf00c"
                                saveHeaderTimer.start()
                            }

                            Timer {
                                id: saveHeaderTimer
                                interval: 1000
                                repeat: false
                                running: true
                                onTriggered: saveHeaderButton.text = "\uf0c7"
                            }
                        }
                    }
                    ColumnLayout {
                        id: headersContainer
                        width: parent.width
                        spacing: 5

                        ListModel {
                            id: headersModel
                        }
                        Repeater {
                            model: headersModel

                            ColumnLayout {
                                spacing: 5

                                RowLayout {
                                    spacing: 10
                                    width: parent.width

                                    ColumnLayout {
                                        spacing: 5

                                        TextField {
                                            Component.onCompleted: text = website
                                            placeholderText: qsTr("Website... ex: api.github.com")
                                            Layout.fillWidth: true
                                            onTextChanged: website = text
                                        }
                                        TextField {
                                            Component.onCompleted: text = header
                                            placeholderText: qsTr("Header... ex: Authorization")
                                            Layout.fillWidth: true
                                            onTextChanged: header = text
                                        }
                                        RowLayout {
                                            id: row
                                            spacing: 0
                                            Layout.fillWidth: true

                                            property bool masked: true

                                            TextField {
                                                id: headerValueField
                                                Component.onCompleted: text = value
                                                placeholderText: qsTr("Value... ex: token {apikey}")
                                                echoMode: row.masked ? TextInput.Password : TextInput.Normal
                                                Layout.fillWidth: true
                                                onTextChanged: value = text
                                            }

                                            ColorButton {
                                                text: row.masked ? "\uf06e" : "\uf070"
                                                backgroundColor: headerValueField.background.color
                                                onClicked: row.masked = !row.masked
                                                Layout.preferredHeight: headerValueField.height
                                                Layout.preferredWidth: 24
                                                font.family: fontAwesome.name
                                            }
                                        }
                                    }

                                    ColorButton {
                                        text: "\uf1f8"
                                        Layout.preferredWidth: 24
                                        Layout.preferredHeight: 24
                                        backgroundColor: "#C43D3D"
                                        font.family: fontAwesome.name
                                        Layout.alignment: Qt.AlignVCenter
                                        onClicked: headersModel.remove(index)
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
                            }
                        }
                    }
                }
            }
        }
    }
}
