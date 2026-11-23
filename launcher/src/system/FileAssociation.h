#pragma once

#include <QString>

namespace napp::system {

// Per-user ".ipkg" file association (HKCU\Software\Classes). No administrator
// rights are needed. // 사용자 단위 ".ipkg" 파일 연결 (관리자 권한 불필요)
// All functions are no-ops on non-Windows platforms. // Windows 외에서는 아무 동작도 하지 않는다
class FileAssociation
{
public:
    // True when .ipkg currently opens with `executable`.
    // .ipkg 가 현재 해당 실행 파일로 열리면 true
    static bool isRegistered(const QString& executable);

    // Registers (or repairs) the association for `executable`.
    // 연결을 등록(또는 복구)한다
    static bool registerFor(const QString& executable);

    // Removes the association if it points at `executable`.
    // 연결이 해당 실행 파일을 가리킬 때만 제거한다
    static bool unregister(const QString& executable);
};

} // namespace napp::system
