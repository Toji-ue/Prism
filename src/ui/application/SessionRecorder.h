#pragma once

#include "application/RecordExport.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QImage>
#include <QList>
#include <QObject>
#include <QString>

/// Records the drawing session so the artist can hand out a proof of process.
///
/// Frames are the flattened canvas, captured at most every kMinIntervalMs, so a
/// timelapse shows the picture being built stroke by stroke. Each frame is
/// hashed together with the previous frame's hash, which makes the exported file
/// tamper-evident: editing or dropping a frame breaks the chain, and `verify()`
/// reports where.
///
/// What it does NOT do: prove who drew the artwork. It proves that the file it
/// ships is the same sequence this app wrote, given that the file itself was
/// published as it was. Say that plainly when handing it to a client.
class SessionRecorder : public QObject {
    Q_OBJECT

public:
    /// One frame per interval: a stroke lasting a second still shows as it grows,
    /// and a three-hour session stays inside the frame budget.
    static constexpr int kMinIntervalMs = 1500;
    /// Long side of a stored frame. Small enough for a shareable file, big
    /// enough to read the drawing.
    static constexpr int kMaxFrameSide = 720;
    /// ponytail: hard cap, ~12 MB of PNGs at the sizes seen so far. Recording
    /// stops there and says so; streaming to disk is the upgrade path.
    static constexpr int kMaxFrames = 400;

    explicit SessionRecorder(QObject* parent = nullptr);

    [[nodiscard]] bool isActive() const noexcept { return m_active; }
    void setActive(bool active);
    [[nodiscard]] int frameCount() const noexcept { return m_frames.size(); }
    [[nodiscard]] bool isFull() const noexcept { return m_frames.size() >= kMaxFrames; }

    /// Stores the canvas, unless the last frame is younger than the interval
    /// (pass force = true for the first and the final frame).
    void capture(const QImage& canvas, const QString& label, bool force = false);
    /// Forget every frame and start a new session id.
    void restart();

    /// Writes a `.prismrec` (JSON with base64 PNG frames and the hash chain).
    [[nodiscard]] bool save(const QString& path, const QSize& canvasSize,
                            QString* error = nullptr) const;
    /// Decodes frame \p index for the video/GIF writers; false when out of range.
    [[nodiscard]] bool frameAt(int index, QImage& out, qint64& ms) const;
    /// The frames as a source the writers pull from, one at a time.
    [[nodiscard]] RecordedFrames recordedFrames() const;
    /// Recomputes the chain of a `.prismrec`. Returns false and fills \p error
    /// with the first broken frame when the file was edited.
    [[nodiscard]] static bool verify(const QString& path, QString* error = nullptr,
                                     int* frames = nullptr);

signals:
    /// A frame was stored. The UI reads `recordedFrames` from this: without it
    /// the export menu stayed disabled for the whole session.
    void frameStored();
    /// Emitted once when the frame budget is reached, so the UI can say so.
    void full();

private:
    struct Frame {
        qint64 ms = 0;
        QString label;
        QByteArray png;
        QString hash;
        QString previousHash;
    };

    [[nodiscard]] static QString hashFrame(const Frame& frame);
    [[nodiscard]] static QString hashParts(const QString& previous, qint64 ms,
                                           const QByteArray& png);

    bool m_active = true;
    QElapsedTimer m_clock;
    qint64 m_lastCaptureMs = -100000;
    QString m_sessionId;
    QList<Frame> m_frames;
};
