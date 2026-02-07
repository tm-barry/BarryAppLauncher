#include "archiveutil.h"

#include <archive.h>
#include <archive_entry.h>
#include <QFile>
#include <QString>

ArchiveUtil::ArchiveUtil() {}

const bool ArchiveUtil::isZip(const QByteArray &data)
{
    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_memory(a, data.constData(), data.size());
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }

    struct archive_entry *entry;
    r = archive_read_next_header(a, &entry);
    archive_read_free(a);
    return (r == ARCHIVE_OK);
}

const bool ArchiveUtil::extractAppImageFromZip(const QByteArray &zipData, const QString &outputFilePath)
{
    struct archive *a = archive_read_new();
    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);

    int r = archive_read_open_memory(a, zipData.constData(), zipData.size());
    if (r != ARCHIVE_OK) {
        archive_read_free(a);
        return false;
    }

    struct archive_entry *entry;
    bool found = false;

    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char *name = archive_entry_pathname(entry);
        QString fileName = QString::fromUtf8(name);

        if (fileName.endsWith(".AppImage", Qt::CaseInsensitive)) {
            QFile outFile(outputFilePath);
            if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                archive_read_free(a);
                return false;
            }

            const void *buff;
            size_t size;
            la_int64_t offset;

            while ((r = archive_read_data_block(a, &buff, &size, &offset)) == ARCHIVE_OK) {
                outFile.write(reinterpret_cast<const char*>(buff), size);
            }

            outFile.close();

            if (r != ARCHIVE_EOF) {
                archive_read_free(a);
                return false;
            }

            found = true;
            break;
        } else {
            archive_read_data_skip(a);
        }
    }

    archive_read_free(a);
    return found;
}
