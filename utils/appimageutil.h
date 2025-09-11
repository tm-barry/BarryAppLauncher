#ifndef APPIMAGEUTIL_H
#define APPIMAGEUTIL_H

#include "utils/updater/updaterfactory.h"

#include <QCryptographicHash>
#include <QProcess>
#include <QString>

struct AppImageUtilMetadata {
public:
    // Base fields
    QString name = QString();
    QString version = QString();
    QString comment = QString();
    int type = 0;
    QString checksum = QString();
    QString categories = QString();
    QString path = QString();
    QString desktopFilePath = QString();
    bool internalIntegration = false;
    QString iconPath = QString();
    QString mountedDesktopContents = QString();
    bool executable = false;

    // Update fields
    QString updateType = QString();
    QString updateUrl = QString();
    QString updateDownloadField = QString();
    QString updateDownloadPattern = QString();
    QString updateDateField = QString();
    QString updateVersionField = QString();
    QList<UpdaterFilter> updateFilters { };
};

enum MetadataAction {
    Default,
    Register,
    Unregister
};

class AppImageUtil
{
public:
    AppImageUtil(const QString& path);
    ~AppImageUtil();

    /**
     * @brief Checks if the appimage is a type 2
     * @param path Path to appimage
     * @return Bool indicating if appimage is type 2
     */
    static const bool isAppImageType2(const QString& path);
    /**
     * @brief Gets the checksum of the given path
     * @param path Path to file you want the checksum
     * @param hashType Hash type algorithm to use
     * @return Checksum of the file at path
     */
    static const QString getChecksum(const QString& path, const QCryptographicHash::Algorithm hashType = QCryptographicHash::Sha256);
    /**
     * @brief Checks if the path is executable
     * @param path Path to file to check is executable
     * @return Bool indicating if file at path is executable
     */
    static const bool isExecutable(const QString& path);
    /**
     * @brief Makes the appimage at path executable
     * @return True if successful, false if not
     */
    bool makeExecutable();
    /**
     * @brief Mounts an appimage
     * and process. The process must be cleaned up using unmountAppImage(...)
     */
    void mountAppImage();
    /**
     * @brief Checks if appimage is currently mounted
     * @return Bool indicating if appimage is mounted
     */
    bool isMounted();
    /**
     * @brief Unmounts the appimage mount process
     */
    void unmountAppImage();
    /**
     * @brief Get the integrated desktop file path
     * @return Integrated desktop file path, or empty if not integrated
     */
    static const QString integratedDesktopPath(const QString& path);
    /**
     * @brief Get the metadata for the appimage.
     * @param integration If true gets the metadata from the appimage's
     * internal desktop
     * @return The appimages metadata parsed from the desktop file.
     * If integrated it will use the integrated desktop file,
     * if not it will use the appimage's internal one.
     */
    AppImageUtilMetadata metadata(MetadataAction action = Default);
    /**
     * @brief Gets the mounted appimages desktop path
     * @return Destkop path of the mounted appimage
     */
    QString getMountedDesktopPath();
    /**
     * @brief Gets the mounted appimages icon path
     * @return Icon path of the mounted appimage
     */
    QString getMountedIconPath();
    /**
     * @brief Registers the app image at the utils path
     * @return New path of registered app image
     *
     */
    QString registerAppImage();
    /**
     * @brief Unregisters the app image at the utils path
     * @return Bool indicating if unregister is successful
     */
    /**
     * @brief Unregisters the app image at the utils path
     * @param deleteAppImage Should the appimage be deleted after unregistered
     * @return Bool indicating if unregister is successful
     */
    bool unregisterAppImage(bool deleteAppImage);
    /**
     * @brief Get ths list of registered appimages
     * @return List of registered appimages
     */
    static const QList<AppImageUtilMetadata> getRegisteredList();
    /**
     * @brief Save the updater settings to the provided desktop file
     * @param desktopFilePath path to the desktop file
     * @param updaterType type of updater
     * @param settings Updater settings to save
     * @return Bool indicating if save successful
     */
    static const bool saveUpdaterSettings(const QString& desktopFilePath, const QString& updaterType, const UpdaterSettings& settings);

private:
    const QString m_path;
    QString m_mountPath;
    QProcess* m_process;
    static const QRegularExpression execLineRegex;
    static const QRegularExpression invalidChars;
    static const QString balIntegrationField;

    static const QString escapeDesktopValue(const QString &value);
    static const void parseDesktopPathForMetadata(const QString& path, AppImageUtilMetadata& metadata, bool storeDesktopContent = false);
    static const QList<UpdaterFilter> parseFilters(const QString &filterStr);
    QString findNextAvailableFilename(const QString& fullPath);
    QString handleIntegrationFileOperation(QString newName);
    static const QStringList getSearchPaths();
    static const QString getLocalIntegrationPath();
    static const bool removeFileOrWarn(const QString& path, const QString& label);
};

#endif // APPIMAGEUTIL_H
