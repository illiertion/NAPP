#pragma once

#include <QString>
#include <QStringList>

namespace napp::system {

// Creates per-user Windows shortcuts (.lnk) through the shell link COM interface.
// No administrator rights are needed and no external tool is spawned.
// 사용자 권한으로 Windows 바로가기(.lnk)를 만든다. 관리자 권한도, 외부 도구 실행도 필요 없다.
class Shortcut
{
public:
    // Writes `name`.lnk on the user's desktop and returns its path, or an empty
    // string with `error` filled in. An existing shortcut with the same name is
    // never overwritten: the file name becomes "name (2)", "name (3)", ...
    // 바탕 화면에 `name`.lnk 를 만들고 경로를 돌려준다. 같은 이름이 있으면 덮어쓰지 않고
    // "name (2)", "name (3)" 처럼 번호를 붙인다.
    static QString createOnDesktop(const QString& name, const QString& target,
                                   const QStringList& arguments, const QString& iconPath,
                                   const QString& workingDirectory, QString* error);
};

} // namespace napp::system
