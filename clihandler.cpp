#include "clihandler.h"
#include "managers/errormanager.h"
#include "utils/appimageutil.h"
#include "utils/stringutil.h"

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QMutex>
#include <QPromise>
#include <QTextStream>
#include <QTimer>

#include <algorithm>
#include <deque>
#include <functional>
#include <memory>
#include <utility>

#include <sys/ioctl.h>
#include <unistd.h>

namespace {
    constexpr QChar kSpinnerChars[] = {'|', '/', '-', '\\'};

    bool stdoutIsTTY()
    {
        static const bool tty = ::isatty(fileno(stdout)) == 1;
        return tty;
    }
}

int CliHandler::m_maxNameWidth;
bool CliHandler::m_inPlace = false;
QVector<QString> CliHandler::m_errors;
QVector<QString> CliHandler::m_warnings;
QMutex CliHandler::m_errorMutex;
QVector<UpdateStatus> CliHandler::m_status;
QMutex CliHandler::m_statusMutex;

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

// ---------------------------------------------------------------------------
// Output helpers (colors, unicode, terminal size, row formatting)
// ---------------------------------------------------------------------------
namespace {

    // Color only on a TTY, and honor https://no-color.org
    bool useColor()
    {
        static const bool c = stdoutIsTTY() && qEnvironmentVariableIsEmpty("NO_COLOR");
        return c;
    }

    QString style(const QString &text, const char *code)
    {
        if (!useColor())
            return text;
        return QString("\033[%1m%2\033[0m").arg(QString::fromLatin1(code), text);
    }

    // Unicode icons only on a TTY with a UTF-8 locale; ASCII fallbacks otherwise
    bool useUnicode()
    {
        static const bool u = []() {
            if (!stdoutIsTTY())
                return false;
            for (const char *var : {"LC_ALL", "LC_CTYPE", "LANG"}) {
                const QByteArray v = qgetenv(var);
                if (!v.isEmpty())
                    return v.toLower().contains("utf");
            }
            return false;
        }();
        return u;
    }

    winsize terminalSize()
    {
        winsize ws{};
        if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0 || ws.ws_col == 0) {
            ws.ws_col = 80;
            ws.ws_row = 24;
        }
        if (ws.ws_row == 0)
            ws.ws_row = 24;
        return ws;
    }

    int terminalWidth() { return terminalSize().ws_col; }
    int terminalRows()  { return terminalSize().ws_row; }

    /**
     * Formats one row of the progress block, e.g.
     *   ↓ Krita      ████████░░░░  62%  Downloading 14.2 MB / 22.9 MB
     *   ✓ Obsidian   updated to 1.6.0
     * @param showBar draw the progress bar (in-place mode only)
     * @param termWidth if > 0, the detail text is truncated to fit
     */
    QString formatRow(const UpdateStatus &row, int nameWidth, bool showBar, int termWidth)
    {
        const bool uni = useUnicode();
        auto pick = [uni](const char *u, const char *a) {
            return QString::fromUtf8(uni ? u : a);
        };

        QString icon;
        QString detail;
        const char *color = "0";

        switch (row.state) {
        case RowState::Waiting:
            icon = pick("·", ".");
            color = "2";
            detail = QStringLiteral("waiting");
            break;

        case RowState::Downloading: {
            icon = pick("↓", ">");
            color = "36";
            if (showBar && row.total > 0) {
                const QString text = StringUtil::getUpdateDownloadText(row.received, row.total);
                const double frac = qBound(0.0, double(row.received) / double(row.total), 1.0);
                constexpr int barWidth = 12;
                const int filled = int(frac * barWidth + 0.5);
                const QChar fullChar(uni ? 0x2588 : '#');
                const QChar emptyChar(uni ? 0x2591 : '-');
                const QString bar = QString(filled, fullChar) + QString(barWidth - filled, emptyChar);
                const QString pct = QString::number(frac * 100.0, 'f', 0).rightJustified(3);
                detail = QString("%1 %2%  %3").arg(bar, pct, text);
            } else if (showBar) {
                // Size unknown: no bar/percent, just show what we have
                detail = StringUtil::getUpdateDownloadText(row.received, row.total);
            } else {
                detail = QStringLiteral("downloading...");
            }
            break;
        }

        case RowState::Extracting:
            icon = pick("»", "*");
            color = "36";
            detail = QStringLiteral("extracting...");
            break;

        case RowState::Installing:
            icon = pick("»", "*");
            color = "36";
            detail = QStringLiteral("installing...");
            break;

        case RowState::Done:
            icon = pick("✓", "+");
            color = "32";
            detail = row.version.isEmpty() ? QStringLiteral("updated")
                                           : QString("updated to %1").arg(row.version);
            break;

        case RowState::Failed:
            icon = pick("✗", "x");
            color = "31";
            detail = QStringLiteral("failed");
            break;
        }

        // Keep each row on one terminal line, otherwise the in-place redraw
        // (cursor-up by row count) drifts.
        if (termWidth > 0) {
            const int avail = termWidth - 1 - (nameWidth + 4);   // icon + space + name + 2 spaces
            detail.truncate(qMax(0, avail));
        }

        const bool colorDetail = (row.state == RowState::Done || row.state == RowState::Failed);

        return style(icon, color)
               + QChar(' ')
               + row.name.leftJustified(nameWidth, QChar(' '))
               + QStringLiteral("  ")
               + (colorDetail ? style(detail, color) : detail);
    }

    /**
     * One-line breakdown of an update check plus any apps that couldn't be checked
     */
    void printCheckSummary(const CollectResult &r)
    {
        QStringList parts;
        auto add = [&](CheckOutcome outcome, const QString &label) {
            const int n = r.count(outcome);
            if (n > 0)
                parts << QString("%1 %2").arg(n).arg(label);
        };
        add(CheckOutcome::UpdateAvailable, QStringLiteral("with updates"));
        add(CheckOutcome::UpToDate,        QStringLiteral("up to date"));
        add(CheckOutcome::TimedOut,        QStringLiteral("timed out"));
        add(CheckOutcome::NoReleaseInfo,   QStringLiteral("returned no release info"));
        add(CheckOutcome::Skipped,         QStringLiteral("skipped (no valid update source)"));

        out() << "Checked " << r.entries.size() << " AppImage(s): " << parts.join(QStringLiteral(", ")) << Qt::endl;

        const QString mark = QString::fromUtf8(useUnicode() ? "!" : "!");
        for (const auto &e : r.entries) {
            if (e.outcome == CheckOutcome::TimedOut)
                out() << style(QString("  %1 %2: timed out").arg(mark, e.name), "33") << Qt::endl;
            else if (e.outcome == CheckOutcome::NoReleaseInfo)
                out() << style(QString("  %1 %2: no release info returned").arg(mark, e.name), "33") << Qt::endl;
        }
        out() << Qt::endl;
    }

} // namespace

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
                                  "appimage_path");
    parser.addOption(infoOption);

    // Add update option
    QCommandLineOption updateOption({"u", "update"},
                                  "Updates appimage",
                                  "appimage_path");
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
        const bool tableOutput = parser.isSet(tableOption);
        result.exitCode = finishCommand(list(columnsStr, tableOutput));
        result.shouldExit = true;
        return result;
    }
    else if (parser.isSet(infoOption)) {
        const QString appImage = parser.value(infoOption);
        result.exitCode = finishCommand(info(appImage));
        result.shouldExit = true;
        return result;
    }
    else if (parser.isSet(updateOption)) {
        const bool force = parser.isSet(forceOption);
        const QString appimage = parser.value(updateOption);
        result.exitCode = finishCommand(update(appimage, force));
        result.shouldExit = true;
        return result;
    }
    else if (parser.isSet(updateAllOption)) {
        const bool force = parser.isSet(forceOption);
        result.exitCode = finishCommand(updateAll(force));
        result.shouldExit = true;
        return result;
    }

    return result;
}

int CliHandler::list(QString columnsStr, bool tableOutput)
{
    auto registeredAppImages = AppImageUtil::getRegisteredList();

    if (registeredAppImages.isEmpty()) {
        out() << "No registered AppImages found." << Qt::endl;
        return 0;
    }

    // Parse columns from string
    QStringList columnsList = columnsStr.split(',', Qt::SkipEmptyParts);

    // Tolerate spaces around names ("name, version") and drop blank entries
    for (auto &col : columnsList)
        col = col.trimmed();
    columnsList.removeAll(QString());

    // Validate columns
    for (const auto& col : std::as_const(columnsList)) {
        if (!VALID_COLUMNS.contains(col)) {
            err() << "Error: Invalid column '" << col << "'. Valid columns are: "
                      << VALID_COLUMNS.join(",") << Qt::endl;
            return 1;
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
            for (qsizetype i = 0; i < maxLines; ++i) {
                for (qsizetype colIdx = 0; colIdx < selectedColumns.size(); ++colIdx) {
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
    return 0;
}

int CliHandler::info(QString path)
{
    if (path.isEmpty()) {
        err() << "Error: No AppImage path provided. Use: --info <appimage_path>" << Qt::endl;
        return 1;
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
    return 0;
}

int CliHandler::update(QString path, bool force)
{
    if (path.isEmpty()) {
        err() << "Error: No AppImage path provided. Use: --update <appimage_path>" << Qt::endl;
        return 1;
    }

    AppImageUtil util(path);
    AppImageUtilMetadata appimage = util.metadata();

    const QString currentVersion = StringUtil::coalesce(appimage.updateCurrentVersion, appimage.version);
    out() << style(QString("%1 (%2)").arg(appimage.name, currentVersion), "1") << Qt::endl << Qt::endl;

    if (appimage.updateType.isEmpty()) {
        err() << "Error: No update type set in " << appimage.desktopFilePath << Qt::endl;
        return 1;
    }

    QEventLoop releaseLoop;
    QList<UpdaterRelease> fetchedReleases;
    bool timedOut = false;
    bool skipped = false;

    // Only the fetch or the timeout may complete this phase. If the timeout
    // wins, the in-flight fetch is aborted so its callback can never run
    // against this function's locals after we've returned.
    auto completed = std::make_shared<bool>(false);

    auto* updater = fetchReleasesAsync(appimage, [&, completed](QList<UpdaterRelease> releases, bool wasSkipped) {
        if (*completed)
            return;
        *completed = true;

        fetchedReleases = releases;
        skipped = wasSkipped;
        releaseLoop.quit();
    });

    if (updater) {
        // Safety timeout (30s)
        QTimer::singleShot(kFetchTimeoutMs, &releaseLoop, [completed, updater, &releaseLoop, &timedOut]() {
            if (*completed)
                return;
            *completed = true;
            timedOut = true;

            // Abort the in-flight fetch
            delete updater;

            releaseLoop.quit();
        });

        execEventLoopLoadingIndicator("Checking for updates", releaseLoop);
    }
    // If no updater was created (no/unknown update type) the callback above
    // already ran synchronously - nothing left to wait for.

    if (timedOut) {
        err() << "Error: Timed out while checking for updates." << Qt::endl;
        return 1;
    }
    if (skipped) {
        err() << "Error: Unable to check for updates (no valid update source; see warnings below)." << Qt::endl;
        return 1;
    }
    if (fetchedReleases.isEmpty()) {
        err() << "Error: No release information was returned for this AppImage." << Qt::endl;
        return 1;
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
        out() << style("Your AppImage is already up to date. No updates available.", "32") << Qt::endl;
        return 0;
    }

    // Print releases
    out() << "Available updates:\n";
    for (int i = 0; i < selectableReleases.size(); ++i) {
        const auto &r = selectableReleases[i];

        QString line;

        // Version / date
        if (!r->version.isEmpty())
            line += r->version;
        if (!r->date.isEmpty()) {
            if (!r->version.isEmpty())
                line += QStringLiteral(" | ");
            line += StringUtil::formatDateTime(r->date);
        }

        // Flags
        if (r->isNew)
            line += r->isLatest ? QStringLiteral("  <-- new (latest)") : QStringLiteral("  <-- new");
        else if (r->isLatest)
            line += QStringLiteral("  <-- latest");

        // Bold latest new release
        const bool bold = r->isNew && r->isLatest;
        out() << "[" << (i + 1) << "] " << (bold ? style(line, "1") : line) << Qt::endl;
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

        QString line = qin.readLine();

        // Treat EOF on stdin (e.g. </dev/null or a truncated pipe) as a
        // cancellation, not as the default selection.
        if (qin.atEnd() && line.isEmpty()) {
            out() << "Update cancelled (no input from stdin)." << Qt::endl;
            return 1;
        }
        line = line.trimmed();

        // Cancel
        if (line.compare("c", Qt::CaseInsensitive) == 0 ||
            line.compare("cancel", Qt::CaseInsensitive) == 0) {
            out() << "Update cancelled by user." << Qt::endl;
            return 0;
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
    out() << Qt::endl;

    // Single update goes through the same progress display as update-all
    PendingUpdate pu{appimage, *chosenRelease};
    const QVector<bool> results = runUpdates({pu});

    out() << Qt::endl;
    if (results.value(0, false)) {
        out() << style("Update completed successfully.", "32") << Qt::endl;
        return 0;
    }

    err() << "Update failed." << Qt::endl;
    return 1;
}

int CliHandler::updateAll(bool force)
{
    auto registeredAppImages = AppImageUtil::getRegisteredList();

    if (registeredAppImages.isEmpty()) {
        out() << "No registered AppImages found." << Qt::endl;
        return 0;
    }

    // These locals outlive the collect callbacks: updateAll blocks in the
    // event loop below until the finished callback fires, and every callback
    // runs before that point.
    QEventLoop checkLoop;
    CollectResult checkResult;
    bool checkFinished = false;
    int checkedDone = 0;
    int checkedTotal = registeredAppImages.size();

    collectReleasesAsync(registeredAppImages,
        [&](CollectResult r) {
            checkResult = std::move(r);
            checkFinished = true;
            checkLoop.quit();
        },
        force,
        kFetchTimeoutMs,
        [&](int done, int total) {
            checkedDone = done;
            checkedTotal = total;
        });

    if (!checkFinished) {
        execEventLoopLoadingIndicator("Checking for updates", checkLoop,
                                      LoadingIndicator::Spinner,
                                      [&]() { return QString("(%1/%2)").arg(checkedDone).arg(checkedTotal); });
    }
    // If no fetches were started (all update types empty/unknown) the finished
    // callback already ran synchronously - nothing left to wait for.

    printCheckSummary(checkResult);

    // Apps we couldn't verify count against the exit code (skipped ones are a
    // configuration matter and are reported as warnings instead).
    const bool checkProblems = checkResult.count(CheckOutcome::TimedOut) > 0
                               || checkResult.count(CheckOutcome::NoReleaseInfo) > 0;

    QVector<PendingUpdate> pendingUpdates = checkResult.updates;

    if (pendingUpdates.isEmpty()) {
        out() << "No updates found." << Qt::endl;
        return checkProblems ? 1 : 0;
    }

    // Display available updates: name, current -> new
    int nameWidth = 0;
    int currentWidth = 0;
    for (const auto &pu : std::as_const(pendingUpdates)) {
        nameWidth = qMax(nameWidth, int(pu.metadata.name.length()));
        const QString cur = StringUtil::coalesce(pu.metadata.updateCurrentVersion, pu.metadata.version);
        currentWidth = qMax(currentWidth, int(cur.length()));
    }

    const QString arrow = QString::fromUtf8(useUnicode() ? "→" : "->");
    out() << style("Updates available:", "1") << Qt::endl;
    for (const auto &pu : std::as_const(pendingUpdates)) {
        const QString cur = StringUtil::coalesce(pu.metadata.updateCurrentVersion, pu.metadata.version);
        out() << "  " << pu.metadata.name.leftJustified(nameWidth, ' ')
              << "  " << cur.leftJustified(currentWidth, ' ')
              << "  " << arrow << "  "
              << style(pu.release.version, "32") << Qt::endl;
    }

    // User confirmation
    out() << Qt::endl << "Update all? [Y/n] ";
    out().flush();
    QTextStream in(stdin);
    QString response = in.readLine();

    // Treat EOF on stdin (e.g. </dev/null or a truncated pipe) as a
    // cancellation, not as a "yes".
    if (in.atEnd() && response.isEmpty()) {
        out() << "Aborting updates (no input from stdin)." << Qt::endl;
        return 1;
    }
    response = response.trimmed();
    if (!response.isEmpty() && response.toLower() != "y") {
        out() << "Aborting updates." << Qt::endl;
        return 0;
    }

    out() << Qt::endl;

    const QVector<bool> results = runUpdates(pendingUpdates);

    // Summary
    int succeeded = 0;
    QStringList failedNames;
    for (int i = 0; i < pendingUpdates.size(); ++i) {
        if (results.value(i, false))
            ++succeeded;
        else
            failedNames << pendingUpdates[i].metadata.name;
    }

    out() << Qt::endl;
    if (failedNames.isEmpty()) {
        out() << style(QString("%1 AppImage(s) updated successfully.").arg(succeeded), "32") << Qt::endl;
    } else {
        out() << style(QString("%1 updated, %2 failed: %3")
                           .arg(succeeded)
                           .arg(failedNames.size())
                           .arg(failedNames.join(QStringLiteral(", "))), "31") << Qt::endl;
    }

    return (failedNames.isEmpty() && !checkProblems) ? 0 : 1;
}

QVector<bool> CliHandler::runUpdates(QVector<PendingUpdate> pending)
{
    QVector<bool> results(pending.size(), false);
    if (pending.isEmpty())
        return results;

    // Initialize status rows and print them once so the in-place redraw has
    // a block to move back over.
    {
        QMutexLocker lock(&m_statusMutex);

        m_status.clear();
        m_status.resize(pending.size());
        m_maxNameWidth = 0;

        for (int i = 0; i < pending.size(); ++i) {
            pending[i].lineIndex = i;
            m_status[i].name = pending[i].metadata.name;
            m_status[i].version = pending[i].release.version;
            m_maxNameWidth = qMax(m_maxNameWidth, int(m_status[i].name.length()));
        }

        // Redraw in place only on a TTY with room for the whole block;
        // otherwise print one line per state change.
        m_inPlace = stdoutIsTTY() && (pending.size() + 2 < terminalRows());

        const int width = m_inPlace ? terminalWidth() : 0;
        for (const auto &row : std::as_const(m_status))
            out() << formatRow(row, m_maxNameWidth, m_inPlace, width) << Qt::endl;
        out().flush();
    }

    // Update Status Output
    QTimer renderTimer;
    QObject::connect(&renderTimer, &QTimer::timeout, []() {
        CliHandler::outputUpdateAllStatus();
    });
    renderTimer.start(100);

    // Run updates with max concurrency
    const int maxConcurrent = std::max(1, SettingsManager::instance()->updateConcurrency());
    std::deque<PendingUpdate> queue(pending.begin(), pending.end());
    int running = 0;

    QEventLoop loop; // Wait for all updates to finish

    // Note: `next` captures itself by reference only; all invocations happen
    // while this function's frame is blocked in `loop.exec()`.
    std::function<void()> next;
    next = [&queue, &running, &results, maxConcurrent, &next, &loop]() {
        while (!queue.empty() && running < maxConcurrent) {
            PendingUpdate pu = queue.front();
            queue.pop_front();

            running++;
            const int idx = pu.lineIndex;

            // Start update
            auto future = updateAppImageAsync(
                pu.metadata.path,
                pu.release.download,
                pu.release.version,
                pu.release.date,
                idx
                );

            // Watch the future
            auto watcher = new QFutureWatcher<bool>();
            watcher->setFuture(future);

            QObject::connect(watcher, &QFutureWatcher<bool>::finished, watcher,
                             [&queue, &running, &results, &next, &loop, watcher, idx]() {
                const bool ok = watcher->future().resultCount() > 0 && watcher->result();
                results[idx] = ok;

                // Make sure the row ends in the right final state even if the
                // progress callback never reported it.
                {
                    QMutexLocker lock(&m_statusMutex);
                    if (idx >= 0 && idx < m_status.size())
                        m_status[idx].state = ok ? RowState::Done : RowState::Failed;
                }

                running--;

                // Launch next update if possible
                next();

                // If everything is done, quit the loop
                if (queue.empty() && running == 0)
                    loop.quit();

                watcher->deleteLater();
            });
        }
    };

    // Kick off first batch
    for (int i = 0; i < maxConcurrent; ++i)
        next();

    // Wait for all updates to finish
    loop.exec();
    renderTimer.stop();
    outputUpdateAllStatus();

    return results;
}

void CliHandler::outputUpdateAllStatus()
{
    QMutexLocker lock(&m_statusMutex);

    if (m_status.isEmpty())
        return;

    if (!m_inPlace) {
        // Line mode (piped output, or too many rows for the terminal): print
        // one line per state change. Download progress ticks are not printed.
        const int width = 0;
        for (auto &row : m_status) {
            if (row.state == row.printedState)
                continue;
            row.printedState = row.state;
            out() << formatRow(row, m_maxNameWidth, false, width) << Qt::endl;
        }
        out().flush();
        return;
    }

    // In-place: go back over the block and redraw every row
    const int width = terminalWidth();
    out() << "\r";
    moveCursorUp(m_status.size());

    for (const auto &row : std::as_const(m_status))
        out() << "\033[2K\r" << formatRow(row, m_maxNameWidth, true, width) << "\n";

    out().flush();
}

QFuture<bool> CliHandler::updateAppImageAsync(const QString &path,
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

        [lineIndex](UpdateState state, qint64 received, qint64 total) {
            QMutexLocker lock(&CliHandler::m_statusMutex);
            if (lineIndex < 0 || lineIndex >= m_status.size())
                return;

            UpdateStatus &row = m_status[lineIndex];
            switch(state) {
            case UpdateState::Downloading:
                row.state = RowState::Downloading;
                row.received = received;
                row.total = total;
                break;
            case UpdateState::Extracting:
                row.state = RowState::Extracting;
                break;
            case UpdateState::Installing:
                row.state = RowState::Installing;
                break;
            case UpdateState::Success:
                row.state = RowState::Done;
                break;
            case UpdateState::Failed:
                row.state = RowState::Failed;
                break;
            default:
                break;
            }
        });

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

IUpdater* CliHandler::fetchReleasesAsync(
    const AppImageUtilMetadata &appimage,
    std::function<void(QList<UpdaterRelease>, bool)> callback)
{
    if (appimage.updateType.isEmpty()) {
        callback({}, true);
        return nullptr;
    }

    UpdaterSettings settings = getUpdaterSettings(appimage);

    auto* updater = UpdaterFactory::create(
        appimage.updateType,
        settings,
        appimage.updateCurrentVersion,
        appimage.updateCurrentDate
        );

    if (!updater) {
        ErrorManager::instance()->reportWarning(
            QString("Unknown update type '%1' for '%2'")
                .arg(appimage.updateType, appimage.name));
        callback({}, true);
        return nullptr;
    }

    QObject::connect(updater, &IUpdater::updatesReady,
                     [updater, callback]() {
                         auto releases = updater->releases();
                         updater->deleteLater();
                         callback(releases, false);
                     });

    updater->fetchUpdatesAsync();

    return updater;
}

void CliHandler::collectReleasesAsync(const QList<AppImageUtilMetadata> &appimages,
                                      std::function<void(CollectResult)> finishedCallback,
                                      bool force,
                                      int timeoutMs,
                                      std::function<void(int, int)> progress)
{
    if (appimages.isEmpty()) {
        finishedCallback({});
        return;
    }

    // This function returns right after starting the first batch of fetches;
    // the callbacks fire later from the caller's event loop. All shared state
    // therefore lives on the heap (shared_ptr), NOT in this function's locals.
    struct CollectState {
        std::deque<AppImageUtilMetadata> queue;
        int running = 0;
        int done = 0;
        int total = 0;
        QVector<PendingUpdate> updates;
        QVector<CheckEntry> entries;
        std::function<void()> next;
        std::function<void(CollectResult)> finished;
        std::function<void(int, int)> progress;
    };

    auto state = std::make_shared<CollectState>();
    state->queue.assign(appimages.begin(), appimages.end());
    state->total = int(appimages.size());
    state->finished = std::move(finishedCallback);
    state->progress = std::move(progress);

    const int maxConcurrent = std::max(1, SettingsManager::instance()->updateConcurrency());

    // Called whenever one fetch finishes (success, failure or timeout)
    auto onItemDone = [](const std::shared_ptr<CollectState> &s,
                         const QString &name,
                         CheckOutcome outcome) {
        s->entries.push_back({name, outcome});
        s->running--;
        s->done++;

        if (s->progress)
            s->progress(s->done, s->total);

        if (s->queue.empty() && s->running == 0) {
            CollectResult result;
            result.updates = s->updates;
            result.entries = s->entries;

            // Fetches complete in arbitrary order; present them sorted
            auto byName = [](const QString &a, const QString &b) {
                return a.compare(b, Qt::CaseInsensitive) < 0;
            };
            std::stable_sort(result.updates.begin(), result.updates.end(),
                             [&](const PendingUpdate &a, const PendingUpdate &b) {
                                 return byName(a.metadata.name, b.metadata.name);
                             });
            std::stable_sort(result.entries.begin(), result.entries.end(),
                             [&](const CheckEntry &a, const CheckEntry &b) {
                                 return byName(a.name, b.name);
                             });

            s->finished(std::move(result));
            return;
        }

        // Copy so we never run a std::function that could be reassigned
        auto n = s->next;
        n();
    };

    // weak_ptr avoids a state -> next -> state reference cycle (leak)
    std::weak_ptr<CollectState> weak = state;
    state->next = [weak, maxConcurrent, force, timeoutMs, onItemDone]() {
        auto st = weak.lock();
        if (!st)
            return;

        while (!st->queue.empty() && st->running < maxConcurrent) {
            AppImageUtilMetadata app = st->queue.front();
            st->queue.pop_front();

            st->running++;

            // Only the fetch or the timeout may complete this item
            auto completed = std::make_shared<bool>(false);

            // Setup a safety timeout for this fetch
            auto *timer = new QTimer;
            timer->setSingleShot(true);

            auto *updater = fetchReleasesAsync(app,
                [st, app, force, timer, completed, onItemDone](QList<UpdaterRelease> releases, bool skipped) {
                    if (*completed)
                        return;
                    *completed = true;

                    timer->stop();
                    timer->deleteLater();

                    CheckOutcome outcome = CheckOutcome::UpToDate;
                    if (skipped) {
                        outcome = CheckOutcome::Skipped;
                    } else if (releases.isEmpty()) {
                        outcome = CheckOutcome::NoReleaseInfo;
                    } else {
                        for (const auto &r : releases) {
                            if (r.isLatest && (force || r.isNew)) {
                                st->updates.push_back({app, r});
                                outcome = CheckOutcome::UpdateAvailable;
                                break;
                            }
                        }
                    }

                    onItemDone(st, app.name, outcome);
                });

            QObject::connect(timer, &QTimer::timeout, timer,
                [st, app, completed, updater, timer, onItemDone]() {
                    if (*completed)
                        return;
                    *completed = true;

                    // Abort in-flight fetch (updater is non-null here, since
                    // a null updater means the callback already completed)
                    delete updater;

                    timer->deleteLater();

                    onItemDone(st, app.name, CheckOutcome::TimedOut);
                });

            // If the callback already ran synchronously (empty/unknown update
            // type), the item is finished: don't start the timeout.
            if (!*completed)
                timer->start(timeoutMs);
        }
    };

    for (int i = 0; i < maxConcurrent; ++i)
        state->next();
}

QStringList CliHandler::getWrappedText(const QString& text, int width, QChar splitChar)
{
    // A non-positive width would make the chunking loop below never advance
    width = std::max(1, width);

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
                                               LoadingIndicator indicator,
                                               std::function<QString()> suffix)
{
    if (!stdoutIsTTY()) {
        // Plain-text fallback for piped/redirected stdout: print the message
        // once instead of animating it in place.
        out() << message << "..." << Qt::endl;
        loop.exec();
        return;
    }

    int spinnerIndex = 0;
    int dotCount = 0;
    constexpr int maxDots = 3;

    // Redraws the whole line each tick so a live suffix (e.g. "(4/12)") works
    auto render = [&]() {
        QString line = message;
        line += QChar(' ');

        if (indicator == LoadingIndicator::Spinner) {
            line += QString(kSpinnerChars[spinnerIndex]);
            spinnerIndex = (spinnerIndex + 1) % 4;
        } else {
            line += QString(dotCount, QChar('.'));
            line += QString(maxDots - dotCount, QChar(' '));
            dotCount = (dotCount + 1) % (maxDots + 1); // 0..3 dots
        }

        if (suffix) {
            line += QStringLiteral("  ");
            line += suffix();
        }

        out() << "\r" << line << "\033[K" << Qt::flush;
    };

    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, render);

    const int interval = (indicator == LoadingIndicator::Spinner) ? 100 : 300;
    timer.start(interval);

    render();     // show immediately instead of waiting for the first tick
    loop.exec();  // Run event loop while async task is working
    timer.stop();

    // Fully clear the line and reset cursor
    out() << "\r\033[K" << Qt::flush;
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

int CliHandler::finishCommand(int exitCode)
{
    printErrors();

    QMutexLocker lock(&m_errorMutex);
    if (exitCode == 0 && !m_errors.isEmpty())
        return 1;
    return exitCode;
}

void CliHandler::handleErrorManager()
{
    QMutexLocker lock(&m_errorMutex);
    m_errors.clear();
    m_warnings.clear();

    // The ErrorManager singleton outlives individual commands, so connect
    // only once to avoid stacking duplicate handlers on repeated calls.
    static bool connected = false;
    if (!connected) {
        connected = true;

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
}