#include "Shortcut.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#if defined(Q_OS_WIN)
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif

namespace napp::system {
namespace {

// Windows never allows these in a file name; everything else (including Hangul)
// is kept as the user typed it.
// Windows 파일 이름에 쓸 수 없는 문자만 바꾸고 나머지(한글 포함)는 그대로 둔다.
QString safeFileName(const QString& name)
{
    QString result = name.trimmed();
    for (QChar& character : result) {
        const ushort code = character.unicode();
        if (code < 0x20 || QStringLiteral("<>:\"/\\|?*").contains(character))
            character = QLatin1Char('-');
    }
    while (result.endsWith(QLatin1Char('.')) || result.endsWith(QLatin1Char(' ')))
        result.chop(1);
    return result;
}

// "C:\Program Files\NAPP" -> "C:\Program Files\NAPP"
// --x  -> --x
QString quotedArgument(const QString& argument)
{
    if (!argument.contains(QLatin1Char(' ')) && !argument.contains(QLatin1Char('\t')))
        return argument;
    QString quoted = argument;
    quoted.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(quoted);
}

} // namespace

QString Shortcut::createOnDesktop(const QString& name, const QString& target,
                                  const QStringList& arguments, const QString& iconPath,
                                  const QString& workingDirectory, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return QString();
    };

    if (target.isEmpty() || !QFileInfo::exists(target))
        return fail(QStringLiteral("실행 파일을 찾을 수 없습니다: %1").arg(target));

    QString desktop = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (desktop.isEmpty())
        return fail(QStringLiteral("바탕 화면 경로를 확인할 수 없습니다."));

    const QString base = safeFileName(name.isEmpty() ? QStringLiteral("NAPP App") : name);
    QString path = QDir(desktop).filePath(base + QStringLiteral(".lnk"));
    for (int index = 2; QFileInfo::exists(path) && index < 1000; ++index)
        path = QDir(desktop).filePath(QStringLiteral("%1 (%2).lnk").arg(base).arg(index));

#if defined(Q_OS_WIN)
    // The COM apartment may already be initialised by Qt (STA). A different
    // model request fails with RPC_E_CHANGED_MODE, which is harmless here.
    // Qt 가 이미 STA 로 초기화했을 수 있다. 다른 모델 요청은 RPC_E_CHANGED_MODE 로
    // 실패하지만 여기서는 문제되지 않는다.
    const HRESULT initialised = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool ownsCom = SUCCEEDED(initialised);

    IShellLinkW* link = nullptr;
    HRESULT result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                      IID_IShellLinkW, reinterpret_cast<void**>(&link));
    if (FAILED(result) || !link) {
        if (ownsCom)
            CoUninitialize();
        return fail(QStringLiteral("바로가기를 만들 수 없습니다 (0x%1).")
                        .arg(quint32(result), 8, 16, QLatin1Char('0')));
    }

    link->SetPath(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(target).utf16()));
    if (!workingDirectory.isEmpty())
        link->SetWorkingDirectory(
            reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(workingDirectory).utf16()));
    if (!arguments.isEmpty()) {
        QStringList quoted;
        for (const QString& argument : arguments)
            quoted.append(quotedArgument(argument));
        link->SetArguments(reinterpret_cast<LPCWSTR>(quoted.join(QLatin1Char(' ')).utf16()));
    }
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath))
        link->SetIconLocation(
            reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(iconPath).utf16()), 0);
    link->SetDescription(reinterpret_cast<LPCWSTR>(name.utf16()));

    IPersistFile* persist = nullptr;
    result = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist));
    if (SUCCEEDED(result) && persist) {
        const QString native = QDir::toNativeSeparators(path);
        result = persist->Save(reinterpret_cast<LPCOLESTR>(native.utf16()), TRUE);
        persist->Release();
    }
    link->Release();
    if (ownsCom)
        CoUninitialize();

    if (FAILED(result) || !QFileInfo::exists(path))
        return fail(QStringLiteral("바로가기 파일을 쓰지 못했습니다."));
    return path;
#else
    Q_UNUSED(arguments);
    Q_UNUSED(iconPath);
    Q_UNUSED(workingDirectory);
    return fail(QStringLiteral("바로가기는 Windows 에서만 지원합니다."));
#endif
}

} // namespace napp::system
