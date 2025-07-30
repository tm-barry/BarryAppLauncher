#include "appimagemanager.h"
#include "errormanager.h"
#include "settingsmanager.h"
#include "providers/memoryimageprovider.h"

#include <QGuiApplication>
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

AppImageMetadata* AppImageManager::appImageMetadata() const {
    return m_appImageMetadata;
}

void AppImageManager::setAppImageMetadata(AppImageMetadata* value) {
    if (m_appImageMetadata == value)
        return;

    if (m_appImageMetadata)
        m_appImageMetadata->deleteLater();

    m_appImageMetadata = value;

    emit appImageMetadataChanged(m_appImageMetadata);
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
        setBusy(true);
        AppImageUtil util(path);
        try {
            AppImageUtilMetadata metadata = util.metadata();
            QImage image(metadata.iconPath);
            MemoryImageProvider::instance()->setImage(path, image);
            QMetaObject::invokeMethod(QGuiApplication::instance(), [=]() {
                auto* appImageMetadata = parseAppImageMetadata(metadata);
                appImageMetadata->setIcon(MemoryImageProvider::instance()->getUrl(path));
                setAppImageMetadata(appImageMetadata);
            });
            setState(AppInfo);
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setBusy(false);
    });
}

void AppImageManager::launchAppImage(const QUrl& url)
{
    launchAppImage(url.toLocalFile());
}

void AppImageManager::launchAppImage(const QString& path)
{
    try {
        bool success = QProcess::startDetached(path);

        if(!success)
        {
            ErrorManager::instance()->reportError("Failed to launch appimage.");
        }
    } catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
    }
}

void AppImageManager::registerAppImage(const QUrl& url)
{
    // registerAppImage(url.toLocalFile());
}

void AppImageManager::registerAppImage(const QString& path)
{
    // QFuture<void> future = QtConcurrent::run([=]() {
    //     setBusy(true);
    //     try {
    //         QString newPath = handleIntegrationFileOperation(path);
    //         if(!newPath.isEmpty())
    //         {
    //             appimage::desktop_integration::IntegrationManager manager;
    //             appimage::core::AppImage appImage(newPath.toUtf8().constData());
    //             manager.registerAppImage(appImage);

    //             // Load new appimage metadata
    //             loadAppImageMetadata(newPath);
    //         }
    //         else
    //             ErrorManager::instance()->reportError("Failed to move/copy appimage.");
    //     } catch (const std::exception &e) {
    //         ErrorManager::instance()->reportError(e.what());
    //     }
    //     setBusy(false);
    // });
}

void AppImageManager::unregisterAppImage(const QUrl& url)
{
    // unregisterAppImage(url.toLocalFile());
}

void AppImageManager::unregisterAppImage(const QString& path)
{
    // QFuture<void> future = QtConcurrent::run([=]() {
    //     setBusy(true);
    //     try {
    //         appimage::desktop_integration::IntegrationManager manager;
    //         manager.unregisterAppImage(path.toUtf8().constData());

    //         // Load new appimage metadata
    //         loadAppImageMetadata(path);
    //     } catch (const std::exception &e) {
    //         ErrorManager::instance()->reportError(e.what());
    //     }
    //     setBusy(false);
    // });
}

// ----------------- Private -----------------

AppImageManager::AppImageManager(QObject *parent)
    : QObject{parent}
{}

const QRegularExpression AppImageManager::invalidChars(R"([/\\:*?"<>|])");

AppImageMetadata* AppImageManager::parseAppImageMetadata(const AppImageUtilMetadata& utilMetadata)
{
    AppImageMetadata::IntegrationType integrationType = AppImageMetadata::IntegrationType::None;
    if(!utilMetadata.desktopFilePath.isEmpty())
    {
        integrationType = utilMetadata.internalIntegration
                              ? AppImageMetadata::IntegrationType::Internal
                              : AppImageMetadata::IntegrationType::External;
    }

    auto* metadata = new AppImageMetadata();
    metadata->setName(utilMetadata.name);
    metadata->setVersion(utilMetadata.version);
    metadata->setComment(utilMetadata.comment);
    metadata->setType(utilMetadata.type);
    metadata->setMd5(utilMetadata.md5);
    metadata->setCategories(utilMetadata.categories);
    metadata->setPath(utilMetadata.path);
    metadata->setIntegration(integrationType);
    metadata->setDesktopFilePath(utilMetadata.desktopFilePath);

    return metadata;
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

    AppImageUtil util(path);
    AppImageUtilMetadata metadata = util.metadata(true);
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
