#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>

namespace napp::system {

SingleInstance::SingleInstance(const QString& key, QObject* parent)
    : QObject(parent)
{
    // Short, stable, per-user name. // 짧고 안정적인 사용자별 이름
    const QByteArray digest = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    m_name = QStringLiteral("napp-launcher-%1").arg(QString::fromLatin1(digest.left(16)));
}

SingleInstance::~SingleInstance() = default;

bool SingleInstance::sendToRunning(const QStringList& files, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(timeoutMs))
        return false; // nobody is listening // 듣고 있는 인스턴스 없음

    QJsonObject message;
    message.insert(QStringLiteral("files"), QJsonArray::fromStringList(files));
    socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact));
    socket.write("\n");
    socket.flush();
    socket.waitForBytesWritten(timeoutMs);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen()
{
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);

    // A crashed instance can leave a stale name behind; remove it before listening.
    // 비정상 종료한 인스턴스가 남긴 이름이 있으면 지우고 리슨
    QLocalServer::removeServer(m_name);
    if (!m_server->listen(m_name))
        return false;

    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

void SingleInstance::onNewConnection()
{
    while (QLocalSocket* socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
            // Cap the message so a rogue local process cannot balloon memory.
            // 악의적 로컬 프로세스가 메모리를 키우지 못하게 크기 제한
            if (socket->bytesAvailable() > 64 * 1024) {
                socket->disconnectFromServer();
                return;
            }
            const QByteArray line = socket->readAll().trimmed();
            const QJsonObject object = QJsonDocument::fromJson(line).object();
            QStringList files;
            for (const QJsonValue& value : object.value(QStringLiteral("files")).toArray())
                files.append(value.toString());
            emit requested(files);
        });
    }
}

} // namespace napp::system
