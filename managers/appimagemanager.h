#ifndef APPIMAGEMANAGER_H
#define APPIMAGEMANAGER_H

#pragma once

#include "models/appimagemetadata.h"
#include "models/appimagemetadatalistmodel.h"

#include <QDir>
#include <QObject>
#include <QUrl>
#include <QFuture>

class AppImageManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AppImageMetadataListModel* appImageList READ appImageList NOTIFY appImageListChanged)
    Q_PROPERTY(AppImageMetadata* appImageMetadata READ appImageMetadata WRITE setAppImageMetadata NOTIFY appImageMetadataChanged)
    Q_PROPERTY(bool loadingAppImageList READ loadingAppImageList WRITE setLoadingAppImageList NOTIFY loadingAppImageListChanged)
    Q_PROPERTY(bool loadingAppImage READ loadingAppImage WRITE setLoadingAppImage NOTIFY loadingAppImageChanged)
    Q_PROPERTY(bool updating READ updating WRITE setUpdating NOTIFY updatingChanged)
    Q_PROPERTY(AppState state READ state WRITE setState NOTIFY stateChanged)
public:
    static AppImageManager* instance();

    enum AppState {
        AppInfo,
        AppList
    };
    Q_ENUM(AppState);

    enum ModalTypes {
        Preferences,
        OpenDialog,
        About,
        ApplyUpdatePreset
    };
    Q_ENUM(ModalTypes);

    AppImageMetadataListModel* appImageList() const;
    void setAppImageList(const QList<AppImageMetadata*>& list);

    AppImageMetadata* appImageMetadata() const;
    void setAppImageMetadata(AppImageMetadata* value);

    bool loadingAppImageList() const;
    void setLoadingAppImageList(bool value);

    bool loadingAppImage() const;
    void setLoadingAppImage(bool value);

    bool updating() const;
    void setUpdating(bool value);

    AppState state() const;
    void setState(AppState value);

    Q_INVOKABLE QFuture<void> registerSelf();
    Q_INVOKABLE void requestModal(ModalTypes modal, QVariant data = QVariant());
    Q_INVOKABLE QFuture<void> loadAppImageList();
    Q_INVOKABLE QFuture<void> loadAppImageMetadata(const QUrl& url);
    Q_INVOKABLE QFuture<void> loadAppImageMetadata(const QString& path);
    Q_INVOKABLE void launchAppImage(const QUrl& url, const bool useTerminal = false);
    Q_INVOKABLE void launchAppImage(const QString& path, const bool useTerminal = false);
    Q_INVOKABLE void openDesktopFileInTextEditor(const QUrl& path);
    Q_INVOKABLE void openDesktopFileInTextEditor(const QString& path);
    Q_INVOKABLE QFuture<void> registerAppImage(const QUrl& url);
    Q_INVOKABLE QFuture<void> registerAppImage(const QString& path);
    Q_INVOKABLE QFuture<void> unregisterAppImage(const QUrl& url, bool deleteAppImage);
    Q_INVOKABLE QFuture<void> unregisterAppImage(const QString& path, bool deleteAppImage);
    Q_INVOKABLE QFuture<void> unlockAppImage(const QUrl& url);
    Q_INVOKABLE QFuture<void> unlockAppImage(const QString& path);
    Q_INVOKABLE QFuture<void> saveUpdateSettings();
    Q_INVOKABLE void checkForUpdate();
    Q_INVOKABLE void checkForAllUpdates();
    Q_INVOKABLE void updateAppImage(const QString& downloadUrl, const QString& version, const QString& date);
    Q_INVOKABLE void updateAllAppImages();
    Q_INVOKABLE void refreshDesktopFile();

private:
    explicit AppImageManager(QObject *parent = nullptr);

    static const QRegularExpression invalidChars;
    AppImageMetadataListModel* m_appImageList = nullptr;
    AppImageMetadata* m_appImageMetadata = nullptr;
    bool m_loadingAppImageList = false;
    bool m_loadingAppImage = false;
    bool m_updating = false;
    AppState m_state = AppList;

    QString appImagePath();
    UpdaterSettings getUpdaterSettings(AppImageMetadata* appImageMetadata);
    void loadMetadataUpdaterReleases(AppImageMetadata* appImageMetadata, std::function<void()> callback = nullptr);
    QFuture<void> loadMetadataUpdaterReleasesAsync(AppImageMetadata* appImage);
    UpdaterReleaseModel* getSelectedRelease(AppImageMetadata* metadata) const;
    QFuture<void> updateAppImageAsync(AppImageMetadata* metadata, UpdaterReleaseModel* release);

    Q_DISABLE_COPY(AppImageManager);

signals:
    void modalRequested(AppImageManager::ModalTypes modal, QVariant data = QVariant());
    void appImageListChanged();
    void appImageMetadataChanged(AppImageMetadata* newValue);
    void loadingAppImageListChanged(bool newValue);
    void loadingAppImageChanged(bool newValue);
    void updatingChanged(bool newValue);
    void stateChanged(AppImageManager::AppState newValue);
};

#endif // APPIMAGEMANAGER_H
