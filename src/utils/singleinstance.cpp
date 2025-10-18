// singleinstance.cpp - see header for details

#include "singleinstance.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent), key_(key)
{
}

bool SingleInstance::ensurePrimary()
{
    if (server_) return true; // already primary

    // First probe: if another instance is alive, connecting will succeed quickly.
    {
        QLocalSocket probe;
        probe.connectToServer(key_);
        if (probe.waitForConnected(150))
        {
            probe.disconnectFromServer();
            return false; // active primary exists
        }
    }

    // No active listener detected. Clean up possible stale entry (Unix) and listen.
    QLocalServer::removeServer(key_);
    QLocalServer *srv = new QLocalServer(this);
    if (!srv->listen(key_))
    {
        delete srv;
        srv = nullptr;
        return false; // failed to become primary
    }
    server_ = srv;
    connect(server_, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);
    return true;
}

bool SingleInstance::notifyRunning(const QByteArray &message)
{
    QLocalSocket sock;
    sock.connectToServer(key_);
    if (!sock.waitForConnected(300))
    {
        return false;
    }
    if (!message.isEmpty())
    {
        sock.write(message);
        sock.flush();
    }
    sock.waitForBytesWritten(300);
    sock.disconnectFromServer();
    return true;
}

QString SingleInstance::keyForAppPath(const QString &appPath)
{
    // Use a short, portable name (no slashes). Named pipe on Windows, pathname on Unix.
    const QByteArray norm = QFileInfo(appPath).absoluteFilePath().toUtf8();
    const QByteArray sha = QCryptographicHash::hash(norm, QCryptographicHash::Sha1).toHex();
    return QString::fromLatin1("eva-%1").arg(QString::fromLatin1(sha.left(32))); // 32 hex chars are enough
}

void SingleInstance::onNewConnection()
{
    while (server_ && server_->hasPendingConnections())
    {
        QLocalSocket *c = server_->nextPendingConnection();
        if (!c) continue;
        // Read whatever the client sends (usually small), then notify.
        QObject::connect(c, &QLocalSocket::readyRead, this, [this, c]()
                         {
                             const QByteArray data = c->readAll();
                             emit received(data); });
        QObject::connect(c, &QLocalSocket::disconnected, c, &QObject::deleteLater);
        // In case client writes immediately and disconnects fast, also emit once now
        // so primary can react even if no data arrives.
        emit received(QByteArray());
    }
}
