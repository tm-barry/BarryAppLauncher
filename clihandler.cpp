#include "clihandler.h"
#include "managers/errormanager.h"
#include "utils/appimageutil.h"
#include "utils/stringutil.h"

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QCoreApplication>
#include <QTimer>
#include <QFutureWatcher>

#include <algorithm>
#include <deque>

namespace {
    constexpr QChar kSpinnerChars[] = {'|', '/', '-', '\\'};
}

QVector<QString> CliHandler::m_errors;
QVector<QString> CliHandler::m_warnings;
QMutex CliHandler::m_errorMutex;
QMutex CliHandler::m_outputMutex;

inline QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

inline QTextStream &err()
{
    static QTextStream stream(stderr);
    return stream;
}

inline void moveCursorUp(int lines) {
    if (lines > 0)
        out() << "\033[" << lines << "A";
}

inline void moveCursorDown(int lines) {
    if (lines > 0)
        out() << "\033[" << lines << "B";
}

inline void clearLine() {
    out() << "\r\033[2K";
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
    auto helpOption = parser.addHelpOption();
    auto versionOption = parser.addVersionOption();
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

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

    // Add update all option
    QCommandLineOption updateAllOption({"U", "update-all"},
                                    "Updates all appimages");
    parser.addOption(updateAllOption);

    // Add force option
    QCommandLineOption forceOption({"f", "force"},
                                   "Force update: show and allow selection of all releases");
    parser.addOption(forceOption);

    // Positional argument for appimage file
    parser.addPositionalArgument("appimage",
                                 "AppImage file to open (optional)", "[appimage]");

    QStringList arguments;
    for (int i = 0; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);
    parser.parse(arguments);

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

    // If cli option, then setup error manager for cli
    if (parser.isSet(listOption) || parser.isSet(infoOption) || parser.isSet(updateOption) || parser.isSet(updateAllOption)) {
        handleErrorManager();
    }

    // Handle/check for cli options
    if (parser.isSet(helpOption)) {
        out() << parser.helpText();
        result.shouldExit = true;
        return result;
    }
    else if (parser.isSet(versionOption)) {
        out() << QCoreApplication::applicationVersion() << Qt::endl;
        result.shouldExit = true;
        return result;
    }
    else if (parser.isSet(listOption)) {
        bool tableOutput = parser.isSet(tableOption);
        list(columnsStr, tableOutput);
        printErrors();
        result.shouldExit = true;
        return result;
    }
    else if (parser.isSet(infoOption)) {
        QString appImage = parser.value(infoOption);
        info(appImage);
        result.shouldExit = true;
        printErrors();
        return result;
    }
    else if (parser.isSet(updateOption)) {
        bool force = parser.isSet(forceOption);
        QString appimage = parser.value(updateOption);
        update(appimage, force);
        result.shouldExit = true;
        printErrors();
        return result;
    }
    else if (parser.isSet(updateAllOption)) {
        bool force = parser.isSet(forceOption);
        updateAll(force);
        printErrors();
        result.shouldExit = true;
        return result;
    }

    return result;
}

void CliHandler::list(QString columnsStr, bool tableOutput)
{
    auto registeredAppImages = AppImageUtil::getRegisteredList();

    if (registeredAppImages.isEmpty()) {
        out() << "No registered AppImages found." << Qt::endl;
        return;
    }

    // Parse columns from string
    QStringList columnsList = columnsStr.split(',', Qt::SkipEmptyParts);

    // Validate columns
    for (const auto& col : std::as_const(columnsList)) {
        if (!VALID_COLUMNS.contains(col)) {
            err() << "Error: Invalid column '" << col << "'. Valid columns are: "
                      << VALID_COLUMNS.join(",") << Qt::endl;
            return;
        }
    }

    QList<ColumnSpec> selectedColumns;
    for (const auto& colName : std::as_const(columnsList)) {
        auto it = std::find_if(COLUMN_CONFIG.begin(), COLUMN_CONFIG.end(),
                               [&](const ColumnSpec& spec){ return spec.key == colName; });
        if (it != COLUMN_CONFIG.end()) {
            selectedColumns.append(*it);
        }
    }

    // Print table header
    out() << "\nRegistered AppImages:" << Qt::endl;

    // Table Output
    if(tableOutput) {
        int tableWidth = 0;
        for (const auto &spec : selectedColumns) {
            tableWidth += spec.width;
        }
        out() << QString(tableWidth, QChar('-')) << Qt::endl;

        // Print column titles dynamically
        for (const auto &spec : selectedColumns) {
            out() << spec.name.leftJustified(spec.width, ' ');
        }
        out() << Qt::endl;
        out() << QString(tableWidth, QChar('-')) << Qt::endl;

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
                    QString value = (i < col.size()) ? col[i] : QString();
                    out() << value.leftJustified(selectedColumns[colIdx].width, ' ');
                }
                out() << Qt::endl;
            }

            // Blank line between entries
            if (&appImage != &registeredAppImages.back()) {
                out() << Qt::endl;
            }
        }
        out() << QString(tableWidth, QChar('-')) << Qt::endl;
    }
    // Stacked Output
    else {
        out() << Qt::endl;
        for (const auto &appImage : registeredAppImages) {
            for (const auto &spec : std::as_const(selectedColumns)) {
                QString text;

                if (spec.key == "name") text = appImage.name;
                else if (spec.key == "version") text = StringUtil::coalesce(appImage.updateCurrentVersion, appImage.version);
                else if (spec.key == "description") text = appImage.comment;
                else if (spec.key == "path") text = appImage.path;

                QString namePadded = spec.name.leftJustified(12, ' ');
                out() << namePadded << ": " << text << Qt::endl;
            }
            out() << Qt::endl;
        }
    }

    // Footer
    out() << "Total: " << registeredAppImages.size() << " AppImage(s)" << Qt::endl << Qt::endl;
}

void CliHandler::info(QString path)
{
    if (path.isEmpty()) {
        err() << "Error: No AppImage path provided. Use: --info <appimage_path>" << Qt::endl;
        return;
    }

    AppImageUtil util(path);
    AppImageUtilMetadata appImage = util.metadata();

    auto printField = [](const QString &key, const QString &value) {
        if (!value.isEmpty()) {
            out() << key.leftJustified(INFO_LABEL_WIDTH, ' ') << ": " << value << Qt::endl;
        }
    };

    out() << Qt::endl;
    out() << (QString("╔") + QString(INFO_WIDTH - 2, QChar(0x2550)) + QString("╗")).toUtf8().constData() << Qt::endl;
    QString title = "AppImage Information";
    int innerWidth = INFO_WIDTH - 2;
    int leftPadding = (innerWidth - title.size()) / 2;
    int rightPadding = innerWidth - title.size() - leftPadding;
    out() << (QString("║")
                  + QString(leftPadding, ' ')
                  + title
                  + QString(rightPadding, ' ')
                  + QString("║")).toUtf8().constData() << Qt::endl;
    out() << (QString("╚") + QString(INFO_WIDTH - 2, QChar(0x2550)) + QString("╝")).toUtf8().constData() << Qt::endl;
    out() << Qt::endl;

    // General Information
    out() << "General Information:" << Qt::endl;
    out() << QString(INFO_WIDTH, QChar('-')) << Qt::endl;
    printField("Name", appImage.name);
    printField("Version", StringUtil::coalesce(appImage.updateCurrentVersion, appImage.version));
    printField("Description", appImage.comment);
    printField("Path", appImage.path);
    printField("Desktop Path", appImage.desktopFilePath);
    printField("Categories", appImage.categories);

    // Update Information
    if(!appImage.updateType.isEmpty()) {
        out() << Qt::endl;
        out() << "Update Information:" << Qt::endl;
        out() << QString(INFO_WIDTH, QChar('-')) << Qt::endl;
        printField("Update Type", appImage.updateType);
        printField("Url", appImage.updateUrl);
        printField("Download Field", appImage.updateDownloadField);
        printField("Download Pattern", appImage.updateDownloadPattern);
        printField("Version Field", appImage.updateVersionField);
        printField("Version Pattern", appImage.updateVersionPattern);
        printField("Date Field", appImage.updateDateField);

        if(!appImage.updateFilters.isEmpty()) {
            out() << Qt::endl;
            out() << "Update Filters:" << Qt::endl;
            out() << QString(INFO_WIDTH, QChar('-')) << Qt::endl;

            for (const auto &filter : std::as_const(appImage.updateFilters)) {
                printField(filter.field, filter.pattern);
            }
        }
    }

    if (!appImage.internalIntegration) {
        out() << Qt::endl;
        printField("Note", "This AppImage appears to have been integrated by another application.");
    }
    out() << Qt::endl;
}

void CliHandler::update(QString path, bool force)
{
    if (path.isEmpty()) {
        err() << "Error: No AppImage path provided. Use: --update <appimage_path>" << Qt::endl;
        return;
    }

    AppImageUtil util(path);
    AppImageUtilMetadata appimage = util.metadata();

    out() << "\033[1m" << appimage.name << " (";
    out() << StringUtil::coalesce(appimage.updateCurrentVersion, appimage.version);
    out() << ") \033[0m\n\n";

    if (appimage.updateType.isEmpty()) {
        err() << "Error: No update type set in " << appimage.desktopFilePath << Qt::endl;
        return;
    }

    QEventLoop releaseLoop;
    QList<UpdaterRelease> fetchedReleases;

    fetchReleasesAsync(appimage, [&](QList<UpdaterRelease> releases) {
        fetchedReleases = releases;
        releaseLoop.quit();
    });

    // Safety timeout (30s)
    QTimer::singleShot(30000, &releaseLoop, &QEventLoop::quit);

    execEventLoopLoadingIndicator("Checking for updates ", releaseLoop);

    if (fetchedReleases.isEmpty()) {
        out() << "No updates found." << Qt::endl;
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
        if (r.isLatest && (force || r.isNew) && defaultIndex == -1)
            defaultIndex = selectableReleases.size() - 1;
    }

    if (selectableReleases.isEmpty()) {
        out() << "Your AppImage is already up to date. No updates available." << Qt::endl;
        return;
    }

    // Print releases
    out() << "Available updates:\n";
    for (int i = 0; i < selectableReleases.size(); ++i) {
        const auto &r = selectableReleases[i];

        out() << "[" << (i + 1) << "] ";

        // Bold latest new release
        bool bold = r->isNew && r->isLatest;
        if (bold) out() << "\033[1m";

        // Version / date
        if (!r->version.isEmpty())
            out() << r->version;
        if (!r->date.isEmpty()) {
            if (!r->version.isEmpty()) out() << " | ";
            out() << StringUtil::formatDateTime(r->date);
        }

        // Flags
        if (r->isNew) out() << "  <-- new" << (r->isLatest ? " (latest)" : "");
        else if (r->isLatest) out() << "  <-- latest";

        if (bold) out() << "\033[0m"; // reset bold
        out() << Qt::endl;
    }

    // Prompt user for selection
    QTextStream qin(stdin);
    int selection = -1;

    while (true) {
        out() << "\nSelect a release to update to";
        if (defaultIndex >= 0)
            out() << " [default: "
                      << selectableReleases[defaultIndex]->version << "]";
        out() << " (1-" << selectableReleases.size() << ", or 'c' to cancel): " << Qt::flush;

        QString line = qin.readLine().trimmed();

        // Cancel
        if (line.compare("c", Qt::CaseInsensitive) == 0 ||
            line.compare("cancel", Qt::CaseInsensitive) == 0) {
            out() << "Update cancelled by user." << Qt::endl;
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

        out() << "Invalid selection. Try again." << Qt::endl;
    }

    const auto* chosenRelease = selectableReleases[selection];
    out() << "\nUpdating to version: " << chosenRelease->version << Qt::endl << Qt::endl;

    QEventLoop updateLoop;
    bool success = false;

    auto future = updateAppImageAsync(
        appimage.name,
        appimage.path,
        chosenRelease->download,
        chosenRelease->version,
        chosenRelease->date,
        0
        );
    QFutureWatcher<bool> watcher;
    watcher.setFuture(future);
    QObject::connect(&watcher, &QFutureWatcher<bool>::finished, &updateLoop,[&]() {
        success = watcher.result();
        updateLoop.quit();
    });
    updateLoop.exec();

    if (success) {
        out() << "\n\nUpdate completed successfully!" << Qt::endl;
    } else {
        err() << "\n\nUpdate failed!" << Qt::endl;
    }
}

void CliHandler::updateAll(bool force)
{
    auto registeredAppImages = AppImageUtil::getRegisteredList();

    if (registeredAppImages.isEmpty()) {
        out() << "No registered AppImages found." << Qt::endl;
        return;
    }

    QEventLoop checkLoop;
    QVector<PendingUpdate> pendingUpdates;
    collectReleasesAsync(registeredAppImages, [&](QVector<PendingUpdate> updates) {
        pendingUpdates = updates;
        checkLoop.quit();
    }, force);
    execEventLoopLoadingIndicator("Checking for updates ", checkLoop);

    if (pendingUpdates.isEmpty()) {
        out() << "No updates found." << Qt::endl;
        return;
    }

    // Display available updates
    out() << "The following updates are available:" << Qt::endl;
    for (const auto &pu : std::as_const(pendingUpdates)) {
        out() << "  - " << pu.metadata.name << ": " << pu.release.version << Qt::endl;
    }

    // User confirmation
    out() << Qt::endl << "Update all? [Y/n] ";
    out().flush();
    QString response;
    QTextStream(stdin) >> response;
    if (!response.isEmpty() && response.toLower() != "y") {
        out() << "Aborting updates." << Qt::endl;
        return;
    }

    out() << Qt::endl;

    // Print out waiting updates
    for (int i = 0; i < pendingUpdates.size(); ++i) {
        pendingUpdates[i].lineIndex = pendingUpdates.size() - i;
        out() << pendingUpdates[i].metadata.name << ": Waiting..." << Qt::endl;
    }

    // Update all with max concurrent
    const int maxConcurrent = std::max(1, SettingsManager::instance()->updateConcurrency());
    auto queue = std::make_shared<std::deque<PendingUpdate>>(pendingUpdates.begin(), pendingUpdates.end());
    auto running = std::make_shared<int>(0);
    auto next = std::make_shared<std::function<void()>>();

    QEventLoop loop; // Wait for all updates to finish

    *next = [queue, running, maxConcurrent, next, &loop, pendingUpdates]() mutable {
        while (!queue->empty() && *running < maxConcurrent) {
            PendingUpdate pu = queue->front();
            queue->pop_front();

            (*running)++;

            // Start update
            auto future = updateAppImageAsync(
                pu.metadata.name,
                pu.metadata.path,
                pu.release.download,
                pu.release.version,
                pu.release.date,
                pu.lineIndex
                );

            // Watch the future
            auto watcher = new QFutureWatcher<bool>();
            watcher->setFuture(future);

            QObject::connect(watcher, &QFutureWatcher<bool>::finished, watcher, [running, next, watcher, queue, loopPtr = &loop]() mutable {
                (*running)--;

                // Launch next update if possible
                (*next)();

                // If everything is done, quit the loop
                if (queue->empty() && *running == 0)
                    loopPtr->quit();

                watcher->deleteLater();
            });
        }
    };

    // Kick off first batch
    for (int i = 0; i < maxConcurrent; ++i)
        (*next)();

    // Wait for all updates to finish
    loop.exec();

    out() << Qt::endl << "All updates finished." << Qt::endl;
}

QFuture<bool> CliHandler::updateAppImageAsync(const QString &name,
                                              const QString &path,
                                              const QString &downloadUrl,
                                              const QString &version,
                                              const QString &date,
                                              const int lineIndex)
{
    auto promise = std::make_shared<QPromise<bool>>();
    QFuture<bool> future = promise->future();

    AppImageUtil::updateAppImage(
        path,
        downloadUrl,
        version,
        date,

        [promise](bool success) {
            promise->addResult(success);
            promise->finish();
        },

        [name, lineIndex, version](UpdateState state, qint64 received, qint64 total) {
            QString status;
            switch(state) {
            case UpdateState::Downloading:
                status = QString("%1: %2 (%3%)")
                             .arg(name,
                                  StringUtil::getUpdateDownloadText(received, total),
                                  total > 0 ? QString::number(received * 100.0 / total, 'f', 1) : "0.0");
                break;
            case UpdateState::Extracting:
                status = QString("%1: Extracting...").arg(name);
                break;
            case UpdateState::Installing:
                status = QString("%1: Installing...").arg(name);
                break;
            case UpdateState::Success:
                status = QString("%1: Updated to %2").arg(name, version);
                break;
            case UpdateState::Failed:
                status = QString("%1: Failed").arg(name);
                break;
            default:
                status = QString("%1: Waiting...").arg(name);
                break;
            }

            QMutexLocker lock(&CliHandler::m_outputMutex);

            out() << "\r";
            moveCursorUp(lineIndex);
            clearLine();
            out() << status.leftJustified(UPDATE_STATUS_WIDTH, ' ') << Qt::flush;
            moveCursorDown(lineIndex);
        }
        );

    return future;
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

    for (const auto& filter : std::as_const(metadata.updateFilters)) {
        settings.filters.append({filter.field, filter.pattern});
    }

    return settings;
}

void CliHandler::fetchReleasesAsync(
    const AppImageUtilMetadata &appimage,
    std::function<void(QList<UpdaterRelease>)> callback)
{
    if (appimage.updateType.isEmpty()) {
        callback({});
        return;
    }

    UpdaterSettings settings = getUpdaterSettings(appimage);

    auto* updater = UpdaterFactory::create(
        appimage.updateType,
        settings,
        appimage.updateCurrentVersion,
        appimage.updateCurrentDate
        );

    if (!updater) {
        callback({});
        return;
    }

    QObject::connect(updater, &IUpdater::updatesReady,
                     [updater, callback]() {
                         auto releases = updater->releases();
                         updater->deleteLater();
                         callback(releases);
                     });

    updater->fetchUpdatesAsync();
}

void CliHandler::collectReleasesAsync(const QList<AppImageUtilMetadata> &appimages,
                                      std::function<void(QVector<PendingUpdate>)> finishedCallback,
                                      bool force,
                                      int timeoutMs)
{
    if (appimages.isEmpty()) {
        finishedCallback({});
        return;
    }

    const int maxConcurrent = std::max(1, SettingsManager::instance()->updateConcurrency());
    auto queue = std::make_shared<std::deque<AppImageUtilMetadata>>(appimages.begin(), appimages.end());
    auto running = std::make_shared<int>(0);
    auto updates = std::make_shared<QVector<PendingUpdate>>();

    auto next = std::make_shared<std::function<void()>>();

    *next = [queue, running, updates, maxConcurrent, force, next, finishedCallback, timeoutMs]() mutable {
        while (!queue->empty() && *running < maxConcurrent) {
            AppImageUtilMetadata app = queue->front();
            queue->pop_front();

            (*running)++;

            // Setup a safety timeout for this fetch
            auto timeoutTimer = new QTimer;
            timeoutTimer->setSingleShot(true);

            fetchReleasesAsync(app, [app, updates, running, queue, next, force, finishedCallback, timeoutTimer](QList<UpdaterRelease> releases) mutable {
                timeoutTimer->stop();
                timeoutTimer->deleteLater();

                (*running)--;

                for (const auto &r : releases) {
                    if (r.isLatest && (force || r.isNew)) {
                        updates->push_back({app, r});
                        break;
                    }
                }

                if (queue->empty() && *running == 0) {
                    finishedCallback(*updates);
                    return;
                }

                (*next)();
            });

            QObject::connect(timeoutTimer, &QTimer::timeout, [running, queue, next, finishedCallback, updates]() {
                (*running)--;

                if (queue->empty() && *running == 0) {
                    finishedCallback(*updates);
                    return;
                }

                (*next)();
            });

            timeoutTimer->start(timeoutMs);
        }
    };

    for (int i = 0; i < maxConcurrent; ++i)
        (*next)();
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
    out() << message << " " << Qt::flush;

    int spinnerIndex = 0;
    int dotCount = 0;
    const int maxDots = 3;

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        if (indicator == LoadingIndicator::Spinner) {
            // Erase previous spinner char and print next
            out() << "\b" << kSpinnerChars[spinnerIndex].toLatin1() << Qt::flush;
            spinnerIndex = (spinnerIndex + 1) % 4;
        }
        else if (indicator == LoadingIndicator::Dots) {
            // Move cursor back to end of message (overwrite previous dots)
            out() << "\r" << message << " ";
            for (int i = 0; i < dotCount; ++i)
                out() << ".";

            // Pad remaining space so old dots are erased
            for (int i = dotCount; i < maxDots; ++i)
                out() << " ";

            out().flush();

            dotCount = (dotCount + 1) % (maxDots + 1); // 0..3 dots
        }
    });

    int interval = (indicator == LoadingIndicator::Spinner) ? 100 : 300;
    timer.start(interval);

    loop.exec();  // Run event loop while async task is working
    timer.stop();

    // Fully clear the line and reset cursor
    out() << "\r" << QString(message.length() + maxDots + 2, QChar(' ')) << "\r" << Qt::flush;
}

void CliHandler::printErrors()
{
    // Print all collected warnings first
    if (!CliHandler::m_warnings.isEmpty()) {
        err() << Qt::endl << "Warnings:" << Qt::endl;
        for (const QString &warning : std::as_const(CliHandler::m_warnings)) {
            err() << "  - " << warning << Qt::endl;
        }
    }

    // Then print all errors
    if (!CliHandler::m_errors.isEmpty()) {
        err() << Qt::endl << "Errors:" << Qt::endl;
        for (const QString &error : std::as_const(CliHandler::m_errors)) {
            err() << "  - " << error << Qt::endl;
        }
    }
}

void CliHandler::handleErrorManager()
{
    QMutexLocker lock(&m_errorMutex);
    m_errors.clear();
    m_warnings.clear();

    QObject::connect(ErrorManager::instance(), &ErrorManager::messageOccurred,
                     [](const QString &msg, ErrorManager::MessageType type) {
                         QMutexLocker lock(&CliHandler::m_errorMutex);

                         if (type == ErrorManager::Error) {
                             CliHandler::m_errors.append(msg);
                         } else if (type == ErrorManager::Warning) {
                             CliHandler::m_warnings.append(msg);
                         }
                     });
}
