#include "application/AppController.h"
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QSaveFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QUuid>

namespace {
QString sessionName(const QString& path) { return QFileInfo(path).fileName(); }
QString nativePath(const QString& path) {
    QFileInfo info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty())
        result = info.absoluteFilePath();
#ifdef Q_OS_WIN
    result = result.toCaseFolded();
#endif
    return result;
}
QJsonObject timelineById(const QJsonObject& manifest, const QString& id) {
    for (const auto& value : manifest["timelines"].toArray()) {
        const auto row = value.toObject();
        if (row["id"].toString() == id)
            return row;
    }
    return {};
}
QStringList sourceNames(const QJsonObject& manifest) {
    QStringList result;
    for (const auto& v : manifest["timelines"].toArray())
        for (const auto& source : v.toObject()["sessions"].toArray())
            if (!result.contains(source.toString()))
                result << source.toString();
    return result;
}
QJsonObject exportManifest(const QJsonObject& all, const QString& id, const QStringList& selected,
                           bool bundle) {
    auto chosen = timelineById(all, id);
    if (chosen.isEmpty())
        return {};
    QJsonArray rows;
    if (!bundle || !selected.isEmpty()) {
        auto sessions = chosen["sessions"].toArray();
        QJsonArray filtered;
        for (const auto& name : sessions)
            if (selected.isEmpty() || selected.contains(name.toString()))
                filtered.append(name);
        chosen["sessions"] = filtered;
        chosen["parent"] = "";
        rows.append(chosen);
    } else {
        QString root = id;
        while (!timelineById(all, root)["parent"].toString().isEmpty())
            root = timelineById(all, root)["parent"].toString();
        for (const auto& v : all["timelines"].toArray()) {
            auto row = v.toObject();
            QString parent = row["id"].toString();
            while (!timelineById(all, parent)["parent"].toString().isEmpty())
                parent = timelineById(all, parent)["parent"].toString();
            if (parent == root)
                rows.append(row);
        }
    }
    for (int i = 0; i < rows.size(); ++i) {
        auto row = rows[i].toObject();
        row.remove("path");
        row.remove("fingerprint");
        rows[i] = row;
    }
    return {{"version", 1}, {"timelines", rows}};
}
struct SourceSnapshot {
    RecordedFrames frames;
    QJsonObject limits;
    QVariantMap counts;
    bool partial = false;
    QString error;
    std::shared_ptr<QTemporaryDir> temporary = std::make_shared<QTemporaryDir>();
};
SourceSnapshot readSources(const QString& root, const QStringList& names, const QString& live,
                           qint64 bytes, bool livePartial,
                           const std::function<bool(int, int)>& progress) {
    SourceSnapshot result;
    QList<RecordedFrames> sources;
    QList<int> ends;
    int count = 0;
    if (!result.temporary->isValid()) {
        result.error = "Cannot prepare recording snapshot.";
        return result;
    }
    for (const auto& name : names) {
        if (progress && !progress(0, 1)) {
            result.error = "Export cancelled.";
            return result;
        }
        QString path = root + "/" + name;
        QFile metadata(path + ".meta.json");
        QJsonObject info;
        if (metadata.open(QIODevice::ReadOnly))
            info = QJsonDocument::fromJson(metadata.readAll()).object();
        result.partial |= name == live
                              ? livePartial
                              : info["state"].toString() != "completed" || info["hasGaps"].toBool();
        if (name == live) {
            path = result.temporary->filePath(name);
            if (!SessionRecorder::saveSnapshot(path, root + "/" + name, bytes, &result.error))
                return result;
        }
        qint64 valid = 0;
        auto source = SessionRecorder::framesFromFile(
            path, &result.error, &valid, true, [&]() { return !progress || progress(0, 1); });
        if (!result.error.isEmpty() || source.count <= 0)
            return result;
        result.limits[name] = double(valid);
        if (name == live) {
            result.limits["_active"] = live;
            result.limits["_partial"] = livePartial;
        }
        if (valid < QFileInfo(path).size())
            result.partial = true;
        if (source.count > 1000000 - count) {
            result.error = "Recording timeline has too many frames.";
            return result;
        }
        result.counts[name] = source.count;
        count += source.count;
        ends.append(count);
        sources.append(std::move(source));
    }
    result.frames.count = count;
    result.frames.progress = progress;
    result.frames.fetch = [sources, ends, temporary = result.temporary](int index, QImage& out,
                                                                        qint64& ms) {
        if (index < 0 || ends.isEmpty() || index >= ends.last())
            return false;
        int part = 0;
        while (index >= ends[part])
            ++part;
        return sources[part].fetch(index - (part ? ends[part - 1] : 0), out, ms);
    };
    return result;
}
} // namespace
bool AppController::validateRecordingDestination(const QString& path) {
    const auto resolved = nativePath(path);
    if (nativePath(QFileInfo(resolved).absolutePath()) ==
        nativePath(QFileInfo(m_recorder->spoolPath()).absolutePath())) {
        setLastError(tr("Choose a destination outside internal Recording storage."));
        return false;
    }
    return true;
}
void AppController::setCaptureInterval(int interval) {
    auto settings = m_recorder->captureSettings();
    settings.intervalMs = interval;
    if (!m_recorder->setCaptureSettings(settings))
        return;
    if (m_recordTailTimer.isActive())
        m_recordTailTimer.start(interval);
    if (!m_selfTestMode)
        QSettings().setValue("recording/intervalMs", interval);
    emit recordingChanged();
}
void AppController::setCaptureResolution(int side) {
    auto settings = m_recorder->captureSettings();
    settings.maxSide = side;
    if (!m_recorder->setCaptureSettings(settings))
        return;
    if (!m_selfTestMode)
        QSettings().setValue("recording/maxSide", side);
    emit recordingChanged();
}
QString AppController::activeRecordingTimeline() const {
    return m_recordTimelines ? m_recordTimelines->activeId() : QString();
}
bool AppController::recordingLinkConflict() const {
    return m_recordTimelines && !m_recordTimelines->conflictId().isEmpty();
}
QVariantList AppController::recordingTimelines() const {
    QVariantList result;
    if (!m_recordTimelines)
        return result;
    for (const auto& v : m_recordTimelines->manifest()["timelines"].toArray()) {
        auto row = v.toObject().toVariantMap();
        row["active"] = row["id"].toString() == activeRecordingTimeline();
        result.append(row);
    }
    return result;
}
QVariantList AppController::recordingSessions(const QString& timeline) const {
    QVariantList result;
    if (!m_recordTimelines)
        return result;
    const auto row = timelineById(m_recordTimelines->manifest(), timeline);
    const QString root = QFileInfo(m_recorder->spoolPath()).absolutePath();
    for (const auto& value : row["sessions"].toArray()) {
        const QString name = value.toString();
        QFile file(root + "/" + name + ".meta.json");
        QJsonObject info;
        if (file.open(QIODevice::ReadOnly))
            info = QJsonDocument::fromJson(file.readAll()).object();
        result.append(QVariantMap{
            {"id", name},
            {"name", info["created"].toString(name.left(8))},
            {"bytes", QFileInfo(root + "/" + name).size()},
            {"active", name == sessionName(m_recorder->spoolPath())},
            {"incomplete", info["state"].toString() != "completed" || info["hasGaps"].toBool()}});
    }
    return result;
}
std::function<bool(int, int)> AppController::recordingProgressCallback() {
    QPointer<AppController> owner(this);
    auto cancel = m_recordCancel;
    auto last = std::make_shared<std::atomic_int>(-1);
    return [owner, cancel, last](int done, int total) {
        if (cancel->load())
            return false;
        const int value = total > 0 ? int(qint64(done) * 100 / total) : 0;
        if (value != last->exchange(value) && owner)
            QMetaObject::invokeMethod(
                owner,
                [owner, value]() {
                    if (owner) {
                        owner->m_recordProgress = value;
                        emit owner->recordingProgressChanged();
                    }
                },
                Qt::QueuedConnection);
        return !cancel->load();
    };
}
bool AppController::runRecordingTask(const std::function<QString()>& work,
                                     const std::function<void()>& success, bool quiet) {
    if (m_exportThread || m_recordingExporting || recordingRepairing())
        return false;
    m_recordCancel = std::make_shared<std::atomic_bool>(false);
    m_recordProgress = 0;
    m_recordingExporting = true;
    m_recordQuiet = quiet;
    if (!quiet)
        setStatusMessage(tr("Processing recording…"));
    emit recordingProgressChanged();
    emit recordingExportingChanged();
    auto error = std::make_shared<QString>();
    auto cancel = m_recordCancel;
    auto thread = QThread::create([work, error]() {
        try {
            *error = work();
        } catch (const std::exception&) {
            *error = "Recording operation failed. Original files were kept.";
        }
    });
    m_exportThread = thread;
    thread->setParent(this);
    connect(thread, &QThread::finished, this, [this, thread, error, success, cancel]() {
        m_exportThread = nullptr;
        m_recordingExporting = false;
        emit recordingExportingChanged();
        if (cancel->load())
            setStatusMessage(tr("Recording operation cancelled."));
        else if (!error->isEmpty())
            setLastError(*error, "recording");
        else
            success();
        emit recordingChanged();
        thread->deleteLater();
    });
    thread->start();
    return true;
}
void AppController::cancelRecordingExport() { m_recordCancel->store(true); }
void AppController::beginRecordingTimeline(const QString& path, const QString& name) {
    if (m_recordQuiet && m_recordingExporting)
        m_recordCancel->store(true);
    m_recordBranchPending = false;
    emit artworkStateChanged();
    ++m_recordPreviewEpoch;
    m_recordTailTimer.stop();
    m_recordCanvasDirty = false;
    m_recordPreviewSource = {};
    m_recordPreview.clear();
    m_recordPreviewTimeline.clear();
    m_recordFrameCounts.clear();
    emit recordingPreviewChanged();
    if (!m_recordTimelines)
        return;
    if (!m_recordTimelines->begin(name, sessionName(m_recorder->spoolPath())))
        setLastError(m_recordTimelines->error(), "recording");
    emit recordingChanged();
    if (path.isEmpty())
        return;
    probeRecordingTimeline(path, m_recorder->spoolPath());
}
void AppController::probeRecordingTimeline(const QString& path, const QString& expected) {
    if (expected != m_recorder->spoolPath())
        return;
    auto hash = std::make_shared<QString>();
    if (runRecordingTask(
            [path, hash]() {
                *hash = RecordingTimeline::fingerprint(path);
                return hash->isEmpty() ? QString("Cannot identify artwork recording link.")
                                       : QString();
            },
            [this, path, hash, expected]() {
                if (expected != m_recorder->spoolPath())
                    return;
                m_recordLinkPath = path;
                m_recordLinkFingerprint = *hash;
                if (!m_recordTimelines->resolve(path, *hash) && !recordingLinkConflict())
                    setLastError(m_recordTimelines->error(), "recording");
                emit recordingChanged();
            }))
        return;
    QTimer::singleShot(100, this,
                       [this, path, expected]() { probeRecordingTimeline(path, expected); });
}
void AppController::recordingSaved(const QString& path, const QString& fingerprint,
                                   const QString& expected) {
    if (!m_recordTimelines || expected != m_recorder->spoolPath())
        return;
    if (recordingLinkConflict()) {
        m_recordLinkPath = path;
        m_recordLinkFingerprint = fingerprint;
        return;
    }
    const auto previous = m_recordTimelines->active()["path"].toString();
#ifdef Q_OS_WIN
    constexpr auto pathCase = Qt::CaseInsensitive;
#else
    constexpr auto pathCase = Qt::CaseSensitive;
#endif
    const bool fork = !previous.isEmpty() && QFileInfo(previous).absoluteFilePath().compare(
                                                 QFileInfo(path).absoluteFilePath(), pathCase) != 0;
    if (fork) {
        if (recordingRepairing()) {
            QTimer::singleShot(50, this, [this, path, fingerprint, expected]() {
                recordingSaved(path, fingerprint, expected);
            });
            return;
        }
        m_recordTailTimer.stop();
        const auto capture = m_recorder->capture(canvasImage(), tr("Save As branch"), true);
        if (!capture)
            setLastError(tr("Save As succeeded, but its previous recording session is incomplete."),
                         "recording");
        m_recordBranchPending = true;
        emit artworkStateChanged();
        // Freeze the parent at Save As completion. New pen events must never append
        // to the immutable history inherited by both branches.
        m_recorder->markFinished();
        QTimer::singleShot(0, this, [this, path, fingerprint, expected]() {
            finishRecordingBranch(path, fingerprint, expected);
        });
        return;
    }
    if (!m_recordTimelines->saved(path, fingerprint))
        setLastError(m_recordTimelines->error(), "recording");
    emit recordingChanged();
}
void AppController::finishRecordingBranch(const QString& path, const QString& fingerprint,
                                          const QString& expected) {
    if (expected != m_recorder->spoolPath())
        return;
    if (!m_recorder->isDrained() || (m_recordingExporting && !m_exportThread)) {
        QTimer::singleShot(25, this, [this, path, fingerprint, expected]() {
            finishRecordingBranch(path, fingerprint, expected);
        });
        return;
    }
    m_recorder->restart();
    m_recorder->setDocumentName(m_documentName);
    if (!m_recordTimelines->saved(path, fingerprint, sessionName(m_recorder->spoolPath())))
        setLastError(m_recordTimelines->error(), "recording");
    m_recorder->capture(canvasImage(), tr("Branch"), true);
    m_recordCanvasDirty = false;
    m_recordBranchPending = false;
    emit artworkStateChanged();
    emit recordingChanged();
    if (!m_pendingDocumentAction.isEmpty())
        QTimer::singleShot(0, this, &AppController::settleDocumentDecision);
}
void AppController::adoptRecordingTimeline() {
    if (!m_recordTimelines)
        return;
    ++m_recordPreviewEpoch;
    m_recordPreviewSource = {};
    m_recordPreview.clear();
    m_recordPreviewTimeline.clear();
    m_recordFrameCounts.clear();
    const QString name = sessionName(m_recorder->spoolPath());
    for (const auto& row : m_recordTimelines->manifest()["timelines"].toArray())
        if (row.toObject()["sessions"].toArray().contains(name)) {
            m_recordTimelines->select(row.toObject()["id"].toString());
            emit recordingChanged();
            emit recordingPreviewChanged();
            return;
        }
    m_recordTimelines->begin(tr("Recovered recording"), name);
    emit recordingChanged();
    emit recordingPreviewChanged();
}
bool AppController::resolveRecordingLink(bool continueExisting) {
    if (!recordingLinkConflict() || documentEditing())
        return false;
    const bool ok =
        continueExisting
            ? m_recordTimelines->resolve(m_recordLinkPath, m_recordLinkFingerprint, true)
            : m_recordTimelines->saved(m_recordLinkPath, m_recordLinkFingerprint);
    emit recordingChanged();
    return ok;
}
bool AppController::deleteRecordingTimeline(const QString& id) {
    if (m_recordingExporting || !m_recordTimelines)
        return false;
    const bool ok = m_recordTimelines->remove(id, {sessionName(m_recorder->spoolPath())});
    if (!ok)
        setLastError(m_recordTimelines->error().isEmpty()
                         ? tr("This timeline is active or recording storage is unavailable.")
                         : m_recordTimelines->error());
    emit recordingChanged();
    emit recoveryChanged();
    return ok;
}
bool AppController::linkRecordingTimeline(const QString& id, const QUrl& artwork) {
    if (documentEditing() || !m_recordTimelines ||
        timelineById(m_recordTimelines->manifest(), id).isEmpty())
        return false;
    const QString path = localPathFrom(artwork);
    auto hash = std::make_shared<QString>();
    return runRecordingTask(
        [path, hash]() {
            *hash = RecordingTimeline::fingerprint(path);
            return hash->isEmpty() ? QString("Cannot read artwork to link.") : QString();
        },
        [this, path, hash, id]() {
            if (!m_recordTimelines->link(id, path, *hash)) {
                setLastError(m_recordTimelines->error().isEmpty()
                                 ? tr("Could not link recording timeline.")
                                 : m_recordTimelines->error());
                return;
            }
            setStatusMessage(tr("Recording linked. Reopen the artwork to continue its timeline."));
            emit recordingChanged();
        });
}
void AppController::refreshRecordingPreview(const QString& id) {
    if (m_recordingExporting || !m_recordTimelines)
        return;
    const auto manifest = exportManifest(m_recordTimelines->manifest(), id, {}, false);
    const auto names = sourceNames(manifest);
    if (names.isEmpty())
        return;
    const QString spool = m_recorder->spoolPath(), root = QFileInfo(spool).absolutePath();
    const qint64 bytes = m_recorder->validBytes();
    if (names.contains(sessionName(spool)) && bytes == 0)
        return;
    auto result = std::make_shared<SourceSnapshot>();
    const auto epoch = ++m_recordPreviewEpoch;
    runRecordingTask(
        [this, root, names, spool, bytes, result]() {
            *result = readSources(root, names, sessionName(spool), bytes, false,
                                  recordingProgressCallback());
            return result->error;
        },
        [this, result, epoch, spool, id]() {
            if (epoch != m_recordPreviewEpoch || spool != m_recorder->spoolPath())
                return;
            m_recordPreviewSource = result->frames;
            m_recordFrameCounts = result->counts;
            m_recordPreviewTimeline = id;
            emit recordingPreviewChanged();
            seekRecordingPreview(result->frames.count - 1);
        });
}
void AppController::seekRecordingPreview(int index) {
    if (m_recordingExporting || index < 0 || index >= m_recordPreviewSource.count)
        return;
    auto image = std::make_shared<QString>();
    const auto source = m_recordPreviewSource;
    const auto epoch = m_recordPreviewEpoch;
    runRecordingTask(
        [source, index, image]() {
            QImage frame;
            qint64 ms = 0;
            if (!source.fetch(index, frame, ms))
                return QString("Cannot read preview frame.");
            frame = frame.scaled(288, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QByteArray bytes;
            QBuffer buffer(&bytes);
            if (!buffer.open(QIODevice::WriteOnly) || !frame.save(&buffer, "PNG"))
                return QString("Cannot prepare preview.");
            *image = "data:image/png;base64," + QString::fromLatin1(bytes.toBase64());
            return QString();
        },
        [this, image, epoch]() {
            if (epoch != m_recordPreviewEpoch)
                return;
            m_recordPreview = *image;
            emit recordingPreviewChanged();
        });
}
bool AppController::exportTimelapse(const QUrl& url, const QVariantMap& values,
                                    const QString& timeline, const QStringList& sessions,
                                    bool partial) {
    if (documentEditing() || saving() || m_recordingExporting || recordingRepairing())
        return false;
    TimelapseExportSettings settings;
    settings.seconds = values.value("seconds", 120).toInt();
    settings.fps = values.value("fps", 30).toInt();
    settings.maxSide = values.value("maxSide", 1920).toInt();
    settings.endSeconds = values.value("endSeconds", 5).toInt();
    settings.introSeconds = values.value("introSeconds", 0).toInt();
    settings.inputFps = values.value("inputFps", 0).toDouble();
    const QString path = localPathFrom(url);
    if (!path.isEmpty() && !validateRecordingDestination(path))
        return false;
    if (path.isEmpty() || !settings.valid() || !m_recordTimelines ||
        timelineById(m_recordTimelines->manifest(), timeline).isEmpty()) {
        setLastError(tr("Choose a timeline and valid export settings."));
        return false;
    }
    if (!partial && (m_recorder->hasError() || m_recorder->hasGaps()) &&
        timeline == activeRecordingTimeline()) {
        setLastError(tr("Confirm Partial export for this incomplete recording."), "recording");
        return false;
    }
    const QString spool = m_recorder->spoolPath();
    const auto capture =
        m_recorder->isActive() ? m_recorder->capture(canvasImage(), tr("Final"), true) : 0;
    if (m_recorder->isActive() && !capture && !partial)
        return false;
    m_recordCancel = std::make_shared<std::atomic_bool>(false);
    m_recordQuiet = false;
    m_recordingExporting = true;
    emit recordingExportingChanged();
    awaitTimelapse(path, settings, timeline, sessions, partial, spool, capture);
    return true;
}
void AppController::awaitTimelapse(const QString& path, const TimelapseExportSettings& settings,
                                   const QString& id, const QStringList& selected, bool partial,
                                   const QString& spool, quint64 capture) {
    if (m_recordCancel->load()) {
        m_recordingExporting = false;
        emit recordingExportingChanged();
        setStatusMessage(tr("Recording export cancelled."));
        return;
    }
    if (spool != m_recorder->spoolPath() || (!partial && m_recorder->hasError())) {
        m_recordingExporting = false;
        emit recordingExportingChanged();
        setLastError(tr("Final recording frame was not acknowledged. Use Partial export."),
                     "recording");
        return;
    }
    if (capture && m_recorder->lastStoredCaptureId() < capture && !m_recorder->hasError()) {
        QTimer::singleShot(25, this, [=, this]() {
            awaitTimelapse(path, settings, id, selected, partial, spool, capture);
        });
        return;
    }
    const bool bundle = QFileInfo(path).suffix().compare("prismrec", Qt::CaseInsensitive) == 0;
    const auto manifest = exportManifest(m_recordTimelines->manifest(), id, selected, bundle);
    const auto names = sourceNames(manifest);
    const QString root = QFileInfo(spool).absolutePath();
    for (const auto& name : names)
        if (nativePath(path) == nativePath(root + "/" + name)) {
            m_recordingExporting = false;
            emit recordingExportingChanged();
            setLastError(tr("Choose a destination outside source recording files."));
            return;
        }
    const qint64 bytes =
        capture ? m_recorder->validBytesThroughCaptureId(capture) : m_recorder->validBytes();
    const bool livePartial = m_recorder->hasError() || m_recorder->hasGaps() ||
                             (capture && m_recorder->lastStoredCaptureId() < capture);
    m_recordingExporting = false;
    // Callback is created inside the worker after runRecordingTask installs its cancel token.
    runRecordingTask(
        [this, path, settings, manifest, names, root, spool, bytes, livePartial, partial,
         bundle]() {
            auto progress = recordingProgressCallback();
            auto snapshot =
                readSources(root, names, sessionName(spool), bytes, livePartial, progress);
            if (!snapshot.error.isEmpty())
                return snapshot.error;
            if (snapshot.partial && !partial)
                return QString(
                    "This timeline contains an incomplete session. Confirm Partial export.");
            QString error;
            if (bundle) {
                if (!RecordingTimeline::writeBundle(path, manifest, root, snapshot.limits, &error,
                                                    progress))
                    return error.isEmpty() ? QString("Could not export recording bundle.") : error;
            } else {
                auto frames = RecordExport::timelapse(snapshot.frames, settings, &error);
                if (frames.count <= 0)
                    return error;
                const auto suffix = QFileInfo(path).suffix().toLower();
                const bool ok = suffix == "mp4"   ? RecordExport::writeMp4(path, frames, &error)
                                : suffix == "gif" ? RecordExport::writeGif(path, frames, &error)
                                                  : false;
                if (!ok)
                    return error.isEmpty() ? QString("Could not write timelapse.") : error;
            }
            return QString();
        },
        [this, path, partial]() {
            setStatusMessage(
                (partial ? tr("Partial recording exported: %1") : tr("Recording exported: %1"))
                    .arg(QFileInfo(path).fileName()));
        },
        false);
}
bool AppController::importRecordingBundle(const QUrl& url) {
    const QString path = localPathFrom(url);
    if (path.isEmpty() || m_recordingExporting)
        return false;
    const QString root = QFileInfo(m_recorder->spoolPath()).absolutePath();
    auto temporary = std::make_shared<QTemporaryDir>(root + "/.import-XXXXXX");
    auto manifest = std::make_shared<QJsonObject>();
    struct Publication {
        QStringList files;
        bool committed = false;
        ~Publication() {
            if (!committed)
                for (const auto& file : files)
                    QFile::remove(file);
        }
    };
    auto publication = std::make_shared<Publication>();
    return runRecordingTask(
        [this, path, temporary, manifest, publication, root]() {
            QString error;
            if (!temporary->isValid())
                return QString("Cannot prepare import.");
            if (RecordingTimeline::isBundle(path)) {
                if (!RecordingTimeline::readBundle(path, temporary->path(), manifest.get(), &error,
                                                   recordingProgressCallback()))
                    return error;
            } else {
                if (!SessionRecorder::verify(path, &error, nullptr,
                                             [this]() { return !m_recordCancel->load(); }))
                    return error;
                const QString name =
                    QUuid::createUuid().toString(QUuid::WithoutBraces) + ".prismrec";
                if (!QFile::copy(path, temporary->filePath(name)))
                    return QString("Cannot stage recording.");
                QSaveFile metadata(temporary->filePath(name + ".meta.json"));
                const auto bytes =
                    QJsonDocument(QJsonObject{{"state", "recovered"}, {"hasGaps", true}}).toJson();
                if (!metadata.open(QIODevice::WriteOnly) || metadata.write(bytes) != bytes.size() ||
                    !metadata.commit())
                    return QString("Cannot stage recording metadata.");
                *manifest = QJsonObject{
                    {"version", 1},
                    {"timelines", QJsonArray{QJsonObject{
                                      {"id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                                      {"name", QFileInfo(path).fileName()},
                                      {"parent", ""},
                                      {"sessions", QJsonArray{name}}}}}};
            }
            // Extraction and publication stay on the destination volume and I/O worker.
            auto next = *manifest;
            auto rows = next["timelines"].toArray();
            QHash<QString, QString> remap;
            for (const auto& name : sourceNames(next)) {
                if (m_recordCancel->load())
                    return QString("Import cancelled.");
                const QString fresh =
                    QUuid::createUuid().toString(QUuid::WithoutBraces) + ".prismrec";
                remap[name] = fresh;
                for (const auto& suffix : QStringList{QString(), QString(".meta.json")}) {
                    const auto target = root + "/" + fresh + suffix;
                    if (QFileInfo::exists(target) ||
                        !QFile::rename(temporary->filePath(name + suffix), target))
                        return QString(
                            "Could not publish imported recording. Original bundle was kept.");
                    publication->files.append(target);
                }
            }
            for (int i = 0; i < rows.size(); ++i) {
                auto r = rows[i].toObject();
                auto sources = r["sessions"].toArray();
                for (int j = 0; j < sources.size(); ++j)
                    sources[j] = remap[sources[j].toString()];
                r["sessions"] = sources;
                rows[i] = r;
            }
            next["timelines"] = rows;
            *manifest = next;
            return QString();
        },
        [this, manifest, publication]() {
            if (!m_recordTimelines->merge(*manifest))
                setLastError(m_recordTimelines->error(), "recording");
            else {
                publication->committed = true;
                setStatusMessage(tr(
                    "Recording imported. Select a timeline to preview, export or link artwork."));
            }
            emit recordingChanged();
            emit recoveryChanged();
        },
        false);
}
