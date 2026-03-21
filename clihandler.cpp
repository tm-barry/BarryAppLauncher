#include "clihandler.h"
#include "utils/appimageutil.h"
#include "utils/stringutil.h"

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QCoreApplication>
#include <QTimer>

#include <algorithm>
#include <iostream>
#include <iomanip>

namespace {
    constexpr QChar kSpinnerChars[] = {'|', '/', '-', '\\'};
}

const QList<ColumnSpec> CliHandler::COLUMN_CONFIG = {
    {"name", "Name", 20},
    {"version", "Version", 20},
    {"description", "Description", 40},
    {"path", "Path", 40, '/'}
};

const QStringList CliHandler::VALID_COLUMNS = {
    "name", "version", "description", "path"
};

CliResult CliHandler::processCLI(int argc, char *argv[])
{
    CliResult result;
    QCommandLineParser parser;
    parser.setApplicationDescription("Integrate and manage AppImages on your desktop");
    parser.addHelpOption();
    parser.addVersionOption();

    // Add list option
    QCommandLineOption listOption({"l", "list"},
                                  "List all registered AppImages");
    parser.addOption(listOption);

    // Add long option
    QCommandLineOption tableOption({"t", "table"},
                                  "Show table output");
    parser.addOption(tableOption);

    // Add columns option
    QCommandLineOption columnsOption({"c", "columns"},
                                     "Columns to display (comma-separated): name,version,path,description",
                                     "columns");
    parser.addOption(columnsOption);

    // Add info option
    QCommandLineOption infoOption({"i", "info"},
                                  "Display info of appimage",
                                  "info");
    parser.addOption(infoOption);

    // Add update option
    QCommandLineOption updateOption({"u", "update"},
                                  "Updates appimage",
                                  "update");
    parser.addOption(updateOption);

    // Add force option
    QCommandLineOption forceOption({"f", "force"},
                                   "Force update: show and allow selection of all releases");
    parser.addOption(forceOption);

    // Positional argument for appimage file
    parser.addPositionalArgument("appimage",
                                 "AppImage file to open (optional)", "[appimage]");

    parser.process(*QCoreApplication::instance());

    // Get positional argument
    const QStringList positionalArgs = parser.positionalArguments();
    if (!positionalArgs.isEmpty()) {
        result.appImageFile = positionalArgs.at(0);
    }

    // Handle column defaults
    QString columnsStr;
    if (parser.isSet(columnsOption)) {
        columnsStr = parser.value(columnsOption);
    } else {
        if (parser.isSet(listOption)) {
            if (parser.isSet(tableOption)) {
                columnsStr = "name,version,description";  // default for list table
            } else {
                columnsStr = "name,version,path,description";  // default for list stacked
            }
        }
    }

    // Handle list option
    if (parser.isSet(listOption)) {
        bool tableOutput = parser.isSet(tableOption);
        listRegisteredAppImages(columnsStr, tableOutput);
        result.shouldExit = true;
        return result;
    }
    else if (parser.isSet(infoOption)) {
        QString appImage = parser.value(infoOption);
        getAppImageInfo(appImage);
        result.shouldExit = true;
        return result;
    }
    else if (parser.isSet(updateOption)) {
        bool force = parser.isSet(forceOption);
        QString appimage = parser.value(updateOption);
        updateAppImage(appimage, force);
        result.shouldExit = true;
        return result;
    }

    return result;
}

void CliHandler::listRegisteredAppImages(QString columnsStr, bool tableOutput)
{
    auto registeredAppImages = AppImageUtil::getRegisteredList();

    if (registeredAppImages.isEmpty()) {
        std::cout << "No registered AppImages found." << std::endl;
        return;
    }

    // Parse columns from string
    QStringList columnsList = columnsStr.split(',', Qt::SkipEmptyParts);

    // Validate columns
    for (const auto& col : columnsList) {
        if (!VALID_COLUMNS.contains(col)) {
            std::cerr << "Error: Invalid column '" << col.toStdString() << "'. Valid columns are: "
                      << VALID_COLUMNS.join(",").toStdString() << std::endl;
            return;
        }
    }

    QList<ColumnSpec> selectedColumns;
    for (const auto& colName : columnsList) {
        auto it = std::find_if(COLUMN_CONFIG.begin(), COLUMN_CONFIG.end(),
                               [&](const ColumnSpec& spec){ return spec.key == colName; });
        if (it != COLUMN_CONFIG.end()) {
            selectedColumns.append(*it);
        }
    }

    // Print table header
    std::cout << "\nRegistered AppImages:" << std::endl;

    // Table Output
    if(tableOutput) {
        int tableWidth = 0;
        for (const auto &spec : selectedColumns) {
            tableWidth += spec.width;
        }
        std::cout << std::string(tableWidth, '-') << std::endl;

        // Print column titles dynamically
        for (const auto &spec : selectedColumns) {
            std::cout << std::left << std::setw(spec.width) << spec.name.toStdString();
        }
        std::cout << std::endl;
        std::cout << std::string(tableWidth, '-') << std::endl;

        // Print each AppImage
        for (const auto &appImage : registeredAppImages) {
            // Wrap text for each column
            QList<QStringList> wrappedColumns;
            for (const auto &spec : selectedColumns) {
                QString text;
                if (spec.key == "name") text = appImage.name;
                else if (spec.key == "version") text = StringUtil::coalesce(appImage.updateCurrentVersion, appImage.version);
                else if (spec.key == "description") text = appImage.comment;
                else if (spec.key == "path") text = appImage.path; // if you want path too

                wrappedColumns.push_back(getWrappedText(text, spec.width - 2, spec.splitChar));
            }

            // Determine the maximum number of lines for this row
            qsizetype maxLines = 0;
            for (const auto &col : wrappedColumns)
                maxLines = std::max(maxLines, col.size());

            // Print all lines for this row
            for (size_t i = 0; i < maxLines; ++i) {
                for (size_t colIdx = 0; colIdx < selectedColumns.size(); ++colIdx) {
                    const auto &col = wrappedColumns[colIdx];
                    std::cout << std::left << std::setw(selectedColumns[colIdx].width)
                              << (i < col.size() ? col[i].toStdString() : "");
                }
                std::cout << std::endl;
            }

            // Blank line between entries
            if (&appImage != &registeredAppImages.back()) {
                std::cout << std::endl;
            }
        }
        std::cout << std::string(tableWidth, '-') << std::endl;
    }
    // Stacked Output
    else {
        std::cout << std::endl;
        for (const auto &appImage : registeredAppImages) {
            for (const auto &spec : selectedColumns) {
                QString text;

                if (spec.key == "name") text = appImage.name;
                else if (spec.key == "version") text = StringUtil::coalesce(appImage.updateCurrentVersion, appImage.version);
                else if (spec.key == "description") text = appImage.comment;
                else if (spec.key == "path") text = appImage.path;

                std::cout << std::left
                          << std::setw(12) << spec.name.toStdString()
                          << ": "
                          << text.toStdString()
                          << std::endl;
            }
            std::cout << std::endl;
        }
    }

    // Footer
    std::cout << "Total: " << registeredAppImages.size() << " AppImage(s)" << std::endl << std::endl;
}

void CliHandler::getAppImageInfo(QString path)
{
    if (path.isEmpty()) {
        std::cerr << "Error: No AppImage path provided. Use: --info <appimage_path>" << std::endl;
        return;
    }

    AppImageUtil util(path);
    AppImageUtilMetadata appImage = util.metadata();

    auto printField = [](const std::string& key, const std::string& value) {
        if(!value.empty()) {
            std::cout << std::left << std::setw(INFO_LABEL_WIDTH) << key << ": " << value << std::endl;
        }
    };

    std::cout << std::endl;
    std::cout << (QString("╔") + QString(INFO_WIDTH - 2, QChar(0x2550)) + QString("╗")).toUtf8().constData() << std::endl;
    QString title = "AppImage Information";
    int innerWidth = INFO_WIDTH - 2;
    int leftPadding = (innerWidth - title.size()) / 2;
    int rightPadding = innerWidth - title.size() - leftPadding;
    std::cout << (QString("║")
                  + QString(leftPadding, ' ')
                  + title
                  + QString(rightPadding, ' ')
                  + QString("║")).toUtf8().constData() << std::endl;
    std::cout << (QString("╚") + QString(INFO_WIDTH - 2, QChar(0x2550)) + QString("╝")).toUtf8().constData() << std::endl;
    std::cout << std::endl;

    // General Information
    std::cout << "General Information:" << std::endl;
    std::cout << std::string(INFO_WIDTH, '-') << std::endl;
    printField("Name", appImage.name.toStdString());
    printField("Version", StringUtil::coalesce(appImage.updateCurrentVersion, appImage.version).toStdString());
    printField("Description", appImage.comment.toStdString());
    printField("Path", appImage.path.toStdString());
    printField("Desktop Path", appImage.desktopFilePath.toStdString());
    printField("Categories", appImage.categories.toStdString());

    // Update Information
    if(!appImage.updateType.isEmpty()) {
        std::cout << std::endl;
        std::cout << "Update Information:" << std::endl;
        std::cout << std::string(INFO_WIDTH, '-') << std::endl;
        printField("Update Type", appImage.updateType.toStdString());
        printField("Url", appImage.updateUrl.toStdString());
        printField("Download Field", appImage.updateDownloadField.toStdString());
        printField("Download Pattern", appImage.updateDownloadPattern.toStdString());
        printField("Version Field", appImage.updateVersionField.toStdString());
        printField("Version Pattern", appImage.updateVersionPattern.toStdString());
        printField("Date Field", appImage.updateDateField.toStdString());

        if(!appImage.updateFilters.isEmpty()) {
            std::cout << std::endl;
            std::cout << "Update Filters:" << std::endl;
            std::cout << std::string(INFO_WIDTH, '-') << std::endl;

            for (const auto &filter : appImage.updateFilters) {
                printField(filter.field.toStdString(), filter.pattern.toStdString());
            }
        }
    }

    if (!appImage.internalIntegration) {
        std::cout << std::endl;
        printField("Note", "This AppImage appears to have been integrated by another application.");
    }
    std::cout << std::endl;
}

void CliHandler::updateAppImage(QString path, bool force)
{
    if (path.isEmpty()) {
        std::cerr << "Error: No AppImage path provided. Use: --update <appimage_path>" << std::endl;
        return;
    }

    AppImageUtil util(path);
    AppImageUtilMetadata appimage = util.metadata();

    std::cout << "\033[1m" << appimage.name.toStdString() << " (";
    std::cout << StringUtil::coalesce(appimage.updateCurrentVersion, appimage.version).toStdString();
    std::cout << ") \033[0m\n\n";

    if (appimage.updateType.isEmpty()) {
        std::cerr << "Error: No update type set in "
                  << appimage.desktopFilePath.toStdString() << std::endl;
        return;
    }

    UpdaterSettings settings = getUpdaterSettings(appimage);
    auto* updater = UpdaterFactory::create(
        appimage.updateType, settings,
        appimage.updateCurrentVersion, appimage.updateCurrentDate);

    // Fetch updates asynchronously
    QList<UpdaterRelease> fetchedReleases;
    QEventLoop loop;
    QObject::connect(updater, &IUpdater::updatesReady, &loop, [&]() {
        fetchedReleases = updater->releases();
        updater->deleteLater();
        loop.quit();
    });

    updater->fetchUpdatesAsync();
    execEventLoopLoadingIndicator("Checking for updates ", loop);

    if (fetchedReleases.isEmpty()) {
        std::cout << "No updates found." << std::endl;
        return;
    }

    // Filter releases according to force / isNew
    QVector<const UpdaterRelease*> selectableReleases;
    int defaultIndex = -1;

    for (int i = 0; i < fetchedReleases.size(); ++i) {
        const auto &r = fetchedReleases[i];
        if (!r.isNew && !force)
            continue;

        selectableReleases.push_back(&r);

        // Default to latest new release
        if (r.isLatest && r.isNew && defaultIndex == -1)
            defaultIndex = selectableReleases.size() - 1;
    }

    if (selectableReleases.isEmpty()) {
        std::cout << "Your AppImage is already up to date. No updates available." << std::endl;
        return;
    }

    // Print releases
    std::cout << "Available updates:\n";
    for (int i = 0; i < selectableReleases.size(); ++i) {
        const auto &r = selectableReleases[i];

        std::cout << "[" << (i + 1) << "] ";

        // Bold latest new release
        bool bold = r->isNew && r->isLatest;
        if (bold) std::cout << "\033[1m";

        // Version / date
        if (!r->version.isEmpty())
            std::cout << r->version.toStdString();
        if (!r->date.isEmpty()) {
            if (!r->version.isEmpty()) std::cout << " | ";
            std::cout << StringUtil::formatDateTime(r->date).toStdString();
        }

        // Flags
        if (r->isNew) std::cout << "  <-- new" << (r->isLatest ? " (latest)" : "");
        else if (r->isLatest) std::cout << "  <-- latest";

        if (bold) std::cout << "\033[0m"; // reset bold
        std::cout << std::endl;
    }

    // Prompt user for selection
    QTextStream qin(stdin);
    int selection = -1;

    while (true) {
        std::cout << "\nSelect a release to update to";
        if (defaultIndex >= 0)
            std::cout << " [default: "
                      << selectableReleases[defaultIndex]->version.toStdString() << "]";
        std::cout << " (1-" << selectableReleases.size() << ", or 'c' to cancel): " << std::flush;

        QString line = qin.readLine().trimmed();

        // Cancel
        if (line.compare("c", Qt::CaseInsensitive) == 0 ||
            line.compare("cancel", Qt::CaseInsensitive) == 0) {
            std::cout << "Update cancelled by user." << std::endl;
            return;
        }

        // Default
        if (line.isEmpty() && defaultIndex >= 0) {
            selection = defaultIndex;
            break;
        }

        // Numeric selection
        bool ok = false;
        int num = line.toInt(&ok);
        if (ok && num >= 1 && num <= selectableReleases.size()) {
            selection = num - 1;
            break;
        }

        std::cout << "Invalid selection. Try again." << std::endl;
    }

    const auto* chosenRelease = selectableReleases[selection];
    std::cout << "\nUpdating to version: "
              << chosenRelease->version.toStdString() << std::endl;

    // TODO: implement actual download/apply logic
}

UpdaterSettings CliHandler::getUpdaterSettings(AppImageUtilMetadata metadata)
{
    UpdaterSettings settings;
    settings.url = metadata.updateUrl;
    settings.versionField = metadata.updateVersionField;
    settings.versionPattern = metadata.updateVersionPattern;
    settings.downloadField = metadata.updateDownloadField;
    settings.downloadPattern = metadata.updateDownloadPattern;
    settings.dateField = metadata.updateDateField;

    for (const auto& filter : metadata.updateFilters) {
        settings.filters.append({filter.field, filter.pattern});
    }

    return settings;
}

QStringList CliHandler::getWrappedText(const QString& text, int width, QChar splitChar)
{
    QStringList lines;
    QString currentLine;

    QStringList parts = text.split(splitChar, Qt::SkipEmptyParts);

    // Reattach delimiter except for last segment
    for (int i = 0; i < parts.size(); ++i) {
        QString word = parts[i];
        if (i < parts.size() - 1)
            word += splitChar;

        if (word.length() > width) {
            if (!currentLine.isEmpty()) {
                lines.append(currentLine);
                currentLine.clear();
            }

            for (int j = 0; j < word.length(); j += width) {
                lines.append(word.mid(j, width));
            }

        } else if (currentLine.length() + word.length() <= width) {

            currentLine += word;

        } else {

            if (!currentLine.isEmpty())
                lines.append(currentLine);

            currentLine = word;
        }
    }

    if (!currentLine.isEmpty())
        lines.append(currentLine);

    return lines.isEmpty() ? QStringList{""} : lines;
}

void CliHandler::execEventLoopLoadingIndicator(const QString &message,
                                               QEventLoop &loop,
                                               LoadingIndicator indicator)
{
    std::cout << message.toStdString() << " " << std::flush;

    int spinnerIndex = 0;
    int dotCount = 0;
    const int maxDots = 3;

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (indicator == LoadingIndicator::Spinner) {
            // Erase previous spinner char and print next
            std::cout << "\b" << kSpinnerChars[spinnerIndex].toLatin1() << std::flush;
            spinnerIndex = (spinnerIndex + 1) % 4;
        }
        else if (indicator == LoadingIndicator::Dots) {
            // Move cursor back to end of message (overwrite previous dots)
            std::cout << "\r" << message.toStdString() << " ";
            for (int i = 0; i < dotCount; ++i)
                std::cout << ".";

            // Pad remaining space so old dots are erased
            for (int i = dotCount; i < maxDots; ++i)
                std::cout << " ";

            std::cout << std::flush;

            dotCount = (dotCount + 1) % (maxDots + 1); // 0..3 dots
        }
    });

    int interval = (indicator == LoadingIndicator::Spinner) ? 100 : 300;
    timer.start(interval);

    loop.exec();  // Run event loop while async task is working
    timer.stop();

    // Fully clear the line and reset cursor
    std::cout << "\r" << std::string(message.length() + maxDots + 2, ' ') << "\r" << std::flush;
}


