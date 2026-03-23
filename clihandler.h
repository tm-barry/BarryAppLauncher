#ifndef CLIHANDLER_H
#define CLIHANDLER_H

#include "utils/appimageutil.h"

#include <QFuture>
#include <QString>
#include <QStringList>

struct CliResult {
    bool shouldExit = false;
    QString appImageFile;
};

struct PendingUpdate {
    AppImageUtilMetadata metadata;
    UpdaterRelease release;
    int lineIndex = -1;
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
     * @return CliResult with exit status and file info
     */
    static CliResult processCLI(int argc, char *argv[]);

private:
    static QVector<QString> m_warnings;
    static QVector<QString> m_errors;
    static QMutex m_errorMutex;
    static QMutex m_outputMutex;
    static const QList<ColumnSpec> COLUMN_CONFIG;
    static const QStringList VALID_COLUMNS;
    static constexpr int INFO_WIDTH = 80;
    static constexpr int INFO_LABEL_WIDTH = 20;
    static constexpr int UPDATE_STATUS_WIDTH = 120;

    /**
     * @brief List all registered AppImages
     */
    static void list(QString columnsStr, bool tableOutput = false);

    /**
     * @brief Gets metadata info of an appimage
     */
    static void info(QString path);

    /**
     * @brief Updates appimage at path
     * @param path of appimage to update
     */
    static void update(QString path, bool force = false);

    /**
     * @brief Updates all appimages with updates
     * @param forces updates even if no new version is found
     */
    static void updateAll(bool force = false);

    /**
     * @brief Updates appimage asynchronously
     * @param name of appimage
     * @param path of appimage
     * @param downloadUrl to get updated appimage
     * @param version of new appimage
     * @param date of new appimage
     * @param lineIndex in terminal to update progress
     * @return
     */
    static QFuture<bool> updateAppImageAsync(const QString &name,
                                             const QString &path,
                                             const QString &downloadUrl,
                                             const QString &version,
                                             const QString &date,
                                             const int lineIndex = 0);

    /**
     * @brief Parses updater settings from appimage metadata
     * @param appimage metadata to parse
     * @return updater settings parsed from metadata
     */
    static UpdaterSettings getUpdaterSettings(AppImageUtilMetadata metadata);

    /**
     * @brief Fetches releases async for a given appimage
     * @param appimage to get releases for
     * @param callback that returns the releases for given appimage
     */
    static void fetchReleasesAsync(
        const AppImageUtilMetadata &appimage,
        std::function<void(QList<UpdaterRelease>)> callback);

    /**
     * @brief Collects available new releases for appimages
     * @param appimages to get pending updates for
     * @param finishedCallback - returns with pending updates when finished
     * @param force - if true it will use latest release even if not new
     */
    static void collectReleasesAsync(const QList<AppImageUtilMetadata> &appimages,
                              std::function<void(QVector<PendingUpdate>)> finishedCallback,
                              bool force = false,
                              int timeoutMs = 30000);

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
     */
    static void execEventLoopLoadingIndicator(const QString &message, QEventLoop &loop, LoadingIndicator indicator = LoadingIndicator::Spinner);

    /**
     * @brief Prints out any errors/warnings
     */
    static void printErrors();

    /**
     * @brief Sets up the error manager handler for cli
     */
    static void handleErrorManager();
};

#endif // CLIHANDLER_H
