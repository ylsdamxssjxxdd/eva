// singleinstance.h - Lightweight single-instance guard using QLocalServer
// Ensures only one instance (per app path) runs; subsequent launches
// signal the primary to raise/activate the main window.

#ifndef SINGLEINSTANCE_H
#define SINGLEINSTANCE_H

#include <QObject>
#include <QString>

class QLocalServer;

// SingleInstance provides a small cross‑platform guard based on QLocalServer.
// Key design points:
// - Uniqueness is per absolute app path (hash), so copies in different folders
//   may run independently as requested.
// - Secondary instances just connect to the existing server and send a small
//   activation message ("ACTIVATE"), then exit.
// - Primary instance emits `received` when a secondary pings it, so the UI can
//   bring the main window to front.
class SingleInstance : public QObject
{
    Q_OBJECT
  public:
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);

    // Try to become the primary instance. Returns true if we now listen.
    bool ensurePrimary();

    // Returns true if this process is the primary (listening) instance.
    bool isPrimary() const noexcept { return server_ != nullptr; }

    // If another instance is already running, send it a short message.
    // Returns true on successful delivery.
    bool notifyRunning(const QByteArray &message = QByteArray("ACTIVATE"));

    // Compose an IPC key from an application file path; stable and unique per path.
    static QString keyForAppPath(const QString &appPath);

  signals:
    void received(const QByteArray &message);

  private slots:
    void onNewConnection();

  private:
    QString key_;
    QLocalServer *server_ = nullptr; // created only in primary instance
};

#endif // SINGLEINSTANCE_H

