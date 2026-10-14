#include "application/SessionRecorder.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace {
constexpr int kRecordVersion = 2;

QByteArray pngBytes(const QImage& image) {
    QByteArray bytes;
    QBuffer buffer(&bytes);
    if (!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer, "PNG"))
        return {};
    return bytes;
}

QByteArray jsonLine(const QJsonObject& object) {
    QByteArray line = QJsonDocument(object).toJson(QJsonDocument::Compact);
    line.append('\n');
    return line;
}
} // namespace

SessionRecorder::SessionRecorder(QObject* parent) : QObject(parent) { restart(); }

bool SessionRecorder::isActive() const {
    std::lock_guard lock(m_mutex);
    return !m_recovered && !m_finished && !m_workerFailed && !m_repairing;
}

bool SessionRecorder::hasError() const {
    std::lock_guard lock(m_mutex);
    return m_workerFailed;
}

QString SessionRecorder::lastError() const {
    std::lock_guard lock(m_mutex);
    return m_lastError;
}

bool SessionRecorder::hasGaps() const {
    std::lock_guard lock(m_mutex);
    return m_hasGaps;
}

SessionRecorder::~SessionRecorder() {
    markFinished();
    stopWorker();
    removeEmptySpool();
}

void SessionRecorder::removeEmptySpool() {
    // A failed write may leave a nonempty header/tail with no acknowledged frames.
    if (!m_spoolPath.isEmpty() && QFileInfo(m_spoolPath).size() == 0) {
        QFile::remove(m_spoolPath);
        QFile::remove(m_spoolPath + QStringLiteral(".meta.json"));
    }
}

bool SessionRecorder::persistMetadata() {
    QJsonObject metadata;
    {
        std::lock_guard lock(m_mutex);
        metadata["session"] = m_sessionId;
        metadata["documentName"] = m_documentName;
        metadata["created"] = m_created;
        metadata["updated"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
        metadata["state"] = m_recovered                       ? QStringLiteral("recovered")
                            : m_workerFailed                  ? QStringLiteral("error")
                            : m_finished && m_pending.empty() ? QStringLiteral("completed")
                                                              : QStringLiteral("recording");
        metadata["hasGaps"] = m_hasGaps;
        metadata["error"] = m_lastError;
    }
    QSaveFile file(m_spoolPath + QStringLiteral(".meta.json"));
    const QByteArray bytes = QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

void SessionRecorder::setDocumentName(const QString& name) {
    if (m_recovered)
        return;
    {
        std::lock_guard lock(m_mutex);
        if (m_documentName == name)
            return;
        m_documentName = name;
        m_metadataDirty = true;
    }
    m_condition.notify_all();
}

void SessionRecorder::markFinished() {
    if (m_recovered || m_finished)
        return;
    {
        std::lock_guard lock(m_mutex);
        m_finished = true;
        if (!m_workerFailed)
            m_metadataDirty = true;
    }
    m_condition.notify_all();
    emit stateChanged();
}

void SessionRecorder::stopWorker() {
    if (!m_worker.joinable())
        return;
    flush();
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_condition.notify_all();
    m_worker.join();
    m_worker = std::jthread{};
}

void SessionRecorder::restart() {
    if (isRepairing())
        return;
    markFinished();
    stopWorker();
    removeEmptySpool();
    m_recovered = false;
    m_finished = false;
    m_frames.clear();
    m_lastCaptureMs = -100000;
    m_timestampOffset = 0;
    m_nextCaptureId = 1;
    m_sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_created = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QString root = qEnvironmentVariable("PRISM_RECOVERY_DIR");
    if (root.isEmpty()) {
        const QString settingsDir = qEnvironmentVariable("PRISM_SETTINGS_DIR");
        root = settingsDir.isEmpty()
                   ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                         QStringLiteral("/recovery")
                   : QDir(settingsDir).filePath(QStringLiteral("recovery"));
    }
    const QString dir = root + QStringLiteral("/recordings");
    m_spoolPath = QDir(dir).filePath(m_sessionId + QStringLiteral(".prismrec"));
    m_canvasSize = {};
    m_validBytes = 0;
    m_clock.start();
    {
        std::lock_guard lock(m_mutex);
        m_stop = false;
        m_workerFailed = false;
        m_hasGaps = false;
        m_lastError.clear();
        m_metadataDirty = true;
        m_lastMetadataWriteMs = 0;
        m_pending.clear();
        m_completed.clear();
    }
    startWorker();
    emit stateChanged();
}

void SessionRecorder::startWorker() {
    {
        std::lock_guard lock(m_mutex);
        m_workerStarting = true;
    }
    const QString previous = m_frames.empty() ? QString() : m_frames.back().hash;
    // Test faults are captured once, before the I/O worker starts.
    m_testFault = qEnvironmentVariable("PRISM_TEST_RECORD_FAULT");
    m_testFailAfter = std::max(0, qEnvironmentVariableIntValue("PRISM_TEST_RECORD_FAIL_AFTER"));
    const int storedFrames = static_cast<int>(m_frames.size());
    m_workerExited.store(false);
    m_worker = std::jthread([this, previous, storedFrames]() {
        writerLoop(previous, storedFrames);
        m_workerExited.store(true);
    });
}

void SessionRecorder::failWorker(const QString& error) {
    {
        std::lock_guard lock(m_mutex);
        m_workerFailed = true;
        m_workerBusy = true;
        m_hasGaps = true;
        m_lastError = error;
        m_pending.clear();
        m_stop = true;
    }
    persistMetadata();
    {
        std::lock_guard lock(m_mutex);
        m_workerStarting = false;
        m_workerBusy = false;
        m_metadataDirty = false;
    }
    m_condition.notify_all();
    QMetaObject::invokeMethod(
        this,
        [this]() {
            drainCompleted();
            emit stateChanged();
        },
        Qt::QueuedConnection);
}

bool SessionRecorder::isRepairing() const {
    std::lock_guard lock(m_mutex);
    return m_repairing;
}

bool SessionRecorder::retry() {
    // Only the legacy/test seam waits. Production UI calls retryAsync().
    if (isRepairing() || m_recovered || m_finished)
        return false;
    if (!hasError()) {
        stopWorker();
        std::lock_guard lock(m_mutex);
        m_workerFailed = true;
    }
    QEventLoop loop;
    bool success = false;
    const auto connection = connect(this, &SessionRecorder::retryFinished, &loop, [&](bool ok) {
        success = ok;
        loop.quit();
    });
    if (!retryAsync()) {
        disconnect(connection);
        return false;
    }
    loop.exec();
    return success;
}

bool SessionRecorder::retryAsync() {
    {
        std::lock_guard lock(m_mutex);
        if (m_recovered || m_finished || m_repairing || !m_workerFailed)
            return false;
        m_repairing = true;
    }
    emit stateChanged();
    QTimer::singleShot(0, this, &SessionRecorder::startRepairWhenReady);
    return true;
}

void SessionRecorder::repairSpool(const QString& path, const std::vector<Frame>& acknowledged,
                                  RepairResult& result) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        result.error =
            tr("Could not create the recording folder. Check storage permissions and retry.");
        return;
    }
    if (QFileInfo(path).size() > 0) {
        if (!scanV2(path, &result.frames, &result.bytes, nullptr, nullptr, &result.error, true,
                    acknowledged.empty()))
            return;
        if (result.frames.size() < acknowledged.size() ||
            (!acknowledged.empty() && result.bytes < acknowledged.back().endOffset)) {
            result.error = tr("An earlier recorded frame is damaged. Retry cannot safely change "
                              "this record; preserve it for recovery.");
            return;
        }
        for (std::size_t i = 0; i < result.frames.size(); ++i) {
            if (i < acknowledged.size()) {
                if (result.frames[i].hash != acknowledged[i].hash ||
                    result.frames[i].offset != acknowledged[i].offset) {
                    result.error =
                        tr("An acknowledged frame changed. This record was kept unchanged.");
                    return;
                }
                result.frames[i].captureId = acknowledged[i].captureId;
            } else
                result.frames[i].captureId = result.nextCaptureId++;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadWrite) || !file.resize(result.bytes) || !file.flush())
            result.error = tr("Could not repair the recording tail. Check disk space and "
                              "permissions, then retry.");
    } else if (!acknowledged.empty()) {
        result.error =
            tr("The earlier drawing record is missing. Retry cannot replace acknowledged frames.");
    }
}

void SessionRecorder::startRepairWhenReady() {
    if (!m_workerExited.load()) {
        QTimer::singleShot(10, this, &SessionRecorder::startRepairWhenReady);
        return;
    }
    // The failed writer has exited and closed its files. This join does not
    // wait for a spool scan or any disk operation on the GUI thread.
    if (m_worker.joinable())
        m_worker.join();
    drainCompleted();
    auto result = std::make_shared<RepairResult>();
    result->nextCaptureId = m_nextCaptureId;
    const auto acknowledged = m_frames;
    const QString path = m_spoolPath;
    m_workerExited.store(false);
    m_worker = std::jthread([this, result, acknowledged, path]() {
        try {
            repairSpool(path, acknowledged, *result);
        } catch (const std::exception&) {
            result->error = tr("Could not repair recording storage. The earlier record was kept.");
        }
        m_workerExited.store(true);
        QMetaObject::invokeMethod(
            this, [this, result]() { finishRepair(result); }, Qt::QueuedConnection);
    });
}

void SessionRecorder::finishRepair(const std::shared_ptr<RepairResult>& result) {
    if (m_worker.joinable())
        m_worker.join();
    if (!result->error.isEmpty()) {
        {
            std::lock_guard lock(m_mutex);
            m_lastError = result->error;
            m_hasGaps = true;
            m_repairing = false;
        }
        emit stateChanged();
        emit retryFinished(false);
        return;
    }
    m_frames = result->frames;
    m_validBytes = result->bytes;
    m_nextCaptureId = result->nextCaptureId;
    m_timestampOffset = std::max(m_timestampOffset + m_clock.elapsed(),
                                 m_frames.empty() ? 0 : m_frames.back().ms + 1);
    m_clock.restart();
    {
        std::lock_guard lock(m_mutex);
        m_stop = false;
        m_workerFailed = false;
        m_lastError.clear();
        m_metadataDirty = true;
    }
    startWorker();
    QTimer::singleShot(10, this, &SessionRecorder::finishRetryStartup);
}

void SessionRecorder::finishRetryStartup() {
    {
        std::lock_guard lock(m_mutex);
        if (m_workerStarting || m_metadataDirty || m_workerBusy) {
            QTimer::singleShot(10, this, &SessionRecorder::finishRetryStartup);
            return;
        }
        m_repairing = false;
    }
    emit stateChanged();
    emit retryFinished(!hasError());
}

QString SessionRecorder::hashParts(const QString& previous, qint64 ms, const QByteArray& png) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(previous.toUtf8());
    hash.addData(QByteArray::number(ms));
    hash.addData(png);
    return QString::fromLatin1(hash.result().toHex());
}

bool SessionRecorder::captureDue() const {
    if (!isActive())
        return false;
    const qint64 now = m_timestampOffset + (m_clock.isValid() ? m_clock.elapsed() : 0);
    return now - m_lastCaptureMs >= kMinIntervalMs;
}

quint64 SessionRecorder::capture(const QImage& canvas, const QString& label, bool force) {
    if (!isActive() || canvas.isNull())
        return 0;
    const qint64 now = m_timestampOffset + (m_clock.isValid() ? m_clock.elapsed() : 0);
    if (!force && now - m_lastCaptureMs < kMinIntervalMs)
        return 0;
    QImage image = canvas.width() > kMaxFrameSide || canvas.height() > kMaxFrameSide
                       ? canvas.scaled(kMaxFrameSide, kMaxFrameSide, Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation)
                       : canvas.copy();
    if (image.isNull())
        return 0;
    quint64 captureId = 0;
    {
        std::lock_guard lock(m_mutex);
        if (m_workerFailed)
            return 0;
        if (!force && m_pending.size() >= 4) {
            if (!m_hasGaps) {
                m_hasGaps = true;
                m_metadataDirty = true;
                QMetaObject::invokeMethod(
                    this, [this]() { emit stateChanged(); }, Qt::QueuedConnection);
            }
            return 0;
        }
        captureId = m_nextCaptureId++;
        m_pending.push_back(Pending{now, label, std::move(image), canvas.size(), captureId});
    }
    m_lastCaptureMs = now;
    m_condition.notify_all();
    return captureId;
}

void SessionRecorder::writerLoop(QString previous, int storedFrames) {
    QFile file(m_spoolPath);
    if (!QDir().mkpath(QFileInfo(m_spoolPath).absolutePath()) ||
        !file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        failWorker(tr("Could not open the drawing record: %1. Check disk space and permissions, "
                      "then retry.")
                       .arg(file.errorString()));
        return;
    }
    {
        std::lock_guard lock(m_mutex);
        m_workerStarting = false;
    }
    m_condition.notify_all();
    for (;;) {
        Pending job;
        bool metadataOnly = false;
        {
            std::unique_lock lock(m_mutex);
            m_condition.wait(lock,
                             [this]() { return m_stop || m_metadataDirty || !m_pending.empty(); });
            if (m_stop && m_pending.empty() && !m_metadataDirty)
                break;
            metadataOnly = m_metadataDirty;
            if (metadataOnly)
                m_metadataDirty = false;
            else {
                job = std::move(m_pending.front());
                m_pending.pop_front();
            }
            m_workerBusy = true;
        }
        if (metadataOnly) {
            if (!persistMetadata()) {
                failWorker(tr("Could not save recording session information. Check disk space and "
                              "permissions, then retry."));
                return;
            }
            {
                std::lock_guard lock(m_mutex);
                m_lastMetadataWriteMs = m_clock.elapsed();
                m_workerBusy = false;
            }
            m_condition.notify_all();
            continue;
        }
        if (file.size() == 0) {
            QJsonObject header;
            header["format"] = QStringLiteral("prism-record");
            header["version"] = kRecordVersion;
            header["session"] = m_sessionId;
            header["canvasWidth"] = job.canvasSize.width();
            header["canvasHeight"] = job.canvasSize.height();
            header["class"] = QStringLiteral("hand-drawn in Prism");
            const QByteArray line = jsonLine(header);
            if (m_testFault == QLatin1String("header") && storedFrames >= m_testFailAfter) {
                file.write(line.left(line.size() / 2));
                file.flush();
                failWorker(tr("Test storage failure while writing a recording header."));
                return;
            }
            if (file.write(line) != line.size() || !file.flush()) {
                failWorker(tr("Could not write the drawing record header: %1. Free disk space, "
                              "then retry.")
                               .arg(file.errorString()));
                return;
            }
        }
        const bool injectFault = !m_testFault.isEmpty() && storedFrames >= m_testFailAfter;
        const QByteArray png = injectFault && m_testFault == QLatin1String("encode")
                                   ? QByteArray()
                                   : pngBytes(job.image);
        if (!png.isEmpty()) {
            const QString hash = hashParts(previous, job.ms, png);
            QJsonObject entry;
            entry["ms"] = job.ms;
            entry["label"] = job.label;
            entry["previousHash"] = previous;
            entry["hash"] = hash;
            entry["png"] = QString::fromLatin1(png.toBase64());
            const QByteArray line = jsonLine(entry);
            const qint64 offset = file.size();
            if (injectFault && m_testFault == QLatin1String("write")) {
                file.write(line.left(line.size() / 2));
                file.flush();
                failWorker(tr("Test storage failure while writing a recording frame."));
                return;
            }
            if (file.write(line) == line.size() && file.flush() &&
                !(injectFault && m_testFault == QLatin1String("flush"))) {
                previous = hash;
                ++storedFrames;
                {
                    std::lock_guard lock(m_mutex);
                    m_completed.push_back(
                        Frame{job.ms, offset, offset + line.size(), hash, job.captureId});
                }
                QMetaObject::invokeMethod(
                    this, [this]() { drainCompleted(); }, Qt::QueuedConnection);
            } else {
                failWorker(
                    tr("Could not store a drawing record frame: %1. Free disk space, then retry.")
                        .arg(file.errorString()));
                return;
            }
        } else {
            failWorker(tr("Could not encode a drawing record frame. Retry recording; the earlier "
                          "frames are preserved."));
            return;
        }
        {
            std::lock_guard lock(m_mutex);
            m_workerBusy = false;
            // Frame progress touches the sidecar at most twice a minute.
            if (m_finished || m_clock.elapsed() - m_lastMetadataWriteMs >= 30000)
                m_metadataDirty = true;
        }
        m_condition.notify_all();
    }
}

void SessionRecorder::drainCompleted() {
    std::deque<Frame> completed;
    {
        std::lock_guard lock(m_mutex);
        completed.swap(m_completed);
    }
    for (auto& frame : completed) {
        m_validBytes = frame.endOffset;
        m_frames.push_back(std::move(frame));
        emit frameStored();
    }
}

bool SessionRecorder::flush() {
    if (m_worker.joinable()) {
        std::unique_lock lock(m_mutex);
        m_condition.wait(lock, [this]() {
            return !m_workerStarting && !m_workerBusy &&
                   (m_workerFailed || (m_pending.empty() && !m_metadataDirty));
        });
    }
    drainCompleted();
    return !hasError();
}

bool SessionRecorder::frameAt(int index, QImage& out, qint64& ms) const {
    if (index < 0 || index >= static_cast<int>(m_frames.size()))
        return false;
    QFile file(m_spoolPath);
    if (!file.open(QIODevice::ReadOnly) || !file.seek(m_frames[index].offset))
        return false;
    const QJsonObject entry = QJsonDocument::fromJson(file.readLine()).object();
    const QByteArray png = QByteArray::fromBase64(entry["png"].toString().toLatin1());
    if (!out.loadFromData(png, "PNG"))
        return false;
    ms = m_frames[index].ms;
    return true;
}

int SessionRecorder::frameCountThroughCaptureId(quint64 captureId) const {
    if (captureId == 0)
        return static_cast<int>(m_frames.size());
    const auto found =
        std::lower_bound(m_frames.begin(), m_frames.end(), captureId,
                         [](const Frame& frame, quint64 id) { return frame.captureId < id; });
    return found != m_frames.end() && found->captureId == captureId
               ? static_cast<int>(std::distance(m_frames.begin(), found)) + 1
               : 0;
}

qint64 SessionRecorder::validBytesThroughCaptureId(quint64 captureId) const {
    const int count = frameCountThroughCaptureId(captureId);
    return count > 0 ? m_frames[count - 1].endOffset : 0;
}

RecordedFrames SessionRecorder::recordedFrames(int maxFrames, quint64 throughCaptureId) {
    if (throughCaptureId == 0)
        flush();
    const int sourceCount = frameCountThroughCaptureId(throughCaptureId);
    const int count = maxFrames > 0 ? std::min(sourceCount, maxFrames) : sourceCount;
    RecordedFrames source;
    source.count = count;
    // The export may continue after the artist opens another document. Keep an
    // immutable index and spool path, never a pointer into the live recorder.
    source.fetch = [path = m_spoolPath,
                    frames = std::vector<Frame>(m_frames.begin(), m_frames.begin() + sourceCount),
                    sourceCount, count](int index, QImage& out, qint64& ms) {
        if (index < 0 || index >= count)
            return false;
        const int sourceIndex =
            count <= 1
                ? 0
                : static_cast<int>((static_cast<qint64>(index) * (sourceCount - 1)) / (count - 1));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || !file.seek(frames[sourceIndex].offset))
            return false;
        const QJsonObject entry = QJsonDocument::fromJson(file.readLine()).object();
        const QByteArray png = QByteArray::fromBase64(entry["png"].toString().toLatin1());
        if (!out.loadFromData(png, "PNG"))
            return false;
        ms = frames[sourceIndex].ms;
        return true;
    };
    return source;
}

bool SessionRecorder::save(const QString& path, const QSize&, QString* error) {
    if (!flush()) {
        if (error)
            *error = lastError();
        return false;
    }
    if (m_frames.empty()) {
        if (error)
            *error = tr("Nothing recorded yet - draw something first.");
        return false;
    }
    return saveSnapshot(path, m_spoolPath, m_validBytes, error);
}

bool SessionRecorder::saveSnapshot(const QString& path, const QString& spoolPath, qint64 validBytes,
                                   QString* error) {
    if (validBytes <= 0) {
        if (error)
            *error = QObject::tr("Nothing recorded yet - draw something first.");
        return false;
    }
    const QFileInfo destinationInfo(path), sourceInfo(spoolPath);
    const QString destination = destinationInfo.exists() ? destinationInfo.canonicalFilePath()
                                                         : destinationInfo.absoluteFilePath();
    const QString source = sourceInfo.canonicalFilePath();
#ifdef Q_OS_WIN
    const bool sameFile = destination.compare(source, Qt::CaseInsensitive) == 0;
#else
    const bool sameFile = destination == source;
#endif
    if (sameFile) {
        if (error)
            *error = tr("Choose a different destination for the record.");
        return false;
    }
    QFile input(spoolPath);
    QSaveFile output(path);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        if (error)
            *error = tr("Could not write %1.").arg(path);
        return false;
    }
    qint64 remaining = validBytes;
    while (remaining > 0) {
        const QByteArray chunk = input.read(std::min<qint64>(remaining, 1024 * 1024));
        if (chunk.isEmpty() || output.write(chunk) != chunk.size()) {
            if (error)
                *error = tr("Could not copy the drawing record.");
            return false;
        }
        remaining -= chunk.size();
    }
    if (!output.commit()) {
        if (error)
            *error = tr("Could not write %1.").arg(path);
        return false;
    }
    return true;
}

bool SessionRecorder::exportRecovered(const QString& source, const QString& destination,
                                      QString* error) {
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error)
            *error = tr("Could not read %1.").arg(source);
        return false;
    }
    const QJsonObject header = QJsonDocument::fromJson(input.readLine()).object();
    input.close();
    qint64 bytes = 0;
    if (header["version"].toInt() == 2) {
        if (!scanV2(source, nullptr, &bytes, nullptr, nullptr, error, true))
            return false;
    } else {
        if (!verify(source, error))
            return false;
        bytes = QFileInfo(source).size();
    }
    return saveSnapshot(destination, source, bytes, error);
}

bool SessionRecorder::scanV2(const QString& path, std::vector<Frame>* found, qint64* validBytes,
                             QString* session, QSize* canvasSize, QString* error, bool allowTail,
                             bool allowEmpty) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QObject::tr("Could not read %1.").arg(path);
        return false;
    }
    const QByteArray first = file.readLine();
    const QJsonObject header = QJsonDocument::fromJson(first).object();
    if (header["format"].toString() != QLatin1String("prism-record") ||
        header["version"].toInt() != 2 || !first.endsWith('\n')) {
        // Only a fresh session with no acknowledged frames may repair its header.
        if (allowTail && allowEmpty && !first.endsWith('\n') && file.atEnd()) {
            if (validBytes)
                *validBytes = 0;
            return true;
        }
        if (error)
            *error = QObject::tr("Not a Prism record v2 file.");
        return false;
    }
    if (session)
        *session = header["session"].toString();
    if (canvasSize)
        *canvasSize = QSize(header["canvasWidth"].toInt(), header["canvasHeight"].toInt());
    qint64 end = file.pos();
    QString previous;
    int index = 0;
    while (!file.atEnd()) {
        const qint64 offset = file.pos();
        const QByteArray line = file.readLine();
        if (!line.endsWith('\n')) {
            if (allowTail)
                break;
            if (error)
                *error = QObject::tr("Frame %1 is incomplete.").arg(index);
            return false;
        }
        const QJsonObject entry = QJsonDocument::fromJson(line).object();
        const QByteArray png = QByteArray::fromBase64(entry["png"].toString().toLatin1());
        const qint64 ms = static_cast<qint64>(entry["ms"].toDouble(-1));
        if (png.isEmpty() || ms < 0 || entry["previousHash"].toString() != previous ||
            entry["hash"].toString() != hashParts(previous, ms, png)) {
            if (allowTail)
                break;
            if (error)
                *error = QObject::tr("Frame %1 does not match its hash.").arg(index);
            return false;
        }
        previous = entry["hash"].toString();
        end = file.pos();
        if (found)
            found->push_back(Frame{ms, offset, end, previous, static_cast<quint64>(index + 1)});
        ++index;
    }
    if (file.error() != QFileDevice::NoError) {
        if (error)
            *error = QObject::tr("Could not read the drawing record: %1.").arg(file.errorString());
        return false;
    }
    if (index == 0 && !allowEmpty) {
        if (error)
            *error = QObject::tr("The record has no frames.");
        return false;
    }
    if (validBytes)
        *validBytes = end;
    return true;
}

void SessionRecorder::readArchive(const QString& path, ArchiveResult& result) {
    if (!scanV2(path, &result.frames, &result.bytes, &result.session, &result.size, &result.error,
                true))
        return;
    QFile metadata(path + QStringLiteral(".meta.json"));
    QJsonObject info;
    if (metadata.open(QIODevice::ReadOnly))
        info = QJsonDocument::fromJson(metadata.readAll()).object();
    // A valid byte prefix does not prove queued frames survived a crash.
    result.gaps = info["state"].toString() != "completed" || info["hasGaps"].toBool() ||
                  QFileInfo(path).size() > result.bytes;
    result.name = info["documentName"].toString();
    result.created =
        info["created"].toString(QFileInfo(path).birthTime().toUTC().toString(Qt::ISODateWithMs));
}

void SessionRecorder::acceptArchive(const QString& path, ArchiveResult& result) {
    m_frames = std::move(result.frames);
    m_validBytes = result.bytes;
    m_sessionId = result.session;
    m_canvasSize = result.size;
    m_spoolPath = path;
    {
        std::lock_guard lock(m_mutex);
        m_recovered = true;
        m_finished = false;
        m_repairing = false;
        m_workerFailed = false;
        m_lastError.clear();
        m_hasGaps = result.gaps;
        m_documentName = result.name;
        m_created = result.created;
        m_metadataDirty = false;
    }
    // Archive pixels, hash chain and source metadata stay immutable.
    emit frameStored();
    emit stateChanged();
}

bool SessionRecorder::adoptRecovered(const QString& path, QString* error) {
    if (isRepairing()) {
        if (error)
            *error = tr("Recording storage is busy.");
        return false;
    }
    ArchiveResult result;
    readArchive(path, result);
    if (!result.error.isEmpty()) {
        if (error)
            *error = result.error;
        return false;
    }
    stopWorker();
    removeEmptySpool();
    acceptArchive(path, result);
    return true;
}

bool SessionRecorder::adoptRecoveredAsync(const QString& path) {
    const bool wasFinished = m_finished;
    {
        std::lock_guard lock(m_mutex);
        if (m_repairing)
            return false;
        m_repairing = true;
        if (!m_recovered && !m_workerFailed) {
            m_finished = true;
            m_metadataDirty = true;
        }
        m_stop = true;
    }
    m_condition.notify_all();
    emit stateChanged();
    QTimer::singleShot(0, this,
                       [this, path, wasFinished]() { startArchiveWhenReady(path, wasFinished); });
    return true;
}

void SessionRecorder::startArchiveWhenReady(const QString& path, bool wasFinished) {
    if (!m_workerExited.load()) {
        QTimer::singleShot(
            10, this, [this, path, wasFinished]() { startArchiveWhenReady(path, wasFinished); });
        return;
    }
    if (m_worker.joinable())
        m_worker.join();
    drainCompleted();
    auto result = std::make_shared<ArchiveResult>();
    m_workerExited.store(false);
    m_worker = std::jthread([this, path, wasFinished, result]() {
        try {
            readArchive(path, *result);
        } catch (const std::exception&) {
            result->error = tr("Could not read recording storage. The current session was kept.");
        }
        m_workerExited.store(true);
        QMetaObject::invokeMethod(
            this,
            [this, path, wasFinished, result]() {
                if (m_worker.joinable())
                    m_worker.join();
                if (result->error.isEmpty()) {
                    removeEmptySpool();
                    acceptArchive(path, *result);
                } else {
                    {
                        std::lock_guard lock(m_mutex);
                        m_repairing = false;
                        m_finished = wasFinished;
                        m_stop = false;
                        if (!m_recovered && !m_workerFailed)
                            m_metadataDirty = true;
                    }
                    if (!m_recovered && !m_finished && !hasError())
                        startWorker();
                    emit stateChanged();
                }
                emit archiveLoaded(result->error.isEmpty(), result->error);
            },
            Qt::QueuedConnection);
    });
}

bool SessionRecorder::verify(const QString& path, QString* error, int* frames) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = tr("Could not read %1.").arg(path);
        return false;
    }
    const QByteArray first = file.readLine();
    const QJsonObject header = QJsonDocument::fromJson(first).object();
    if (header["version"].toInt() == 2) {
        std::vector<Frame> found;
        if (!scanV2(path, &found, nullptr, nullptr, nullptr, error, false))
            return false;
        if (frames)
            *frames = static_cast<int>(found.size());
        return true;
    }

    // v1 is a single JSON object. Its hash input and field names must stay
    // unchanged for records already given to artists and clients.
    file.seek(0);
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    if (root["format"].toString() != QLatin1String("prism-record") ||
        root["version"].toInt() != 1) {
        if (error)
            *error = tr("Not a supported Prism record file.");
        return false;
    }
    QString previous;
    int index = 0;
    for (const QJsonValue& value : root["frames"].toArray()) {
        const QJsonObject entry = value.toObject();
        const QByteArray png = QByteArray::fromBase64(entry["png"].toString().toLatin1());
        if (png.isEmpty() || entry["previousHash"].toString() != previous ||
            entry["hash"].toString() !=
                hashParts(previous, static_cast<qint64>(entry["ms"].toDouble()), png)) {
            if (error)
                *error = tr("Frame %1 does not match its hash.").arg(index);
            return false;
        }
        previous = entry["hash"].toString();
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
