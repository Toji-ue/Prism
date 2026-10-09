#include "application/CollabSession.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>

namespace {

constexpr quint16 kDefaultPort = 48291;
constexpr quint32 kMaxFrameBytes = 256 * 1024 * 1024; // a snapshot can be large

/// First LAN-looking IPv4 of this machine, so the host dialog has something to
/// show. A home/office address (192.168/10/172.16) is the one people can
/// actually type; 169.254.x is a link-local "no DHCP" address, and loopback is
/// the last resort for two Prism instances on one PC.
QString lanAddress()
{
    static const QList<QPair<QHostAddress, int>> kPrivate = {
        QHostAddress::parseSubnet(QStringLiteral("192.168.0.0/16")),
        QHostAddress::parseSubnet(QStringLiteral("10.0.0.0/8")),
        QHostAddress::parseSubnet(QStringLiteral("172.16.0.0/12")),
    };
    QString linkLocal;
    QString other;
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress& address : addresses) {
        if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isLoopback())
            continue;
        for (const auto& subnet : kPrivate) {
            if (address.isInSubnet(subnet))
                return address.toString();
        }
        if (address.toString().startsWith(QLatin1String("169.254."))) {
            if (linkLocal.isEmpty())
                linkLocal = address.toString();
        } else if (other.isEmpty()) {
            other = address.toString();
        }
    }
    if (!other.isEmpty())
        return other;
    return linkLocal.isEmpty() ? QStringLiteral("127.0.0.1") : linkLocal;
}

QByteArray packFrame(const QJsonObject& message)
{
    const QByteArray payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QByteArray frame;
    frame.reserve(payload.size() + 4);
    frame.append(char((payload.size() >> 24) & 0xFF));
    frame.append(char((payload.size() >> 16) & 0xFF));
    frame.append(char((payload.size() >> 8) & 0xFF));
    frame.append(char(payload.size() & 0xFF));
    frame.append(payload);
    return frame;
}

} // namespace

CollabSession::CollabSession(QObject* parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &CollabSession::onHostConnection);
}

bool CollabSession::host(quint16 port, const QString& userName)
{
    leave();
    if (!m_server.listen(QHostAddress::AnyIPv4, port))
        return false;
    m_role = Role::Host;
    m_userName = userName.isEmpty() ? QStringLiteral("Host") : userName;
    m_userId = 0;
    m_nextUserId = 1;
    m_roomAddress = QStringLiteral("%1:%2").arg(lanAddress()).arg(m_server.serverPort());
    setPeerCount(0);
    emit stateChanged();
    return true;
}

bool CollabSession::connectToHost(const QString& address, const QString& userName)
{
    leave();
    const QString host = address.section(QLatin1Char(':'), 0, 0);
    bool ok = false;
    const quint16 port = static_cast<quint16>(address.section(QLatin1Char(':'), 1).toUInt(&ok));
    if (host.isEmpty() || !ok || port == 0) {
        emit failed(QStringLiteral("The room address must look like 192.168.1.5:%1")
                        .arg(kDefaultPort));
        return false;
    }
    m_role = Role::Guest;
    m_userName = userName;
    m_snapshotPending = true;
    auto* socket = new QTcpSocket(this);
    m_sockets = { socket };
    wireSocket(socket);
    // Deliberately no waitForConnected: a typo in the address must not freeze
    // the window for the connect timeout. The answer - joined, or cannot reach
    // - arrives on the event loop.
    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        sendFrame(socket, { { "t", "hello" }, { "name", m_userName } });
        setPeerCount(1);
        emit stateChanged();
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit failed(QStringLiteral("Cannot reach that room - check the address, and that both "
                                   "machines are on the same network."));
        leave();
    });
    socket->connectToHost(host, port);
    emit stateChanged();
    return true;
}

void CollabSession::leave()
{
    if (m_role == Role::Offline && m_sockets.isEmpty() && !m_server.isListening())
        return;
    for (QTcpSocket* socket : m_sockets) {
        // Drop the handlers first: leaving on purpose must not come back as
        // "lost the connection to the room" (the socket does disconnect).
        socket->disconnect(this);
        socket->disconnectFromHost();
        socket->deleteLater();
    }
    m_sockets.clear();
    m_rx.clear();
    m_rxLen.clear();
    m_server.close();
    m_role = Role::Offline;
    m_snapshotPending = false;
    m_pendingOps.clear();
    m_cursors = {};
    m_roomAddress.clear();
    setPeerCount(0);
    emit stateChanged();
}

void CollabSession::sendStroke(const QJsonObject& op)
{
    if (!active())
        return;
    const QJsonObject full = op;
    if (m_role == Role::Host) {
        // The host painted this stroke locally already: relay only.
        broadcast(full);
    } else {
        sendFrame(m_sockets.value(0), full);
    }
}

void CollabSession::sendCursor(double x, double y)
{
    if (!active())
        return;
    const QJsonObject op = { { "t", "cursor" }, { "u", m_userId }, { "x", x }, { "y", y } };
    if (m_role == Role::Host) {
        QJsonObject cursor = m_cursors.value(QString::number(m_userId)).toObject();
        cursor.insert("x", x);
        cursor.insert("y", y);
        cursor.insert("name", m_userName);
        cursor.insert("color", assignColor(m_userId));
        QJsonObject all = m_cursors;
        all.insert(QString::number(m_userId), cursor);
        m_cursors = all;
        broadcast(op);
        emit cursorsUpdated(m_cursors);
    } else {
        sendFrame(m_sockets.value(0), op);
    }
}

void CollabSession::onHostConnection()
{
    while (auto* socket = m_server.nextPendingConnection()) {
        m_sockets.append(socket);
        wireSocket(socket);
    }
}

void CollabSession::wireSocket(QTcpSocket* socket)
{
    connect(socket, &QTcpSocket::readyRead, this, &CollabSession::onSocketReady);
    connect(socket, &QTcpSocket::disconnected, this, &CollabSession::onSocketGone);
}

void CollabSession::onSocketReady()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;
    QByteArray& buffer = m_rx[socket];
    quint32& expected = m_rxLen[socket];
    buffer.append(socket->readAll());
    while (true) {
        if (expected == 0) {
            if (buffer.size() < 4)
                return;
            expected = (quint32(quint8(buffer[0])) << 24) | (quint32(quint8(buffer[1])) << 16)
                       | (quint32(quint8(buffer[2])) << 8) | quint32(quint8(buffer[3]));
            buffer.remove(0, 4);
            if (expected == 0 || expected > kMaxFrameBytes) {
                emit failed(QStringLiteral("Bad collab frame length (%1)").arg(expected));
                socket->disconnectFromHost();
                return;
            }
        }
        if (static_cast<quint32>(buffer.size()) < expected)
            return;
        const QJsonObject message =
            QJsonDocument::fromJson(buffer.left(static_cast<int>(expected))).object();
        buffer.remove(0, static_cast<int>(expected));
        expected = 0;
        handleFrame(socket, message);
    }
}

void CollabSession::onSocketGone()
{
    auto* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket)
        return;
    if (m_role == Role::Host) {
        const int id = socket->property("userId").toInt();
        m_sockets.removeOne(socket);
        m_rx.remove(socket);
        m_rxLen.remove(socket);
        socket->deleteLater();
        QJsonObject all = m_cursors;
        all.remove(QString::number(id));
        m_cursors = all;
        // Tell everyone who is left.
        QJsonArray peers;
        peers.append(QJsonObject{ { "id", 0 }, { "name", m_userName } });
        for (QTcpSocket* peer : m_sockets) {
            peers.append(QJsonObject{ { "id", peer->property("userId").toInt() },
                                       { "name", peer->property("userName").toString() } });
        }
        broadcast({ { "t", "peers" }, { "peers", peers } });
        emit cursorsUpdated(m_cursors);
        setPeerCount(m_sockets.size());
    } else {
        emit failed(QStringLiteral("Lost the connection to the room"));
        leave();
    }
    emit stateChanged();
}

void CollabSession::sendFrame(QTcpSocket* socket, const QJsonObject& message)
{
    if (socket && socket->state() == QAbstractSocket::ConnectedState)
        socket->write(packFrame(message));
}

void CollabSession::broadcast(const QJsonObject& message, QTcpSocket* except)
{
    for (QTcpSocket* socket : m_sockets) {
        if (socket != except)
            sendFrame(socket, message);
    }
}

void CollabSession::handleFrame(QTcpSocket* socket, const QJsonObject& message)
{
    const QString type = message.value(QLatin1String("t")).toString();
    if (type == QLatin1String("hello") && m_role == Role::Host) {
        const int id = m_nextUserId++;
        const QString name = message.value(QLatin1String("name")).toString();
        socket->setProperty("userId", id);
        socket->setProperty("userName", name);
        // The host never receives a peer list of its own room, so the newcomer
        // is added here: without it their pointer arrives as coordinates with
        // no name and no colour and draws as a black blob.
        QJsonObject all = m_cursors;
        all.insert(QString::number(id),
                   QJsonObject{ { "name", name }, { "color", assignColor(id) } });
        m_cursors = all;
        emit cursorsUpdated(m_cursors);
        // The document the guests paint in is the host's, snapshot and all.
        const QByteArray doc = provideSnapshot ? provideSnapshot() : QByteArray();
        sendFrame(socket, { { "t", "welcome" },
                            { "id", id },
                            { "doc", QString::fromLatin1(doc.toBase64()) },
                            { "peers", QJsonArray{ QJsonObject{ { "id", 0 },
                                                                 { "name", m_userName } } } } });
        QJsonArray peers;
        peers.append(QJsonObject{ { "id", 0 }, { "name", m_userName } });
        for (QTcpSocket* peer : m_sockets) {
            peers.append(QJsonObject{ { "id", peer->property("userId").toInt() },
                                       { "name", peer->property("userName").toString() } });
        }
        broadcast({ { "t", "peers" }, { "peers", peers } }, socket);
        setPeerCount(m_sockets.size());
        emit stateChanged();
        return;
    }
    if (type == QLatin1String("welcome") && m_role == Role::Guest) {
        m_userId = message.value(QLatin1String("id")).toInt();
        m_snapshotPending = true;
        // The peer list rides along with the welcome, so the host's pointer has
        // a name and a colour from the first move.
        mergePeers(message.value(QLatin1String("peers")).toArray());
        emit snapshotReceived(QByteArray::fromBase64(
                                  message.value(QLatin1String("doc")).toString().toLatin1()),
                              message);
        return;
    }
    if (type == QLatin1String("peers")) {
        mergePeers(message.value(QLatin1String("peers")).toArray());
        return;
    }
    if (type == QLatin1String("cursor")) {
        const QString key = QString::number(message.value(QLatin1String("u")).toInt());
        QJsonObject all = m_cursors;
        QJsonObject cursor = all.value(key).toObject();
        cursor.insert("x", message.value(QLatin1String("x")));
        cursor.insert("y", message.value(QLatin1String("y")));
        all.insert(key, cursor);
        m_cursors = all;
        if (m_role == Role::Host)
            broadcast(message, socket);
        emit cursorsUpdated(m_cursors);
        return;
    }
    if (type == QLatin1String("stroke")) {
        if (m_snapshotPending) {
            // Ops raced the snapshot load: keep them, replay in order once the
            // new document is in.
            m_pendingOps.append(message);
            return;
        }
        if (m_role == Role::Host) {
            // The sender already painted it; everyone else has not.
            broadcast(message, socket);
        } else {
            // Relayed by the host already, nothing to forward.
        }
        emit strokeReceived(message);
        return;
    }
}

void CollabSession::snapshotLoaded()
{
    if (m_role != Role::Guest || !m_snapshotPending)
        return;
    m_snapshotPending = false;
    const QList<QJsonObject> pending = m_pendingOps;
    m_pendingOps.clear();
    for (const QJsonObject& op : pending)
        emit strokeReceived(op);
}

void CollabSession::setPeerCount(int count)
{
    if (m_peerCount == count)
        return;
    m_peerCount = count;
    emit stateChanged();
}

void CollabSession::mergePeers(const QJsonArray& peers)
{
    QJsonObject all = m_cursors;
    QStringList live;
    for (const QJsonValue& value : peers) {
        const QJsonObject peer = value.toObject();
        const int id = peer.value(QLatin1String("id")).toInt();
        const QString key = QString::number(id);
        live.append(key);
        QJsonObject cursor = all.value(key).toObject();
        cursor.insert("name", peer.value(QLatin1String("name")));
        cursor.insert("color", assignColor(id));
        all.insert(key, cursor);
    }
    // Someone who left takes their pointer with them.
    for (const QString& key : all.keys())
        if (!live.contains(key))
            all.remove(key);
    m_cursors = all;
    emit cursorsUpdated(m_cursors);
}

QString CollabSession::assignColor(int userId) const
{
    static const char* const kPalette[] = { "#e0533d", "#3d9fe0", "#59b75c", "#c774d8",
                                            "#d8a83d", "#3dd8c0", "#e03da4", "#8a9fe0" };
    return QString::fromLatin1(kPalette[static_cast<std::size_t>(userId) % 8]);
}
