#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

#include <QJsonObject>
#include <QJsonArray>
#include <functional>

/// LAN collab transport: the host runs a QTcpServer, guests connect, and every
/// message is a length-prefixed JSON frame. Host is the only authority: guests
/// send their ops to it, it applies arrival order and relays to everyone.
///
/// Everything runs on the GUI thread on purpose - the traffic is one op per
/// pen sample from a handful of people, which the event loop absorbs without
/// noticing, and a second thread around the document is exactly the race the
/// AGENTS.md "ask first" list warns about.
///
/// ponytail: no security (anyone on the Wi-Fi who knows the address joins),
/// no reconnect, no host migration - the room dies with the host. Add auth
/// and relay when collab leaves the trusted-LAN scope.
class CollabSession : public QObject {
    Q_OBJECT
public:
    enum class Role { Offline, Host, Guest };
    Q_ENUM(Role)

    explicit CollabSession(QObject* parent = nullptr);

    /// Hosts a room on \p port (0 = pick one). The document currently open in
    /// the controller is the shared document.
    bool host(quint16 port, const QString& userName);
    /// Connects to "host[:port]". On success the controller receives the
    /// host's document as a snapshot (see snapshotReceived).
    bool connectToHost(const QString& address, const QString& userName);
    void leave();

    Role role() const { return m_role; }
    /// This member's id: 0 for the host, 1.. for guests.
    int userId() const { return m_userId; }
    /// What the other side needs to type, e.g. "192.168.1.5:48291".
    QString roomAddress() const { return m_roomAddress; }
    int peerCount() const { return m_peerCount; }
    bool active() const { return m_role != Role::Offline; }

    /// A finished stroke, serialised. Routed through the host, which relays it
    /// (and applies it itself) in arrival order.
    void sendStroke(const QJsonObject& op);
    /// Remote pointer position, throttled by the caller.
    void sendCursor(double x, double y);
    /// Guest side, after the snapshot was loaded: strokes that raced the load
    /// are replayed in order.
    void snapshotLoaded();

    /// The host answers "hello" with the current document; the controller sets
    /// this (it owns ProjectIO). One callback, no document access in here.
    std::function<QByteArray()> provideSnapshot;

signals:
    void stateChanged();                       // role / peer count / address
    void failed(const QString& message);       // join refused, socket dropped
    void strokeReceived(const QJsonObject& op); // to apply locally
    void snapshotReceived(const QByteArray& projectBytes, const QJsonObject& welcome);
    void cursorsUpdated(const QJsonObject& cursors); // {"id": {x,y,name,color}, ...}

private slots:
    void onHostConnection();
    void onSocketReady();
    void onSocketGone();

private:
    void wireSocket(QTcpSocket* socket);
    void sendFrame(QTcpSocket* socket, const QJsonObject& message);
    void handleFrame(QTcpSocket* socket, const QJsonObject& message);
    void broadcast(const QJsonObject& message, QTcpSocket* except = nullptr);
    void setPeerCount(int count);
    QString assignColor(int userId) const;
    /// Folds a peer list (names + colours) into the cursor map, so a pointer
    /// arriving as bare coordinates still knows whose it is and what colour.
    void mergePeers(const QJsonArray& peers);

    QTcpServer m_server;
    // Host: every guest socket. Guest: the single connection to the host.
    QList<QTcpSocket*> m_sockets;
    // Per-socket frame reader: two guests sending at once must not share bytes.
    QHash<QTcpSocket*, QByteArray> m_rx;
    QHash<QTcpSocket*, quint32> m_rxLen;
    QList<QJsonObject> m_pendingOps; // guest: strokes that raced the snapshot
    QJsonObject m_cursors;           // "userId" -> {x, y, name, color}
    Role m_role = Role::Offline;
    QString m_userName;
    QString m_roomAddress;
    int m_userId = 0;             // host is 0, guests get 1, 2, ...
    int m_nextUserId = 1;
    int m_peerCount = 0;
    bool m_snapshotPending = false; // guest: drop ops until the snapshot loaded
};
