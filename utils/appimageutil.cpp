#include "appimageutil.h"
#include "managers/errormanager.h"
#include "managers/settingsmanager.h"
#include "utils/archiveutil.h"
#include "utils/networkutil.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QSettings>
#include <QRegularExpression>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonParseError>
#include <QtConcurrent/QtConcurrentRun>

// ----------------- Public -----------------

AppImageUtil::AppImageUtil(const QString& path)
    : m_path(path), m_mountPath(QString()), m_process(nullptr) {}

AppImageUtil::~AppImageUtil()
{
    unmountAppImage();
}

const bool AppImageUtil::isAppImageType2(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        ErrorManager::instance()->reportError("Failed to open file: " + path);
        return false;
    }

    file.seek(8);
    QByteArray magic = file.read(3);

    return magic == QByteArray("AI\2");
}

const QString AppImageUtil::getChecksum(const QString& path, const QCryptographicHash::Algorithm hashType) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        ErrorManager::instance()->reportError("Failed to open file:" + path);
        return QString();
    }

    QCryptographicHash hash(hashType);

    if (!hash.addData(&file)) {
        ErrorManager::instance()->reportError("Failed to read file for hashing:" + path);
        return QString();
    }

    return hash.result().toHex();
}

const bool AppImageUtil::isExecutable(const QString& path) {
    QFileInfo fileInfo(path);
    return fileInfo.isExecutable();
}

const bool AppImageUtil::makeExecutable(const QString& path)
{
    QFile file(path);
    if (!file.exists()) {
        ErrorManager::instance()->reportError("File does not exist: " + path);
        return false;
    }

    QFile::Permissions perms = file.permissions();
    perms |= QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther;

    if (!file.setPermissions(perms)) {
        ErrorManager::instance()->reportError("Failed to set executable permissions for: " + path);
        return false;
    }

    return true;
}

void AppImageUtil::mountAppImageAsync()
{
    unmountAppImage();

    if (!isExecutable(m_path)) {
        ErrorManager::instance()->reportError("File is not executable: " + m_path);
        emit mountFinished(false);
        return;
    }

    m_process = new QProcess(this);
    m_process->setProgram(m_path);
    m_process->setArguments({"--appimage-mount"});
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    // Called as soon as any stdout is available
    connect(m_process, &QProcess::readyReadStandardOutput, this,
            &AppImageUtil::onMountStdoutReady);

    // Called when process exits
    connect(m_process, &QProcess::finished, this,
            &AppImageUtil::onMountFinished);

    m_process->start();
}

bool AppImageUtil::mountAppImage(int mountTimeoutMs)
{
    QEventLoop loop;
    bool result = false;

    QMetaObject::Connection conn = connect(this, &AppImageUtil::mountFinished, [&](bool success) {
        result = success;
        loop.quit();
    });

    mountAppImageAsync();

    QTimer mountTimer;
    mountTimer.setSingleShot(true);
    QObject::connect(&mountTimer, &QTimer::timeout, [&]() {
        // Only apply timeout if we are still mounting
        if (m_process && !m_tempExtractDir.isEmpty()) {
            return;
        }

        if (m_process) {
            ErrorManager::instance()->reportError("AppImage mount timed out.");
            unmountAppImage();
        }
        loop.quit();
    });
    mountTimer.start(mountTimeoutMs);

    loop.exec();
    disconnect(conn);

    return result;
}

void AppImageUtil::onMountStdoutReady()
{
    if (!m_process)
        return;

    QString output = QString::fromUtf8(m_process->readAllStandardOutput()).trimmed();
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    QString possiblePath = lines.isEmpty() ? QString() : lines.last();

    if (possiblePath.isEmpty())
        return;

    auto checkMount = new QTimer(this);
    checkMount->setInterval(50);
    checkMount->setSingleShot(false);

    connect(checkMount, &QTimer::timeout, this, [this, possiblePath, checkMount]() {
        if (!m_process || m_process->state() != QProcess::Running) {
            checkMount->stop();
            checkMount->deleteLater();
            emit mountFinished(false);
            return;
        }

        if (QDir(possiblePath).exists()) {
            m_mountPath = possiblePath;
            emit mountFinished(true);
            checkMount->stop();
            checkMount->deleteLater();

            if (m_process) {
                disconnect(m_process, &QProcess::readyReadStandardOutput,
                           this, &AppImageUtil::onMountStdoutReady);
            }
        }
    });

    checkMount->start();
}


void AppImageUtil::onMountFinished(int exitCode, QProcess::ExitStatus status)
{
    // If already mounted, no fallback
    if (!m_mountPath.isEmpty())
        return;

    // Fallback to extract
    delete m_process;
    m_process = nullptr;
    extractAppImage();
}

void AppImageUtil::extractAppImage()
{
    // Create a unique temp folder
    QString tempDir = QDir::tempPath() + "/appimage-" + QUuid::createUuid().toString();
    QDir dir(tempDir);
    if (!dir.mkpath(".")) {
        ErrorManager::instance()->reportError("Failed to create temp directory: " + tempDir);
        emit mountFinished(false);
        return;
    }

    m_tempExtractDir = tempDir;

    m_process = new QProcess(this);
    m_process->setProgram(m_path);
    m_process->setArguments({"--appimage-extract"});
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setWorkingDirectory(tempDir);

    connect(m_process, &QProcess::finished,
            this, &AppImageUtil::onExtractFinished);

    m_process->start();
}

void AppImageUtil::onExtractFinished(int exitCode, QProcess::ExitStatus status)
{
    if (status != QProcess::NormalExit || exitCode != 0) {
        ErrorManager::instance()->reportError("AppImage does not support mount or extract.");
        unmountAppImage();
        emit mountFinished(false);
        return;
    }

    QDir tempDir(m_tempExtractDir);
    if (m_tempExtractDir.isEmpty() || !tempDir.exists()) {
        ErrorManager::instance()->reportError("Extraction failed.");
        unmountAppImage();
        emit mountFinished(false);
        return;
    }

    // Get the first subdirectory inside the temp extraction folder
    QStringList subDirs = tempDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (!subDirs.isEmpty()) {
        m_mountPath = tempDir.filePath(subDirs.first());
    } else {
        m_mountPath = m_tempExtractDir;  // fallback in case no subfolders
    }

    emit mountFinished(true);
}

bool AppImageUtil::isMounted()
{
    return m_process && m_process->state() != QProcess::NotRunning && !m_mountPath.isEmpty();
}

void AppImageUtil::unmountAppImage()
{
    // Kill process
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            if (!m_process->waitForFinished(2000)) {
                m_process->kill();
                m_process->waitForFinished(1000);
            }
        }
        delete m_process;
        m_process = nullptr;
    }

    // Try to unmount/remove
    if (!m_mountPath.isEmpty() && QDir(m_mountPath).exists()) {
        if(m_tempExtractDir.isEmpty()) {
            auto tryUnmount = [](const QString &cmd, const QStringList &args) {
                QProcess p;
                p.start(cmd, args);
                p.waitForFinished(3000);
            };

            // fusermount
            tryUnmount("fusermount", {"-u", m_mountPath});

            // umount if still exists
            if (QDir(m_mountPath).exists()) {
                tryUnmount("umount", {m_mountPath});
            }
        }

        // remove mount directory
        QDir dir(m_mountPath);
        dir.removeRecursively();
    }

    // Remove temp extract directory
    if (!m_tempExtractDir.isEmpty()) {
        QDir tempDir(m_tempExtractDir);
        if (tempDir.exists())
            tempDir.removeRecursively();
    }

    m_mountPath.clear();
    m_tempExtractDir.clear();
}

const QString AppImageUtil::integratedDesktopPath(const QString& path)
{
    const QStringList searchPaths = getSearchPaths();

    for (const QString& dirPath : searchPaths) {
        QDir dir(dirPath);
        const QStringList desktopFiles = dir.entryList(QStringList() << "*.desktop", QDir::Files);

        for (const QString& fileName : desktopFiles) {
            QString filePath = dir.absoluteFilePath(fileName);
            QFile file(filePath);

            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();

                if (line.startsWith("Exec=")) {
                    QString execLine = line.mid(QString("Exec=").length());
                    const QStringList execCommandParts = QProcess::splitCommand(execLine);

                    for (const QString& execCommand : execCommandParts)
                    {
                        if (execCommand == path) {
                            return filePath;
                        }
                    }
                    break;
                }
            }
        }
    }
    return QString();
}

AppImageUtilMetadata AppImageUtil::metadata(MetadataAction action)
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(("Failed to open file: " + m_path).toStdString());
    }

    bool isValid = isAppImageType2(m_path);
    // Currently only supporting appimage type 2
    if(!isValid)
    {
        throw std::runtime_error("Invalid/unsupported appimage type");
    }

    AppImageUtilMetadata metadata;
    metadata.path = m_path;
    metadata.type = 2;
    metadata.executable = isExecutable(m_path);

    // If we the action is Register, we don't want to look at the integrated desktop file.
    QString desktopPath = action != MetadataAction::Register ? integratedDesktopPath(m_path) : QString();
    if(!desktopPath.isEmpty())
    {
        metadata.desktopFilePath = desktopPath;
        parseDesktopPathForMetadata(desktopPath, metadata);
    }
    else if((metadata.executable && action == MetadataAction::Default) || action == MetadataAction::Register)
    {
        if(!isMounted())
            mountAppImage();

        QString mountedDesktopPath = getMountedDesktopPath();
        if (!mountedDesktopPath.isEmpty())
        {
            parseDesktopPathForMetadata(mountedDesktopPath, metadata, action == MetadataAction::Register);
            metadata.iconPath = getMountedIconPath();
        }
    }
    else
    {
        // The appimage is not executable, so we give it the name of the file
        // and calculate the checksum.
        QFileInfo fileInfo(m_path);
        metadata.name = fileInfo.completeBaseName();
        metadata.checksum = getChecksum(m_path);
    }

    if(metadata.executable && metadata.version.isEmpty())
    {
        auto md5 = getChecksum(m_path, QCryptographicHash::Md5);
        metadata.version = md5.left(6);
    }

    return metadata;
}

QString AppImageUtil::getMountedDesktopPath()
{
    if (m_mountPath.isEmpty())
        return {};

    QDir mountDir(m_mountPath);

    // Wait up to 1 second for the .desktop file
    for (int i = 0; i < 20; ++i) {
        QFileInfoList desktopFiles = mountDir.entryInfoList({"*.desktop"}, QDir::Files);
        if (!desktopFiles.isEmpty())
            return desktopFiles.first().absoluteFilePath();

        QThread::msleep(50);
    }

    return {};
}

QString AppImageUtil::getMountedIconPath()
{
    if (m_mountPath.isEmpty())
        return QString();

    QString mountedDesktopPath = getMountedDesktopPath();
    if (!mountedDesktopPath.isEmpty())
    {
        AppImageUtilMetadata metadata;
        parseDesktopPathForMetadata(mountedDesktopPath, metadata);

        // 1. Look for common image formats
        QStringList filters = { "*.png", "*.svg", "*.xpm", "*.ico" };
        QStringList iconDirs = {
            m_mountPath + "/usr/share/icons/hicolor/scalable",
            m_mountPath + "/usr/share/icons/hicolor/512x512",
            m_mountPath + "/usr/share/icons/hicolor/256x256",
            m_mountPath + "/usr/share/icons/hicolor/192x192",
            m_mountPath + "/usr/share/icons/hicolor/128x128",
            m_mountPath + "/usr/share/icons/hicolor/96x96",
            m_mountPath + "/usr/share/icons/hicolor/64x64",
            m_mountPath + "/usr/share/pixmaps"
        };

        for (const QString& iconDirPath : iconDirs) {
            if (!QDir(iconDirPath).exists())
                continue;

            QDirIterator dirIterator(iconDirPath, filters, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);

            while (dirIterator.hasNext()) {
                QFileInfo fileInfo(dirIterator.next());
                if (fileInfo.completeBaseName() == QFileInfo(metadata.iconPath).completeBaseName()) {
                    return fileInfo.absoluteFilePath();
                }
            }
        }

        // 2: Absolute or relative path with extension
        QDir mountDir(m_mountPath);
        QStringList imageExtensions = { "png", "svg", "xpm", "ico" };
        for (const QString& ext : imageExtensions) {
            QString filename = metadata.iconPath + "." + ext;
            QFileInfo iconInfo(mountDir.filePath(filename));
            if (iconInfo.exists() && iconInfo.isFile()) {
                return iconInfo.absoluteFilePath();
            }
        }
    }

    // 3. Check for .DirIcon
    QString dirIconPath = m_mountPath + "/.DirIcon";
    QFileInfo dirIcon(dirIconPath);
    if (dirIcon.exists()) {
        if (dirIcon.isSymLink()) {
            QString target = dirIcon.symLinkTarget();
            if (QFile::exists(target))
                return target;
        } else {
            return dirIcon.absoluteFilePath();
        }
    }

    return QString();
}

QString AppImageUtil::registerAppImage()
{
    // Move/Copy appimage
    AppImageUtilMetadata utilMetadata = metadata(MetadataAction::Register);
    QString newAppImagePath = handleIntegrationFileOperation(utilMetadata.name);
    if(newAppImagePath.isEmpty())
    {
        ErrorManager::instance()->reportError("Failed to move/copy appimage.");
        return QString();
    }

    QString baseAppImageName = QFileInfo(newAppImagePath).completeBaseName();
    QString newAppImageFolder = QFileInfo(newAppImagePath).absolutePath();

    // If appimage path contains spaces, quote it
    QString cleanNewAppImagePath = newAppImagePath;
    if (cleanNewAppImagePath.contains('"'))
        cleanNewAppImagePath.replace('"', "");

    if(cleanNewAppImagePath.contains(' '))
        cleanNewAppImagePath = "\"" + cleanNewAppImagePath + "\"";

    // Copy icon
    QString mountedIconPath = getMountedIconPath();
    QFileInfo mountedIconInfo(mountedIconPath);
    QString newIconFileName = baseAppImageName;
    if (!mountedIconInfo.suffix().isEmpty()) {
        newIconFileName += "." + mountedIconInfo.suffix();
    }
    QString newIconPath = QDir(newAppImageFolder).filePath(".icons/" + newIconFileName);
    QDir().mkpath(QFileInfo(newIconPath).absolutePath());
    bool imageSuccess = QFile::copy(mountedIconPath, newIconPath);
    if(!imageSuccess)
    {
        ErrorManager::instance()->reportError("Failed to create icon");
    }
    QString cleanNewIconPath = newIconPath;
    if (cleanNewIconPath.contains('"'))
        cleanNewIconPath.replace('"', "");

    // Add integration meta
    QString newDesktopContent = utilMetadata.mountedDesktopContents;
    if (!newDesktopContent.endsWith('\n'))
        newDesktopContent += '\n';
    newDesktopContent += balIntegrationField + '\n';

    // Replace Exec/TryExec/Icon
    QStringList lines = newDesktopContent.split('\n');
    for (QString& line : lines) {
        if(line.startsWith("Exec="))
        {
            line = parseExecLine(line, newAppImagePath);
        }
        else if(line.startsWith("TryExec="))
        {
            line = QString("TryExec=%1").arg(cleanNewAppImagePath);
        }
        else if(line.startsWith("Icon="))
        {
            line = QString("Icon=%1").arg(cleanNewIconPath);
        }
    }
    newDesktopContent = lines.join('\n');

    QString newDesktopFileName = baseAppImageName + ".desktop";
    QString newDesktopPath = QDir(getLocalIntegrationPath()).filePath(newDesktopFileName);

    // Ensure directory exists
    QDir().mkpath(QFileInfo(newDesktopPath).absolutePath());

    QFile file(newDesktopPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&file);
        out << newDesktopContent;
        file.close();
    } else {
        ErrorManager::instance()->reportError(QString("Failed to create deskop entry: %1").arg(file.errorString()));
        return QString();
    }

    return newAppImagePath;
}

bool AppImageUtil::unregisterAppImage(bool deleteAppImage)
{
    if (!QFile::exists(m_path)) {
        ErrorManager::instance()->reportError("Source file does not exist: " + m_path);
        return false;
    }

    AppImageUtilMetadata utilMetadata = metadata(MetadataAction::Unregister);

    if (!removeFileOrWarn(utilMetadata.iconPath, "icon file")) return false;
    if (!removeFileOrWarn(utilMetadata.desktopFilePath, "desktop file")) return false;
    if (deleteAppImage && !removeFileOrWarn(utilMetadata.path, "AppImage file")) return false;

    return true;
}

const QList<AppImageUtilMetadata> AppImageUtil::getRegisteredList()
{
    QList<AppImageUtilMetadata> list;
    QDir dir(SettingsManager::instance()->appImageDefaultLocation().toLocalFile());
    const QFileInfoList files = dir.entryInfoList(
        QStringList() << "*.AppImage" << "*.appimage",
        QDir::Files | QDir::NoSymLinks
        );

    for (const QFileInfo &fileInfo : files)
    {
        QString path = fileInfo.absoluteFilePath();
        QString desktopPath = integratedDesktopPath(path);

        if(!desktopPath.isEmpty())
        {
            AppImageUtilMetadata utilMetadata;
            utilMetadata.path = path;
            utilMetadata.type = isAppImageType2(path) ? 2 : 1;
            utilMetadata.desktopFilePath = desktopPath;
            parseDesktopPathForMetadata(desktopPath, utilMetadata);

            if(utilMetadata.version.isEmpty())
            {
                auto md5 = getChecksum(path, QCryptographicHash::Md5);
                utilMetadata.version = md5.left(6);
            }

            list.append(utilMetadata);
        }
    }

    return list;
}

const bool AppImageUtil::saveUpdaterSettings(const QString& desktopFilePath,
                                             const QString& updaterType,
                                             const UpdaterSettings& settings)
{
    if (!QFile::exists(desktopFilePath)) {
        ErrorManager::instance()->reportError("Desktop file does not exist: " + desktopFilePath);
        return false;
    }

    QFile file(desktopFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ErrorManager::instance()->reportError("Cannot read desktop file: " + desktopFilePath);
        return false;
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();

    // Remove any existing AppImage updater keys
    QStringList newLines;
    bool inDesktopEntry = false;

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
            inDesktopEntry = (trimmed == "[Desktop Entry]");
            newLines.append(line);
            continue;
        }

        if (inDesktopEntry) {
            if (trimmed.startsWith("X-AppImage-BAL-Update") ||
                (!updaterType.isEmpty() && trimmed.startsWith("X-AppImage-BAL="))) {
                continue;
            }
        }

        newLines.append(line);
    }

    // Only add updater fields if updaterType is set
    if (!updaterType.isEmpty()) {
        QStringList updaterLines;
        updaterLines.append("X-AppImage-BAL=true");
        updaterLines.append("X-AppImage-BAL-UpdateType=" + updaterType);

        auto addIfNotEmpty = [&](const QString& key, const QString& value) {
            if (!value.isEmpty())
                updaterLines.append(key + "=" + escapeDesktopValue(value));
        };

        addIfNotEmpty("X-AppImage-BAL-UpdateUrl", settings.url);
        addIfNotEmpty("X-AppImage-BAL-UpdateDownloadField", settings.downloadField);
        addIfNotEmpty("X-AppImage-BAL-UpdateDownloadPattern", settings.downloadPattern);
        addIfNotEmpty("X-AppImage-BAL-UpdateDateField", settings.dateField);
        addIfNotEmpty("X-AppImage-BAL-UpdateVersionField", settings.versionField);
        addIfNotEmpty("X-AppImage-BAL-UpdateVersionPattern", settings.versionPattern);

        if (!settings.filters.isEmpty()) {
            QJsonArray filterArray;
            for (const auto& filter : settings.filters) {
                QJsonObject obj;
                obj["field"] = filter.field;
                obj["pattern"] = filter.pattern;
                filterArray.append(obj);
            }
            QJsonDocument doc(filterArray);
            QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
            updaterLines.append("X-AppImage-BAL-UpdateFilters=\"" + escapeDesktopValue(jsonStr) + "\"");
        }

        // Insert updateLines at the end of the [Desktop Entry] section
        int insertPos = -1;
        bool inDesktopEntry = false;

        for (int i = 0; i < newLines.size(); ++i) {
            QString trimmed = newLines[i].trimmed();
            if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                if (inDesktopEntry) {
                    // We reached next section, stop
                    insertPos = i;
                    break;
                }
                inDesktopEntry = (trimmed == "[Desktop Entry]");
            }
        }

        // If still -1, either there is only [Desktop Entry] section or it's the last section
        if (insertPos == -1) {
            insertPos = newLines.size();
        }

        // Skip trailing empty lines
        while (insertPos > 0 && newLines[insertPos - 1].trimmed().isEmpty()) {
            --insertPos;
        }


        for (const QString &line : updaterLines) {
            newLines.insert(insertPos++, line);
        }

    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        ErrorManager::instance()->reportError("Cannot write desktop file: " + desktopFilePath);
        return false;
    }

    QTextStream out(&file);
    out << newLines.join("\n") << "\n";
    file.close();

    return true;
}

const bool AppImageUtil::refreshDesktopFile(const QString& appImagePath, const QString& updateVersion, const QString& updateDate)
{
    // Integrated Desktop Contents
    QString desktopPath = integratedDesktopPath(appImagePath);
    if(desktopPath.isEmpty()) {
        ErrorManager::instance()->reportError("Failed to find integrated desktop file for: " + appImagePath);
        return false;
    }
    AppImageUtilMetadata intMetadata;
    parseDesktopPathForMetadata(desktopPath, intMetadata, true);

    // AppImage Desktop Contents
    AppImageUtil util(appImagePath);
    util.mountAppImage();
    AppImageUtilMetadata metadata;
    QString mountedDesktopPath = util.getMountedDesktopPath();
    QString mountedIconPath = util.getMountedIconPath();
    if(mountedDesktopPath.isEmpty()) {
        ErrorManager::instance()->reportError("Failed to find mounted desktop file for: " + appImagePath);
        return false;
    }
    parseDesktopPathForMetadata(mountedDesktopPath, metadata, true);

    // Copy new icon
    if(!mountedIconPath.isEmpty())
    {
        QSettings desktopFile(desktopPath, QSettings::IniFormat);
        desktopFile.beginGroup("Desktop Entry");
        QString iconPath = desktopFile.value("Icon").toString();
        desktopFile.endGroup();

        QFile::remove(iconPath);
        QFile::copy(mountedIconPath, iconPath);
    }

    util.unmountAppImage();

    QString desktopContents = intMetadata.mountedDesktopContents;
    QString mountedDesktopContents = metadata.mountedDesktopContents;

    // parse exec line
    QStringList lines = mountedDesktopContents.split('\n');
    for (QString& line : lines) {
        if(line.startsWith("Exec="))
        {
            line = parseExecLine(line, appImagePath);
        }
    }
    mountedDesktopContents = lines.join('\n');

    updateDesktopKey(desktopContents, mountedDesktopContents, "Exec");
    updateDesktopKey(desktopContents, mountedDesktopContents, "Name");
    updateDesktopKey(desktopContents, mountedDesktopContents, "Comment");
    updateDesktopKey(desktopContents, mountedDesktopContents, "Categories");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateType");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateUrl");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateDownloadField");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateDownloadPattern");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateDateField");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateVersionField");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateVersionPattern");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateFilters");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateCurrentVersion");
    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-BAL-UpdateCurrentDate");

    QString fallbackVersion = updateVersion;
    if(fallbackVersion.isEmpty())
    {
        auto md5 = getChecksum(appImagePath, QCryptographicHash::Md5);
        fallbackVersion = md5.left(6);
    }

    updateDesktopKey(desktopContents, mountedDesktopContents, "X-AppImage-Version", fallbackVersion);

    if (!updateVersion.isEmpty())
        updateDesktopKey(desktopContents, QString("X-AppImage-BAL-UpdateCurrentVersion=" + updateVersion), "X-AppImage-BAL-UpdateCurrentVersion");
    if (!updateDate.isEmpty())
        updateDesktopKey(desktopContents, QString("X-AppImage-BAL-UpdateCurrentDate=" + updateDate), "X-AppImage-BAL-UpdateCurrentDate");

    QFile outFile(desktopPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        ErrorManager::instance()->reportError("Failed to write updated desktop file: " + desktopPath);
        return false;
    }

    outFile.write(desktopContents.toUtf8());
    outFile.close();

    return true;
}

void AppImageUtil::updateAppImage(const QString& appImagePath, const QString& downloadUrl,
                                        const QString& version, const QString& date,
                                        std::function<void(bool)> finishedCallback,
                                        std::function<void(UpdateState, qint64, qint64)> progressCallback)
{
    auto invokeFinished = [finishedCallback](bool success) {
        if (!finishedCallback)
            return;

        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [finishedCallback, success]() {
                finishedCallback(success);
            }, Qt::QueuedConnection);
    };

    if (appImagePath.isEmpty() || downloadUrl.isEmpty()) {
        invokeFinished(false);
        return;
    }

    auto invokeProgress = [progressCallback](UpdateState state, qint64 received = -1, qint64 total = -1) {
        if (!progressCallback)
            return;

        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [progressCallback, state, received, total]() {
                progressCallback(state, received, total);
            }, Qt::QueuedConnection);
    };

    QNetworkReply* reply = NetworkUtil::networkManager()->get(QNetworkRequest(QUrl(downloadUrl)));

    if (progressCallback) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, [invokeProgress](qint64 received, qint64 total) {
            invokeProgress(UpdateState::Downloading, received, total);
        });
    }

    QObject::connect(reply, &QNetworkReply::finished, [=]() {
        if (reply->error() != QNetworkReply::NoError) {
            ErrorManager::instance()->reportError(
                "Download failed: " + reply->errorString());
            reply->deleteLater();
            invokeProgress(UpdateState::Failed);
            invokeFinished(false);
            return;
        }

        QByteArray data = reply->readAll();
        reply->deleteLater();

        invokeProgress(UpdateState::Extracting);

        // Move heavy work off the UI thread
        QThreadPool::globalInstance()->start([data = std::move(data), appImagePath, version, date, invokeProgress, invokeFinished]() {
            bool success = false;
            QString newPath = appImagePath + ".new";

            if (ArchiveUtil::isZip(data)) {
                success = ArchiveUtil::extractAppImageFromZip(data, newPath);
            } else {
                QFile newFile(newPath);
                if (newFile.open(QIODevice::WriteOnly)) {
                    newFile.write(data);
                    newFile.close();
                    success = true;
                }
            }

            if (success && QFile::exists(newPath)) {
                invokeProgress(UpdateState::Installing);
                makeExecutable(newPath);

                if (SettingsManager::instance()->keepBackup()) {
                    QFile::remove(appImagePath + ".bak");
                    QFile::rename(appImagePath, appImagePath + ".bak");
                } else {
                    QFile::remove(appImagePath);
                }

                success = QFile::rename(newPath, appImagePath);
            }

            if (!refreshDesktopFile(appImagePath, version, date)) {
                success = false;
            }

            invokeProgress(success ? UpdateState::Success : UpdateState::Failed);
            invokeFinished(success);
        });
    });
}

// ----------------- Private -----------------

const QRegularExpression AppImageUtil::execLineRegex(R"(^Exec=(?:env\s+((?:\S+=\S+\s?)*))?(".*?"|\S+)(?:\s+([^\n\r]*))?$)");
const QRegularExpression AppImageUtil::invalidChars(R"([/\\:*?"<>|])");
const QString AppImageUtil::balIntegrationField = "X-AppImage-BAL=true";

const QString AppImageUtil::escapeDesktopValue(const QString &value)
{
    QString v = value;
    v.replace("\\", "\\\\");   // backslash first
    v.replace("\n", "\\n");    // newlines
    v.replace("\t", "\\t");    // tabs
    v.replace("\"", "\\\"");   // quotes
    return v;
}

void AppImageUtil::parseDesktopPathForMetadata(const QString& path, AppImageUtilMetadata& metadata, bool storeDesktopContent)
{
    QSettings desktopFile(path, QSettings::IniFormat);
    desktopFile.beginGroup("Desktop Entry");

    // Base fields
    metadata.name = desktopFile.value("Name").toString();
    metadata.version = desktopFile.value("X-AppImage-Version").toString();
    metadata.comment = desktopFile.value("Comment").toString();
    metadata.categories = desktopFile.value("Categories").toString();
    metadata.iconPath = desktopFile.value("Icon").toString();
    metadata.internalIntegration = desktopFile.value("X-AppImage-BAL").toBool();

    // Update fields
    metadata.updateType = desktopFile.value("X-AppImage-BAL-UpdateType").toString();
    metadata.updateUrl = desktopFile.value("X-AppImage-BAL-UpdateUrl").toString();
    metadata.updateDownloadField = desktopFile.value("X-AppImage-BAL-UpdateDownloadField").toString();
    metadata.updateDownloadPattern = desktopFile.value("X-AppImage-BAL-UpdateDownloadPattern").toString();
    metadata.updateDateField = desktopFile.value("X-AppImage-BAL-UpdateDateField").toString();
    metadata.updateVersionField = desktopFile.value("X-AppImage-BAL-UpdateVersionField").toString();
    metadata.updateVersionPattern = desktopFile.value("X-AppImage-BAL-UpdateVersionPattern").toString();
    metadata.updateFilters = parseFilters(desktopFile.value("X-AppImage-BAL-UpdateFilters").toString());
    metadata.updateCurrentVersion = desktopFile.value("X-AppImage-BAL-UpdateCurrentVersion").toString();
    metadata.updateCurrentDate = desktopFile.value("X-AppImage-BAL-UpdateCurrentDate").toString();

    desktopFile.endGroup();

    if (storeDesktopContent)
    {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file);
            metadata.mountedDesktopContents = in.readAll();
        }
        else
        {
            metadata.mountedDesktopContents = QString();
        }
    }
}

const QList<UpdaterFilter> AppImageUtil::parseFilters(const QString &filterStr)
{
    QList<UpdaterFilter> filters;

    if (filterStr.isEmpty())
        return filters;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(filterStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        ErrorManager::instance()->reportError("Failed to parse UpdateFilters JSON:" + err.errorString());
        return filters;
    }

    for (const QJsonValue &val : doc.array()) {
        if (!val.isObject())
            continue;

        QJsonObject obj = val.toObject();
        UpdaterFilter f;
        f.field   = obj.value("field").toString();
        f.pattern = obj.value("pattern").toString();
        filters.append(f);
    }

    return filters;
}

QString AppImageUtil::findNextAvailableFilename(const QString& fullPath) {
    QFileInfo fileInfo(fullPath);
    QDir dir = fileInfo.dir();
    QString baseName = fileInfo.completeBaseName();
    QString extension = fileInfo.suffix();

    QString fileName = fileInfo.fileName();
    int counter = 1;

    while (dir.exists(fileName)) {
        if (extension.isEmpty()) {
            fileName = QString("%1(%2)").arg(baseName).arg(counter);
        } else {
            fileName = QString("%1(%2).%3").arg(baseName).arg(counter).arg(extension);
        }
        counter++;
    }

    return dir.filePath(fileName);
}

QString AppImageUtil::handleIntegrationFileOperation(QString appName)
{
    if (!QFile::exists(m_path)) {
        qWarning() << "Source file does not exist:" << m_path;
        return QString();
    }

    // Replace spaces and invalid characters
    appName.replace(' ', "-");
    appName.replace(invalidChars, "_");

    if(appName.isEmpty())
    {
        QFileInfo info(m_path);
        appName = info.baseName();
    }
    QString fileName = QString("%1.appimage").arg((appName).trimmed().toLower());
    QDir dir(SettingsManager::instance()->appImageDefaultLocation().toLocalFile());
    QString newPath = dir.filePath(fileName);

    if(m_path == newPath)
        return m_path;

    newPath = findNextAvailableFilename(newPath);

    // Ensure directory exists
    QDir().mkpath(QFileInfo(newPath).absolutePath());

    bool success = false;
    switch(SettingsManager::instance()->appImageFileOperation()) {
    case SettingsManager::Move:
        success = QFile::rename(m_path, newPath);
        break;
    case SettingsManager::Copy:
    default:
        success = QFile::copy(m_path, newPath);
        break;
    }

    return success ? newPath : QString();
}

const QStringList AppImageUtil::getSearchPaths()
{
    return {
        "/usr/share/applications",
        "/usr/local/share/applications",
        QDir::homePath() + "/.local/share/applications"
    };
}

const QString AppImageUtil::getLocalIntegrationPath()
{
    return QDir::homePath() + "/.local/share/applications";
}

const bool AppImageUtil::removeFileOrWarn(const QString& path, const QString& label)
{
    if (QFile::exists(path)) {
        if (!QFile::remove(path)) {
            ErrorManager::instance()->reportError("Failed to remove " + label + ": " + path);
            return false;
        }
    } else {
        qWarning() << label << "does not exist:" << path;
    }
    return true;
}

void AppImageUtil::updateDesktopKey(QString& targetContents,
                                          const QString& sourceContents,
                                          const QString& key,
                                          const QString& fallback)
{
    QRegularExpression rx("^" + QRegularExpression::escape(key) + R"(=.+$)",
                          QRegularExpression::MultilineOption);

    QRegularExpressionMatch match = rx.match(sourceContents);
    QString lineToUse;

    if (match.hasMatch()) {
        lineToUse = match.captured(0);
    } else if (!fallback.isEmpty()) {
        lineToUse = key + "=" + fallback;
    } else {
        // nothing to update
        return;
    }

    if (rx.match(targetContents).hasMatch()) {
        // Replace existing line in target
        targetContents.replace(rx, lineToUse);
    } else {
        // Insert into [Desktop Entry] section or append
        QRegularExpression entryRx(R"(^\[Desktop Entry\])",
                                   QRegularExpression::MultilineOption);
        QRegularExpressionMatch entryMatch = entryRx.match(targetContents);
        if (entryMatch.hasMatch()) {
            int sectionStart = entryMatch.capturedEnd();

            // Find the start of the next section
            QRegularExpression nextSectionRx(R"(^\s*\[[^\]]+\].*$)",
                                             QRegularExpression::MultilineOption);
            QRegularExpressionMatch nextMatch = nextSectionRx.match(targetContents, sectionStart);

            int insertPos = nextMatch.hasMatch()
                                ? nextMatch.capturedStart()
                                : targetContents.length();

            // Backtrack to skip trailing blank lines before inserting
            while (insertPos > sectionStart &&
                   (targetContents[insertPos - 1] == '\n' ||
                    targetContents[insertPos - 1] == '\r'))
            {
                --insertPos;
            }

            // Insert with a newline before
            targetContents.insert(insertPos, "\n" + lineToUse);
        } else {
            targetContents.append("\n" + lineToUse);
        }
    }
}

const QString AppImageUtil::parseExecLine(const QString& line, const QString& appImagePath)
{
    if(line.startsWith("Exec="))
    {
        QString cleanNewAppImagePath = appImagePath;
        if (cleanNewAppImagePath.contains('"'))
            cleanNewAppImagePath.replace('"', "");

        if(cleanNewAppImagePath.contains(' '))
            cleanNewAppImagePath = "\"" + cleanNewAppImagePath + "\"";

        QRegularExpressionMatch match = execLineRegex.match(line);
        if (match.hasMatch()) {
            QString envPart = match.captured(1);  // May be empty
            QString args = match.captured(3); // May be empty

            // trim parts
            envPart = envPart.trimmed();
            args = args.trimmed();

            QString newExecLine = "Exec=";
            if (!envPart.isEmpty())
                newExecLine += "env " + envPart + " ";
            newExecLine += cleanNewAppImagePath;
            if (!args.isEmpty())
                newExecLine += " " + args;
            return newExecLine;
        }
    }

    return line;
}
