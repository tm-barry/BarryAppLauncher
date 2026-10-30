#ifndef CLIHANDLER_H
#define CLIHANDLER_H

#include "utils/appimageutil.h"

#include <QChar>
#include <QEventLoop>
#include <QFuture>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

struct CliResult {
    bool shouldExit = false;
    QString appImageFile;
    int exitCode = 0;   // process exit code to use when shouldExit is true
};

struct PendingUpdate {
    AppImageUtilMetadata metadata;
    UpdaterRelease release;
    int lineIndex = -1;
};

/// What happened when we checked a single AppImage for updates
enum class CheckOutcome {
    UpdateAvailable,
    UpToDate,
    NoReleaseInfo,   // updater ran but returned no releases (likely a fetch error)
    TimedOut,
    Skipped          // no/unknown update type configured
};

struct CheckEntry {
    QString name;
    CheckOutcome outcome = CheckOutcome::UpToDate;
};

/// Result of checking a set of AppImages for updates
struct CollectResult {
    QVector<PendingUpdate> updates;
    QVector<CheckEntry> entries;

    int count(CheckOutcome outcome) const {
        int n = 0;
        for (const auto &e : entries)
            if (e.outcome == outcome)
                ++n;
        return n;
    }
};

/// Display state of one row in the update progress block
enum class RowState {
    Waiting,
    Downloading,
    Extracting,
    Installing,
    Done,
    Failed
};

struct UpdateStatus {
    QString name;
    QString version;                              // version being updated to
    RowState state = RowState::Waiting;
    RowState printedState = RowState::Waiting;    // used for line-by-line (non in-place) output
    qint64 received = 0;
    qint64 total = 0;
};

struct ColumnSpec {
    QString key = "";
    QString name = "";
    int width = 0;
    QChar splitChar = ' ';
};

enum class LoadingIndicator {
    Spinner,
    Dots
};

class CliHandler {
public:
    /**
     * @brief Process command-line arguments
     * @param argc Argument count from main()
     * @param argv Argument values from main()
     * @return CliResult with exit status, exit code and file info
     */
    static CliResult processCLI(int argc, char *argv[]);

private:
    static int m_maxNameWidth;
    static bool m_inPlace;   // true when the progress block is redrawn in place (TTY)
    static QVector<QString> m_warnings;
    static QVector<QString> m_errors;
    static QMutex m_errorMutex;
    static QVector<UpdateStatus> m_status;
    static QMutex m_statusMutex;
    static const QList<ColumnSpec> COLUMN_CONFIG;
    static const QStringList VALID_COLUMNS;
    static constexpr int INFO_WIDTH = 80;
    static constexpr int INFO_LABEL_WIDTH = 20;
    static constexpr int kFetchTimeoutMs = 30000;   // per-AppImage release fetch timeout

    /**
     * @brief List all registered AppImages
     * @return process exit code (0 = success)
     */
    static int list(QString columnsStr, bool tableOutput = false);

    /**
     * @brief Gets metadata info of an appimage
     * @return process exit code (0 = success)
     */
    static int info(QString path);

    /**
     * @brief Updates appimage at path
     * @param path of appimage to update
     * @return process exit code (0 = success)
     */
    static int update(QString path, bool force = false);

    /**
     * @brief Updates all appimages with updates
     * @param forces updates even if no new version is found
     * @return process exit code (0 = everything checked and updated fine)
     */
    static int updateAll(bool force = false);

    /**
     * @brief Redraws (or appends to) the update progress block
     */
    static void outputUpdateAllStatus();

    /**
     * @brief Runs the given updates (respecting the concurrency setting) while
     *        showing a live progress row for each one. Blocks in an event loop.
     * @param pending updates to run
     * @return per-update success flags, same order as pending
     */
    static QVector<bool> runUpdates(QVector<PendingUpdate> pending);

    /**
     * @brief Updates appimage asynchronously
     * @param path of appimage
     * @param downloadUrl to get updated appimage
     * @param version of new appimage
     * @param date of new appimage
     * @param lineIndex row in the status block to report progress to
     */
    static QFuture<bool> updateAppImageAsync(const QString &path,
                                             const QString &downloadUrl,
                                             const QString &version,
                                             const QString &date,
                                             const int lineIndex);

    /**
     * @brief Parses updater settings from appimage metadata
     * @param appimage metadata to parse
     * @return updater settings parsed from metadata
     */
    static UpdaterSettings getUpdaterSettings(AppImageUtilMetadata metadata);

    /**
     * @brief Fetches releases async for a given appimage
     * @param appimage to get releases for
     * @param callback called with the releases and a "skipped" flag. skipped is
     *        true when no fetch could be started (no/unknown update type).
     * @return created updater used to perform the fetch, or nullptr if no
     *         fetch was started. When nullptr is returned the callback has
     *         already been invoked synchronously (releases empty, skipped =
     *         true), so callers must not wait on it.
     */
    static IUpdater* fetchReleasesAsync(
        const AppImageUtilMetadata &appimage,
        std::function<void(QList<UpdaterRelease>, bool)> callback);

    /**
     * @brief Collects available new releases for appimages
     * @param appimages to get pending updates for
     * @param finishedCallback - returns the check result when all are finished.
     *        Everything it needs must outlive this call (it fires later from
     *        the caller's event loop).
     * @param force - if true it will use latest release even if not new
     * @param timeoutMs - per-appimage fetch timeout
     * @param progress - optional, called as (done, total) after each appimage
     */
    static void collectReleasesAsync(const QList<AppImageUtilMetadata> &appimages,
                              std::function<void(CollectResult)> finishedCallback,
                              bool force = false,
                              int timeoutMs = kFetchTimeoutMs,
                              std::function<void(int, int)> progress = nullptr);

    /**
     * @brief Splits text into lines by the given width
     * @param text to be split
     * @param width to wrap text to next line
     * @return List of the split lines
     */
    static QStringList getWrappedText(const QString& text, int width, QChar splitChar = ' ');

    /**
     * @brief Shows a message with a loading indicator while event loop is running
     * @param message to display while loop is executing
     * @param loop to exec
     * @param indicator style of the indicator
     * @param suffix optional, called on every tick to append live text (e.g. progress)
     */
    static void execEventLoopLoadingIndicator(const QString &message,
                                              QEventLoop &loop,
                                              LoadingIndicator indicator = LoadingIndicator::Spinner,
                                              std::function<QString()> suffix = nullptr);

    /**
     * @brief Prints out any errors/warnings
     */
    static void printErrors();

    /**
     * @brief Prints errors/warnings and returns the final exit code
     *        (forced to 1 if any error was reported)
     */
    static int finishCommand(int exitCode);

    /**
     * @brief Sets up the error manager handler for cli
     */
    static void handleErrorManager();
};

#endif // CLIHANDLER_H