#include "appimagemanager.h"
#include "errormanager.h"
#include "settingsmanager.h"
#include "providers/memoryimageprovider.h"
#include "utils/updater/updaterfactory.h"
#include "utils/terminalutil.h"
#include "utils/texteditorutil.h"

#include <algorithm>
#include <functional>
#include <memory>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFuture>
#include <QGuiApplication>
#include <QImage>
#include <QList>
#include <QPointer>
#include <QProcess>
#include <QPromise>
#include <QRegularExpression>
#include <QThread>
#include <QThreadPool>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

// ----------------- Local helpers -----------------

namespace {

using GuardedMetadata = QPointer<AppImageMetadata>;

// Runs fn on obj's thread: immediately if we're already there, otherwise queued.
template <typename Fn>
void runOnThreadOf(QObject* obj, Fn&& fn)
{
    if (QThread::currentThread() == obj->thread())
        fn();
    else
        QMetaObject::invokeMethod(obj, std::forward<Fn>(fn), Qt::QueuedConnection);
}

// "Busy" is reference counted so overlapping operations (e.g. register -> reload metadata)
// don't flip loadingAppImage off while another one is still running. The counter is only
// touched on the manager's thread, so no locking is needed.
int g_busyCount = 0;

void busyBegin(AppImageManager* manager)
{
    runOnThreadOf(manager, [manager]() {
        if (g_busyCount++ == 0)
            manager->setLoadingAppImage(true);
    });
}

void busyEnd(AppImageManager* manager)
{
    runOnThreadOf(manager, [manager]() {
        if (--g_busyCount == 0)
            manager->setLoadingAppImage(false);
    });
}

// Runs fn on a worker thread while the manager is flagged busy. Exceptions are reported.
template <typename Fn>
QFuture<void> runBusy(AppImageManager* manager, Fn fn)
{
    busyBegin(manager);
    return QtConcurrent::run([manager, fn = std::move(fn)]() mutable {
        try {
            fn();
        } catch (const std::exception& e) {
            ErrorManager::instance()->reportError(e.what());
        } catch (...) {
            ErrorManager::instance()->reportError("Unknown error.");
        }
        busyEnd(manager);
    });
}

// Safe to call from any thread.
void reloadListLater(AppImageManager* manager)
{
    runOnThreadOf(manager, [manager]() { manager->loadAppImageList(); });
}

template <typename Container>
QList<GuardedMetadata> toGuarded(const Container& items)
{
    QList<GuardedMetadata> out;
    out.reserve(static_cast<int>(items.size()));
    for (auto* item : items)
        out.append(item);
    return out;
}

void applyProgress(AppImageMetadata* metadata, UpdateState state, qint64 received, qint64 total)
{
    metadata->setUpdateProgressState(state);
    metadata->setUpdateBytesReceived(received);
    metadata->setUpdateBytesTotal(total);
}

// Runs `start(item, done)` for every item with at most `limit` in flight, then `allDone()`.
// `done` may be called from any thread; bookkeeping always happens on the main thread.
template <typename Item>
struct LimitedQueue {
    QList<Item> pending;
    int running = 0;
    int limit = 1;
    std::function<void(const Item&, std::function<void()>)> start;
    std::function<void()> allDone;
};

template <typename Item>
void pumpQueue(const std::shared_ptr<LimitedQueue<Item>>& q)
{
    while (!q->pending.isEmpty() && q->running < q->limit) {
        const Item item = q->pending.takeFirst();
        ++q->running;

        auto called = std::make_shared<bool>(false);   // guards against a double "done"
        q->start(item, [q, called]() {
            if (*called)
                return;
            *called = true;
            QMetaObject::invokeMethod(QCoreApplication::instance(), [q]() {
                --q->running;
                pumpQueue(q);
            }, Qt::QueuedConnection);
        });
    }

    if (q->pending.isEmpty() && q->running == 0 && q->allDone) {
        auto allDone = std::move(q->allDone);
        q->allDone = nullptr;
        allDone();
    }
}

template <typename Item>
void runLimited(QList<Item> items, int limit,
                std::function<void(const Item&, std::function<void()>)> start,
                std::function<void()> allDone)
{
    auto q = std::make_shared<LimitedQueue<Item>>();
    q->pending = std::move(items);
    q->limit = std::max(1, limit);
    q->start = std::move(start);
    q->allDone = std::move(allDone);
    pumpQueue(q);
}

} // namespace

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

    for (auto* metadata : list)
        m_appImageList->addMetadata(metadata);

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

// The property setters below can be called from any thread; the change is applied (and the
// signal emitted) on the manager's own thread.

bool AppImageManager::loadingAppImageList() const { return m_loadingAppImageList; }
void AppImageManager::setLoadingAppImageList(bool value) {
    runOnThreadOf(this, [this, value]() {
        if (m_loadingAppImageList == value)
            return;
        m_loadingAppImageList = value;
        emit loadingAppImageListChanged(value);
    });
}

bool AppImageManager::loadingAppImage() const { return m_loadingAppImage; }
void AppImageManager::setLoadingAppImage(bool value) {
    runOnThreadOf(this, [this, value]() {
        if (m_loadingAppImage == value)
            return;
        m_loadingAppImage = value;
        emit loadingAppImageChanged(value);
    });
}

bool AppImageManager::updating() const { return m_updating; }
void AppImageManager::setUpdating(bool value) {
    runOnThreadOf(this, [this, value]() {
        if (m_updating == value)
            return;
        m_updating = value;
        emit updatingChanged(value);
    });
}

AppImageManager::AppState AppImageManager::state() const {
    return m_state;
}

void AppImageManager::setState(AppState value) {
    runOnThreadOf(this, [this, value]() {
        if (m_state == value)
            return;
        m_state = value;
        emit stateChanged(value);
    });
}

void AppImageManager::requestModal(ModalTypes modal, QVariant data)
{
    emit modalRequested(modal, data);
}

// ----------------- Loading -----------------

QFuture<void> AppImageManager::loadAppImageList()
{
    // Coalesce: if a load is already in flight, hand back its future.
    if (m_loadFuture.isRunning())
        return m_loadFuture;

    auto promise = std::make_shared<QPromise<void>>();
    m_loadFuture = promise->future();

    setLoadingAppImageList(true);

    QPointer<AppImageManager> self(this);

    QThreadPool::globalInstance()->start([self, promise]() {
        QList<AppImageUtilMetadata> utilList;
        bool ok = true;

        try {
            utilList = AppImageUtil::getRegisteredList();

            auto* images = MemoryImageProvider::instance();
            images->clearImages();
            for (const auto& app : std::as_const(utilList))
                images->setImage(app.path, QImage(app.iconPath));
        } catch (const std::exception& e) {
            ErrorManager::instance()->reportError(e.what());
            ok = false;
        }

        // Always finish the promise (even if the manager is gone), otherwise anything
        // waiting on the future, and every later load, would hang.
        QMetaObject::invokeMethod(QGuiApplication::instance(), [self, utilList, ok, promise]() {
            if (self) {
                if (ok) {
                    QList<AppImageMetadata*> appList;
                    appList.reserve(utilList.size());
                    for (const auto& app : utilList) {
                        auto* meta = AppImageMetadata::createFromUtil(app, self);
                        meta->setIcon(MemoryImageProvider::instance()->getUrl(app.path));
                        appList.append(meta);
                    }
                    self->setAppImageList(appList);
                }
                self->setLoadingAppImageList(false);
            }
            promise->finish();
        }, Qt::QueuedConnection);
    });

    return m_loadFuture;
}

QFuture<void> AppImageManager::loadAppImageMetadata(const QUrl& url) {
    return loadAppImageMetadata(url.toLocalFile());
}

QFuture<void> AppImageManager::loadAppImageMetadata(const QString& path) {
    return runBusy(this, [this, path]() {
        AppImageUtil util(path);
        const AppImageUtilMetadata metadata = util.metadata();

        const bool hasIcon = !metadata.iconPath.isEmpty();
        if (hasIcon) {
            auto* images = MemoryImageProvider::instance();
            images->removeImage(path);
            images->setImage(path, QImage(metadata.iconPath));
        }

        // Everything the UI observes is applied together on the GUI thread.
        runOnThreadOf(this, [this, metadata, path, hasIcon]() {
            auto* appImageMetadata = AppImageMetadata::createFromUtil(metadata, this);
            if (hasIcon)
                appImageMetadata->setIcon(MemoryImageProvider::instance()->getUrl(path));
            setAppImageMetadata(appImageMetadata);
            setState(AppInfo);
        });
    });
}

// ----------------- Actions -----------------

QFuture<void> AppImageManager::registerSelf()
{
    return runBusy(this, [this]() {
        const QString path = appImagePath();
        if (path.isEmpty()) {
            ErrorManager::instance()->reportError("Unable to locate the running AppImage.");
            return;
        }

        if (!AppImageUtil::integratedDesktopPath(path).isEmpty()) {
            ErrorManager::instance()->reportError("BarryAppLauncher is already registered in the desktop menu.");
            return;
        }

        AppImageUtil util(path);
        const QString newPath = util.registerAppImage();
        if (newPath.isEmpty())
            return;

        // Integrated successfully: relaunch from the new location and close this instance.
        QProcess::startDetached(newPath);
        QMetaObject::invokeMethod(QCoreApplication::instance(), []() { QCoreApplication::quit(); },
                                  Qt::QueuedConnection);
    });
}

void AppImageManager::launchAppImage(const QUrl& url, const bool useTerminal)
{
    launchAppImage(url.toLocalFile(), useTerminal);
}

void AppImageManager::launchAppImage(const QString& path, const bool useTerminal)
{
    try {
        const bool success = useTerminal ? TerminalUtil::launchInTerminal(path)
                                         : QProcess::startDetached(path);
        if (!success)
            ErrorManager::instance()->reportError("Failed to launch appimage.");
    } catch (const std::exception& e) {
        ErrorManager::instance()->reportError(e.what());
    }
}

void AppImageManager::openDesktopFileInTextEditor(const QUrl& url)
{
    // (This used to call launchAppImage(), which would try to execute the .desktop file.)
    openDesktopFileInTextEditor(url.toLocalFile());
}

void AppImageManager::openDesktopFileInTextEditor(const QString& path)
{
    try {
        if (!TextEditorUtil::launchInTextEditor(path))
            ErrorManager::instance()->reportError("Failed to open desktop file.");
    } catch (const std::exception& e) {
        ErrorManager::instance()->reportError(e.what());
    }
}

QFuture<void> AppImageManager::registerAppImage(const QUrl& url)
{
    return registerAppImage(url.toLocalFile());
}

QFuture<void> AppImageManager::registerAppImage(const QString& path)
{
    return runBusy(this, [this, path]() {
        AppImageUtil util(path);
        const QString newPath = util.registerAppImage();
        if (newPath.isEmpty())
            return;

        loadAppImageMetadata(newPath);
        reloadListLater(this);
    });
}

QFuture<void> AppImageManager::unregisterAppImage(const QUrl& url, bool deleteAppImage)
{
    return unregisterAppImage(url.toLocalFile(), deleteAppImage);
}

QFuture<void> AppImageManager::unregisterAppImage(const QString& path, bool deleteAppImage)
{
    return runBusy(this, [this, path, deleteAppImage]() {
        AppImageUtil util(path);
        if (!util.unregisterAppImage(deleteAppImage))
            return;

        reloadListLater(this);
        if (deleteAppImage)
            setState(AppList);
        else
            loadAppImageMetadata(path);
    });
}

QFuture<void> AppImageManager::unlockAppImage(const QUrl& url)
{
    return unlockAppImage(url.toLocalFile());
}

QFuture<void> AppImageManager::unlockAppImage(const QString& path)
{
    return runBusy(this, [this, path]() {
        if (AppImageUtil::makeExecutable(path))
            loadAppImageMetadata(path);
    });
}

QFuture<void> AppImageManager::saveUpdateSettings()
{
    QPointer<AppImageMetadata> metadata = m_appImageMetadata;
    if (!metadata)
        return QtFuture::makeReadyVoidFuture();

    // Read the QML-facing object here on the GUI thread; only the file I/O goes to the worker.
    const QString desktopFilePath = metadata->desktopFilePath();
    const QString updateType = metadata->updateType();
    const UpdaterSettings settings = getUpdaterSettings(metadata);

    return runBusy(this, [metadata, desktopFilePath, updateType, settings]() {
        if (!AppImageUtil::saveUpdaterSettings(desktopFilePath, updateType, settings))
            return;

        if (auto* target = metadata.data())
            QMetaObject::invokeMethod(target, "setUpdateDirty", Qt::QueuedConnection, Q_ARG(bool, false));
    });
}

void AppImageManager::refreshDesktopFile()
{
    if (!m_appImageMetadata)
        return;

    // Mounting the AppImage can take a while, so this no longer blocks the GUI thread.
    const QString path = m_appImageMetadata->path();
    runBusy(this, [this, path]() {
        if (!AppImageUtil::refreshDesktopFile(path))
            return;

        loadAppImageMetadata(path);
        reloadListLater(this);
    });
}

// ----------------- Updates -----------------

void AppImageManager::checkForUpdate()
{
    busyBegin(this);
    try {
        loadMetadataUpdaterReleases(m_appImageMetadata, [this]() { busyEnd(this); });
    } catch (const std::exception& e) {
        ErrorManager::instance()->reportError(e.what());
        busyEnd(this);
    }
}

void AppImageManager::checkForAllUpdates()
{
    loadAppImageList().then(this, [this]() {
        setLoadingAppImageList(true);

        runLimited<GuardedMetadata>(
            toGuarded(m_appImageList->items()),
            SettingsManager::instance()->updateConcurrency(),
            [this](const GuardedMetadata& item, std::function<void()> done) {
                if (!item) {
                    done();
                    return;
                }
                loadMetadataUpdaterReleases(item, std::move(done));
            },
            [this]() {
                setLoadingAppImageList(false);
                m_appImageList->updateAllItems();
            });
    });
}

void AppImageManager::updateAppImage(const QString& downloadUrl, const QString& version, const QString& date)
{
    GuardedMetadata metadata = m_appImageMetadata;
    if (!metadata) {
        ErrorManager::instance()->reportError("No AppImage selected.");
        return;
    }

    // Capture the path now: the selected metadata object can be replaced mid-update.
    const QString path = metadata->path();

    setUpdating(true);
    try {
        AppImageUtil::updateAppImage(path, downloadUrl, version, date,
                                     [this, path](bool success) {
                                         if (success) {
                                             loadAppImageMetadata(path);
                                             loadAppImageList();
                                         }
                                         setUpdating(false);
                                     },
                                     [metadata](UpdateState state, qint64 received, qint64 total) {
                                         if (metadata)
                                             applyProgress(metadata, state, received, total);
                                     });
    } catch (const std::exception& e) {
        ErrorManager::instance()->reportError(e.what());
        setUpdating(false);
    }
}

void AppImageManager::updateAllAppImages()
{
    if (m_updating)
        return;

    setUpdating(true);

    runLimited<GuardedMetadata>(
        toGuarded(m_appImageList->items()),
        SettingsManager::instance()->updateConcurrency(),
        [this](const GuardedMetadata& item, std::function<void()> done) {
            auto* release = item ? getSelectedRelease(item) : nullptr;
            if (!release) {
                done();
                return;
            }
            updateAppImageAsync(item, release).then(this, [done]() { done(); });
        },
        [this]() {
            const auto items = m_appImageList->items();
            const bool anyFailed = std::any_of(items.begin(), items.end(), [](auto* item) {
                return item->updateProgressState() == AppImageMetadata::UpdateProgressState::Failed;
            });

            // Keep the per-item failure state visible; only reload when everything succeeded.
            if (!anyFailed)
                loadAppImageList();

            setUpdating(false);
        });
}

// ----------------- Private -----------------

AppImageManager::AppImageManager(QObject *parent)
    : QObject(parent), m_appImageList(new AppImageMetadataListModel(this))
{}

const QRegularExpression AppImageManager::invalidChars(R"([/\\:*?"<>|])");

QString AppImageManager::appImagePath() {
    const QString path = qEnvironmentVariable("APPIMAGE");
    return QFile::exists(path) ? path : QString();
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

    for (const auto& filter : metadata->getUpdateFilters())
        settings.filters.append({filter->field(), filter->pattern()});

    return settings;
}

void AppImageManager::loadMetadataUpdaterReleases(AppImageMetadata* appImageMetadata, std::function<void()> callback)
{
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
    // False positive: this copy of `callback` is captured by the connect() lambda below. Qt
    // heap-allocates the slot object and owns it for the lifetime of the connection (freed when
    // the single-shot connection fires or `updater` is destroyed), which the analyzer can't see.
    auto finish = [callback]() { if (callback) callback(); };

    QPointer<AppImageMetadata> metadata = appImageMetadata;
    if (!metadata || metadata->updateType().isEmpty()) {
        finish();
        return;
    }

    auto* updater = UpdaterFactory::create(metadata->updateType(),
                                           getUpdaterSettings(metadata),
                                           metadata->updateCurrentVersion(),
                                           metadata->updateCurrentDate());
    if (!updater) {
        finish();
        return;
    }

    connect(updater, &IUpdater::updatesReady, this, [updater, metadata, finish]() {
        if (metadata) {
            metadata->clearUpdaterReleases();
            for (const auto& r : updater->releases()) {
                auto* releaseModel = new UpdaterReleaseModel(metadata);
                releaseModel->setVersion(r.version);
                releaseModel->setDate(r.date);
                releaseModel->setDownload(r.download);
                releaseModel->setIsNew(r.isNew);
                releaseModel->setIsSelected(r.isNew && r.isLatest);
                metadata->addUpdaterRelease(releaseModel);
            }
        }
        updater->deleteLater();
        finish();
    }, Qt::SingleShotConnection);

    updater->fetchUpdatesAsync();
}

QFuture<void> AppImageManager::loadMetadataUpdaterReleasesAsync(AppImageMetadata* appImage)
{
    auto promise = std::make_shared<QPromise<void>>();
    QFuture<void> future = promise->future();

    loadMetadataUpdaterReleases(appImage, [promise]() { promise->finish(); });

    return future;
}

UpdaterReleaseModel* AppImageManager::getSelectedRelease(AppImageMetadata* metadata) const
{
    auto releases = metadata->updaterReleases();
    if (!releases.count || !releases.at)
        return nullptr;

    const int count = releases.count(&releases);
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

    GuardedMetadata guarded = metadata;
    // Copy what we need: clearUpdaterReleases() deletes the release model.
    const QString newVersion = release->version();

    AppImageUtil::updateAppImage(metadata->path(),
                                 release->download(),
                                 newVersion,
                                 release->date(),
                                 [promise, guarded, newVersion](bool success) {
                                     if (success && guarded) {
                                         guarded->setVersion(newVersion);
                                         guarded->clearUpdaterReleases();
                                     }
                                     promise->finish();
                                 },
                                 [this, guarded](UpdateState state, qint64 received, qint64 total) {
                                     if (!guarded)
                                         return;
                                     applyProgress(guarded, state, received, total);
                                     m_appImageList->updateItem(guarded);
                                 });

    return future;
}