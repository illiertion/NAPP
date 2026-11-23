#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QLocalServer;

namespace napp::system {

// Single instance guard built on a local socket. Unlike a lock file it can
// forward the files a second launch was asked to open (double-clicked .ipkg).
// 로컬 소켓 기반 단일 실행 보호. 락 파일과 달리, 두 번째 실행이 열려던 파일
// (더블클릭한 .ipkg)을 첫 번째 인스턴스에 전달할 수 있다.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(const QString& key, QObject* parent = nullptr);
    ~SingleInstance() override;

    // Tries to hand `files` to a running instance. Returns true when another
    // instance accepted them (the caller should exit).
    // 이미 실행 중인 인스턴스에 파일을 넘긴다. 성공하면 true (호출자는 종료)
    bool sendToRunning(const QStringList& files, int timeoutMs = 400);

    // Becomes the primary instance. Returns false if listening failed.
    // 첫 번째 인스턴스가 된다. 리슨 실패 시 false
    bool listen();

signals:
    // A later launch asked this instance to open `files` (may be empty = focus).
    // 이후 실행이 이 인스턴스에 파일 열기(비어 있으면 포커스)를 요청함
    void requested(const QStringList& files);

private:
    void onNewConnection();

    QString m_name;
    QLocalServer* m_server = nullptr;
};

} // namespace napp::system
