import QtQuick

IconButton {
    id: copyButton

    property string copyText: ""

    text: "\uf0c5"
    onClicked: {
        ClipboardManager.copyToClipboard(copyText)
        text = "\uf00c"
        copyButtonTimer.start()
    }

    Timer {
        id: copyButtonTimer
        interval: 1000
        repeat: false
        running: true
        onTriggered: copyButton.text = "\uf0c5"
    }
}
