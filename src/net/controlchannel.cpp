#include "controlchannel.h"

#include <QDataStream>
#include <QJsonDocument>
#include <QSignalBlocker>

namespace
{
constexpr QDataStream::ByteOrder kByteOrder = QDataStream::BigEndian;

QJsonObject parseJson(const QByteArray &payload)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
    {
        return {};
    }
    return doc.object();
}
} // namespace

ControlChannel::ControlChannel(QObject *parent)
    : QObject(parent)
{
    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &ControlChannel::handleNewConnection);
}

ControlChannel::~ControlChannel()
{
    // 注意：Qt 的 QObject 子对象会在父对象析构的“最后阶段”（QObject::~QObject）才被销毁。
    // 如果在析构函数里 emit 信号，接收方很可能已经先释放了 UI/成员资源，导致退出时出现
    // “QCoreApplication::postEvent: Unexpected null receiver”甚至崩溃（Win7 上更明显）。
    // 因此这里在清理网络资源前先阻断信号，确保析构过程不会回调到上层业务对象。
    const QSignalBlocker blocker(this);
    stopHost();
    disconnectFromHost();
}

bool ControlChannel::startHost(quint16 port)
{
    if (!server_) return false;
    if (server_->isListening()) server_->close();
    const bool ok = server_->listen(QHostAddress::Any, port);
    if (!ok)
    {
        emit hostClientChanged(false, QStringLiteral("listen failed"));
    }
    return ok;
}

void ControlChannel::stopHost()
{
    if (server_ && server_->isListening()) server_->close();
    closeHostSocket(QStringLiteral("host stop"));
}

QString ControlChannel::hostPeer() const
{
    if (hostSocket_.isNull()) return QString();
    return QStringLiteral("%1:%2").arg(hostSocket_->peerAddress().toString()).arg(hostSocket_->peerPort());
}

void ControlChannel::connectToHost(const QString &host, quint16 port)
{
    disconnectFromHost();
    controllerSocket_ = new QTcpSocket(this);
    controllerBuffer_.clear();
    controllerState_ = ControllerState::Connecting;
    emit controllerStateChanged(controllerState_, QStringLiteral("connecting"));
    connect(controllerSocket_, &QTcpSocket::readyRead, this, &ControlChannel::handleControllerReadyRead);
    connect(controllerSocket_, &QTcpSocket::connected, this, &ControlChannel::handleControllerConnected);
    connect(controllerSocket_, &QTcpSocket::disconnected, this, &ControlChannel::handleControllerDisconnected);
    connect(controllerSocket_, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, &ControlChannel::handleSocketError);
    controllerSocket_->connectToHost(host, port);
}

void ControlChannel::disconnectFromHost()
{
    if (!controllerSocket_.isNull())
    {
        controllerSocket_->disconnectFromHost();
        controllerSocket_->deleteLater();
    }
    controllerSocket_.clear();
    controllerBuffer_.clear();
    if (controllerState_ != ControllerState::Idle)
    {
        controllerState_ = ControllerState::Idle;
        emit controllerStateChanged(controllerState_, QStringLiteral("disconnected"));
    }
}

bool ControlChannel::sendToController(const QJsonObject &obj)
{
    return writeFrame(hostSocket_.data(), obj);
}

bool ControlChannel::sendToHost(const QJsonObject &obj)
{
    return writeFrame(controllerSocket_.data(), obj);
}

void ControlChannel::handleNewConnection()
{
    if (!server_) return;
    QTcpSocket *incoming = server_->nextPendingConnection();
    if (!incoming) return;

    if (!hostSocket_.isNull())
    {
        // Already controlled; politely reject and close.
        QJsonObject busy;
        busy.insert(QStringLiteral("type"), QStringLiteral("reject"));
        busy.insert(QStringLiteral("reason"), QStringLiteral("busy"));
        writeFrame(incoming, busy);
        incoming->disconnectFromHost();
        incoming->deleteLater();
        return;
    }

    hostSocket_ = incoming;
    hostBuffer_.clear();
    connect(hostSocket_, &QTcpSocket::readyRead, this, &ControlChannel::handleHostReadyRead);
    connect(hostSocket_, &QTcpSocket::disconnected, this, &ControlChannel::handleHostDisconnected);
    connect(hostSocket_, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this, &ControlChannel::handleSocketError);
    emit hostClientChanged(true, QStringLiteral("connected"));
}

void ControlChannel::handleHostReadyRead()
{
    if (hostSocket_.isNull()) return;
    processBuffer(hostSocket_.data(), hostBuffer_, [this](const QJsonObject &obj)
                  { emit hostCommandArrived(obj); });
}

void ControlChannel::handleHostDisconnected()
{
    closeHostSocket(QStringLiteral("peer closed"));
}

void ControlChannel::handleControllerReadyRead()
{
    if (controllerSocket_.isNull()) return;
    processBuffer(controllerSocket_.data(), controllerBuffer_, [this](const QJsonObject &obj)
                  { emit controllerEventArrived(obj); });
}

void ControlChannel::handleControllerConnected()
{
    controllerState_ = ControllerState::Connected;
    emit controllerStateChanged(controllerState_, QStringLiteral("connected"));
}

void ControlChannel::handleControllerDisconnected()
{
    disconnectFromHost();
}

void ControlChannel::handleSocketError(QAbstractSocket::SocketError error)
{
    // 对控制端：连接失败（如未开放控制）时给出明确原因，避免静默断开
    if (!controllerSocket_.isNull() && controllerSocket_->state() == QAbstractSocket::UnconnectedState)
    {
        controllerSocket_->deleteLater();
        controllerSocket_.clear();
        controllerBuffer_.clear();
        if (controllerState_ != ControllerState::Idle)
        {
            controllerState_ = ControllerState::Idle;
            const QString reason = (error == QAbstractSocket::ConnectionRefusedError) ? QStringLiteral("refused") : QStringLiteral("error");
            emit controllerStateChanged(controllerState_, reason);
        }
    }
    if (!hostSocket_.isNull() && hostSocket_->state() == QAbstractSocket::UnconnectedState)
    {
        closeHostSocket(QStringLiteral("error"));
    }
}

void ControlChannel::processBuffer(QTcpSocket *sock, QByteArray &buffer, const std::function<void(const QJsonObject &)> &handler)
{
    if (!sock || !handler) return;
    buffer.append(sock->readAll());
    while (buffer.size() >= static_cast<int>(sizeof(quint32)))
    {
        QDataStream stream(buffer);
        stream.setByteOrder(kByteOrder);
        quint32 len = 0;
        stream >> len;
        if (buffer.size() < static_cast<int>(sizeof(quint32) + len)) break;
        QByteArray payload = buffer.mid(sizeof(quint32), len);
        buffer.remove(0, sizeof(quint32) + len);
        const QJsonObject obj = parseJson(payload);
        if (obj.isEmpty()) continue;
        handler(obj);
    }
}

bool ControlChannel::writeFrame(QTcpSocket *sock, const QJsonObject &obj)
{
    if (!sock) return false;
    const QByteArray payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray frame;
    QDataStream out(&frame, QIODevice::WriteOnly);
    out.setByteOrder(kByteOrder);
    out << quint32(payload.size());
    frame.append(payload);
    const qint64 written = sock->write(frame);
    return written == frame.size();
}

void ControlChannel::closeHostSocket(const QString &reason)
{
    if (!hostSocket_.isNull())
    {
        hostSocket_->disconnectFromHost();
        hostSocket_->deleteLater();
    }
    hostSocket_.clear();
    hostBuffer_.clear();
    emit hostClientChanged(false, reason);
}
