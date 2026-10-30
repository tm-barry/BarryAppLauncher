import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "utils"

Item {
    id: root

    Component.onCompleted: {
        if (!AppImageManager.loadingAppImageList
                && AppImageManager.appImageList.count === 0) {
            AppImageManager.loadAppImageList()
        }
    }

    FormatterUtil { id: formatter }

    // Load the icon font once instead of once per release-menu delegate.
    FontLoader {
        id: fontAwesome
        source: "/assets/fonts/fa-6-solid-900.otf"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 650
            Layout.alignment: Qt.AlignHCenter
            spacing: 8

            ColorButton {
                id: checkForUpdatesBtn
                text: AppImageManager.appImageList.hasAnyNewRelease
                      ? qsTr("Check Again") : qsTr("Check for Updates")
                Layout.preferredWidth: 140
                enabled: !AppImageManager.loadingAppImageList
                         && !AppImageManager.updating
                onClicked: AppImageManager.checkForAllUpdates()
                Accessible.name: qsTr("Check for updates")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Check all registered AppImages for updates")
            }

            ColorButton {
                id: updateAllBtn
                text: qsTr("Update All")
                Layout.preferredWidth: 100
                visible: AppImageManager.appImageList.hasAnyNewRelease
                enabled: !AppImageManager.loadingAppImageList
                         && !AppImageManager.updating
                onClicked: AppImageManager.updateAllAppImages()
                Accessible.name: qsTr("Update all AppImages")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Install the selected updates")
            }

            Item { Layout.fillWidth: true }

            IconButton {
                text: "\uf2d0"
                Layout.preferredWidth: 32
                Layout.preferredHeight: checkForUpdatesBtn.height
                onClicked: SettingsManager.appListCompactView =
                           !SettingsManager.appListCompactView
                Accessible.name: SettingsManager.appListCompactView
                                  ? qsTr("Switch to default view")
                                  : qsTr("Switch to compact view")
                ToolTip.visible: hovered
                ToolTip.text: SettingsManager.appListCompactView
                              ? qsTr("Default View") : qsTr("Compact View")
            }

            IconButton {
                text: "\uf021"
                Layout.preferredWidth: 32
                Layout.preferredHeight: checkForUpdatesBtn.height
                enabled: !AppImageManager.loadingAppImageList
                         && !AppImageManager.updating
                onClicked: AppImageManager.loadAppImageList()
                Accessible.name: qsTr("Reload registered AppImages")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Reload registered AppImages")
            }
        }

        // Loading is shown independently of the empty state so an empty model
        // during a scan is not mistaken for a genuinely empty app list.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 650
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            visible: AppImageManager.loadingAppImageList
            spacing: 12

            Item { Layout.fillHeight: true }

            BusyIndicator {
                running: AppImageManager.loadingAppImageList
                Layout.alignment: Qt.AlignHCenter
            }

            Label {
                text: qsTr("Loading registered AppImages…")
                Layout.alignment: Qt.AlignHCenter
                horizontalAlignment: Text.AlignHCenter
            }

            Item { Layout.fillHeight: true }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 650
            Layout.alignment: Qt.AlignHCenter
            Layout.fillHeight: true
            visible: !AppImageManager.loadingAppImageList
                     && AppImageManager.appImageList.count === 0
            spacing: 20

            Item { Layout.fillHeight: true }

            Label {
                text: qsTr("No registered AppImages found. Change the default AppImage location in preferences or open an AppImage to register.")
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: 10
                Layout.alignment: Qt.AlignHCenter

                Button {
                    text: qsTr("Preferences")
                    Layout.preferredWidth: 150
                    onClicked: AppImageManager.requestModal(
                                   AppImageManager.Preferences)
                }

                Button {
                    text: qsTr("Open AppImage")
                    Layout.preferredWidth: 150
                    onClicked: AppImageManager.requestModal(
                                   AppImageManager.OpenDialog)
                }
            }

            Item { Layout.fillHeight: true }
        }

        SortFilterProxyModel {
            id: sortedAppImages
            model: AppImageManager.appImageList

            sorters: [
                RoleSorter {
                    roleName: "hasNewRelease"
                    priority: 0
                    sortOrder: "DescendingOrder"
                },
                RoleSorter {
                    roleName: "name"
                    priority: 1
                }
            ]
        }

        ListView {
            id: appListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: 650
            Layout.alignment: Qt.AlignHCenter
            spacing: 5
            clip: true
            model: sortedAppImages
            visible: !AppImageManager.loadingAppImageList
                     && AppImageManager.appImageList.count > 0

            // Keep only one release menu open. Closing it on scroll avoids
            // menus becoming detached from recycled ListView delegates.
            property var openUpdateMenu: null

            section.property: "hasNewRelease"
            section.criteria: ViewSection.FullString

            onContentYChanged: {
                if (openUpdateMenu)
                    openUpdateMenu.close()
            }

            section.delegate: Rectangle {
                required property string section
                width: ListView.view.width
                height: 32
                color: "transparent"

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    font: Qt.font({ bold: true })
                    text: parent.section === "true"
                          ? qsTr("Updates Available") : qsTr("Registered")
                }
            }

            delegate: ItemDelegate {
                id: appDelegate
                width: ListView.view.width

                onClicked: {
                    if (!AppImageManager.updating)
                        AppImageManager.loadAppImageMetadata(model.path)
                }

                background: Rectangle {
                    property color normalColor: Qt.lighter(appDelegate.palette.base, 2.2)
                    property color hoverColor: Qt.darker(normalColor, 1.1)
                    property color pressedColor: Qt.darker(normalColor, 1.3)
                    property color disabledColor: Qt.darker(normalColor, 1.4)

                    radius: 8
                    color: !appDelegate.enabled ? disabledColor
                           : appDelegate.down ? pressedColor
                           : appDelegate.hovered ? hoverColor : normalColor
                }

                contentItem: RowLayout {
                    id: appItemListItem
                    spacing: SettingsManager.appListCompactView ? 5 : 10
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10

                    property var selectedRelease: updaterReleases.find(
                                                      release => release.isSelected)
                    property bool hasSelected: !!selectedRelease

                    Item {
                        Layout.preferredWidth: SettingsManager.appListCompactView ? 32 : 48
                        Layout.preferredHeight: SettingsManager.appListCompactView ? 32 : 48

                        Image {
                            id: iconImage
                            anchors.fill: parent
                            source: model.icon
                            sourceSize.width: width
                            sourceSize.height: height
                            fillMode: Image.PreserveAspectFit
                            visible: status === Image.Ready
                        }

                        // A simple fallback keeps broken/missing icon paths from
                        // leaving an empty area in the application row.
                        Rectangle {
                            anchors.fill: parent
                            radius: 6
                            color: appDelegate.palette.mid
                            opacity: 0.18
                            visible: iconImage.status !== Image.Ready
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "◈"
                            font.pixelSize: SettingsManager.appListCompactView ? 21 : 30
                            color: appDelegate.palette.text
                            opacity: 0.55
                            visible: iconImage.status !== Image.Ready
                        }


                        IconButton {
                            id: updateOptionsBtn
                            enabled: !AppImageManager.updating
                            visible: hasNewRelease
                            text: "\uf35b"
                            palette.buttonText: appItemListItem.hasSelected
                                               ? "#28a745" : undefined
                            width: SettingsManager.appListCompactView ? 20 : 24
                            height: SettingsManager.appListCompactView ? 20 : 24
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            anchors.bottomMargin: -5
                            Accessible.name: qsTr("Choose update version")
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("Choose which version to install")
                            onClicked: {
                                if (appListView.openUpdateMenu
                                        && appListView.openUpdateMenu !== updateOptionsMenuInternal) {
                                    appListView.openUpdateMenu.close()
                                }
                                appListView.openUpdateMenu = updateOptionsMenuInternal
                                updateOptionsMenuInternal.open()
                            }
                        }

                        Menu {
                            id: updateOptionsMenuInternal
                            x: Math.max(0, updateOptionsBtn.x
                                         + updateOptionsBtn.width - width)
                            y: updateOptionsBtn.y + updateOptionsBtn.height
                            width: 190
                            height: Math.min(releaseList.contentHeight, 210)

                            onClosed: {
                                if (appListView.openUpdateMenu === updateOptionsMenuInternal)
                                    appListView.openUpdateMenu = null
                            }

                            contentItem: ListView {
                                id: releaseList
                                clip: true
                                model: updaterReleases
                                spacing: 0
                                implicitHeight: contentHeight

                                delegate: MenuItem {
                                    id: releaseMenuItem
                                    width: releaseList.width

                                    onTriggered: {
                                        const initialValue = isSelected
                                        updaterReleases.forEach(release =>
                                            release.isSelected = false)
                                        isSelected = !initialValue
                                    }

                                    contentItem: RowLayout {
                                        spacing: 10

                                        Text {
                                            text: isSelected ? "\uf00c" : ""
                                            color: releaseMenuItem.palette.buttonText
                                            Layout.preferredWidth: 16
                                            horizontalAlignment: Text.AlignLeft
                                            verticalAlignment: Text.AlignVCenter
                                            font.family: fontAwesome.name
                                        }

                                        Text {
                                            text: isNew ? version + " *" : version
                                            color: releaseMenuItem.palette.buttonText
                                            font.bold: isNew
                                            Layout.fillWidth: true
                                            horizontalAlignment: Text.AlignLeft
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                    }
                                }
                            }
                        }
                    }

                    ColumnLayout {
                        spacing: SettingsManager.appListCompactView ? 0 : 5
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter

                        RowLayout {
                            spacing: 10
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter

                            Label {
                                text: name
                                font: Qt.font({ pixelSize: SettingsManager.appListCompactView ? 12 : 14, bold: true })
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                                horizontalAlignment: Text.AlignLeft
                            }

                            Label {
                                text: appItemListItem.selectedRelease?.version
                                      ?? updateCurrentVersion ?? version
                                font: Qt.font({ pixelSize: SettingsManager.appListCompactView ? 12 : 14, bold: appItemListItem.hasSelected })
                                opacity: 0.75
                                horizontalAlignment: Text.AlignRight
                            }
                        }

                        Label {
                            text: comment
                            font: Qt.font({ pixelSize: SettingsManager.appListCompactView ? 11 : 13 })
                            visible: comment && (!AppImageManager.updating || !hasNewRelease)
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignLeft
                        }

                        Item {
                            height: SettingsManager.appListCompactView ? 20 : 24
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignHCenter
                            visible: appItemListItem.hasSelected
                                     && AppImageManager.updating

                            ProgressBar {
                                anchors.fill: parent
                                from: 0
                                to: 1
                                value: updateBytesTotal > 0
                                       ? updateBytesReceived / updateBytesTotal : 0
                                indeterminate: updateBytesTotal < 0
                            }

                            Text {
                                anchors.centerIn: parent
                                text: updateBytesTotal > 0
                                      ? formatter.getUpdateDownloadText(updateBytesReceived,
                                                                        updateBytesTotal)
                                      : formatter.getUpdateProgressText(updateProgressState)
                                color: appDelegate.palette.text
                                font.pixelSize: SettingsManager.appListCompactView ? 10 : 12
                            }
                        }
                    }
                }
            }
        }
    }
}
