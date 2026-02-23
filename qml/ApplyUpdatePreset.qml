import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: applyPresetWindow
    title: qsTr("Apply Preset")
    width: 480
    minimumWidth: 360
    height: 400
    minimumHeight: 100
    flags: Qt.Dialog | Qt.WindowTitleHint
    modality: Qt.ApplicationModal

    property QtObject preset
    property string templateUrl: ""
    property var placeholderList: []
    property var placeholderValues: ({})
    property bool allFieldsValid: false
    property string previewString: ""

    function extractPlaceholders() {
        const regex = /\{([^}]+)\}/g
        let matches = []
        let m
        while ((m = regex.exec(templateUrl)) !== null) {
            if (!matches.includes(m[1]))
                matches.push(m[1])
        }

        if (JSON.stringify(placeholderList) !== JSON.stringify(matches)) {
            placeholderList = matches
            for (let key in placeholderValues) {
                if (!matches.includes(key))
                    delete placeholderValues[key]
            }
            validate()
            generatePreview()
        }
    }

    function validate() {
        for (let key of placeholderList) {
            if (!placeholderValues[key] || placeholderValues[key].trim(
                        ) === "") {
                allFieldsValid = false
                return
            }
        }
        allFieldsValid = true
    }

    function generatePreview() {
        let preview = templateUrl
        for (let key of placeholderList) {
            const val = placeholderValues[key] || ""
            const encoded = encodeURIComponent(val)
            const regex = new RegExp("\\{" + key + "\\}", "g")
            preview = preview.replace(regex, encoded)
        }
        previewString = preview
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10
        anchors.margins: 10

        ScrollView {
            id: scrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth

            ColumnLayout {
                width: scrollView.availableWidth
                spacing: 10

                RoundedGroupBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 5

                        Label {
                            text: qsTr("Template URL")
                            font.bold: true
                            font.pixelSize: 14
                        }
                        TransparentTextArea {
                            text: templateUrl
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }

                RoundedGroupBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        Repeater {
                            model: placeholderList
                            delegate: ColumnLayout {
                                spacing: 5
                                Label {
                                    text: "Value for {" + modelData + "}"
                                    font.bold: true
                                    font.pixelSize: 14
                                }
                                RoundedTextArea {
                                    text: placeholderValues[modelData] || ""
                                    singleLine: true
                                    placeholderText: qsTr("Enter a value...")
                                    wrapMode: TextEdit.Wrap
                                    Layout.fillWidth: true
                                    onTextChanged: {
                                        placeholderValues[modelData] = text
                                        validate()
                                        generatePreview()
                                    }
                                }
                            }
                        }
                    }
                }

                RoundedGroupBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 10

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 5

                        Label {
                            text: qsTr("Preview URL")
                            font.bold: true
                            font.pixelSize: 14
                        }
                        TransparentTextArea {
                            text: previewString
                            readOnly: true
                            wrapMode: TextEdit.Wrap
                            Layout.fillWidth: true
                            color: !allFieldsValid ? "red" : palette.text
                        }
                    }
                }
            }
        }
        RowLayout {
            spacing: 12
            Layout.alignment: Qt.AlignRight
            Button {
                text: qsTr("Cancel")
                onClicked: applyPresetWindow.close()
            }
            Button {
                text: qsTr("Apply")
                enabled: allFieldsValid
                onClicked: {
                    UpdatePresetManager.applyPreset(preset, previewString)
                    applyPresetWindow.close()
                }
            }
        }
    }

    onVisibleChanged: {
        templateUrl = ""
        if (visible && preset) {
            templateUrl = preset.url
            placeholderValues = ({})
            extractPlaceholders()
        }
    }
}
