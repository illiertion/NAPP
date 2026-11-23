#include "napp/core/semver.h"

#include <QRegularExpression>

namespace napp {
namespace {

const QRegularExpression& semverPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)"
                       "(?:-([0-9A-Za-z.-]+))?(?:\\+([0-9A-Za-z.-]+))?$"));
    return pattern;
}

// Numeric identifiers compare numerically, alphanumeric ones lexically,
// numeric identifiers always sort lower (SemVer 2.0.0 rule 11.4).
int compareIdentifier(const QString& left, const QString& right)
{
    const bool leftNumeric = !left.isEmpty() && left.toLongLong() >= 0
        && left.front().isDigit();
    const bool rightNumeric = !right.isEmpty() && right.toLongLong() >= 0
        && right.front().isDigit();

    if (leftNumeric && rightNumeric)
        return left.toLongLong() < right.toLongLong() ? -1 : (left.toLongLong() > right.toLongLong() ? 1 : 0);
    if (leftNumeric != rightNumeric)
        return leftNumeric ? -1 : 1;
    return QString::compare(left, right);
}

} // namespace

std::optional<SemVer> SemVer::parse(const QString& text)
{
    const auto match = semverPattern().match(text);
    if (!match.hasMatch())
        return std::nullopt;

    SemVer version;
    for (int index = 1; index <= 3; ++index)
        version.m_parts.append(match.captured(index).toInt());

    if (!match.captured(4).isEmpty())
        version.m_prerelease = match.captured(4).split(QLatin1Char('.'));
    version.m_build = match.captured(5);
    return version;
}

int SemVer::compare(const SemVer& other) const
{
    for (int index = 0; index < 3; ++index) {
        const int left = partAt(index);
        const int right = other.partAt(index);
        if (left != right)
            return left < right ? -1 : 1;
    }

    // A version without a prerelease has higher precedence.
    if (m_prerelease.isEmpty() && other.m_prerelease.isEmpty())
        return 0;
    if (m_prerelease.isEmpty())
        return 1;
    if (other.m_prerelease.isEmpty())
        return -1;

    const int shared = qMin(m_prerelease.size(), other.m_prerelease.size());
    for (int index = 0; index < shared; ++index) {
        const int result = compareIdentifier(m_prerelease.at(index), other.m_prerelease.at(index));
        if (result != 0)
            return result;
    }
    if (m_prerelease.size() == other.m_prerelease.size())
        return 0;
    return m_prerelease.size() < other.m_prerelease.size() ? -1 : 1;
}

std::optional<int> SemVer::compareStrings(const QString& left, const QString& right)
{
    const auto a = parse(left);
    const auto b = parse(right);
    if (!a || !b)
        return std::nullopt;
    return a->compare(*b);
}

QString SemVer::toString() const
{
    if (isNull())
        return {};

    QString text = QStringLiteral("%1.%2.%3").arg(m_parts.at(0)).arg(m_parts.at(1)).arg(m_parts.at(2));
    if (!m_prerelease.isEmpty())
        text += QLatin1Char('-') + m_prerelease.join(QLatin1Char('.'));
    if (!m_build.isEmpty())
        text += QLatin1Char('+') + m_build;
    return text;
}

} // namespace napp
