#ifndef APPIMAGEUTIL_H
#define APPIMAGEUTIL_H

#include <QProcess>
#include <QString>

struct AppImageUtilMetadata {
public:
    QString name = QString();
    QString version = QString();
    QString comment = QString();
    int type = 0;
    QString md5 = QString();
    QString categories = QString();
    QString path = QString();
    QString desktopFilePath = QString();
    bool internalIntegration = false;
    QString iconPath = QString();
    QString mountedDesktopContents = QString();
};

class AppImageUtil
{
public:
    AppImageUtil(const QString& path);
    ~AppImageUtil();

    /**
     * @brief Checks if the appimage is a type 2
     * @return Bool indicating if appimage is type 2
     */
    bool isAppImageType2();
    /**
     * @brief Gets the md5
     * @return Md5 of the AppImageUtil path
     */
    QString getMd5();
    /**
     * @brief Checks if the path is executable
     * @return Bool indicating if file is executable
     */
    bool isExecutable();
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
    QString integratedDesktopPath();
    /**
     * @brief Get the metadata for the appimage.
     * @param integration If true gets the metadata from the appimage's
     * internal desktop
     * @return The appimages metadata parsed from the desktop file.
     * If integrated it will use the integrated desktop file,
     * if not it will use the appimage's internal one.
     */
    AppImageUtilMetadata metadata(bool integration = false);
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
    bool unregisterAppImage();

private:
    const QString m_path;
    QString m_mountPath;
    QProcess* m_process;
    static const QRegularExpression execLineRegex;
    static const QRegularExpression invalidChars;
    static const QString balIntegrationField;

    void parseDesktopPathForMetadata(const QString& path, AppImageUtilMetadata& metadata, bool integration = false);
    QString findNextAvailableFilename(const QString& fullPath);
    QString handleIntegrationFileOperation(QString newName);
    QStringList getSearchPaths();
    QString getLocalIntegrationPath();
};

#endif // APPIMAGEUTIL_H
