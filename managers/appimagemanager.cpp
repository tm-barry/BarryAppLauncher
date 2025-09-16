#include "appimagemanager.h"
#include "errormanager.h"
#include "providers/memoryimageprovider.h"
#include "utils/updater/updaterfactory.h"
#include "utils/semverutil.h"
#include "utils/stringutil.h"
#include "utils/terminalutil.h"
#include "utils/texteditorutil.h"

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

void AppImageManager::registerSelf()
{
    QFuture<void> future = QtConcurrent::run([=]() {
        setLoadingAppImage(true);
        try {
            QString path = appImagePath();
            if(!path.isEmpty())
            {
                AppImageUtil util(path);
                QString integratedDesktopPath = util.integratedDesktopPath(path);

                // Check if not already integrated
                if(integratedDesktopPath.isEmpty())
                {
                    QString newPath = util.registerAppImage();

                    // If we integrated successfully, close and restart the app.
                    if(!newPath.isEmpty())
                    {
                        QProcess::startDetached(newPath);
                        QCoreApplication::quit();
                    }
                }
                else
                {
                    ErrorManager::instance()->reportError("BarryAppLauncher is already registered in the desktop menu.");
                }
            }
            else
            {
                ErrorManager::instance()->reportError("Unable to find appimage: " + path);
            }
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setLoadingAppImage(false);
    });
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

void AppImageManager::openDesktopFileInTextEditor(const QUrl& url)
{
    launchAppImage(url.toLocalFile());
}

void AppImageManager::openDesktopFileInTextEditor(const QString& path)
{
    try {
        if(!TextEditorUtil::launchInTextEditor(path))
        {
            ErrorManager::instance()->reportError("Failed to open desktop file.");
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

void AppImageManager::saveUpdateSettings()
{
    QFuture<void> future = QtConcurrent::run([=]() {
        setLoadingAppImage(true);
        try {
            QPointer<AppImageMetadata> metadata = m_appImageMetadata;
            if (!metadata)
                return;

            UpdaterSettings settings = getUpdaterSettings(metadata);

            bool saved = AppImageUtil::saveUpdaterSettings(
                metadata->desktopFilePath(),
                metadata->updateType(),
                settings
             );

            if (saved && metadata) {
                QMetaObject::invokeMethod(metadata, "setUpdateDirty", Qt::QueuedConnection, Q_ARG(bool, false));
            }
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setLoadingAppImage(false);
    });
}

void AppImageManager::checkForUpdate()
{
    setLoadingAppImage(true);
    try {
        QPointer<AppImageMetadata> metadata = m_appImageMetadata;
        if (!metadata)
            return;

        UpdaterSettings settings = getUpdaterSettings(metadata);
        auto* updater = UpdaterFactory::create(metadata->updateType(), settings);

        connect(updater, &IUpdater::updatesReady, this, [this, updater, metadata]() {
            if (!metadata) {
                updater->deleteLater();
                setLoadingAppImage(false);
                return;
            }

            metadata->clearUpdaterReleases();
            for (const auto &r : updater->releases()) {
                auto* releaseModel = new UpdaterReleaseModel(metadata);
                releaseModel->setVersion(r.version);
                releaseModel->setDate(r.date);
                releaseModel->setDownload(r.download);

                bool isNew = (metadata->updateCurrentVersion().isEmpty()
                              || (SemVerUtil::compareStrings(r.version, metadata->updateCurrentVersion()) == 1))
                             && (metadata->updateCurrentDate().isEmpty()
                                 || (StringUtil::parseDateTime(r.date) > StringUtil::parseDateTime(metadata->updateCurrentDate())));
                releaseModel->setIsNew(isNew);

                metadata->addUpdaterRelease(releaseModel);
            }
            updater->deleteLater();
            setLoadingAppImage(false);
        });

        updater->fetchUpdatesAsync();
    } catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
        setLoadingAppImage(false);
    }
}

// ----------------- Private -----------------

AppImageManager::AppImageManager(QObject *parent)
    : QObject(parent), m_appImageList(new AppImageMetadataListModel(this))
{}

const QRegularExpression AppImageManager::invalidChars(R"([/\\:*?"<>|])");

QString AppImageManager::appImagePath() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path = env.value("APPIMAGE");
    return QFile::exists(path) ? path : QString();
}

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
    metadata->setUpdateType(utilMetadata.updateType);
    metadata->setUpdateUrl(utilMetadata.updateUrl);
    metadata->setUpdateDownloadField(utilMetadata.updateDownloadField);
    metadata->setUpdateDownloadPattern(utilMetadata.updateDownloadPattern);
    metadata->setUpdateDateField(utilMetadata.updateDateField);
    metadata->setUpdateVersionField(utilMetadata.updateVersionField);
    for (const auto& filter : utilMetadata.updateFilters) {
        auto* filterModel = new UpdaterFilterModel(metadata);
        filterModel->setField(filter.field);
        filterModel->setPattern(filter.pattern);
        metadata->addUpdateFilter(filterModel);
    }
    metadata->setUpdateCurrentDate(utilMetadata.updateCurrentDate);
    metadata->setUpdateCurrentVersion(utilMetadata.updateCurrentVersion);
    metadata->setUpdateDirty(false);

    return metadata;
}

UpdaterSettings AppImageManager::getUpdaterSettings(AppImageMetadata* metadata)
{
    UpdaterSettings settings;
    settings.url = metadata->updateUrl();
    settings.versionField = metadata->updateVersionField();
    settings.downloadField = metadata->updateDownloadField();
    settings.downloadPattern = metadata->updateDownloadPattern();
    settings.dateField = metadata->updateDateField();

    for (const auto& filter : metadata->getUpdateFilters()) {
        settings.filters.append({filter->field(), filter->pattern()});
    }

    return settings;
}
