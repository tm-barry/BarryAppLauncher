#include "appimageutil.h"
#include "managers/errormanager.h"
#include "managers/settingsmanager.h"
#include "utils/archiveutil.h"
#include "utils/networkutil.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMutex>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSharedPointer>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <QLoggingCategory>

#include <stdexcept>

// Mount diagnostics. Silent by default (only critical messages show); enable with e.g.
//   QT_LOGGING_RULES="barry.appimage.mount.*=true" ./BarryAppLauncher
Q_LOGGING_CATEGORY(lcMount, "barry.appimage.mount", QtCriticalMsg)

// ----------------- Local helpers -----------------

namespace {

QMutex g_desktopScanMutex;

// Strips quotes and wraps in quotes only when the path contains spaces (for Exec= lines).
QString quotePath(QString path)
{
    path.remove('"');
    return path.contains(' ') ? QLatin1Char('"') + path + QLatin1Char('"') : path;
}

bool looksLikeZip(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && file.read(4) == QByteArray("PK\x03\x04", 4);
}

// Environment for child AppImage processes. If this launcher is itself an AppImage (or is
// started from an IDE), variables such as APPIMAGE / APPDIR / LD_LIBRARY_PATH would leak into
// the child and can change how its runtime behaves, so strip them.
QProcessEnvironment cleanChildEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char* key : {"APPIMAGE", "APPDIR", "ARGV0", "OWD", "LD_LIBRARY_PATH", "LD_PRELOAD"})
        env.remove(QString::fromLatin1(key));
    return env;
}

// Reverses AppImageUtil::escapeDesktopValue (and handles the standard desktop-entry escapes).
QString unescapeDesktopValue(QString value)
{
    if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"'))
        value = value.mid(1, value.size() - 2);

    if (!value.contains('\\'))
        return value;

    QString out;
    out.reserve(value.size());
    for (int i = 0; i < value.size(); ++i) {
        const QChar c = value[i];
        if (c != '\\' || i + 1 >= value.size()) {
            out += c;
            continue;
        }
        const QChar next = value[++i];
        switch (next.unicode()) {
        case 'n': out += '\n'; break;
        case 't': out += '\t'; break;
        case 'r': out += '\r'; break;
        case 's': out += ' ';  break;
        default:  out += next; break;   // \\ and \" and anything else
            }
    }
    return out;
}

// Reads the [Desktop Entry] group into a key/value map. Done by hand rather than with
// QSettings, because QSettings treats commas as list separators and silently turns values
// such as regex patterns ("\d{1,3}") or comments into empty strings.
QHash<QString, QString> readDesktopEntry(const QString& path, QString* rawContents = nullptr)
{
    QHash<QString, QString> entries;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (rawContents)
            rawContents->clear();
        return entries;
    }

    const QString text = QString::fromUtf8(file.readAll());
    if (rawContents)
        *rawContents = text;

    bool inEntry = false;
    const QStringList lines = text.split('\n');
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        if (line.startsWith('[') && line.endsWith(']')) {
            inEntry = (line == "[Desktop Entry]");
            continue;
        }
        if (!inEntry)
            continue;

        const int eq = line.indexOf('=');
        if (eq <= 0)
            continue;

        const QString key = line.left(eq).trimmed();
        if (!entries.contains(key))
            entries.insert(key, unescapeDesktopValue(line.mid(eq + 1).trimmed()));
    }
    return entries;
}

// Calls visit(desktopFilePath, execParts) for the first Exec= line of every .desktop file in
// dirs. Stops early when visit returns true.
template <typename Visitor>
void forEachDesktopExec(const QStringList& dirs, Visitor&& visit)
{
    for (const QString& dirPath : dirs) {
        const QFileInfoList files = QDir(dirPath).entryInfoList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QFileInfo& info : files) {
            QFile file(info.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            while (!file.atEnd()) {
                const QByteArray line = file.readLine().trimmed();
                if (!line.startsWith("Exec="))
                    continue;

                const QStringList parts = QProcess::splitCommand(QString::fromUtf8(line.mid(5)));
                if (visit(info.absoluteFilePath(), parts))
                    return;
                break;  // only the first Exec= matters
            }
        }
    }
}

} // namespace

// ----------------- Public -----------------

AppImageUtil::AppImageUtil(const QString& path)
    : m_path(path), m_mountPath(QString()), m_process(nullptr) {}

AppImageUtil::~AppImageUtil()
{
    unmountAppImage();
}

// A real type 2 AppImage is an ELF binary with the "AI\x02" marker at offset 8.
// Checking the ELF magic as well means HTML error pages, zips, etc. are rejected.
const bool AppImageUtil::isAppImageType2(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        ErrorManager::instance()->reportError("Failed to open file: " + path);
        return false;
    }

    const QByteArray header = file.read(11);
    return header.size() == 11
           && header.startsWith(QByteArray("\x7f" "ELF", 4))
           && header.mid(8, 3) == QByteArray("AI\2", 3);
}

const QString AppImageUtil::getChecksum(const QString& path, const QCryptographicHash::Algorithm hashType) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        ErrorManager::instance()->reportError("Failed to open file: " + path);
        return QString();
    }

    QCryptographicHash hash(hashType);
    if (!hash.addData(&file)) {
        ErrorManager::instance()->reportError("Failed to read file for hashing: " + path);
        return QString();
    }

    return hash.result().toHex();
}

const bool AppImageUtil::isExecutable(const QString& path) {
    return QFileInfo(path).isExecutable();
}

const bool AppImageUtil::makeExecutable(const QString& path)
{
    if (!QFile::exists(path)) {
        ErrorManager::instance()->reportError("File does not exist: " + path);
        return false;
    }

    const QFile::Permissions perms = QFile::permissions(path)
                                     | QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther;

    if (!QFile::setPermissions(path, perms)) {
        ErrorManager::instance()->reportError("Failed to set executable permissions for: " + path);
        return false;
    }
    return true;
}

// ----------------- Mounting -----------------

void AppImageUtil::mountAppImageAsync()
{
    unmountAppImage();
    m_mountOutput.clear();

    if (!isExecutable(m_path)) {
        ErrorManager::instance()->reportError("File is not executable: " + m_path);
        emit mountFinished(false);
        return;
    }

    m_process = new QProcess(this);
    m_process->setProgram(m_path);
    m_process->setArguments({"--appimage-mount"});
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    // Behave like a clean terminal run: no inherited stdin pipe, no leaked AppImage environment.
    m_process->setStandardInputFile(QProcess::nullDevice());
    m_process->setProcessEnvironment(cleanChildEnvironment());

    connect(m_process, &QProcess::readyReadStandardOutput, this, &AppImageUtil::onMountStdoutReady);
    connect(m_process, &QProcess::finished, this, &AppImageUtil::onMountFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(lcMount) << "failed to start:" << (m_process ? m_process->errorString() : QString());
            onMountFinished(-1, QProcess::CrashExit);
        }
    });

    qCDebug(lcMount) << "starting" << m_path << "--appimage-mount";
    m_process->start();
}

bool AppImageUtil::mountAppImage(int mountTimeoutMs)
{
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    bool done = false;
    bool result = false;

    const QMetaObject::Connection conn = connect(this, &AppImageUtil::mountFinished, &loop,
                                                 [&](bool success) {
                                                     result = success;
                                                     done = true;
                                                     loop.quit();
                                                 });

    connect(&timer, &QTimer::timeout, &loop, [&]() {
        // Extraction can legitimately take a long time; only time out the mount attempt.
        if (!m_tempExtractDir.isEmpty())
            return;

        qCWarning(lcMount) << "timed out after" << mountTimeoutMs << "ms. process state:"
                           << (m_process ? int(m_process->state()) : -1)
                           << "mountPath:" << m_mountPath
                           << "output:" << m_mountOutput
                           << "- falling back to extraction";

        // Stop the silent mount attempt, then extract instead. mountFinished ends the loop.
        unmountAppImage();
        extractAppImage();
    });

    mountAppImageAsync();

    // mountFinished may already have fired synchronously (e.g. file not executable).
    if (!done) {
        timer.start(mountTimeoutMs);
        loop.exec();
    }

    disconnect(conn);
    return result;
}

void AppImageUtil::onMountStdoutReady()
{
    if (!m_process || !m_mountPath.isEmpty())
        return;

    const QByteArray data = m_process->readAllStandardOutput();
    m_mountOutput += data;
    qCDebug(lcMount) << "stdout:" << data;

    // Only look at complete lines, so a half-received path is never accepted.
    const int lastNewline = m_mountOutput.lastIndexOf('\n');
    if (lastNewline < 0)
        return;

    // The runtime prints the mount directory; take the last line that is an absolute path.
    const QStringList lines = QString::fromUtf8(m_mountOutput.left(lastNewline))
                                  .split('\n', Qt::SkipEmptyParts);

    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QString candidate = it->trimmed();
        const bool exists = candidate.startsWith('/') && QDir(candidate).exists();
        // The runtime may print the path slightly before the directory is ready, so also
        // accept its own ".mount_" naming and let getMountedDesktopPath() poll for contents.
        const bool looksLikeMount = candidate.startsWith('/')
                                    && QFileInfo(candidate).fileName().startsWith(".mount_");
        qCDebug(lcMount) << "candidate:" << candidate
                         << "exists:" << exists << "looksLikeMount:" << looksLikeMount;

        if (exists || looksLikeMount) {
            m_mountPath = candidate;
            disconnect(m_process, &QProcess::readyReadStandardOutput, this, &AppImageUtil::onMountStdoutReady);
            emit mountFinished(true);
            return;
        }
    }
}

void AppImageUtil::onMountFinished(int exitCode, QProcess::ExitStatus status)
{
    // Already mounted: the process exiting is expected during unmount.
    if (!m_mountPath.isEmpty())
        return;

    qCWarning(lcMount) << "process ended before a mount path was found. exitCode:" << exitCode
                       << "status:" << int(status)
                       << "error:" << (m_process ? m_process->errorString() : QString())
                       << "output:" << m_mountOutput
                       << "- falling back to extraction";

    // Mount failed (commonly: no FUSE). Fall back to extracting.
    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
    }
    extractAppImage();
}

void AppImageUtil::extractAppImage()
{
    const QString tempDir = QDir::tempPath() + "/appimage-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().mkpath(tempDir)) {
        ErrorManager::instance()->reportError("Failed to create temp directory: " + tempDir);
        emit mountFinished(false);
        return;
    }

    m_tempExtractDir = tempDir;

    m_process = new QProcess(this);
    m_process->setProgram(m_path);
    m_process->setArguments({"--appimage-extract"});
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setStandardInputFile(QProcess::nullDevice());
    m_process->setProcessEnvironment(cleanChildEnvironment());
    m_process->setWorkingDirectory(tempDir);

    connect(m_process, &QProcess::finished, this, &AppImageUtil::onExtractFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            onExtractFinished(-1, QProcess::CrashExit);
    });

    qCDebug(lcMount) << "extracting to" << tempDir;
    m_process->start();
}

void AppImageUtil::onExtractFinished(int exitCode, QProcess::ExitStatus status)
{
    const QDir tempDir(m_tempExtractDir);

    if (status != QProcess::NormalExit || exitCode != 0) {
        ErrorManager::instance()->reportError("AppImage does not support mount or extract.");
        unmountAppImage();
        emit mountFinished(false);
        return;
    }

    if (m_tempExtractDir.isEmpty() || !tempDir.exists()) {
        ErrorManager::instance()->reportError("Extraction failed.");
        unmountAppImage();
        emit mountFinished(false);
        return;
    }

    // --appimage-extract creates "squashfs-root" inside the working directory.
    const QStringList subDirs = tempDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    m_mountPath = subDirs.isEmpty() ? m_tempExtractDir : tempDir.filePath(subDirs.first());

    emit mountFinished(true);
}

bool AppImageUtil::isMounted()
{
    if (m_mountPath.isEmpty())
        return false;

    // An extracted tree stays valid after the extract process exits; a FUSE mount does not.
    return !m_tempExtractDir.isEmpty()
           || (m_process && m_process->state() != QProcess::NotRunning);
}

void AppImageUtil::unmountAppImage()
{
    if (m_process) {
        // Disconnect first so terminating the process can't trigger the extract fallback.
        m_process->disconnect(this);

        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            if (!m_process->waitForFinished(2000)) {
                m_process->kill();
                m_process->waitForFinished(1000);
            }
        }
        // deleteLater: this may be called from one of the process's own signal handlers.
        m_process->deleteLater();
        m_process = nullptr;
    }

    if (m_tempExtractDir.isEmpty()) {
        // FUSE mount: terminating the process normally unmounts; only shell out if it didn't.
        if (!m_mountPath.isEmpty() && QDir(m_mountPath).exists()) {
            const QList<QPair<QString, QStringList>> attempts = {
                {"fusermount",  {"-u", m_mountPath}},
                {"fusermount3", {"-u", m_mountPath}},
                {"umount",      {m_mountPath}},
                };
            for (const auto& attempt : attempts) {
                if (QDir(m_mountPath).isEmpty())
                    break;
                QProcess p;
                p.start(attempt.first, attempt.second);
                p.waitForFinished(3000);
            }
            // rmdir only succeeds on an empty directory, so a still-mounted image is never touched.
            QDir().rmdir(m_mountPath);
        }
    } else {
        QDir(m_tempExtractDir).removeRecursively();
    }

    m_mountPath.clear();
    m_tempExtractDir.clear();
    m_mountOutput.clear();
}

// ----------------- Integration lookup -----------------

const QString AppImageUtil::integratedDesktopPath(const QString& path)
{
    QMutexLocker locker(&g_desktopScanMutex);

    QString result;
    forEachDesktopExec(getSearchPaths(), [&](const QString& desktopFile, const QStringList& execParts) {
        if (!execParts.contains(path))
            return false;
        result = desktopFile;
        return true;
    });
    return result;
}

// ----------------- Metadata -----------------

AppImageUtilMetadata AppImageUtil::metadata(MetadataAction action)
{
    // Currently only supporting appimage type 2
    if (!isAppImageType2(m_path))
        throw std::runtime_error("Invalid/unsupported appimage type");

    AppImageUtilMetadata md;
    md.path = m_path;
    md.type = 2;
    md.executable = isExecutable(m_path);

    // When registering we must not look at an already-integrated desktop file.
    const bool registering = (action == MetadataAction::Register);
    const QString integrated = registering ? QString() : integratedDesktopPath(m_path);

    if (!integrated.isEmpty()) {
        md.desktopFilePath = integrated;
        parseDesktopPathForMetadata(integrated, md);
    }
    else if (registering || (md.executable && action == MetadataAction::Default)) {
        if (!isMounted())
            mountAppImage();

        const QString mountedDesktop = getMountedDesktopPath();
        if (!mountedDesktop.isEmpty()) {
            parseDesktopPathForMetadata(mountedDesktop, md, registering);
            md.iconPath = getMountedIconPath();
        }
    }
    else {
        // Not executable, so we can't look inside: use the file name and a checksum.
        md.name = QFileInfo(m_path).completeBaseName();
        md.checksum = getChecksum(m_path);
    }

    if (md.executable && md.version.isEmpty())
        md.version = getChecksum(m_path, QCryptographicHash::Md5).left(6);

    return md;
}

QString AppImageUtil::getMountedDesktopPath()
{
    if (m_mountPath.isEmpty())
        return {};

    // A FUSE mount can take a moment to populate; an extracted tree is ready immediately.
    const int attempts = m_tempExtractDir.isEmpty() ? 100 : 1;   // up to ~5 s for a FUSE mount
    for (int i = 0; i < attempts; ++i) {
        // Fresh QDir each time: QDir caches its listing and would never see new files.
        const QDir mountDir(m_mountPath);
        const QStringList files = mountDir.entryList({"*.desktop"}, QDir::Files);
        if (!files.isEmpty())
            return mountDir.absoluteFilePath(files.first());

        if (i + 1 < attempts)
            QThread::msleep(50);
    }
    return {};
}

QString AppImageUtil::getMountedIconPath()
{
    if (m_mountPath.isEmpty())
        return QString();

    const QString desktopPath = getMountedDesktopPath();
    const QString iconName = desktopPath.isEmpty() ? QString() : readDesktopEntry(desktopPath).value("Icon");

    if (!iconName.isEmpty()) {
        // 1. Look through the standard icon directories, best quality first
        const QStringList filters = { "*.png", "*.svg", "*.xpm", "*.ico" };
        const QStringList iconDirs = {
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

            QDirIterator it(iconDirPath, filters, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QFileInfo info(it.next());
                // completeBaseName() alone breaks reverse-DNS names ("org.foo.App" -> "org.foo"),
                // so also accept an exact file name match.
                if (info.completeBaseName() == iconName || info.fileName() == iconName)
                    return info.absoluteFilePath();
            }
        }

        // 2. Path (relative to the image root) with an image extension appended
        for (const QString& ext : {"png", "svg", "xpm", "ico"}) {
            const QFileInfo info(m_mountPath + "/" + iconName + "." + ext);
            if (info.isFile())
                return info.absoluteFilePath();
        }
    }

    // 3. .DirIcon
    const QFileInfo dirIcon(m_mountPath + "/.DirIcon");
    if (dirIcon.exists()) {
        if (!dirIcon.isSymLink())
            return dirIcon.absoluteFilePath();

        const QString target = dirIcon.symLinkTarget();
        if (QFile::exists(target))
            return target;
    }

    return QString();
}

// ----------------- Register / unregister -----------------

QString AppImageUtil::registerAppImage()
{
    // Read everything we need *before* moving/copying the file, so a failure leaves it untouched.
    const AppImageUtilMetadata md = metadata(MetadataAction::Register);
    if (md.mountedDesktopContents.isEmpty()) {
        ErrorManager::instance()->reportError("Could not read the desktop entry inside the AppImage.");
        return QString();
    }

    const QString newAppImagePath = handleIntegrationFileOperation(md.name);
    if (newAppImagePath.isEmpty()) {
        ErrorManager::instance()->reportError("Failed to move/copy appimage.");
        return QString();
    }

    const QFileInfo newInfo(newAppImagePath);
    const QString baseName = newInfo.completeBaseName();

    // Copy icon (md.iconPath already points at the icon inside the mounted image)
    QString newIconPath;
    if (!md.iconPath.isEmpty()) {
        const QString suffix = QFileInfo(md.iconPath).suffix();
        newIconPath = QDir(newInfo.absolutePath()).filePath(
            ".icons/" + baseName + (suffix.isEmpty() ? QString() : "." + suffix));

        QDir().mkpath(QFileInfo(newIconPath).absolutePath());
        QFile::remove(newIconPath);
        if (!QFile::copy(md.iconPath, newIconPath)) {
            ErrorManager::instance()->reportError("Failed to create icon");
            newIconPath.clear();
        }
    }
    newIconPath.remove('"');

    // Rewrite Exec/TryExec/Icon
    QStringList lines = md.mountedDesktopContents.split('\n');
    for (QString& line : lines) {
        if (line.startsWith("Exec="))
            line = parseExecLine(line, newAppImagePath);
        else if (line.startsWith("TryExec="))
            line = "TryExec=" + newAppImagePath;   // TryExec is a plain path, never quoted
        else if (!newIconPath.isEmpty() && line.startsWith("Icon="))
            line = "Icon=" + newIconPath;
    }
    QString content = lines.join('\n');
    if (!content.endsWith('\n'))
        content += '\n';

    // Integration marker goes inside [Desktop Entry], not at the end of whatever section is last.
    updateDesktopKey(content, balIntegrationField, "X-AppImage-BAL");

    // Persist the version so listing doesn't have to re-hash the whole AppImage every time.
    if (!md.version.isEmpty())
        updateDesktopKey(content, QString(), "X-AppImage-Version", md.version);

    const QString desktopPath = QDir(getLocalIntegrationPath()).filePath(baseName + ".desktop");
    QDir().mkpath(QFileInfo(desktopPath).absolutePath());

    QSaveFile file(desktopPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ErrorManager::instance()->reportError("Failed to create desktop entry: " + file.errorString());
        return QString();
    }
    file.write(content.toUtf8());
    if (!file.commit()) {
        ErrorManager::instance()->reportError("Failed to write desktop entry: " + file.errorString());
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

    const AppImageUtilMetadata md = metadata(MetadataAction::Unregister);

    return removeFileOrWarn(md.iconPath, "icon file")
           && removeFileOrWarn(md.desktopFilePath, "desktop file")
           && (!deleteAppImage || removeFileOrWarn(md.path, "AppImage file"));
}

const QList<AppImageUtilMetadata> AppImageUtil::getRegisteredList()
{
    const QDir dir(SettingsManager::instance()->appImageDefaultLocation().toLocalFile());
    const QFileInfoList files = dir.entryInfoList(
        QStringList() << "*.AppImage" << "*.appimage",
        QDir::Files | QDir::NoSymLinks);

    // Scan the desktop directories once instead of once per AppImage.
    QHash<QString, QString> desktopByExecPath;
    {
        QMutexLocker locker(&g_desktopScanMutex);
        forEachDesktopExec(getSearchPaths(), [&](const QString& desktopFile, const QStringList& execParts) {
            for (const QString& part : execParts) {
                if (part.startsWith('/') && !desktopByExecPath.contains(part))
                    desktopByExecPath.insert(part, desktopFile);
            }
            return false;
        });
    }

    QList<AppImageUtilMetadata> list;
    list.reserve(files.size());

    for (const QFileInfo& fileInfo : files) {
        const QString path = fileInfo.absoluteFilePath();
        const QString desktopPath = desktopByExecPath.value(path);
        if (desktopPath.isEmpty())
            continue;

        AppImageUtilMetadata md;
        md.path = path;
        md.type = isAppImageType2(path) ? 2 : 1;
        md.desktopFilePath = desktopPath;
        parseDesktopPathForMetadata(desktopPath, md);

        if (md.version.isEmpty())
            md.version = getChecksum(path, QCryptographicHash::Md5).left(6);

        list.append(md);
    }

    return list;
}

// ----------------- Desktop file editing -----------------

const bool AppImageUtil::saveUpdaterSettings(const QString& desktopFilePath,
                                             const QString& updaterType,
                                             const UpdaterSettings& settings)
{
    QFile in(desktopFilePath);
    if (!in.exists()) {
        ErrorManager::instance()->reportError("Desktop file does not exist: " + desktopFilePath);
        return false;
    }
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text)) {
        ErrorManager::instance()->reportError("Cannot read desktop file: " + desktopFilePath);
        return false;
    }
    QStringList lines = QString::fromUtf8(in.readAll()).split('\n');
    in.close();

    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    // Single pass: drop old updater keys and remember where [Desktop Entry] ends.
    QStringList out;
    out.reserve(lines.size() + 8);
    bool inEntry = false;
    int entryEnd = -1;

    for (const QString& line : std::as_const(lines)) {
        const QString trimmed = line.trimmed();

        if (trimmed.startsWith('[') && trimmed.endsWith(']')) {
            inEntry = (trimmed == "[Desktop Entry]");
            out.append(line);
            if (inEntry)
                entryEnd = out.size();
            continue;
        }

        if (inEntry && (trimmed.startsWith("X-AppImage-BAL-Update")
                        || (!updaterType.isEmpty() && trimmed.startsWith("X-AppImage-BAL=")))) {
            continue;
        }

        out.append(line);
        if (inEntry && !trimmed.isEmpty())
            entryEnd = out.size();   // insert after the last non-empty line of the entry
    }

    if (!updaterType.isEmpty()) {
        QStringList updaterLines = {
            "X-AppImage-BAL=true",
            "X-AppImage-BAL-UpdateType=" + updaterType
        };

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
            const QString jsonStr = QString::fromUtf8(QJsonDocument(filterArray).toJson(QJsonDocument::Compact));
            updaterLines.append("X-AppImage-BAL-UpdateFilters=\"" + escapeDesktopValue(jsonStr) + "\"");
        }

        int insertPos = (entryEnd >= 0) ? entryEnd : out.size();
        for (const QString& line : std::as_const(updaterLines))
            out.insert(insertPos++, line);
    }

    QSaveFile outFile(desktopFilePath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ErrorManager::instance()->reportError("Cannot write desktop file: " + desktopFilePath);
        return false;
    }
    outFile.write((out.join('\n') + '\n').toUtf8());
    if (!outFile.commit()) {
        ErrorManager::instance()->reportError("Failed to save desktop file: " + outFile.errorString());
        return false;
    }

    return true;
}

const bool AppImageUtil::refreshDesktopFile(const QString& appImagePath, const QString& updateVersion, const QString& updateDate)
{
    // Currently integrated desktop file
    const QString desktopPath = integratedDesktopPath(appImagePath);
    if (desktopPath.isEmpty()) {
        ErrorManager::instance()->reportError("Failed to find integrated desktop file for: " + appImagePath);
        return false;
    }
    AppImageUtilMetadata current;
    parseDesktopPathForMetadata(desktopPath, current, true);

    // Desktop file inside the (new) AppImage
    AppImageUtil util(appImagePath);
    if (!util.mountAppImage()) {
        ErrorManager::instance()->reportError("Failed to mount AppImage: " + appImagePath);
        return false;
    }

    const QString mountedDesktopPath = util.getMountedDesktopPath();
    if (mountedDesktopPath.isEmpty()) {
        ErrorManager::instance()->reportError("Failed to find mounted desktop file for: " + appImagePath);
        return false;
    }

    AppImageUtilMetadata mounted;
    parseDesktopPathForMetadata(mountedDesktopPath, mounted, true);

    // Replace the integrated icon with the new one
    const QString mountedIconPath = util.getMountedIconPath();
    if (!mountedIconPath.isEmpty() && !current.iconPath.isEmpty()) {
        QFile::remove(current.iconPath);
        QFile::copy(mountedIconPath, current.iconPath);
    }

    util.unmountAppImage();

    // Carry the fields the AppImage owns over to the integrated file.
    // (User-configured X-AppImage-BAL-Update* keys are deliberately left alone.)
    QStringList sourceLines = mounted.mountedDesktopContents.split('\n');
    for (QString& line : sourceLines) {
        if (line.startsWith("Exec="))
            line = parseExecLine(line, appImagePath);
    }
    const QString source = sourceLines.join('\n');

    QString contents = current.mountedDesktopContents;
    for (const char* key : {"Exec", "Name", "Comment", "Categories"})
        updateDesktopKey(contents, source, key);

    // Only hash the file when nothing else can provide a version.
    QString fallbackVersion = updateVersion;
    if (fallbackVersion.isEmpty() && mounted.version.isEmpty())
        fallbackVersion = getChecksum(appImagePath, QCryptographicHash::Md5).left(6);
    updateDesktopKey(contents, source, "X-AppImage-Version", fallbackVersion);

    if (!updateVersion.isEmpty())
        updateDesktopKey(contents, "X-AppImage-BAL-UpdateCurrentVersion=" + updateVersion, "X-AppImage-BAL-UpdateCurrentVersion");
    if (!updateDate.isEmpty())
        updateDesktopKey(contents, "X-AppImage-BAL-UpdateCurrentDate=" + updateDate, "X-AppImage-BAL-UpdateCurrentDate");

    QSaveFile outFile(desktopPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        ErrorManager::instance()->reportError("Failed to write updated desktop file: " + desktopPath);
        return false;
    }
    outFile.write(contents.toUtf8());
    if (!outFile.commit()) {
        ErrorManager::instance()->reportError("Failed to save updated desktop file: " + outFile.errorString());
        return false;
    }

    return true;
}

// ----------------- Update -----------------

void AppImageUtil::updateAppImage(const QString& appImagePath, const QString& downloadUrl,
                                  const QString& version, const QString& date,
                                  std::function<void(bool)> finishedCallback,
                                  std::function<void(UpdateState, qint64, qint64)> progressCallback)
{
    // Callbacks always run on the main thread.
    auto postToMain = [](std::function<void()> fn) {
        QMetaObject::invokeMethod(QCoreApplication::instance(), std::move(fn), Qt::QueuedConnection);
    };

    auto finish = [=](bool success) {
        if (finishedCallback)
            postToMain([finishedCallback, success]() { finishedCallback(success); });
    };

    auto progress = [=](UpdateState state, qint64 received = -1, qint64 total = -1) {
        if (progressCallback)
            postToMain([progressCallback, state, received, total]() { progressCallback(state, received, total); });
    };

    auto fail = [=](const QString& message) {
        if (!message.isEmpty())
            ErrorManager::instance()->reportError(message);
        progress(UpdateState::Failed);
        finish(false);
    };

    if (appImagePath.isEmpty() || downloadUrl.isEmpty()) {
        finish(false);
        return;
    }

    // Stream the download to disk instead of buffering the whole AppImage in memory.
    const QString partPath = appImagePath + QStringLiteral(".download");
    auto partFile = QSharedPointer<QFile>::create(partPath);
    if (!partFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail("Cannot create download file: " + partPath);
        return;
    }
    auto writeFailed = QSharedPointer<bool>::create(false);

    QNetworkRequest request{QUrl(downloadUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = NetworkUtil::networkManager()->get(request);

    auto drain = [reply, partFile, writeFailed]() {
        const QByteArray chunk = reply->readAll();
        if (chunk.isEmpty() || *writeFailed)
            return;
        if (partFile->write(chunk) != chunk.size()) {
            *writeFailed = true;
            reply->abort();
        }
    };

    QObject::connect(reply, &QNetworkReply::readyRead, reply, drain);

    if (progressCallback) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, reply, [progress](qint64 received, qint64 total) {
            progress(UpdateState::Downloading, received, total);
        });
    }

    QObject::connect(reply, &QNetworkReply::finished, reply, [=]() {
        drain();
        partFile->close();

        const QNetworkReply::NetworkError netError = reply->error();
        const QString netMessage = reply->errorString();
        reply->deleteLater();

        if (*writeFailed) {
            QFile::remove(partPath);
            fail("Failed to write downloaded data to disk: " + partPath);
            return;
        }
        if (netError != QNetworkReply::NoError) {
            QFile::remove(partPath);
            fail("Download failed: " + netMessage);
            return;
        }

        progress(UpdateState::Extracting);

        // Heavy work (extract, validate, swap, remount) stays off the UI thread.
        QThreadPool::globalInstance()->start([=]() {
            const QString newPath = appImagePath + QStringLiteral(".new");
            QFile::remove(newPath);

            // 1. Produce the candidate file
            bool prepared = false;
            if (looksLikeZip(partPath)) {
                QFile zipFile(partPath);
                const QByteArray data = zipFile.open(QIODevice::ReadOnly) ? zipFile.readAll() : QByteArray();
                zipFile.close();
                prepared = !data.isEmpty() && ArchiveUtil::extractAppImageFromZip(data, newPath);
                QFile::remove(partPath);
            } else {
                prepared = QFile::rename(partPath, newPath);
            }

            if (!prepared || !QFile::exists(newPath)) {
                QFile::remove(partPath);
                QFile::remove(newPath);
                fail("Failed to prepare the downloaded AppImage.");
                return;
            }

            // 2. Validate BEFORE touching the installed AppImage
            if (!AppImageUtil::isAppImageType2(newPath)) {
                QFile::remove(newPath);
                fail("The downloaded file is not a valid AppImage. The current version was kept.");
                return;
            }
            if (!AppImageUtil::makeExecutable(newPath)) {
                QFile::remove(newPath);
                fail(QString());
                return;
            }

            progress(UpdateState::Installing);

            // 3. Swap in the new file, keeping the old one until everything has succeeded
            const bool keepBackup = SettingsManager::instance()->keepBackup();
            const QString oldPath = appImagePath + (keepBackup ? QStringLiteral(".bak") : QStringLiteral(".old"));
            QFile::remove(oldPath);

            const bool hadOld = QFile::exists(appImagePath);
            if (hadOld && !QFile::rename(appImagePath, oldPath)) {
                QFile::remove(newPath);
                fail("Failed to back up the current AppImage.");
                return;
            }

            auto rollback = [&]() {
                QFile::remove(appImagePath);
                if (hadOld)
                    QFile::rename(oldPath, appImagePath);
            };

            if (!QFile::rename(newPath, appImagePath)) {
                rollback();
                QFile::remove(newPath);
                fail("Failed to install the new AppImage.");
                return;
            }

            // 4. Refreshing the desktop entry mounts the new AppImage, which doubles as a
            //    check that it actually runs. If it can't, restore the previous version.
            if (!refreshDesktopFile(appImagePath, version, date)) {
                rollback();
                fail("The updated AppImage could not be opened. The previous version was restored.");
                return;
            }

            if (!keepBackup)
                QFile::remove(oldPath);

            progress(UpdateState::Success);
            finish(true);
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
    v.replace("\n", "\\n");
    v.replace("\t", "\\t");
    v.replace("\"", "\\\"");
    return v;
}

void AppImageUtil::parseDesktopPathForMetadata(const QString& path, AppImageUtilMetadata& metadata, bool storeDesktopContent)
{
    QString rawContents;
    const QHash<QString, QString> entry = readDesktopEntry(path, storeDesktopContent ? &rawContents : nullptr);

    metadata.name = entry.value("Name");
    metadata.version = entry.value("X-AppImage-Version");
    metadata.comment = entry.value("Comment");
    metadata.categories = entry.value("Categories");
    metadata.iconPath = entry.value("Icon");
    metadata.internalIntegration = entry.value("X-AppImage-BAL").compare("true", Qt::CaseInsensitive) == 0;

    metadata.updateType = entry.value("X-AppImage-BAL-UpdateType");
    metadata.updateUrl = entry.value("X-AppImage-BAL-UpdateUrl");
    metadata.updateDownloadField = entry.value("X-AppImage-BAL-UpdateDownloadField");
    metadata.updateDownloadPattern = entry.value("X-AppImage-BAL-UpdateDownloadPattern");
    metadata.updateDateField = entry.value("X-AppImage-BAL-UpdateDateField");
    metadata.updateVersionField = entry.value("X-AppImage-BAL-UpdateVersionField");
    metadata.updateVersionPattern = entry.value("X-AppImage-BAL-UpdateVersionPattern");
    metadata.updateFilters = parseFilters(entry.value("X-AppImage-BAL-UpdateFilters"));
    metadata.updateCurrentVersion = entry.value("X-AppImage-BAL-UpdateCurrentVersion");
    metadata.updateCurrentDate = entry.value("X-AppImage-BAL-UpdateCurrentDate");

    if (storeDesktopContent)
        metadata.mountedDesktopContents = rawContents;
}

const QList<UpdaterFilter> AppImageUtil::parseFilters(const QString &filterStr)
{
    QList<UpdaterFilter> filters;
    if (filterStr.isEmpty())
        return filters;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(filterStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        ErrorManager::instance()->reportError("Failed to parse UpdateFilters JSON: " + err.errorString());
        return filters;
    }

    const QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        if (!val.isObject())
            continue;

        const QJsonObject obj = val.toObject();
        UpdaterFilter f;
        f.field = obj.value("field").toString();
        f.pattern = obj.value("pattern").toString();
        filters.append(f);
    }
    return filters;
}

QString AppImageUtil::findNextAvailableFilename(const QString& fullPath) {
    const QFileInfo fileInfo(fullPath);
    const QDir dir = fileInfo.dir();
    const QString baseName = fileInfo.completeBaseName();
    const QString extension = fileInfo.suffix();

    QString fileName = fileInfo.fileName();
    for (int counter = 1; dir.exists(fileName); ++counter) {
        fileName = extension.isEmpty()
        ? QString("%1(%2)").arg(baseName).arg(counter)
        : QString("%1(%2).%3").arg(baseName).arg(counter).arg(extension);
    }

    return dir.filePath(fileName);
}

QString AppImageUtil::handleIntegrationFileOperation(QString appName)
{
    if (!QFile::exists(m_path)) {
        qWarning() << "Source file does not exist:" << m_path;
        return QString();
    }

    appName.replace(' ', "-");
    appName.replace(invalidChars, "_");
    if (appName.isEmpty())
        appName = QFileInfo(m_path).baseName();

    const QDir dir(SettingsManager::instance()->appImageDefaultLocation().toLocalFile());
    QString newPath = dir.filePath(QString("%1.appimage").arg(appName.trimmed().toLower()));

    if (m_path == newPath)
        return m_path;

    newPath = findNextAvailableFilename(newPath);
    QDir().mkpath(QFileInfo(newPath).absolutePath());

    // QFile::rename falls back to copy+remove across filesystems.
    const bool success = (SettingsManager::instance()->appImageFileOperation() == SettingsManager::Move)
                             ? QFile::rename(m_path, newPath)
                             : QFile::copy(m_path, newPath);

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
    if (!QFile::exists(path)) {
        qWarning() << label << "does not exist:" << path;
        return true;
    }
    if (!QFile::remove(path)) {
        ErrorManager::instance()->reportError("Failed to remove " + label + ": " + path);
        return false;
    }
    return true;
}

// Copies "key=value" from sourceContents into targetContents (replacing or inserting inside
// [Desktop Entry]). If the source has no such key, fallback (when non-empty) is used instead.
void AppImageUtil::updateDesktopKey(QString& targetContents,
                                    const QString& sourceContents,
                                    const QString& key,
                                    const QString& fallback)
{
    const QRegularExpression rx("^" + QRegularExpression::escape(key) + "=.+$",
                                QRegularExpression::MultilineOption);

    QString lineToUse;
    const QRegularExpressionMatch sourceMatch = rx.match(sourceContents);
    if (sourceMatch.hasMatch())
        lineToUse = sourceMatch.captured(0);
    else if (!fallback.isEmpty())
        lineToUse = key + "=" + fallback;
    else
        return;   // nothing to update

    // Replace in place. (Not QString::replace(regex, str): that would interpret "\1" etc.
    // in the value as back-references.)
    const QRegularExpressionMatch targetMatch = rx.match(targetContents);
    if (targetMatch.hasMatch()) {
        targetContents.replace(targetMatch.capturedStart(), targetMatch.capturedLength(), lineToUse);
        return;
    }

    static const QRegularExpression entryRx(R"(^\[Desktop Entry\])", QRegularExpression::MultilineOption);
    static const QRegularExpression nextSectionRx(R"(^\s*\[[^\]]+\].*$)", QRegularExpression::MultilineOption);

    const QRegularExpressionMatch entryMatch = entryRx.match(targetContents);
    if (!entryMatch.hasMatch()) {
        targetContents.append("\n" + lineToUse);
        return;
    }

    const int sectionStart = entryMatch.capturedEnd();
    const QRegularExpressionMatch nextMatch = nextSectionRx.match(targetContents, sectionStart);
    int insertPos = nextMatch.hasMatch() ? nextMatch.capturedStart() : targetContents.length();

    // Skip back over trailing blank lines so the key lands next to the others
    while (insertPos > sectionStart
           && (targetContents[insertPos - 1] == '\n' || targetContents[insertPos - 1] == '\r')) {
        --insertPos;
    }

    targetContents.insert(insertPos, "\n" + lineToUse);
}

const QString AppImageUtil::parseExecLine(const QString& line, const QString& appImagePath)
{
    if (!line.startsWith("Exec="))
        return line;

    const QRegularExpressionMatch match = execLineRegex.match(line);
    if (!match.hasMatch())
        return line;

    const QString envPart = match.captured(1).trimmed();   // may be empty
    const QString args = match.captured(3).trimmed();      // may be empty

    QString result = "Exec=";
    if (!envPart.isEmpty())
        result += "env " + envPart + " ";
    result += quotePath(appImagePath);
    if (!args.isEmpty())
        result += " " + args;
    return result;
}