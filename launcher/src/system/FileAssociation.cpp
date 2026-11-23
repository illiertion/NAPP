#include "FileAssociation.h"

#include <QDir>
#include <QSettings>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <shlobj.h>
#endif

namespace napp::system {
namespace {

constexpr auto kClassesRoot = "HKEY_CURRENT_USER\\Software\\Classes";
constexpr auto kProgId = "NAPP.ipkg";

#if defined(Q_OS_WIN)
QString commandFor(const QString& executable)
{
    // "C:\path\NAPP.exe" "%1"
    return QStringLiteral("\"%1\" \"%2\"")
        .arg(QDir::toNativeSeparators(executable), QStringLiteral("%1"));
}

void notifyShell()
{
    // Tell Explorer the association changed so icons refresh without a re-login.
    // 로그아웃 없이 아이콘이 갱신되도록 탐색기에 변경을 알림
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
#endif

} // namespace

bool FileAssociation::isRegistered(const QString& executable)
{
#if defined(Q_OS_WIN)
    QSettings classes(QString::fromLatin1(kClassesRoot), QSettings::NativeFormat);
    const QString progId = classes.value(QStringLiteral(".ipkg/.")).toString();
    if (progId != QLatin1String(kProgId))
        return false;
    const QString command =
        classes.value(QStringLiteral("NAPP.ipkg/shell/open/command/.")).toString();
    return command.compare(commandFor(executable), Qt::CaseInsensitive) == 0;
#else
    Q_UNUSED(executable);
    return false;
#endif
}

bool FileAssociation::registerFor(const QString& executable)
{
#if defined(Q_OS_WIN)
    QSettings classes(QString::fromLatin1(kClassesRoot), QSettings::NativeFormat);
    const QString native = QDir::toNativeSeparators(executable);

    classes.setValue(QStringLiteral(".ipkg/."), QString::fromLatin1(kProgId));
    classes.setValue(QStringLiteral(".ipkg/Content Type"), QStringLiteral("application/x-ipkg"));
    classes.setValue(QStringLiteral("NAPP.ipkg/."), QStringLiteral("NAPP Package"));
    classes.setValue(QStringLiteral("NAPP.ipkg/DefaultIcon/."), QStringLiteral("\"%1\",0").arg(native));
    classes.setValue(QStringLiteral("NAPP.ipkg/shell/open/command/."), commandFor(executable));
    classes.sync();
    if (classes.status() != QSettings::NoError)
        return false;
    notifyShell();
    return true;
#else
    Q_UNUSED(executable);
    return false;
#endif
}

bool FileAssociation::unregister(const QString& executable)
{
#if defined(Q_OS_WIN)
    if (!isRegistered(executable))
        return true;
    QSettings classes(QString::fromLatin1(kClassesRoot), QSettings::NativeFormat);
    classes.remove(QStringLiteral(".ipkg"));
    classes.remove(QStringLiteral("NAPP.ipkg"));
    classes.sync();
    notifyShell();
    return classes.status() == QSettings::NoError;
#else
    Q_UNUSED(executable);
    return true;
#endif
}

} // namespace napp::system
