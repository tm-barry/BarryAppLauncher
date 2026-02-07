#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#include <QDateTime>
#include <QStringList>

namespace StringUtil {

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

}

#endif // STRINGUTIL_H
