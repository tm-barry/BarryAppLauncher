#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include <QDateTime>
#include <QStringList>

namespace StringUtil {

inline QString coalesce(const QString& a, const QString& b) {
    return a.isEmpty() ? b : a;
}

inline QDateTime parseDateTime(const QString &input) {
    static const QList<QString> customFormats = {
        "yyyy-MM-dd HH:mm:ss",
        "yyyy/MM/dd HH:mm:ss",
        "dd/MM/yyyy HH:mm:ss",
        "MM/dd/yyyy HH:mm:ss",
        "yyyy-MM-dd",
        "dd/MM/yyyy",
        "MM/dd/yyyy"
    };

    QDateTime dt = QDateTime::fromString(input, Qt::ISODate);
    if (dt.isValid())
        return dt;

    dt = QDateTime::fromString(input, Qt::ISODateWithMs);
    if (dt.isValid())
        return dt;

    dt = QDateTime::fromString(input, Qt::RFC2822Date);
    if (dt.isValid())
        return dt;

    for (const QString &fmt : customFormats) {
        dt = QDateTime::fromString(input, fmt);
        if (dt.isValid())
            return dt;
    }

    return QDateTime();
}

inline QString formatDateTime(const QString &rawDate)
{
    QDateTime dt = parseDateTime(rawDate);
    if (!dt.isValid()) {
        return rawDate;
    }
    return dt.toString("yyyy-MM-dd HH:mm");
}

inline QString formatBytes(qint64 bytes) {
    if (bytes < 0) return "";

    const qint64 gb = 1024LL * 1024 * 1024;
    const qint64 mb = 1024LL * 1024;
    const qint64 kb = 1024LL;

    if (bytes >= gb) return QString::number(bytes / double(gb), 'f', 2) + " GB";
    if (bytes >= mb) return QString::number(bytes / double(mb), 'f', 1) + " MB";
    if (bytes >= kb) return QString::number(bytes / double(kb), 'f', 0) + " KB";
    return QString::number(bytes) + " B";
}

inline QString getUpdateDownloadText(qint64 received, qint64 total) {
    if (total <= 0) return formatBytes(received);

    const qint64 gb = 1024LL * 1024 * 1024;
    const qint64 mb = 1024LL * 1024;
    const qint64 kb = 1024LL;

    QString unit = "B";
    qint64 divisor = 1;
    int precision = 0;

    if (total >= gb) { unit = "GB"; divisor = gb; precision = 2; }
    else if (total >= mb) { unit = "MB"; divisor = mb; precision = 1; }
    else if (total >= kb) { unit = "KB"; divisor = kb; precision = 0; }

    double receivedF = received / double(divisor);
    double totalF = total / double(divisor);

    return QString("%1 %2 / %3 %2")
        .arg(receivedF, 0, 'f', precision)
        .arg(unit)
        .arg(totalF, 0, 'f', precision);
}

}

#endif // STRINGUTIL_H
