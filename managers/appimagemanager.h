#ifndef APPIMAGEMANAGER_H
#define APPIMAGEMANAGER_H

#pragma once

#include "models/appimagemetadata.h"
#include "models/appimagemetadatalistmodel.h"
#include "utils/appimageutil.h"

#include <QDir>
#include <QObject>
#include <QUrl>

class AppImageManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AppImageMetadataListModel* appImageList READ appImageList NOTIFY appImageListChanged)
    Q_PROPERTY(AppImageMetadata* appImageMetadata READ appImageMetadata WRITE setAppImageMetadata NOTIFY appImageMetadataChanged)
    Q_PROPERTY(bool loadingAppImageList READ loadingAppImageList WRITE setLoadingAppImageList NOTIFY loadingAppImageListChanged)
    Q_PROPERTY(bool loadingAppImage READ loadingAppImage WRITE setLoadingAppImage NOTIFY loadingAppImageChanged)
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
        About
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

    AppState state() const;
    void setState(AppState value);

    bool isRunningAsAppImage();
    QString appImagePath();

    Q_INVOKABLE void requestModal(ModalTypes modal);
    Q_INVOKABLE void loadAppImageList();
    Q_INVOKABLE void loadAppImageMetadata(const QUrl& url);
    Q_INVOKABLE void loadAppImageMetadata(const QString& path);
    Q_INVOKABLE void launchAppImage(const QUrl& url, const bool useTerminal = false);
    Q_INVOKABLE void launchAppImage(const QString& path, const bool useTerminal = false);
    Q_INVOKABLE void registerAppImage(const QUrl& url);
    Q_INVOKABLE void registerAppImage(const QString& path);
    Q_INVOKABLE void unregisterAppImage(const QUrl& url, bool deleteAppImage);
    Q_INVOKABLE void unregisterAppImage(const QString& path, bool deleteAppImage);
    Q_INVOKABLE void unlockAppImage(const QUrl& url);
    Q_INVOKABLE void unlockAppImage(const QString& path);

private:
    explicit AppImageManager(QObject *parent = nullptr);

    static const QRegularExpression invalidChars;
    AppImageMetadataListModel* m_appImageList = nullptr;
    AppImageMetadata* m_appImageMetadata = nullptr;
    bool m_loadingAppImageList = false;
    bool m_loadingAppImage = false;
    AppState m_state = AppList;

    AppImageMetadata* parseAppImageMetadata(const AppImageUtilMetadata& appImageMetadata);

    Q_DISABLE_COPY(AppImageManager);

signals:
    void modalRequested(AppImageManager::ModalTypes modal);
    void appImageListChanged();
    void appImageMetadataChanged(AppImageMetadata* newValue);
    void loadingAppImageListChanged(bool newValue);
    void loadingAppImageChanged(bool newValue);
    void stateChanged(AppImageManager::AppState newValue);
};

#endif // APPIMAGEMANAGER_H
