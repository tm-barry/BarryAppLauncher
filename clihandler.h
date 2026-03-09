#ifndef CLIHANDLER_H
#define CLIHANDLER_H

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
     * @brief Splits text into lines by the given width
     * @param text to be split
     * @param width to wrap text to next line
     * @return List of the split lines
     */
    static QStringList getWrappedText(const QString& text, int width, QChar splitChar = ' ');
};

#endif // CLIHANDLER_H
