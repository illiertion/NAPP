#pragma once

#include <QString>
#include <QStringList>

#include <optional>

namespace napp {

// Semantic Versioning 2.0.0 with precedence comparison (§10 of the IPKG spec).
class SemVer
{
public:
    SemVer() = default;

    static std::optional<SemVer> parse(const QString& text);
    static bool isValid(const QString& text) { return parse(text).has_value(); }

    // -1 / 0 / 1 following SemVer precedence; build metadata is ignored.
    int compare(const SemVer& other) const;
    static std::optional<int> compareStrings(const QString& left, const QString& right);

    bool operator<(const SemVer& other) const { return compare(other) < 0; }
    bool operator==(const SemVer& other) const { return compare(other) == 0; }

    QString toString() const;
    bool isNull() const { return m_parts.isEmpty(); }

    int major() const { return partAt(0); }
    int minor() const { return partAt(1); }
    int patch() const { return partAt(2); }

private:
    int partAt(int index) const { return index < m_parts.size() ? m_parts.at(index) : 0; }

    QList<int> m_parts;
    QStringList m_prerelease;
    QString m_build;
};

} // namespace napp
