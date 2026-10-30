import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: mainWindow

    width: 540
    minimumWidth: 480
    height: 680
    minimumHeight: 480
    visible: true
    title: qsTr("AppImage Manager")

    // The visible page is kept stable while a pending navigation is confirmed.
    property int displayedState: AppImageManager.state
    property string pendingAction: ""
    property int pendingState: AppImageManager.AppList
    property var pendingAppImage: null
    property bool waitingForSave: false

    function pageHasUnsavedChanges() {
        return pageContent.item
                && pageContent.item.hasUnsavedChanges === true
    }

    function clearPendingAction() {
        pendingAction = ""
        pendingAppImage = null
        waitingForSave = false
    }

    function performPendingAction() {
        const action = pendingAction
        const targetState = pendingState
        const appImage = pendingAppImage
        clearPendingAction()

        if (action === "state") {
            AppImageManager.state = targetState
        } else if (action === "load") {
            AppImageManager.loadAppImageMetadata(appImage)
        } else if (action === "quit") {
            Qt.quit()
        }
    }

    function requestQuit() {
        if (!pageHasUnsavedChanges()) {
            Qt.quit()
            return
        }

        pendingAction = "quit"
        unsavedChangesDialog.open()
    }

    function requestNavigation(targetState) {
        if (targetState === displayedState)
            return

        if (!pageHasUnsavedChanges()) {
            AppImageManager.state = targetState
            return
        }

        pendingAction = "state"
        pendingState = targetState
        unsavedChangesDialog.open()
    }

    function requestLoadAppImage(appImage) {
        if (!pageHasUnsavedChanges()) {
            AppImageManager.loadAppImageMetadata(appImage)
            return
        }

        pendingAction = "load"
        pendingAppImage = appImage
        unsavedChangesDialog.open()
    }

    function resolveUnsavedChanges(choice) {
        if (choice === "stay") {
            clearPendingAction()
            unsavedChangesDialog.close()
            return
        }

        if (choice === "discard") {
            // Discard means abandoning the in-memory changes.
            // The next time AppInfo is opened, metadata is fully reloaded.
            unsavedChangesDialog.close()
            performPendingAction()
            return
        }

        if (choice === "save") {
            if (!pageContent.item) {
                clearPendingAction()
                unsavedChangesDialog.close()
                return
            }

            waitingForSave = true

            const saved = pageContent.item.saveUpdateSettings()

            if (saved) {
                unsavedChangesDialog.close()
                performPendingAction()
            }
        }
    }

    FontLoader {
        id: fontAwesome
        source: "/assets/fonts/fa-6-solid-900.otf"
    }

    Component.onCompleted: {
        if (FileArg) {
            AppImageManager.loadAppImageMetadata(FileArg)
        }
    }

    onClosing: function(close) {
        if (pageHasUnsavedChanges()) {
            close.accepted = false
            pendingAction = "quit"
            unsavedChangesDialog.open()
        }
    }

    QtObject {
        id: modalManager

        property var aboutModal: null
        property var preferencesModal: null
        property var applyPresetModal: null

        function openAboutModal() {
            if (aboutModal === null) {
                const component = Qt.createComponent("About.qml")

                if (component.status !== Component.Ready) {
                    console.error("Failed to load About.qml:",
                                  component.errorString())
                    return
                }

                const dialog = component.createObject(null)

                if (dialog === null) {
                    console.error("Failed to create About.qml:",
                                  component.errorString())
                    return
                }

                aboutModal = dialog
                dialog.transientParent = mainWindow

                dialog.closing.connect(function() {
                    dialog.destroy()
                    if (aboutModal === dialog)
                        aboutModal = null
                })
            }

            aboutModal.show()
        }

        function openPreferencesModal() {
            if (preferencesModal === null) {
                const component = Qt.createComponent("Preferences.qml")

                if (component.status !== Component.Ready) {
                    console.error("Failed to load Preferences.qml:",
                                  component.errorString())
                    return
                }

                const dialog = component.createObject(null)

                if (dialog === null) {
                    console.error("Failed to create Preferences.qml:",
                                  component.errorString())
                    return
                }

                preferencesModal = dialog
                dialog.transientParent = mainWindow

                dialog.closing.connect(function() {
                    dialog.destroy()
                    if (preferencesModal === dialog)
                        preferencesModal = null
                })
            }

            preferencesModal.show()
        }

        function openApplyPresetModal(preset) {
            if (applyPresetModal === null) {
                const component = Qt.createComponent("ApplyUpdatePreset.qml")

                if (component.status !== Component.Ready) {
                    console.error("Failed to load ApplyUpdatePreset.qml:",
                                  component.errorString())
                    return
                }

                const dialog = component.createObject(null)

                if (dialog === null) {
                    console.error("Failed to create ApplyUpdatePreset.qml:",
                                  component.errorString())
                    return
                }

                applyPresetModal = dialog
                dialog.transientParent = mainWindow

                dialog.closing.connect(function() {
                    dialog.destroy()
                    if (applyPresetModal === dialog)
                        applyPresetModal = null
                })
            }

            applyPresetModal.preset = preset
            applyPresetModal.show()
        }
    }

    Connections {
        target: ErrorManager

        function onMessageOccurred(message, messageType) {
            errorDialog.text = message

            switch (messageType) {
            case ErrorManager.Error:
                errorDialog.title = qsTr("Error")
                break
            case ErrorManager.Warning:
                errorDialog.title = qsTr("Warning")
                break
            default:
                errorDialog.title = qsTr("Message")
                break
            }

            errorDialog.open()
        }
    }

    Connections {
        target: AppImageManager

        function onStateChanged() {
            mainWindow.displayedState = AppImageManager.state
        }

        function onModalRequested(modal, data) {
            switch (modal) {
            case AppImageManager.Preferences:
                modalManager.openPreferencesModal()
                break
            case AppImageManager.OpenDialog:
                fileDialog.open()
                break
            case AppImageManager.ApplyUpdatePreset:
                modalManager.openApplyPresetModal(data)
                break
            }
        }
    }

    MessageDialog {
        id: errorDialog

        title: ""
        text: ""
        buttons: MessageDialog.Ok
    }

    Connections {
        target: AppImageManager.appImageMetadata

        function onUpdateDirtyChanged() {
            // Complete the pending action once the asynchronous save succeeds.
            Qt.callLater(function() {
                if (mainWindow.waitingForSave
                        && !mainWindow.pageHasUnsavedChanges()) {
                    unsavedChangesDialog.close()
                    mainWindow.performPendingAction()
                }
            })
        }
    }

    // Use a custom modal overlay so the application behind the dialog is
    // visibly dimmed, independent of the active Qt Quick Controls style.
    Overlay.modal: Rectangle {
        color: "#80000000"
    }

    Dialog {
        id: unsavedChangesDialog

        title: qsTr("Unsaved Update Settings")
        modal: true
        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.NoButton

        // An explicit width avoids the Fusion Dialog implicitWidth loop that
        // can occur when its content and footer both participate in sizing.
        width: Math.min(440, mainWindow.width - 40)
        padding: 20

        contentItem: ColumnLayout {
            spacing: 12

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("You have unsaved update settings. Would you like to save them before leaving this AppImage?")
            }

            Label {
                Layout.fillWidth: true
                visible: mainWindow.waitingForSave
                wrapMode: Text.WordWrap
                text: qsTr("Applying update settings...")
                opacity: 0.75
            }
        }

        footer: DialogButtonBox {
            alignment: Qt.AlignRight
            padding: 8
            spacing: 8

            Button {
                text: qsTr("Stay")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                onClicked: mainWindow.resolveUnsavedChanges("stay")
            }

            Button {
                text: qsTr("Discard")
                enabled: !mainWindow.waitingForSave
                DialogButtonBox.buttonRole: DialogButtonBox.DestructiveRole
                onClicked: mainWindow.resolveUnsavedChanges("discard")
            }

            Button {
                text: qsTr("Save")
                highlighted: true
                enabled: !mainWindow.waitingForSave
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                onClicked: mainWindow.resolveUnsavedChanges("save")
            }
        }
    }

    FileDialog {
        id: fileDialog

        title: qsTr("Open AppImage")
        currentFolder: StandardPaths.writableLocation(
                           StandardPaths.HomeLocation)
        nameFilters: [
            qsTr("AppImage Files (*.AppImage *.appimage)"),
            qsTr("All Files (*)")
        ]
        fileMode: FileDialog.OpenFile

        onAccepted: {
            requestLoadAppImage(fileDialog.selectedFile)
        }
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")

            Action {
                text: qsTr("&Open AppImage...")
                onTriggered: fileDialog.open()
                enabled: !AppImageManager.loadingAppImage
                         && !AppImageManager.updating
            }

            MenuSeparator {}

            Action {
                text: qsTr("&Preferences")
                onTriggered: modalManager.openPreferencesModal()
                enabled: !AppImageManager.updating
            }

            MenuSeparator {}

            Action {
                text: qsTr("&Quit")
                onTriggered: mainWindow.requestQuit()
            }
        }

        Menu {
            title: qsTr("&Help")

            Action {
                text: qsTr("&About")
                onTriggered: modalManager.openAboutModal()
            }
        }
    }

    header: headerLoader.item

    Loader {
        id: headerLoader

        sourceComponent: mainWindow.displayedState === AppImageManager.AppInfo
                         ? appInfoHeaderComponent
                         : null
    }

    Component {
        id: appInfoHeaderComponent

        ToolBar {
            background: null

            RowLayout {
                anchors.fill: parent

                ToolButton {
                    Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter

                    onClicked: mainWindow.requestNavigation(AppImageManager.AppList)

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Back to App List")
                    Accessible.name: qsTr("Back to App List")

                    contentItem: Row {
                        spacing: 10

                        anchors.centerIn: parent

                        Label {
                            text: "\uf104"
                            font: Qt.font({
                                family: fontAwesome.name,
                                pixelSize: 18
                            })
                        }

                        Label {
                            text: qsTr("App List")
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                }
            }
        }
    }

    DropArea {
        anchors.fill: parent
        z: 1

        onDropped: function(dropEvent) {
            let accepted = false

            for (let i = 0; i < dropEvent.urls.length; i++) {
                const localPath = dropEvent.urls[i].toLocalFile()

                if (localPath
                        && localPath.toLowerCase().endsWith(".appimage")) {
                    accepted = true

                    Qt.callLater(function() {
                        mainWindow.requestLoadAppImage(localPath)
                    })
                }
            }

            if (accepted)
                dropEvent.acceptProposedAction()
        }
    }

    Loader {
        id: pageContent

        anchors.fill: parent
        source: mainWindow.displayedState === AppImageManager.AppInfo
                ? "AppInfo.qml"
                : "AppList.qml"
    }

    footer: ProgressBar {
        id: busyIndicator

        indeterminate: true
        implicitHeight: 6
        visible: AppImageManager.loadingAppImage
                 || AppImageManager.loadingAppImageList
                 || AppImageManager.updating
    }
}
