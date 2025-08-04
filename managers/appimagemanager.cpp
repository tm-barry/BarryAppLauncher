#include "appimagemanager.h"
#include "errormanager.h"
#include "providers/memoryimageprovider.h"
#include "utils/terminalutil.h"

#include <QGuiApplication>
#include <QtConcurrent/QtConcurrentRun>
#include <QDir>
#include <QImage>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

// ----------------- Public -----------------

AppImageManager* AppImageManager::instance() {
    static AppImageManager singleton;
    return &singleton;
}

AppImageMetadataListModel* AppImageManager::appImageList() const
{
    return m_appImageList;
}

void AppImageManager::setAppImageList(const QList<AppImageMetadata*>& list)
{
    m_appImageList->clear();

    for (auto* metadata : list) {
        m_appImageList->addMetadata(metadata);
    }

    emit appImageListChanged();
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

bool AppImageManager::loadingAppImageList() const {
    return m_loadingAppImageList;
}

void AppImageManager::setLoadingAppImageList(bool value) {
    if (m_loadingAppImageList == value)
        return;
    m_loadingAppImageList = value;
    emit loadingAppImageListChanged(value);
}

bool AppImageManager::loadingAppImage() const {
    return m_loadingAppImage;
}

void AppImageManager::setLoadingAppImage(bool value) {
    if (m_loadingAppImage == value)
        return;
    m_loadingAppImage = value;
    emit loadingAppImageChanged(value);
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

bool AppImageManager::isRunningAsAppImage() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    return env.contains("APPIMAGE") && QFile::exists(env.value("APPIMAGE"));
}

QString AppImageManager::appImagePath() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    return env.value("APPIMAGE");
}

void AppImageManager::requestModal(ModalTypes modal)
{
    emit modalRequested(modal);
}

void AppImageManager::loadAppImageList()
{
    QFuture<void> future = QtConcurrent::run([=]() {
        setLoadingAppImageList(true);
        try {
            auto utilList = AppImageUtil::getRegisteredList();
            for (const auto& app : utilList) {
                QImage image(app.iconPath);
                MemoryImageProvider::instance()->setImage(app.path, image);
            }
            QMetaObject::invokeMethod(QGuiApplication::instance(), [=]() {
                QList<AppImageMetadata*> appList;
                for (const auto& app : utilList) {
                    auto* meta = parseAppImageMetadata(app);
                    meta->setIcon(MemoryImageProvider::instance()->getUrl(app.path));
                    appList.append(meta);
                }
                setAppImageList(appList);
            });
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setLoadingAppImageList(false);
    });
}

void AppImageManager::loadAppImageMetadata(const QUrl& url) {
    loadAppImageMetadata(url.toLocalFile());
}

void AppImageManager::loadAppImageMetadata(const QString& path) {
    QFuture<void> future = QtConcurrent::run([=]() {
        setLoadingAppImage(true);
        AppImageUtil util(path);
        try {
            AppImageUtilMetadata metadata = util.metadata();
            if(!metadata.iconPath.isEmpty())
            {
                QImage image(metadata.iconPath);
                MemoryImageProvider::instance()->setImage(path, image);
            }
            QMetaObject::invokeMethod(QGuiApplication::instance(), [=]() {
                auto* appImageMetadata = parseAppImageMetadata(metadata);
                if(!metadata.iconPath.isEmpty())
                {
                    appImageMetadata->setIcon(MemoryImageProvider::instance()->getUrl(path));
                }
                setAppImageMetadata(appImageMetadata);
            });
            setState(AppInfo);
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setLoadingAppImage(false);
    });
}

void AppImageManager::launchAppImage(const QUrl& url, const bool useTerminal)
{
    launchAppImage(url.toLocalFile(), useTerminal);
}

void AppImageManager::launchAppImage(const QString& path, const bool useTerminal)
{
    try {
        bool success = false;
        if(!useTerminal)
        {
            success = QProcess::startDetached(path);
        }
        else
        {
            success = TerminalUtil::launchInTerminal(path);
        }

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
    registerAppImage(url.toLocalFile());
}

void AppImageManager::registerAppImage(const QString& path)
{
    QFuture<void> future = QtConcurrent::run([=]() {
        setLoadingAppImage(true);
        try {
            AppImageUtil util(path);
            QString newPath = util.registerAppImage();
            if(!newPath.isEmpty())
            {
                loadAppImageMetadata(newPath);
                loadAppImageList();
            }
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setLoadingAppImage(false);
    });
}

void AppImageManager::unregisterAppImage(const QUrl& url, bool deleteAppImage)
{
    unregisterAppImage(url.toLocalFile(), deleteAppImage);
}

void AppImageManager::unregisterAppImage(const QString& path, bool deleteAppImage)
{
    QFuture<void> future = QtConcurrent::run([=]() {
        setLoadingAppImage(true);
        try {
            AppImageUtil util(path);
            if(util.unregisterAppImage(deleteAppImage))
            {
                loadAppImageList();
                if(deleteAppImage)
                {
                    setState(AppList);
                }
                else
                {
                    loadAppImageMetadata(path);
                }
            }
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setLoadingAppImage(false);
    });
}

void AppImageManager::unlockAppImage(const QUrl& url)
{
    unlockAppImage(url.toLocalFile());
}

void AppImageManager::unlockAppImage(const QString& path)
{
    QFuture<void> future = QtConcurrent::run([=]() {
        setLoadingAppImage(true);
        try {
            AppImageUtil util(path);
            if(util.makeExecutable())
            {
                loadAppImageMetadata(path);
            }
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setLoadingAppImage(false);
    });
}

// ----------------- Private -----------------

AppImageManager::AppImageManager(QObject *parent)
    : QObject(parent), m_appImageList(new AppImageMetadataListModel(this))
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

    auto* metadata = new AppImageMetadata(this);
    metadata->setName(utilMetadata.name);
    metadata->setVersion(utilMetadata.version);
    metadata->setComment(utilMetadata.comment);
    metadata->setType(utilMetadata.type);
    metadata->setChecksum(utilMetadata.checksum);
    metadata->setCategories(utilMetadata.categories);
    metadata->setPath(utilMetadata.path);
    metadata->setIntegration(integrationType);
    metadata->setDesktopFilePath(utilMetadata.desktopFilePath);
    metadata->setExecutable(utilMetadata.executable);

    return metadata;
}
