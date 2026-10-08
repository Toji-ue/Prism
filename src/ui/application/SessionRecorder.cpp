#include "application/SessionRecorder.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <QVariant>

namespace {

constexpr int kRecordVersion = 1;

QByteArray pngBytes(const QImage& image)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

} // namespace

SessionRecorder::SessionRecorder(QObject* parent)
    : QObject(parent)
{
    restart();
}

void SessionRecorder::restart()
{
    m_frames.clear();
    m_lastCaptureMs = -100000;
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_clock.start();
}

void SessionRecorder::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    if (active) {
        // Only the interval matters; the session keeps its frames, so toggling
        // recording off and on does not lose the timelapse.
        m_clock.restart();
        m_lastCaptureMs = -100000;
    }
}

QString SessionRecorder::hashParts(const QString& previous, qint64 ms, const QByteArray& png)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(previous.toUtf8());
    hash.addData(QByteArray::number(ms));
    hash.addData(png);
    return QString::fromLatin1(hash.result().toHex());
}

QString SessionRecorder::hashFrame(const Frame& frame)
{
    return hashParts(frame.previousHash, frame.ms, frame.png);
}

void SessionRecorder::capture(const QImage& canvas, const QString& label, bool force)
{
    if (!m_active || canvas.isNull())
        return;
    if (isFull()) {
        if (!force) {
            m_active = false;
            emit full();
            return;
        }
        // Full buffer, but the artist is exporting right now: the file has to
        // end with what is on the canvas at this moment, so the newest stored
        // frame gives way to it instead of the export stopping mid-drawing.
        // Dropping the last frame keeps the hash chain intact - each frame only
        // hashes the one before it.
        m_frames.removeLast();
    }
    const qint64 now = m_clock.isValid() ? m_clock.elapsed() : 0;
    if (!force && now - m_lastCaptureMs < kMinIntervalMs)
        return;

    QImage frame = canvas;
    if (frame.width() > kMaxFrameSide || frame.height() > kMaxFrameSide)
        frame = frame.scaled(kMaxFrameSide, kMaxFrameSide, Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);

    Frame stored;
    stored.ms = now;
    stored.label = label;
    stored.png = pngBytes(frame);
    if (stored.png.isEmpty())
        return;
    stored.previousHash = m_frames.isEmpty() ? QString() : m_frames.last().hash;
    stored.hash = hashFrame(stored);
    m_frames.append(stored);
    m_lastCaptureMs = now;
    emit frameStored();
}

bool SessionRecorder::frameAt(int index, QImage& out, qint64& ms) const
{
    if (index < 0 || index >= m_frames.size())
        return false;
    const Frame& frame = m_frames.at(index);
    if (!out.loadFromData(frame.png, "PNG"))
        return false;
    ms = frame.ms;
    return true;
}

RecordedFrames SessionRecorder::recordedFrames() const
{
    RecordedFrames source;
    source.count = m_frames.size();
    source.fetch = [this](int index, QImage& out, qint64& ms) {
        return frameAt(index, out, ms);
    };
    return source;
}

bool SessionRecorder::save(const QString& path, const QSize& canvasSize, QString* error) const
{
    if (m_frames.isEmpty()) {
        if (error)
            *error = tr("Nothing recorded yet - draw something first.");
        return false;
    }

    QJsonObject root;
    root["format"] = QStringLiteral("prism-record");
    root["version"] = kRecordVersion;
    root["session"] = m_sessionId;
    root["canvasWidth"] = canvasSize.width();
    root["canvasHeight"] = canvasSize.height();
    root["class"] = QStringLiteral("hand-drawn in Prism");

    QJsonArray frames;
    for (const Frame& frame : m_frames) {
        QJsonObject entry;
        entry["ms"] = frame.ms;
        entry["label"] = frame.label;
        entry["hash"] = frame.hash;
        entry["previousHash"] = frame.previousHash;
        entry["png"] = QString::fromLatin1(frame.png.toBase64());
        frames.append(entry);
    }
    root["frames"] = frames;

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = tr("Could not write %1.").arg(path);
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit()) {
        if (error)
            *error = tr("Could not write %1.").arg(path);
        return false;
    }
    return true;
}

bool SessionRecorder::verify(const QString& path, QString* error, int* frames)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = tr("Could not read %1.").arg(path);
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        if (error)
            *error = tr("Not a Prism record file.");
        return false;
    }
    const QJsonObject root = doc.object();
    if (root["format"].toString() != QLatin1String("prism-record")) {
        if (error)
            *error = tr("Not a Prism record file.");
        return false;
    }
    if (root["version"].toInt() != kRecordVersion) {
        if (error)
            *error = tr("Record version %1 is not supported.").arg(root["version"].toInt());
        return false;
    }

    QString previous;
    int index = 0;
    for (const QJsonValue& value : root["frames"].toArray()) {
        const QJsonObject entry = value.toObject();
        const QByteArray png = QByteArray::fromBase64(entry["png"].toString().toLatin1());
        if (png.isEmpty()) {
            if (error)
                *error = tr("Frame %1 has no image data.").arg(index);
            return false;
        }
        if (entry["previousHash"].toString() != previous) {
            if (error)
                *error = tr("Frame %1 does not follow the previous one.").arg(index);
            return false;
        }
        const QString expected =
            hashParts(previous, static_cast<qint64>(entry["ms"].toDouble()), png);
        if (entry["hash"].toString() != expected) {
            if (error)
                *error = tr("Frame %1 does not match its hash - the file was edited.").arg(index);
            return false;
        }
        previous = expected;
        ++index;
    }
    if (index == 0) {
        if (error)
            *error = tr("The record has no frames.");
        return false;
    }
    if (frames)
        *frames = index;
    return true;
}
