#ifndef CONTROLCHANNEL_H
#define CONTROLCHANNEL_H

#include <QByteArray>
#include <QJsonObject>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <functional>

// Lightweight duplex channel for controller<->host coordination.
// Provides framed JSON messages with a single active controller.
class ControlChannel : public QObject
{
    Q_OBJECT

  public:
    enum class ControllerState
    {
        Idle,
        Connecting,
        Connected
    };

    explicit ControlChannel(QObject *parent = nullptr);
    ~ControlChannel() override;

    // Host-side: listen for a controller. Rejects extra connections.
    bool startHost(quint16 port);
    void stopHost();
    bool hasHostClient() const { return !hostSocket_.isNull(); }
    QString hostPeer() const;

    // Controller-side: connect to a host.
    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    ControllerState controllerState() const { return controllerState_; }

    // Send framed JSON. Returns false on missing socket.
    bool sendToController(const QJsonObject &obj);
    bool sendToHost(const QJsonObject &obj);

  signals:
    void hostClientChanged(bool connected, const QString &reason);
    void hostCommandArrived(const QJsonObject &payload); // controller -> host
    void controllerEventArrived(const QJsonObject &payload); // host -> controller
    void controllerStateChanged(ControlChannel::ControllerState state, const QString &reason);

  private slots:
    void handleNewConnection();
    void handleHostReadyRead();
    void handleHostDisconnected();
    void handleControllerReadyRead();
    void handleControllerConnected();
    void handleControllerDisconnected();
    void handleSocketError(QAbstractSocket::SocketError error);

  private:
    void processBuffer(QTcpSocket *sock, QByteArray &buffer, const std::function<void(const QJsonObject &)> &handler);
    bool writeFrame(QTcpSocket *sock, const QJsonObject &obj);
    void closeHostSocket(const QString &reason);

    QTcpServer *server_ = nullptr;
    QPointer<QTcpSocket> hostSocket_;
    QByteArray hostBuffer_;

    QPointer<QTcpSocket> controllerSocket_;
    QByteArray controllerBuffer_;
    ControllerState controllerState_ = ControllerState::Idle;
};

#endif // CONTROLCHANNEL_H
