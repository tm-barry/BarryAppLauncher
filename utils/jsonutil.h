#ifndef JSONUTIL_H
#define JSONUTIL_H

#include "managers/errormanager.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>

class JsonUtil
{
public:
    static QList<QJsonValue> getValuesByPath(const QJsonValue &root, const QString &path) {
        QList<QJsonValue> current { root };

        if (path.isEmpty())
            return { root };

        QStringList segments = path.split('.', Qt::SkipEmptyParts);
        QRegularExpression indexRe(R"(^(.*)\[(\d+)\]$)");

        for (QString &seg : segments) {
            seg = seg.trimmed();
            QList<QJsonValue> next;

            bool isArrayWildcard = seg.endsWith("[*]");
            int arrayIndex = -1;
            QString key = seg;

            // Check for numeric index like "assets[0]"
            QRegularExpressionMatch match = indexRe.match(seg);
            if (match.hasMatch()) {
                key = match.captured(1);
                arrayIndex = match.captured(2).toInt();
            } else if (isArrayWildcard) {
                key = seg.left(seg.length() - 3);
            }

            for (const QJsonValue &val : current) {
                QJsonValue child;

                // Object access
                if (val.isObject() && !key.isEmpty()) {
                    const QJsonObject obj = val.toObject();
                    child = obj.value(key);
                }

                // Array access at root or intermediate
                else if (val.isArray() && key.isEmpty()) {
                    child = val;
                }

                // Wildcard [*]
                if (isArrayWildcard && child.isArray()) {
                    for (const QJsonValue &arrVal : child.toArray())
                        next.append(arrVal);
                }
                // Numeric index [n]
                else if (arrayIndex >= 0 && child.isArray()) {
                    QJsonArray arr = child.toArray();
                    if (arrayIndex < arr.size())
                    {
                        next.append(arr[arrayIndex]);
                    }
                    else
                    {
                        QString message = QString("Index %1 out of range for key %2").arg(arrayIndex).arg(key);
                        ErrorManager::instance()->reportError(message);
                    }
                }
                // Normal key access
                else if (arrayIndex < 0 && !isArrayWildcard) {
                    if (!child.isUndefined())
                        next.append(child);
                }
            }

            current = next;
            if (current.isEmpty()) break;
        }

        return current;
    }
};

#endif // JSONUTIL_H
