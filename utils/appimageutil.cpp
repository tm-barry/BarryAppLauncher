#include "appimageutil.h"
#include "managers/errormanager.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QThread>

// ----------------- Public -----------------

AppImageUtil::AppImageUtil(const QString& path)
    : m_path(path), m_mountPath(QString()), m_process(nullptr) {}

AppImageUtil::~AppImageUtil()
{
    unmountAppImage();
}

bool AppImageUtil::isAppImageType2() {
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        ErrorManager::instance()->reportError("Failed to open file: " + m_path);
        return false;
    }

    file.seek(8);
    QByteArray magic = file.read(3);

    return magic == QByteArray("AI\2");
}

QString AppImageUtil::getMd5() {
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) {
        ErrorManager::instance()->reportError("Failed to open file:" + m_path);
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);

    if (!hash.addData(&file)) {
        ErrorManager::instance()->reportError("Failed to read file for hashing:" + m_path);
        return QString();
    }

    return hash.result().toHex();
}

bool AppImageUtil::isExecutable() {
    QFileInfo fileInfo(m_path);
    return fileInfo.isExecutable();
}

bool AppImageUtil::makeExecutable()
{
    QFile file(m_path);
    if (!file.exists()) {
        ErrorManager::instance()->reportError("File does not exist: " + m_path);
        return false;
    }

    QFile::Permissions perms = file.permissions();
    perms |= QFileDevice::ExeUser | QFileDevice::ExeGroup | QFileDevice::ExeOther;

    if (!file.setPermissions(perms)) {
        ErrorManager::instance()->reportError("Failed to set executable permissions for: " + m_path);
        return false;
    }

    return true;
}

void AppImageUtil::mountAppImage() {
    unmountAppImage();

    if(!isExecutable())
        makeExecutable();

    QProcess* process = new QProcess();
    m_process = process;
    process->setProgram(m_path);
    process->setArguments(QStringList() << "--appimage-mount");
    process->setProcessChannelMode(QProcess::MergedChannels);

    process->start();
    if (!process->waitForStarted(3000)) {
        ErrorManager::instance()->reportError("Failed to start AppImage mount process.");
        unmountAppImage();
        return;
    }

    if (!process->waitForReadyRead(1000)) {
        ErrorManager::instance()->reportError("No output from AppImage mount.");
        unmountAppImage();
        return;
    }

    // Wait up to 3 second for the mount to become available
    m_mountPath = QString::fromUtf8(process->readAll()).trimmed();
    bool pathExists = false;
    for (int i = 0; i < 30; ++i) {
        if (QDir(m_mountPath).exists()) {
            pathExists = true;
            break;
        }
        QThread::msleep(100);
    }

    if (!pathExists) {
        ErrorManager::instance()->reportError("Invalid mount path:" + m_mountPath);
        unmountAppImage();
        return;
    }
}

bool AppImageUtil::isMounted()
{
    return m_process && m_process->state() != QProcess::NotRunning && !m_mountPath.isEmpty();
}

void AppImageUtil::unmountAppImage()
{
    if (!m_mountPath.isEmpty()) {
        QProcess umountProcess;
        QStringList args;

        args << "-u" << m_mountPath;  // fusermount -u /mount/path
        umountProcess.start("fusermount", args);
        if (!umountProcess.waitForFinished(3000)) {
            // fallback to "umount" if fusermount fails or is unavailable
            umountProcess.start("umount", QStringList() << m_mountPath);
            umountProcess.waitForFinished(3000);
        }

        // Remove the directory if it still exists
        QDir mountDir(m_mountPath);
        if (mountDir.exists()) {
            mountDir.removeRecursively();
        }

        m_mountPath.clear();
    }

    if (!m_process)
        return;

    m_process->terminate();
    if (!m_process->waitForFinished(2000)) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }

    delete m_process;
    m_process = nullptr;
}

QString AppImageUtil::integratedDesktopPath()
{
    const QStringList searchPaths = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        QDir::homePath() + "/.local/share/applications"
    };

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
                    const QStringList execCommandParts = execLine.split(' ');

                    for (const QString& execCommand : execCommandParts)
                    {
                        if (execCommand == m_path) {
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

AppImageUtilMetadata AppImageUtil::metadata(bool forceInternal)
{
    AppImageUtilMetadata metadata;
    metadata.path = m_path;
    metadata.type = isAppImageType2() ? 2 : 1;
    metadata.md5 = getMd5();
    QString desktopPath = !forceInternal ? integratedDesktopPath() : QString();

    if(!desktopPath.isEmpty())
    {
        metadata.desktopFilePath = desktopPath;
        parseDesktopPathForMetadata(desktopPath, metadata);
    }
    else
    {
        if(!isMounted())
            mountAppImage();

        QDir mountDir(m_mountPath);
        QFileInfoList desktopFiles = mountDir.entryInfoList(QStringList() << "*.desktop", QDir::Files);

        if (!desktopFiles.isEmpty())
        {
            parseDesktopPathForMetadata(desktopFiles.first().absoluteFilePath(), metadata);
            metadata.iconPath = getMountedIconPath();
        }
    }

    return metadata;
}

// ----------------- Private -----------------

void AppImageUtil::parseDesktopPathForMetadata(const QString& path, AppImageUtilMetadata& metadata)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    QString desktopContent = in.readAll();
    file.close();

    const QStringList lines = desktopContent.split('\n');
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.startsWith("Name=")) {
            metadata.name = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("X-AppImage-Version=")) {
            metadata.version = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("Comment=")) {
            metadata.comment = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("Categories=")) {
            metadata.categories = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("Icon=")) {
            metadata.iconPath = trimmed.section('=', 1).trimmed();
        }
        else if (trimmed.startsWith("X-AppImage-BAL=true")) {
            metadata.internalIntegration = true;
        }
    }
}

QString AppImageUtil::getMountedIconPath()
{
    if (m_mountPath.isEmpty())
        return QString();

    QDir dir(m_mountPath);

    // 1. Check for .DirIcon
    QString dirIconPath = dir.filePath(".DirIcon");
    if (QFile::exists(dirIconPath))
        return dirIconPath;

    QDir mountDir(m_mountPath);
    QFileInfoList desktopFiles = mountDir.entryInfoList(QStringList() << "*.desktop", QDir::Files);
    if (!desktopFiles.isEmpty())
    {
        AppImageUtilMetadata metadata;
        parseDesktopPathForMetadata(desktopFiles.first().absoluteFilePath(), metadata);

        // 2: Absolute or relative path with extension
        QStringList imageExtensions = { "png", "svg", "xpm", "ico" };
        for (const QString& ext : imageExtensions) {
            QString filename = metadata.iconPath + "." + ext;
            QFileInfo iconInfo(mountDir.filePath(filename));
            if (iconInfo.exists() && iconInfo.isFile()) {
                return iconInfo.absoluteFilePath();
            }
        }

        // 3. Look for common image formats
        QStringList filters = { "*.png", "*.svg", "*.xpm", "*.ico" };
        QStringList iconDirs = {
            m_mountPath + "/usr/share/icons/hicolor/scalable",
            m_mountPath + "/usr/share/icons/hicolor/256x256",
            m_mountPath + "/usr/share/icons/hicolor/192x192",
            m_mountPath + "/usr/share/icons/hicolor/128x128",
            m_mountPath + "/usr/share/icons/hicolor/96x96",
            m_mountPath + "/usr/share/icons/hicolor/64x64",
            m_mountPath + "/usr/share/pixmaps"
        };

        for (const QString& iconDirPath : iconDirs) {
            QDir iconDir(iconDirPath);
            if (!iconDir.exists())
                continue;

            QDirIterator dirIterator(iconDirPath, filters, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);

            while (dirIterator.hasNext()) {
                QFileInfo fileInfo(dirIterator.next());
                if (fileInfo.completeBaseName() == metadata.iconPath) {
                    return fileInfo.absoluteFilePath();
                }
            }
        }
    }

    return QString();
}
