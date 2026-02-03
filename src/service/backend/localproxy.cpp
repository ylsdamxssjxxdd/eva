#include "service/backend/localproxy.h"

#include <QAbstractSocket>
#include <QHostAddress>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>
#include <QtGlobal>

namespace
{
QHostAddress resolveHost(const QString &host)
{
    if (host.isEmpty())
        return QHostAddress(QHostAddress::Any);
    QHostAddress addr;
    if (addr.setAddress(host))
        return addr;
    return QHostAddress(QHostAddress::Any);
}

QByteArray http503Response(const QString &reason)
{
    const QByteArray body = QStringLiteral(R"({"error":"%1"})").arg(reason).toUtf8();
    QByteArray response("HTTP/1.1 503 Service Unavailable\r\n"
                        "Content-Type: application/json\r\n"
                        "Connection: close\r\n"
                        "Content-Length: ");
    response += QByteArray::number(body.size());
    response += QByteArrayLiteral("\r\n\r\n");
    response += body;
    return response;
}
} // namespace

class LocalProxyServer::ProxySession : public QObject
{
    Q_OBJECT

  public:
    ProxySession(LocalProxyServer *owner, QTcpSocket *client);
    ~ProxySession() override;

    void setBackendEndpoint(const QString &host, quint16 port);
    void start(bool backendReady);
    void handleBackendReady();
    void handleBackendDown(const QString &reason);

  signals:
    void finished(QObject *self);
    void activity();

  private slots:
    void onClientReadyRead();
    void onClientDisconnected();
    void onBackendReadyRead();
    void onBackendConnected();
    void onBackendDisconnected();
    void onBackendError(QAbstractSocket::SocketError);
    void onWaitTimeout();

  private:
    void ensureBackendSocket();
    void flushPendingClientData();
    void send503AndClose(const QString &reason);
    void closeSockets();

    QPointer<QTcpSocket> client_;
    QPointer<QTcpSocket> backend_;
    QString backendHost_;
    quint16 backendPort_ = 0;
    bool backendConnected_ = false;
    bool waitingForBackend_ = false;
    bool responseSent_ = false;
    QByteArray pendingClientData_;
    QTimer waitTimer_;
};

LocalProxyServer::ProxySession::ProxySession(LocalProxyServer *owner, QTcpSocket *client)
    : QObject(owner), client_(client)
{
    if (client_)
    {
        client_->setParent(this);
        client_->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        connect(client_, &QTcpSocket::readyRead, this, &ProxySession::onClientReadyRead);
        connect(client_, &QTcpSocket::disconnected, this, &ProxySession::onClientDisconnected);
    }

    waitTimer_.setParent(this);
    waitTimer_.setSingleShot(true);
    connect(&waitTimer_, &QTimer::timeout, this, &ProxySession::onWaitTimeout);
}

LocalProxyServer::ProxySession::~ProxySession()
{
    closeSockets();
}

void LocalProxyServer::ProxySession::setBackendEndpoint(const QString &host, quint16 port)
{
    backendHost_ = host;
    backendPort_ = port;
    if (backend_ && backend_->state() == QAbstractSocket::ConnectedState)
    {
        if (backend_->peerPort() != port || backend_->peerName() != host)
        {
            backend_->disconnectFromHost();
        }
    }
}

void LocalProxyServer::ProxySession::start(bool backendReady)
{
    waitingForBackend_ = !backendReady;
    if (backendReady)
    {
        ensureBackendSocket();
    }
    else
    {
        waitTimer_.start(30000); // wait up to 30s for backend
    }
}

void LocalProxyServer::ProxySession::handleBackendReady()
{
    if (!client_ || client_->state() == QAbstractSocket::UnconnectedState)
        return;
    if (backendConnected_ || !waitingForBackend_)
        return;
    waitingForBackend_ = false;
    ensureBackendSocket();
}

void LocalProxyServer::ProxySession::handleBackendDown(const QString &reason)
{
    send503AndClose(reason);
}

void LocalProxyServer::ProxySession::ensureBackendSocket()
{
    if (backend_)
    {
        if (backend_->state() == QAbstractSocket::ConnectedState)
        {
            flushPendingClientData();
            return;
        }
        backend_->deleteLater();
    }
    backend_ = new QTcpSocket(this);
    backend_->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    connect(backend_, &QTcpSocket::readyRead, this, &ProxySession::onBackendReadyRead);
    connect(backend_, &QTcpSocket::connected, this, &ProxySession::onBackendConnected);
    connect(backend_, &QTcpSocket::disconnected, this, &ProxySession::onBackendDisconnected);
    connect(backend_, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &ProxySession::onBackendError);
    backend_->connectToHost(backendHost_, backendPort_);
    waitTimer_.start(15000); // backend connect timeout
}

void LocalProxyServer::ProxySession::flushPendingClientData()
{
    if (!backend_ || backend_->state() != QAbstractSocket::ConnectedState)
        return;
    if (!pendingClientData_.isEmpty())
    {
        backend_->write(pendingClientData_);
        pendingClientData_.clear();
        backend_->flush();
    }
}

void LocalProxyServer::ProxySession::send503AndClose(const QString &reason)
{
    if (responseSent_)
        return;
    responseSent_ = true;
    if (client_ && client_->state() == QAbstractSocket::ConnectedState)
    {
        const QByteArray payload = http503Response(reason);
        client_->write(payload);
        client_->flush();
        client_->disconnectFromHost();
    }
    closeSockets();
    emit finished(this);
}

void LocalProxyServer::ProxySession::closeSockets()
{
    if (backend_)
    {
        backend_->disconnect(this);
        backend_->disconnectFromHost();
        backend_->deleteLater();
        backend_ = nullptr;
    }
    if (client_)
    {
        client_->disconnect(this);
        if (client_->state() != QAbstractSocket::UnconnectedState)
            client_->disconnectFromHost();
        client_->deleteLater();
        client_ = nullptr;
    }
}

void LocalProxyServer::ProxySession::onClientReadyRead()
{
    if (!client_)
        return;
    const QByteArray data = client_->readAll();
    if (data.isEmpty())
        return;
    emit activity();
    if (backendConnected_)
    {
        if (backend_)
            backend_->write(data);
    }
    else
    {
        pendingClientData_.append(data);
    }
}

void LocalProxyServer::ProxySession::onClientDisconnected()
{
    closeSockets();
    emit finished(this);
}

void LocalProxyServer::ProxySession::onBackendReadyRead()
{
    if (!backend_ || !client_)
        return;
    const QByteArray data = backend_->readAll();
    if (data.isEmpty())
        return;
    emit activity();
    client_->write(data);
}

void LocalProxyServer::ProxySession::onBackendConnected()
{
    backendConnected_ = true;
    waitTimer_.stop();
    flushPendingClientData();
}

void LocalProxyServer::ProxySession::onBackendDisconnected()
{
    backendConnected_ = false;
    if (!client_ || client_->state() == QAbstractSocket::UnconnectedState)
    {
        closeSockets();
        emit finished(this);
        return;
    }
    send503AndClose(QStringLiteral("backend disconnected"));
}

void LocalProxyServer::ProxySession::onBackendError(QAbstractSocket::SocketError)
{
    if (backendConnected_)
    {
        send503AndClose(QStringLiteral("backend error"));
    }
    else
    {
        send503AndClose(QStringLiteral("backend unavailable"));
    }
}

void LocalProxyServer::ProxySession::onWaitTimeout()
{
    if (!backendConnected_)
    {
        send503AndClose(QStringLiteral("backend timeout"));
    }
}

LocalProxyServer::LocalProxyServer(QObject *parent)
    : QObject(parent), server_(new QTcpServer(this))
{
    connect(server_.data(), &QTcpServer::newConnection, this, &LocalProxyServer::onNewConnection);
}

LocalProxyServer::~LocalProxyServer()
{
    stop();
}

bool LocalProxyServer::start(const QString &host, quint16 port, QString *errorMessage)
{
    if (isListening())
    {
        if (listenHost_ == host && listenPort_ == port)
            return true;
        stop();
    }

    const QHostAddress addr = resolveHost(host);
    if (!server_->listen(addr, port))
    {
        const QString err = server_->errorString();
        if (errorMessage)
            *errorMessage = err;
        emit proxyError(err);
        server_->close();
        return false;
    }

    listenHost_ = host;
    listenPort_ = server_->serverPort();
    return true;
}

void LocalProxyServer::stop()
{
    if (server_)
        server_->close();
    shutdownSessions(QStringLiteral("proxy stopped"));
    listenHost_.clear();
    listenPort_ = 0;
}

bool LocalProxyServer::isListening() const
{
    return server_ && server_->isListening();
}

QString LocalProxyServer::listenHost() const
{
    return listenHost_;
}

quint16 LocalProxyServer::listenPort() const
{
    return listenPort_;
}

void LocalProxyServer::setBackendEndpoint(const QString &host, quint16 port)
{
    backendHost_ = host;
    backendPort_ = port;
    for (ProxySession *session : qAsConst(sessions_))
    {
        if (session)
            session->setBackendEndpoint(host, port);
    }
}

QString LocalProxyServer::backendHost() const
{
    return backendHost_;
}

quint16 LocalProxyServer::backendPort() const
{
    return backendPort_;
}

void LocalProxyServer::setBackendAvailable(bool ready)
{
    if (backendReady_ == ready)
        return;

    backendReady_ = ready;
    if (ready)
    {
        wakePending_ = false;
        const auto pending = pendingSessions_;
        pendingSessions_.clear();
        for (ProxySession *session : pending)
        {
            if (session)
                session->handleBackendReady();
        }
        return;
    }

    wakePending_ = false;
    for (ProxySession *session : qAsConst(sessions_))
    {
        if (!session) continue;
        if (!pendingSessions_.contains(session))
            pendingSessions_.append(session);
    }
}

bool LocalProxyServer::backendAvailable() const
{
    return backendReady_;
}

void LocalProxyServer::shutdownSessions(const QString &reason)
{
    const auto copy = sessions_;
    for (ProxySession *session : copy)
    {
        if (session)
            session->handleBackendDown(reason);
    }
    sessions_.clear();
    pendingSessions_.clear();
}

void LocalProxyServer::onNewConnection()
{
    while (server_->hasPendingConnections())
    {
        QTcpSocket *client = server_->nextPendingConnection();
        if (!client)
            continue;
        auto *session = new ProxySession(this, client);
        session->setBackendEndpoint(backendHost_, backendPort_);
        connect(session, &ProxySession::finished, this, &LocalProxyServer::onSessionFinished);
        connect(session, &ProxySession::activity, this, &LocalProxyServer::onSessionActivity);
        attachSession(session);
        if (backendReady_)
        {
            session->start(true);
        }
        else
        {
            if (!pendingSessions_.contains(session))
                pendingSessions_.append(session);
            session->start(false);
            requestWakeIfNeeded();
        }
    }
}

void LocalProxyServer::onSessionFinished(QObject *sessionObj)
{
    if (!sessionObj)
        return;
    auto *session = static_cast<ProxySession *>(sessionObj);
    sessions_.removeAll(session);
    pendingSessions_.removeAll(session);
    session->deleteLater();
}

void LocalProxyServer::onSessionActivity()
{
    emit externalActivity();
}

void LocalProxyServer::requestWakeIfNeeded()
{
    if (backendReady_ || wakePending_)
        return;
    wakePending_ = true;
    emit wakeRequested();
}

void LocalProxyServer::attachSession(ProxySession *session)
{
    if (!session)
        return;
    if (!sessions_.contains(session))
        sessions_.append(session);
}

#include "localproxy.moc"
