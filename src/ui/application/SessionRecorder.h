#pragma once

#include "application/RecordExport.h"

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QSize>
#include <QString>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

/// An append-only drawing record. PNG encoding and disk writes run on an I/O
/// worker; the GUI keeps only frame offsets and hashes, even after eight hours.
class SessionRecorder : public QObject {
    Q_OBJECT
public:
    static constexpr int kMinIntervalMs = 1500;
    static constexpr int kMaxFrameSide = 720;
    static constexpr int kMaxTimelapseFrames = 4 * 60 * 12;

    explicit SessionRecorder(QObject* parent = nullptr);
    ~SessionRecorder() override;

    [[nodiscard]] bool isActive() const;
    [[nodiscard]] bool hasError() const;
    [[nodiscard]] QString lastError() const;
    [[nodiscard]] bool hasGaps() const;
    [[nodiscard]] quint64 lastStoredCaptureId() const noexcept {
        return m_frames.empty() ? 0 : m_frames.back().captureId;
    }
    [[nodiscard]] int frameCount() const noexcept { return static_cast<int>(m_frames.size()); }
    [[nodiscard]] QString spoolPath() const { return m_spoolPath; }
    [[nodiscard]] qint64 validBytes() const noexcept { return m_validBytes; }
    quint64 capture(const QImage& canvas, const QString& label, bool force = false);
    [[nodiscard]] bool captureDue() const;
    void restart();
    bool flush();
    bool retry();
    bool retryAsync();
    [[nodiscard]] bool isRepairing() const;
    void setDocumentName(const QString& name);
    void markFinished();

    /// Copy the complete valid prefix to a v2 .prismrec without loading it all.
    [[nodiscard]] bool save(const QString& path, const QSize& canvasSize, QString* error = nullptr);
    [[nodiscard]] static bool saveSnapshot(const QString& path, const QString& spoolPath,
                                           qint64 validBytes, QString* error = nullptr);
    [[nodiscard]] static bool exportRecovered(const QString& source, const QString& destination,
                                              QString* error = nullptr);
    [[nodiscard]] bool frameAt(int index, QImage& out, qint64& ms) const;
    [[nodiscard]] RecordedFrames recordedFrames(int maxFrames = 0, quint64 throughCaptureId = 0);
    [[nodiscard]] qint64 validBytesThroughCaptureId(quint64 captureId) const;
    [[nodiscard]] int frameCountThroughCaptureId(quint64 captureId) const;
    /// Open a crashed session. A truncated last line is ignored, keeping every
    /// earlier frame and its hash chain.
    [[nodiscard]] bool adoptRecovered(const QString& path, QString* error = nullptr);
    bool adoptRecoveredAsync(const QString& path);
    [[nodiscard]] static bool verify(const QString& path, QString* error = nullptr,
                                     int* frames = nullptr);

signals:
    void frameStored();
    void stateChanged();
    void retryFinished(bool success);
    void archiveLoaded(bool success, const QString& error);

private:
    struct Frame {
        qint64 ms = 0;
        qint64 offset = 0;
        qint64 endOffset = 0;
        QString hash;
        quint64 captureId = 0;
    };
    struct Pending {
        qint64 ms = 0;
        QString label;
        QImage image;
        QSize canvasSize;
        quint64 captureId = 0;
    };
    struct RepairResult {
        std::vector<Frame> frames;
        qint64 bytes = 0;
        quint64 nextCaptureId = 0;
        QString error;
    };
    struct ArchiveResult {
        std::vector<Frame> frames;
        qint64 bytes = 0;
        QString session, name, created, error;
        QSize size;
        bool gaps = false;
    };
    static void readArchive(const QString& path, ArchiveResult& result);
    void acceptArchive(const QString& path, ArchiveResult& result);
    void startArchiveWhenReady(const QString& path, bool wasFinished);
    void startRepairWhenReady();
    void finishRepair(const std::shared_ptr<RepairResult>& result);
    void finishRetryStartup();
    static void repairSpool(const QString& path, const std::vector<Frame>& acknowledged,
                            RepairResult& result);
    static QString hashParts(const QString& previous, qint64 ms, const QByteArray& png);
    static bool scanV2(const QString& path, std::vector<Frame>* found, qint64* validBytes,
                       QString* session, QSize* canvasSize, QString* error, bool allowTail,
                       bool allowEmpty = false);
    void writerLoop(QString previous, int storedFrames);
    void drainCompleted();
    void stopWorker();
    void startWorker();
    void failWorker(const QString& error);
    bool persistMetadata();
    void removeEmptySpool();

    bool m_recovered = false;
    bool m_finished = false;
    QElapsedTimer m_clock;
    qint64 m_lastCaptureMs = -100000;
    qint64 m_timestampOffset = 0;
    quint64 m_nextCaptureId = 1;
    QString m_sessionId;
    QString m_documentName;
    QString m_created;
    QString m_spoolPath;
    QSize m_canvasSize;
    qint64 m_validBytes = 0;
    std::vector<Frame> m_frames;
    std::jthread m_worker;
    mutable std::mutex m_mutex;
    std::condition_variable m_condition;
    std::deque<Pending> m_pending;
    std::deque<Frame> m_completed;
    bool m_workerBusy = false;
    bool m_workerFailed = false;
    bool m_workerStarting = false;
    bool m_hasGaps = false;
    QString m_lastError;
    bool m_metadataDirty = false;
    qint64 m_lastMetadataWriteMs = 0;
    QString m_testFault;
    int m_testFailAfter = 0;
    bool m_stop = false;
    bool m_repairing = false;
    std::atomic_bool m_workerExited{true};
};
