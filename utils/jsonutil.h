#ifndef JSONUTIL_H
#define JSONUTIL_H

#include "managers/errormanager.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QRegularExpression>

class JsonUtil
{
public:
    static QList<QJsonValue> getValuesByPath(const QJsonValue &root, const QString &path) {
        QList<QJsonValue> current { root };

        if (path.isEmpty())
            return { root };

        QStringList segments = splitPathPreserveBrackets(path);

        static const QRegularExpression indexRe(R"(^(.*)\[(\d+)\]$)");
        // Supports regex:, rx:, with optional field
        static const QRegularExpression multiRegexRe(R"(^(.*)\[(?:regex|rx):([^:,]*):(.*)\]$)");

        for (QString &seg : segments) {
            seg = seg.trimmed();
            QList<QJsonValue> next;

            bool isArrayWildcard = seg.endsWith("[*]");
            int arrayIndex = -1;
            QList<QPair<QString, QRegularExpression>> regexFilters;
            QString key = seg;

            // Numeric index like "assets[0]"
            QRegularExpressionMatch match = indexRe.match(seg);
            if (match.hasMatch()) {
                key = match.captured(1);
                arrayIndex = match.captured(2).toInt();
            }
            // Multi-regex filter like "[rx:field:pattern,...]" or "[rx::pattern,...]" (field optional)
            else {
                QRegularExpressionMatch regexMatch = multiRegexRe.match(seg);
                if (regexMatch.hasMatch()) {
                    key = regexMatch.captured(1);

                    QString combinedFilters = regexMatch.captured(2) + ":" + regexMatch.captured(3);
                    QStringList filters = combinedFilters.split(',', Qt::SkipEmptyParts);

                    for (const QString &f : std::as_const(filters)) {
                        QString filter = f.trimmed();

                        // Split on first colon
                        int firstColon = filter.indexOf(':');
                        QString field;
                        QString pattern;
                        if (firstColon >= 0) {
                            field = filter.left(firstColon).trimmed();
                            pattern = filter.mid(firstColon + 1).trimmed();
                        } else {
                            field.clear();
                            pattern = filter;
                        }

                        regexFilters.append(qMakePair(field, QRegularExpression(pattern)));
                    }
                }
                else if (isArrayWildcard) {
                    key = seg.left(seg.length() - 3);
                }
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
                    QJsonArray arr = child.toArray();
                    for (const QJsonValue &arrVal : std::as_const(arr))
                        next.append(arrVal);
                }
                // Numeric index [n]
                else if (arrayIndex >= 0 && child.isArray()) {
                    QJsonArray arr = child.toArray();
                    if (arrayIndex < arr.size())
                        next.append(arr[arrayIndex]);
                    else
                        ErrorManager::instance()->reportError(
                            QString("Index %1 out of range for key %2").arg(arrayIndex).arg(key));
                }
                // Multi-regex filter
                else if (!regexFilters.isEmpty() && child.isArray()) {
                    QJsonArray arr = child.toArray();
                    for (const QJsonValue &arrVal : std::as_const(arr)) {
                        if (!arrVal.isObject() && !arrVal.isString()) continue;
                        const QJsonObject obj = arrVal.isObject() ? arrVal.toObject() : QJsonObject();

                        bool allMatch = true;
                        for (const auto &filterPair : regexFilters) {
                            QString fieldValue;

                            if (!filterPair.first.isEmpty() && obj.contains(filterPair.first))
                                fieldValue = obj.value(filterPair.first).toString();
                            else
                                fieldValue = arrVal.toString(); // fallback to whole value

                            if (!filterPair.second.match(fieldValue).hasMatch()) {
                                allMatch = false;
                                break;
                            }
                        }
                        if (allMatch)
                            next.append(arrVal);
                    }
                }
                // Normal key access
                else if (arrayIndex < 0 && !isArrayWildcard && regexFilters.isEmpty()) {
                    if (!child.isUndefined())
                        next.append(child);
                }
            }

            current = next;
            if (current.isEmpty()) break;
        }

        return current;
    }

    static QStringList splitPathPreserveBrackets(const QString &path) {
        QStringList segments;
        QString current;
        int bracketLevel = 0;

        for (int i = 0; i < path.length(); ++i) {
            QChar c = path[i];

            if (c == '[') bracketLevel++;
            else if (c == ']') bracketLevel--;

            if (c == '.' && bracketLevel == 0) {
                if (!current.isEmpty())       // <-- Skip empty segments
                    segments.append(current);
                current.clear();
            } else {
                current.append(c);
            }
        }

        if (!current.isEmpty())
            segments.append(current);

        return segments;
    }
};

#endif // JSONUTIL_H
