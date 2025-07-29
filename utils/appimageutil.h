#ifndef APPIMAGEUTIL_H
#define APPIMAGEUTIL_H

#include <QProcess>
#include <QString>

struct AppImageUtilMetadata {
public:
    QString name;
    QString version;
    QString comment;
    int type;
    QString md5;
    QString categories;
    QString path;
    QString desktopFilePath;
    bool internalIntegration;
    QString iconPath;
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
     * @param forceInternal If true gets the metadata from the appimage's
     * internal desktop
     * @return The appimages metadata parsed from the desktop file.
     * If integrated it will use the integrated desktop file,
     * if not it will use the appimage's internal one.
     */
    AppImageUtilMetadata metadata(bool forceInternal = false);

private:
    const QString m_path;
    QString m_mountPath;
    QProcess* m_process;

    void parseDesktopPathForMetadata(const QString& path, AppImageUtilMetadata& metadata);
    QString getMountedIconPath();
};

#endif // APPIMAGEUTIL_H
