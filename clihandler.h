#ifndef CLIHANDLER_H
#define CLIHANDLER_H

#include "utils/appimageutil.h"

#include <QString>
#include <QStringList>

struct CliResult {
    bool shouldExit = false;
    QString appImageFile;
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
    static const QList<ColumnSpec> COLUMN_CONFIG;
    static const QStringList VALID_COLUMNS;
    static constexpr int INFO_WIDTH = 80;
    static constexpr int INFO_LABEL_WIDTH = 20;

    /**
     * @brief List all registered AppImages
     */
    static void listRegisteredAppImages(QString columnsStr, bool tableOutput = false);

    /**
     * @brief Gets metadata info of an appimage
     */
    static void getAppImageInfo(QString path);

    /**
     * @brief Updates appimage at path
     * @param path of appimage to update
     */
    static void updateAppImage(QString path, bool force = false);

    /**
     * @brief Parses updater settings from appimage metadata
     * @param appimage metadata to parse
     * @return updater settings parsed from metadata
     */
    static void extracted(AppImageUtilMetadata &metadata,
                          UpdaterSettings &settings);
    static UpdaterSettings getUpdaterSettings(AppImageUtilMetadata metadata);

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
};

#endif // CLIHANDLER_H
