#ifndef VERSIONUTIL_H
#define VERSIONUTIL_H

#include <QDate>
#include <QString>
#include <QVersionNumber>
#include <QRegularExpression>

enum class VersionType {
    Invalid,
    SemVer,
    Date,
    CommitHash,
    FallbackString
};

struct SemVer {
    QVersionNumber version;   // numeric part (1.2.3)
    QString prerelease;       // e.g., "beta", "rc1", "18143"
    QString buildMetadata;    // optional, ignored for comparisons
    bool isValid() const { return !version.isNull(); }
};

struct Version {
    SemVer semver;
    QDate date;
    QString commitHash;
    QString raw;

    bool hasSemVer() const { return semver.isValid(); }
    bool hasDate() const { return date.isValid(); }
    bool hasCommit() const { return !commitHash.isEmpty(); }
};

class VersionUtil
{
public:
    // Parse a version strings
    static Version parseVersion(const QString &input) {
        Version v;
        QString cleaned = input.trimmed();
        v.raw = cleaned;

        // try semver
        SemVer sv = parseSemVer(cleaned);
        if (sv.isValid()) {
            v.semver = sv;
        }

        // try date
        QRegularExpression dateRe(R"((\d{4})-(\d{2})-(\d{2}))");
        QRegularExpressionMatch dm = dateRe.match(cleaned);

        if (dm.hasMatch()) {
            int year = dm.captured(1).toInt();
            int month = dm.captured(2).toInt();
            int day = dm.captured(3).toInt();

            QDate d(year, month, day);
            if (d.isValid()) {
                v.date = d;
            }
        }

        // try commit hash
        QRegularExpression hashRe(R"(\b[0-9a-fA-F]{7,}\b)");
        if (hashRe.match(cleaned).hasMatch()) {
            v.commitHash = cleaned;
        }

        return v;
    }

    // Compare version strings
    // Returns -1 if v1 < v2, 0 if equal, 1 if v1 > v2
    static int compareVersions(const QString &v1, const QString &v2) {
        Version a = parseVersion(v1);
        Version b = parseVersion(v2);
        int result = 0;

        if(a.hasSemVer() && b.hasSemVer())
        {
            result = compareSemVer(a.semver, b.semver);
        }

        if(result == 0)
        {
            if(a.hasDate() && b.hasDate())
            {
                result = a.date < b.date ? -1 : (a.date > b.date ? 1 : 0);
            }

            if(result == 0)
            {
                if(a.hasCommit() && b.hasCommit())
                {
                    result = QString::compare(a.commitHash, b.commitHash, Qt::CaseInsensitive);
                }

                if(result == 0)
                {
                    result = QString::compare(a.raw, b.raw, Qt::CaseInsensitive) != 0 ? 1 : 0;
                }
            }
        }

        return result;
    }

private:
    // Parse a semantic version string
    static SemVer parseSemVer(const QString &input) {
        SemVer sv;
        QString cleaned = input.trimmed();

        // Strip leading non-digits (e.g., "v1.2.3")
        cleaned.remove(QRegularExpression("^[^0-9]+"));

        // Regex: numeric version + optional prerelease + optional build metadata
        QRegularExpression re(R"(^(\d+(?:\.\d+)*)(?:-([0-9A-Za-z\.-]+))?(?:\+([0-9A-Za-z\.-]+))?$)");
        QRegularExpressionMatch m = re.match(cleaned);

        if (m.hasMatch()) {
            sv.version = QVersionNumber::fromString(m.captured(1));
            sv.prerelease = m.captured(2);
            sv.buildMetadata = m.captured(3);
        }

        return sv;
    }

    // Compare two SemVer structs
    // Returns -1 if a < b, 0 if equal, 1 if a > b
    static int compareSemVer(const SemVer &a, const SemVer &b) {
        int cmp = QVersionNumber::compare(a.version, b.version);
        if (cmp != 0) return cmp;

        // Handle pre-release comparison
        if (a.prerelease.isEmpty() && b.prerelease.isEmpty()) return 0;
        if (a.prerelease.isEmpty()) return 1;   // release > pre-release
        if (b.prerelease.isEmpty()) return -1;  // pre-release < release

        return comparePreRelease(a.prerelease, b.prerelease);
    }

    // Numeric-aware pre-release comparison
    static int comparePreRelease(const QString &a, const QString &b) {
        bool ok1, ok2;
        int num1 = a.toInt(&ok1);
        int num2 = b.toInt(&ok2);

        if (ok1 && ok2) return (num1 < num2) ? -1 : (num1 > num2 ? 1 : 0);
        return QString::compare(a, b, Qt::CaseInsensitive);
    }
};

#endif // VERSIONUTIL_H
