#include "appimagemanager.h"
#include "errormanager.h"
#include "settingsmanager.h"
#include "providers/memoryimageprovider.h"
#include "utils/updater/updaterfactory.h"
#include "utils/stringutil.h"
#include "utils/terminalutil.h"
#include "utils/texteditorutil.h"
#include "utils/versionutil.h"

#include <deque>
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

bool AppImageManager::loadingAppImageList() const { return m_loadingAppImageList; }
void AppImageManager::setLoadingAppImageList(bool value) {
    if (m_loadingAppImageList == value)
        return;
    m_loadingAppImageList = value;
    emit loadingAppImageListChanged(value);
}

bool AppImageManager::loadingAppImage() const { return m_loadingAppImage; }
void AppImageManager::setLoadingAppImage(bool value) {
    if (m_loadingAppImage == value)
        return;
    m_loadingAppImage = value;
    emit loadingAppImageChanged(value);
}

bool AppImageManager::updating() const { return m_updating; }
void AppImageManager::setUpdating(bool value) {
    if (m_updating == value)
        return;
    m_updating = value;
    emit updatingChanged(value);
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
    auto promise = QSharedPointer<QPromise<void>>::create();

    QThreadPool::globalInstance()->start([this, promise]() {
        try {
            auto utilList = AppImageUtil::getRegisteredList();
            // load icons
            for (const auto& app : utilList) {
                QImage image(app.iconPath);
                MemoryImageProvider::instance()->setImage(app.path, image);
            }

            // update appList on gui thread
            QMetaObject::invokeMethod(QGuiApplication::instance(), [this, utilList, promise]() {
                QList<AppImageMetadata*> appList;
                for (const auto& app : utilList) {
                    auto* meta = parseAppImageMetadata(app);
                    meta->setIcon(MemoryImageProvider::instance()->getUrl(app.path));
                    appList.append(meta);
                }
                setAppImageList(appList);
                setLoadingAppImageList(false);
                promise->finish();
            }, Qt::QueuedConnection);

        } catch (const std::exception &e) {
            ErrorManager::instance()->reportError(e.what());
            QMetaObject::invokeMethod(QGuiApplication::instance(), [this, promise]() {
                setLoadingAppImageList(false);
                promise->finish();
            }, Qt::QueuedConnection);
        }
    });

    setLoadingAppImageList(true);
    return promise->future();
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
    loadAppImageList().then([this] {
        setLoadingAppImageList(true);

        auto queue = std::make_shared<std::deque<AppImageMetadata*>>(
            m_appImageList->items().begin(),
            m_appImageList->items().end()
            );

        const int maxConcurrent = std::max(1, SettingsManager::instance()->updateConcurrency());
        auto running = std::make_shared<int>(0);
        auto next = std::make_shared<std::function<void()>>();
        QPointer<AppImageManager> self(this);
        *next = [self, queue, running, maxConcurrent, next]() mutable {

            if (!queue->empty() && *running < maxConcurrent) {

                auto* metadata = queue->front();
                queue->pop_front();

                (*running)++;

                self->loadMetadataUpdaterReleases(metadata, [self, running, next]() mutable {

                    if (!self)
                        return;

                    (*running)--;

                    QMetaObject::invokeMethod(qApp, [next]() { (*next)(); }, Qt::QueuedConnection);
                });

                QMetaObject::invokeMethod(qApp, [next]() { (*next)(); }, Qt::QueuedConnection);

                return;
            }

            if (queue->empty() && *running == 0) {
                self->setLoadingAppImageList(false);
                self->m_appImageList->updateAllItems();
                self->m_appImageList->sort();
            }
        };

        for (int i = 0; i < maxConcurrent; ++i)
            (*next)();
    });
}

void AppImageManager::updateAppImage(const QString& downloadUrl, const QString& version, const QString& date)
{
    setUpdating(true);
    try {
        AppImageUtil::updateAppImage(m_appImageMetadata->path(), downloadUrl, version, date,
                                     [this](bool success) {
                                         if(success) {
                                             loadAppImageMetadata(m_appImageMetadata->path());
                                             loadAppImageList();
                                         }

                                         setUpdating(false);
                                     },
                                     [this](UpdateState state, qint64 received, qint64 total) {
                                         m_appImageMetadata->setUpdateProgressState(state);
                                         m_appImageMetadata->setUpdateBytesReceived(received);
                                         m_appImageMetadata->setUpdateBytesTotal(total);
                                     }
                                     );
    } catch (const std::exception &e) {
        ErrorManager::instance()->reportError(e.what());
        setUpdating(false);
    }
}

void AppImageManager::updateAllAppImages()
{
    setUpdating(true);

    auto queue = std::make_shared<std::deque<AppImageMetadata*>>(
        m_appImageList->items().begin(), m_appImageList->items().end()
        );
    const int maxConcurrent = std::max(1, SettingsManager::instance()->updateConcurrency());
    auto running = std::make_shared<int>(0);

    auto next = std::make_shared<std::function<void()>>();

    *next = [this, queue, running, maxConcurrent, next]() mutable {
        if (!queue->empty() && *running < maxConcurrent) {
            auto* metadata = queue->front();
            queue->pop_front();

            auto* release = getSelectedRelease(metadata);
            if (!release) {
                QMetaObject::invokeMethod(qApp, [next]() { (*next)(); }, Qt::QueuedConnection);
                return;
            }

            (*running)++;
            auto future = updateAppImageAsync(metadata, release);

            future.then([running, next, queue]() mutable {
                (*running)--;
                QMetaObject::invokeMethod(qApp, [next]() { (*next)(); }, Qt::QueuedConnection);
            });

            QMetaObject::invokeMethod(qApp, [next]() { (*next)(); }, Qt::QueuedConnection);
            return;
        }

        if (queue->empty() && *running == 0) {
            bool anyFailed = false;
            for (auto* item : m_appImageList->items()) {
                if (item->updateProgressState() == AppImageMetadata::UpdateProgressState::Failed) {
                    anyFailed = true;
                    break;
                }
            }

            if (!anyFailed) {
                loadAppImageList();
            }

            setUpdating(false);
        }
    };

    for (int i = 0; i < maxConcurrent; ++i)
        (*next)();
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
                          || (VersionUtil::compareVersions(r.version, metadata->updateCurrentVersion()) == 1))
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
                                 [promise, metadata, this](bool) mutable {
                                     if(metadata->updateProgressState() == AppImageMetadata::Success)
                                     {
                                         auto selectedRelease = metadata->getSelectedRelease();
                                         if(selectedRelease != nullptr)
                                         {
                                             metadata->setVersion(selectedRelease->version());
                                         }
                                         metadata->clearUpdaterReleases();
                                         m_appImageList->sort();
                                     }
                                     promise->finish();
                                 },
                                 [promise, metadata, this](UpdateState state, qint64 received, qint64 total) mutable {
                                     metadata->setUpdateProgressState(state);
                                     metadata->setUpdateBytesReceived(received);
                                     metadata->setUpdateBytesTotal(total);
                                     m_appImageList->updateItem(metadata);
                                 });

    return future;
}
