#include "napp/ipkg/path_rules.h"

#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace napp::ipkg::paths {
namespace {

bool hasControlCharacter(const QString& text)
{
    for (const QChar character : text) {
        if (character.unicode() < 0x20 || character.unicode() == 0x7F)
            return true;
    }
    return false;
}

bool hasForbiddenCharacter(const QString& text)
{
    static const QString forbidden = QStringLiteral("<>:\"|?*");
    for (const QChar character : text) {
        if (forbidden.contains(character))
            return true;
    }
    return false;
}

} // namespace

bool isReservedWindowsName(const QString& segment)
{
    static const QSet<QString> reserved{
        QStringLiteral("con"), QStringLiteral("prn"), QStringLiteral("aux"), QStringLiteral("nul"),
        QStringLiteral("com1"), QStringLiteral("com2"), QStringLiteral("com3"), QStringLiteral("com4"),
        QStringLiteral("com5"), QStringLiteral("com6"), QStringLiteral("com7"), QStringLiteral("com8"),
        QStringLiteral("com9"), QStringLiteral("lpt1"), QStringLiteral("lpt2"), QStringLiteral("lpt3"),
        QStringLiteral("lpt4"), QStringLiteral("lpt5"), QStringLiteral("lpt6"), QStringLiteral("lpt7"),
        QStringLiteral("lpt8"), QStringLiteral("lpt9"),
    };

    QString stem = segment;
    const int dot = stem.indexOf(QLatin1Char('.'));
    if (dot >= 0)
        stem = stem.left(dot);
    return reserved.contains(stem.trimmed().toLower());
}

Result<void> validateEntryName(const QString& name, bool isDirectory, int maxPathBytes)
{
    if (name.isEmpty())
        return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("빈 엔트리 이름"));

    if (name.toUtf8().size() > maxPathBytes)
        return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("경로 길이 초과: %1").arg(name));

    if (hasControlCharacter(name))
        return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("제어 문자 포함: %1").arg(name));

    if (name.contains(QLatin1Char('\\')))
        return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("역슬래시 경로 구분자: %1").arg(name));

    if (name.startsWith(QLatin1Char('/')))
        return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("절대 경로: %1").arg(name));

    // "C:" style drive prefixes and UNC style "//server".
    static const QRegularExpression drivePattern(QStringLiteral("^[A-Za-z]:"));
    if (drivePattern.match(name).hasMatch())
        return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("드라이브 문자 경로: %1").arg(name));

    if (name.contains(QStringLiteral("//")))
        return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("빈 경로 세그먼트: %1").arg(name));

    const QStringList segments = name.split(QLatin1Char('/'));
    for (int index = 0; index < segments.size(); ++index) {
        const QString& segment = segments.at(index);
        const bool last = index == segments.size() - 1;

        if (segment.isEmpty()) {
            if (last && isDirectory)
                continue; // trailing slash of a directory entry
            return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("빈 경로 세그먼트: %1").arg(name));
        }

        if (segment == QStringLiteral("..") || segment == QStringLiteral("."))
            return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("상대 경로 세그먼트: %1").arg(name));

        if (hasForbiddenCharacter(segment))
            return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("금지 문자 포함: %1").arg(name));

        if (segment.endsWith(QLatin1Char('.')) || segment.endsWith(QLatin1Char(' ')))
            return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("세그먼트 끝 점/공백: %1").arg(name));

        if (isReservedWindowsName(segment))
            return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("예약된 파일 이름: %1").arg(name));
    }

    if (!isDirectory && name.endsWith(QLatin1Char('/')))
        return Result<void>::fail(ErrorCode::UnsafePath, QStringLiteral("파일 엔트리가 /로 끝남: %1").arg(name));

    return Result<void>::ok();
}

QString duplicationKey(const QString& name)
{
    QString key = name;
    while (key.endsWith(QLatin1Char('/')))
        key.chop(1);
    return key.toCaseFolded();
}

} // namespace napp::ipkg::paths
