#include "clihandler.h"
#include "utils/appimageutil.h"

#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QCoreApplication>

#include <algorithm>
#include <iostream>
#include <iomanip>

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
                else if (spec.key == "version") text = appImage.version;
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
                else if (spec.key == "version") text = appImage.version;
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
    printField("Version", appImage.version.toStdString());
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
