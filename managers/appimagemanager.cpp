#include "appimagemanager.h"
#include "errormanager.h"
#include "settingsmanager.h"
#include "providers/memoryimageprovider.h"

#include <appimage/appimage.h>
#include <appimage/core/AppImage.h>
#include <appimage/desktop_integration/IntegrationManager.h>

#include <QtConcurrent/QtConcurrentRun>
#include <QDir>
#include <QImage>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

// ----------------- Public -----------------

AppImageManager* AppImageManager::instance() {
    static AppImageManager singleton;
    return &singleton;
}

AppImageMetadata AppImageManager::appImageMetadata() {
    return m_appImageMetadata;
}

void AppImageManager::setAppImageMetadata(AppImageMetadata value) {
    m_appImageMetadata = value;
    emit appImageMetadataChanged(value);
}

bool AppImageManager::busy() const {
    return m_busy;
}

void AppImageManager::setBusy(bool value) {
    if (m_busy == value)
        return;
    m_busy = value;
    emit busyChanged(value);
}

AppImageManager::AppState AppImageManager::state() const {
    return m_state;
}

void AppImageManager::setState(AppState value) {
    if (m_state == value)
        return;
    m_state = value;
    emit stateChanged(value);
}

void AppImageManager::loadAppImageMetadata(const QUrl& url) {
    loadAppImageMetadata(url.toLocalFile());
}

void AppImageManager::loadAppImageMetadata(const QString& path) {
    QFuture<void> future = QtConcurrent::run([=]() {
        AppImageMetadata appImageMetadata;
        setBusy(true);
        try {
            appImageMetadata = getAppImageMetadata(path);
            setAppImageMetadata(appImageMetadata);
            setState(AppInfo);

        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setBusy(false);
    });
}

bool AppImageManager::unlockAppImage(const QUrl& url)
{
    return unlockAppImage(url.toLocalFile());
}

bool AppImageManager::unlockAppImage(const QString& path)
{
    try {
        QFile file(path);
        if (!file.exists()) {
            ErrorManager::instance()->reportError("File does not exist: " + path);
            return false;
        }

        QFile::Permissions perms = file.permissions();
        perms |= QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther;

        if (!file.setPermissions(perms)) {
            ErrorManager::instance()->reportError("Failed to set executable permissions for: " + path);
            return false;
        }
    } catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
        return false;
    }

    return true;
}

void AppImageManager::launchAppImage(const QUrl& url)
{
    launchAppImage(url.toLocalFile());
}

void AppImageManager::launchAppImage(const QString& path)
{
    try {
        QProcess::startDetached(path);
    } catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
    }
}

void AppImageManager::registerAppImage(const QUrl& url)
{
    registerAppImage(url.toLocalFile());
}

void AppImageManager::registerAppImage(const QString& path)
{
    QFuture<void> future = QtConcurrent::run([=]() {
        setBusy(true);
        try {
            QString newPath = handleIntegrationFileOperation(path);
            if(!newPath.isEmpty())
            {
                appimage::desktop_integration::IntegrationManager manager;
                appimage::core::AppImage appImage(newPath.toUtf8().constData());
                manager.registerAppImage(appImage);

                // Load new appimage metadata
                loadAppImageMetadata(newPath);
            }
            else
                ErrorManager::instance()->reportError("Failed to move/copy appimage.");
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setBusy(false);
    });
}

void AppImageManager::unregisterAppImage(const QUrl& url)
{
    unregisterAppImage(url.toLocalFile());
}

void AppImageManager::unregisterAppImage(const QString& path)
{
    QFuture<void> future = QtConcurrent::run([=]() {
        setBusy(true);
        try {
            appimage::desktop_integration::IntegrationManager manager;
            manager.unregisterAppImage(path.toUtf8().constData());

            // Load new appimage metadata
            loadAppImageMetadata(path);
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setBusy(false);
    });
}

// ----------------- Private -----------------

AppImageManager::AppImageManager(QObject *parent)
    : QObject{parent}
{}

const QRegularExpression AppImageManager::invalidChars(R"([/\\:*?"<>|])");

AppImageMetadata AppImageManager::getAppImageMetadata(const QString& path) {
    AppImageMetadata appImageMetadata;
    appImageMetadata.path = path;
    int type = appimage_get_type(path.toUtf8().constData(), false);
    appImageMetadata.type = type;
    appImageMetadata.executable = isExecutable(path);

    if (type <= 0) {
        ErrorManager::instance()->reportError("Unsupported AppImage type");
        return appImageMetadata;
    }

    char* md5 = appimage_get_md5(path.toUtf8().constData());
    appImageMetadata.md5 = QString::fromUtf8(md5);
    QString desktopPath = appimage_registered_desktop_file_path(path.toUtf8().constData(), md5, false);
    if(desktopPath != nullptr)
    {
        appImageMetadata.integration = AppImageMetadata::Internal;
    }
    else
    {
        desktopPath = getDesktopFileForExecutable(path);
        if(desktopPath != nullptr)
        {
            appImageMetadata.integration = AppImageMetadata::External;
        }
    }

    QString desktopContent;
    if(appImageMetadata.integration == AppImageMetadata::None) {
        desktopContent = getInternalAppImageDesktopContent(path);
    }
    else
    {
        appImageMetadata.desktopFilePath = desktopPath;
        desktopContent = getExternalAppImageDesktopContent(desktopPath);
    }

    MemoryImageProvider::instance()->setImage(path, getAppImageIcon(path));
    appImageMetadata.icon = MemoryImageProvider::instance() -> getUrl(path);

    loadMetadataFromDesktopContent(appImageMetadata, desktopContent);

    return appImageMetadata;
}

QString AppImageManager::getDesktopFileForExecutable(const QString& executablePath) {
    const QStringList searchPaths = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        QDir::homePath() + "/.local/share/applications"
    };

    for (const QString& dirPath : searchPaths) {
        QDir dir(dirPath);
        const QStringList desktopFiles = dir.entryList(QStringList() << "*.desktop", QDir::Files);

        for (const QString& fileName : desktopFiles) {
            QString filePath = dir.absoluteFilePath(fileName);
            QFile file(filePath);

            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();

                if (line.startsWith("Exec=")) {
                    QString execLine = line.mid(QString("Exec=").length());
                    const QStringList execCommandParts = execLine.split(' ');

                    for (const QString& execCommand : execCommandParts)
                    {
                        if (execCommand == executablePath) {
                            return filePath;
                        }
                    }
                }
            }
        }
    }
    return QString();
}

QString AppImageManager::getInternalAppImageDesktopContent(const QString& appImagePath)
{
    QString desktopContent;
    QString desktopPath;
    char** files = appimage_list_files(appImagePath.toUtf8().constData());
    for (int i = 0; files[i] != nullptr; ++i) {
        QString entry = QString::fromUtf8(files[i]);
        if (entry.endsWith(".desktop")) {
            desktopPath = entry;
            break;
        }
    }

    appimage_string_list_free(files);

    if (desktopPath.isEmpty()) {
        qWarning() << "No .desktop file found:" << appImagePath;
    }
    else {
        char* buffer = nullptr;
        unsigned long bufferSize = 0;
        bool success = appimage_read_file_into_buffer_following_symlinks(
            appImagePath.toUtf8().constData(),
            desktopPath.toUtf8().constData(),
            &buffer,
            &bufferSize
            );

        if (!success || !buffer || bufferSize == 0) {
            qWarning() << "Failed to read .desktop file contents:" << appImagePath;
            return {};
        }

        desktopContent = QString::fromUtf8(QByteArray(buffer, bufferSize));
        free(buffer);
    }

    return desktopContent;
}

QString AppImageManager::getExternalAppImageDesktopContent(const QString& desktopPath)
{
    QFile file(desktopPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    return content;
}

QImage AppImageManager::getAppImageIcon(const QString& path)
{
    QImage iconImage;
    char* buffer = nullptr;
    unsigned long bufferSize = 0;
    bool success = appimage_read_file_into_buffer_following_symlinks(
        path.toUtf8().constData(),
        ".DirIcon",
        &buffer,
        &bufferSize
        );

    if (!success || !buffer || bufferSize == 0) {
        qWarning() << "Failed to get appimage icon:" << path;
        return iconImage;
    }

    QByteArray arr(buffer, bufferSize);
    iconImage.loadFromData(arr);
    free(buffer);
    return iconImage;
}

void AppImageManager::loadMetadataFromDesktopContent(AppImageMetadata& appImageMetadata, const QString& desktopContent)
{
    const QStringList lines = desktopContent.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("Name=")) {
            appImageMetadata.name = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("X-AppImage-Version=")) {
            appImageMetadata.version = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("Comment=")) {
            appImageMetadata.comment = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("Categories=")) {
            appImageMetadata.categories = trimmed.section('=', 1).trimmed();
        }
    }
}

bool AppImageManager::isExecutable(const QString &filePath) {
    QFileInfo fileInfo(filePath);
    return fileInfo.isExecutable();
}

QString AppImageManager::findNextAvailableFilename(const QString& fullPath) {
    QFileInfo fileInfo(fullPath);
    QDir dir = fileInfo.dir();
    QString baseName = fileInfo.completeBaseName();
    QString extension = fileInfo.suffix();

    QString fileName = fileInfo.fileName();
    int counter = 1;

    while (dir.exists(fileName)) {
        if (extension.isEmpty()) {
            fileName = QString("%1(%2)").arg(baseName).arg(counter);
        } else {
            fileName = QString("%1(%2).%3").arg(baseName).arg(counter).arg(extension);
        }
        counter++;
    }

    return dir.filePath(fileName);
}

QString AppImageManager::handleIntegrationFileOperation(const QString& path)
{
    if (!QFile::exists(path)) {
        qWarning() << "Source file does not exist:" << path;
        return "";
    }

    AppImageMetadata metadata;
    QString desktopContent = getInternalAppImageDesktopContent(path);
    loadMetadataFromDesktopContent(metadata, desktopContent);
    QString appName = metadata.name;
    appName.replace(invalidChars, "_");
    if(appName.isEmpty())
    {
        QFileInfo info(path);
        appName = info.baseName();
    }
    QString fileName = QString("%1.appimage").arg((appName).trimmed().toLower());
    QDir dir(SettingsManager::instance()->appImageDefaultLocation().toLocalFile());
    QString newPath = dir.filePath(fileName);
    newPath = findNextAvailableFilename(newPath);

    bool success = false;
    switch(SettingsManager::instance()->appImageFileOperation()) {
    case SettingsManager::Move:
        success = QFile::rename(path, newPath);
        break;
    case SettingsManager::Copy:
    default:
        success = QFile::copy(path, newPath);
        break;
    }

    return success ? newPath : "";
}
