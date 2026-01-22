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

    m_appImageList->sort();

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

QFuture<void> AppImageManager::registerSelf()
{
    return QtConcurrent::run([=, this]() {
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

QFuture<void> AppImageManager::loadAppImageList()
{
    return QtConcurrent::run([=, this]() {
        setLoadingAppImageList(true);
        try {
            auto utilList = AppImageUtil::getRegisteredList();
            for (const auto& app : utilList) {
                QImage image(app.iconPath);
                MemoryImageProvider::instance()->setImage(app.path, image);
            }
            QMetaObject::invokeMethod(QGuiApplication::instance(), [=, this]() {
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

QFuture<void> AppImageManager::loadAppImageMetadata(const QUrl& url) {
    return loadAppImageMetadata(url.toLocalFile());
}

QFuture<void> AppImageManager::loadAppImageMetadata(const QString& path) {
    return QtConcurrent::run([=, this]() {
        setLoadingAppImage(true);
        AppImageUtil util(path);
        try {
            AppImageUtilMetadata metadata = util.metadata();
            if(!metadata.iconPath.isEmpty())
            {
                QImage image(metadata.iconPath);
                MemoryImageProvider::instance()->setImage(path, image);
            }
            QMetaObject::invokeMethod(QGuiApplication::instance(), [=, this]() {
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

QFuture<void> AppImageManager::registerAppImage(const QUrl& url)
{
    return registerAppImage(url.toLocalFile());
}

QFuture<void> AppImageManager::registerAppImage(const QString& path)
{
    return QtConcurrent::run([=, this]() {
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

QFuture<void> AppImageManager::unregisterAppImage(const QUrl& url, bool deleteAppImage)
{
    return unregisterAppImage(url.toLocalFile(), deleteAppImage);
}

QFuture<void> AppImageManager::unregisterAppImage(const QString& path, bool deleteAppImage)
{
    return QtConcurrent::run([=, this]() {
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

QFuture<void> AppImageManager::unlockAppImage(const QUrl& url)
{
    return unlockAppImage(url.toLocalFile());
}

QFuture<void> AppImageManager::unlockAppImage(const QString& path)
{
    return QtConcurrent::run([=, this]() {
        setLoadingAppImage(true);
        try {
            if(AppImageUtil::makeExecutable(path))
            {
                loadAppImageMetadata(path);
            }
        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
        }
        setLoadingAppImage(false);
    });
}

QFuture<void> AppImageManager::saveUpdateSettings()
{
    return QtConcurrent::run([=, this]() {
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
        loadMetadataUpdaterReleases(m_appImageMetadata, [this]() { setLoadingAppImage(false); });
    } catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
        setLoadingAppImage(false);
    }
}

void AppImageManager::checkForAllUpdates()
{
    loadAppImageList().waitForFinished();
    setLoadingAppImageList(true);
    try {
        QList<QFuture<void>> futures;
        futures.reserve(m_appImageList->items().size());

        for (AppImageMetadata* appImage : m_appImageList->items()) {
            futures << loadMetadataUpdaterReleasesAsync(appImage);
        }

        if (futures.isEmpty()) {
            setLoadingAppImageList(false);
            return;
        }

        auto all = QtFuture::whenAll(futures.begin(), futures.end());

        all.then([this](auto) {
            setLoadingAppImageList(false);
            m_appImageList->updateAllItems();
            m_appImageList->sort();
        });
    }
    catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
        setLoadingAppImageList(false);
    }
}

void AppImageManager::updateAppImage(const QString& downloadUrl, const QString& version, const QString& date)
{
    setLoadingAppImage(true);
    try {
        AppImageUtil::updateAppImage(m_appImageMetadata->path(), downloadUrl, version, date, [this](bool success) {
            if(success) {
                loadAppImageMetadata(m_appImageMetadata->path());
                loadAppImageList();
            }

            setLoadingAppImage(false);
        });
    } catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
        setLoadingAppImage(false);
    }
}

void AppImageManager::updateAllAppImages()
{
    setLoadingAppImage(true);

    try {
        QList<QFuture<void>> futures;

        for (auto* metadata : m_appImageList->items()) {
            auto* selectedRelease = getSelectedRelease(metadata);
            if (!selectedRelease)
                continue;

            futures << updateAppImageAsync(metadata, selectedRelease);
        }

        if (futures.isEmpty()) {
            setLoadingAppImage(false);
            return;
        }

        auto all = QtFuture::whenAll(futures.begin(), futures.end());
        all.then([this](auto) {
            loadAppImageList();
            setLoadingAppImage(false);
        });
    }
    catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
        setLoadingAppImage(false);
    }
}

void AppImageManager::refreshDesktopFile()
{
    setLoadingAppImage(true);
    try {
        if(AppImageUtil::refreshDesktopFile(m_appImageMetadata->path())) {
            loadAppImageMetadata(m_appImageMetadata->path());
            loadAppImageList();
        }
    } catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
        setLoadingAppImage(false);
    }
    setLoadingAppImage(false);
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
    metadata->setUpdateVersionPattern(utilMetadata.updateVersionPattern);
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
    settings.versionPattern = metadata->updateVersionPattern();
    settings.downloadField = metadata->updateDownloadField();
    settings.downloadPattern = metadata->updateDownloadPattern();
    settings.dateField = metadata->updateDateField();

    for (const auto& filter : metadata->getUpdateFilters()) {
        settings.filters.append({filter->field(), filter->pattern()});
    }

    return settings;
}

void AppImageManager::loadMetadataUpdaterReleases(AppImageMetadata* appImageMetadata, std::function<void()> callback)
{
    QPointer<AppImageMetadata> metadata = appImageMetadata;
    if (!metadata || metadata->updateType().isEmpty())
    {
        if (callback) callback();
        return;
    }

    UpdaterSettings settings = getUpdaterSettings(metadata);
    auto* updater = UpdaterFactory::create(metadata->updateType(), settings);

    connect(updater, &IUpdater::updatesReady, this, [this, updater, metadata, callback]() {
        if (!metadata) {
            updater->deleteLater();
            if (callback) callback();
            return;
        }

        metadata->clearUpdaterReleases();
        bool markedLatest = false;
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

            if(isNew && !markedLatest) {
                releaseModel->setIsSelected(true);
                markedLatest = true;
            }

            metadata->addUpdaterRelease(releaseModel);
        }
        updater->deleteLater();
        if (callback) callback();
    });

    updater->fetchUpdatesAsync();
}

QFuture<void> AppImageManager::loadMetadataUpdaterReleasesAsync(AppImageMetadata* appImage)
{
    auto promise = QSharedPointer<QPromise<void>>::create();

    loadMetadataUpdaterReleases(appImage, [promise]() {
        promise->finish();
    });

    return promise->future();
}

UpdaterReleaseModel* AppImageManager::getSelectedRelease(AppImageMetadata* metadata) const
{
    auto releases = metadata->updaterReleases();
    if (!releases.count || !releases.at)
        return nullptr;

    int count = releases.count(&releases);
    for (int i = 0; i < count; ++i) {
        auto* release = releases.at(&releases, i);
        if (release && release->isSelected())
            return release;
    }
    return nullptr;
}

QFuture<void> AppImageManager::updateAppImageAsync(AppImageMetadata* metadata, UpdaterReleaseModel* release)
{
    auto promise = std::make_shared<QPromise<void>>();
    QFuture<void> future = promise->future();

    AppImageUtil::updateAppImage(metadata->path(),
                                 release->download(),
                                 release->version(),
                                 release->date(),
                                 [promise](bool) mutable {
                                     promise->finish();
                                 });

    return future;
}

