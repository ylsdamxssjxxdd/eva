#ifndef LOCALPROXY_H
#define LOCALPROXY_H

#include <QObject>
#include <QScopedPointer>
#include <QTcpServer>
#include <QList>
#include <QString>

// Lightweight TCP proxy that keeps the user-facing port alive while
// lazily starting or stopping the real llama.cpp backend.
class LocalProxyServer : public QObject
{
    Q_OBJECT

  public:
    explicit LocalProxyServer(QObject *parent = nullptr);
    ~LocalProxyServer() override;

    bool start(const QString &host, quint16 port, QString *errorMessage = nullptr);
    void stop();

    bool isListening() const;
    QString listenHost() const;
    quint16 listenPort() const;

    void setBackendEndpoint(const QString &host, quint16 port);
    QString backendHost() const;
    quint16 backendPort() const;

    void setBackendAvailable(bool ready);
    bool backendAvailable() const;

    void shutdownSessions(const QString &reason);

  signals:
    void wakeRequested();
    void externalActivity();
    void proxyError(const QString &message);

  private slots:
    void onNewConnection();
    void onSessionFinished(QObject *sessionObj);
    void onSessionActivity();

  private:
    class ProxySession;

    void requestWakeIfNeeded();
    void attachSession(ProxySession *session);

    QScopedPointer<QTcpServer> server_;
    QString listenHost_;
    quint16 listenPort_ = 0;

    QString backendHost_ = QStringLiteral("127.0.0.1");
    quint16 backendPort_ = 0;
    bool backendReady_ = false;
    bool wakePending_ = false;

    QList<ProxySession *> sessions_;
    QList<ProxySession *> pendingSessions_;
};

#endif // LOCALPROXY_H
