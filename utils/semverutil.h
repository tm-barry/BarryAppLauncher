#ifndef SEMVERUTIL_H
#define SEMVERUTIL_H

#include <QString>
#include <QVersionNumber>
#include <QRegularExpression>

class SemVerUtil
{
public:
    struct SemVer {
        QVersionNumber version;   // numeric part (1.2.3)
        QString prerelease;       // e.g., "beta", "rc1", "18143"
        QString buildMetadata;    // optional, ignored for comparisons

        bool isValid() const { return !version.isNull(); }
    };

    // Parse a semantic version string
    static SemVer parse(const QString &input) {
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
    static int compare(const SemVer &a, const SemVer &b) {
        int cmp = QVersionNumber::compare(a.version, b.version);
        if (cmp != 0) return cmp;

        // Handle pre-release comparison
        if (a.prerelease.isEmpty() && b.prerelease.isEmpty()) return 0;
        if (a.prerelease.isEmpty()) return 1;   // release > pre-release
        if (b.prerelease.isEmpty()) return -1;  // pre-release < release

        return comparePreRelease(a.prerelease, b.prerelease);
    }

    // Compare two version strings directly
    static int compareStrings(const QString &v1, const QString &v2) {
        return compare(parse(v1), parse(v2));
    }

private:
    // Numeric-aware pre-release comparison
    static int comparePreRelease(const QString &a, const QString &b) {
        bool ok1, ok2;
        int num1 = a.toInt(&ok1);
        int num2 = b.toInt(&ok2);

        if (ok1 && ok2) return (num1 < num2) ? -1 : (num1 > num2 ? 1 : 0);
        return QString::compare(a, b, Qt::CaseInsensitive);
    }
};

#endif // SEMVERUTIL_H
