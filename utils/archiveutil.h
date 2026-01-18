#ifndef ARCHIVEUTIL_H
#define ARCHIVEUTIL_H

#include <QByteArray>

class ArchiveUtil
{
public:
    ArchiveUtil();

    static const bool isZip(const QByteArray &data);
    static const bool extractAppImageFromZip(const QByteArray &zipData, const QString &outputFilePath);
};

#endif // ARCHIVEUTIL_H
