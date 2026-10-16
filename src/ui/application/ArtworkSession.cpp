#include "application/ArtworkSession.h"
#include "project/ProjectIO.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QUuid>

namespace {
QString previousPath(const QString& path) {
    return path.left(path.size() - 6) + QStringLiteral(".previous.prism");
}
bool backupSnapshot(const QString& path) {
    if (!QFileInfo::exists(path))
        return true;
    QFile source(path);
    QSaveFile previous(previousPath(path));
    if (!source.open(QIODevice::ReadOnly) || !previous.open(QIODevice::WriteOnly))
        return false;
    while (!source.atEnd()) {
        const QByteArray chunk = source.read(64 * 1024);
        if (chunk.isEmpty() || previous.write(chunk) != chunk.size())
            return false;
    }
    return previous.commit();
}
} // namespace

ArtworkSession::ArtworkSession(prism::core::Document& document, prism::core::History& history,
                               QString root, QObject* parent)
    : QObject(parent), m_document(document), m_history(history), m_root(std::move(root)) {
    const int testInterval = qEnvironmentVariableIntValue("PRISM_AUTOSAVE_INTERVAL_MS");
    m_timer.setInterval(testInterval > 0 ? testInterval : 45000);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ArtworkSession::protectNow);
}

ArtworkSession::~ArtworkSession() {
    m_timer.stop();
    // Shutdown follows the guarded close. Drain already-owned snapshots even if
    // the event loop has stopped; normal writing never waits on the UI thread.
    if (m_writer) {
        m_writer->wait();
        disconnect(m_writer, nullptr, this, nullptr);
        delete m_writer;
    }
    for (const auto& job : m_jobs) {
        if (m_discardEpochs.contains(job.epoch))
            continue;
        if (!job.manual && !backupSnapshot(job.path))
            continue;
        (void)prism::io::ProjectIO::save(*job.snapshot, job.path);
    }
}

std::shared_ptr<const prism::core::Document> ArtworkSession::snapshot() const {
    auto copy =
        std::make_shared<prism::core::Document>(m_document.canvasSize(), m_document.colorProfile());
    copy->setCanvasOrigin(m_document.canvasOrigin());
    for (const auto& layer : m_document.layers())
        copy->addLayer(layer->clone());
    return copy;
}

void ArtworkSession::beginDocument(const QString& path, const QString& name, bool initialContents) {
    m_timer.stop();
    ++m_epoch;
    m_projectPath = path;
    m_name = name;
    m_recoveryPath.clear();
    m_lastProtected = {};
    m_protectedRevision = m_history.stateId();
    m_protectionPending = false;
    m_manualPending = false;
    m_saveStatus = path.isEmpty() ? QStringLiteral("Not saved") : QStringLiteral("Saved");
    m_autosaveStatus = "Idle";
    if (initialContents) {
        m_history.markUnsaved();
        m_protectedRevision = std::numeric_limits<std::uint64_t>::max();
        edited();
    }
    emit stateChanged();
}

void ArtworkSession::edited() {
    if (!m_history.isModified() && !m_projectPath.isEmpty())
        m_saveStatus = "Saved";
    else if (!m_manualPending)
        m_saveStatus = "Unsaved changes";
    if (m_history.isModified())
        m_discardEpochs.erase(m_epoch);
    if (m_enabled && m_history.stateId() != m_protectedRevision) {
        if (!m_timer.isActive() && !m_protectionPending)
            m_timer.start();
        // A prior protected revision remains useful while newer edits are pending.
        if (m_autosaveStatus == "Idle")
            m_autosaveStatus = "Pending";
    }
    emit stateChanged();
}

void ArtworkSession::setStrokeActive(bool active) {
    m_strokeActive = active;
    if (!active && m_protectionPending)
        protectNow();
}

void ArtworkSession::ensureRecoveryPath() {
    if (m_recoveryPath.isEmpty()) {
        const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_recoveryPath = QDir(m_root + "/artwork").filePath(id + ".prism");
    }
    // A saved checkpoint may have removed the sidecar names; later edits can
    // reuse this session path and must restore the readable recovery labels.
    const QString id = QFileInfo(m_recoveryPath).completeBaseName();
    QSettings settings;
    settings.setValue("recoveryNames/" + id, m_name);
    settings.setValue("recoveryNames/" + id + ".previous",
                      tr("%1 (previous snapshot)").arg(m_name));
}

void ArtworkSession::protectNow() {
    if (!m_enabled || m_history.stateId() == m_protectedRevision)
        return;
    if (m_strokeActive || m_writer) {
        m_protectionPending = true;
        return;
    }
    if (!QDir().mkpath(m_root + "/artwork")) {
        m_autosaveStatus = "Error";
        m_protectionPending = false;
        emit stateChanged();
        emit errorOccurred(tr(
            "Autosave could not create its recovery folder. Check storage permissions and retry."));
        m_timer.start();
        return;
    }
    ensureRecoveryPath();
    try {
        m_jobs.push_back({snapshot(), m_recoveryPath, m_recoveryPath, m_name, m_epoch,
                          m_history.stateId(), false});
    } catch (const std::bad_alloc&) {
        m_autosaveStatus = "Error";
        emit errorOccurred(tr("Not enough memory to prepare autosave. Save your project and close "
                              "unused applications."));
        m_timer.start();
        emit stateChanged();
        return;
    }
    m_protectionPending = false;
    m_autosaveStatus = "Saving";
    emit stateChanged();
    startNextJob();
}

void ArtworkSession::removeRecovery(const QString& path) {
    if (path.isEmpty())
        return;
    for (const auto& file : {path, previousPath(path)}) {
        QFile::remove(file);
        QSettings().remove("recoveryNames/" + QFileInfo(file).completeBaseName());
    }
    emit recoveryChanged();
}

void ArtworkSession::endDocument(bool discard) {
    m_timer.stop();
    if (discard) {
        m_discardEpochs.insert(m_epoch);
        if (!m_writer)
            removeRecovery(m_recoveryPath);
    } else if (m_enabled && m_history.isModified() && m_history.stateId() != m_protectedRevision) {
        ensureRecoveryPath();
        if (QDir().mkpath(m_root + "/artwork")) {
            try {
                m_jobs.push_back({snapshot(), m_recoveryPath, m_recoveryPath, m_name, m_epoch,
                                  m_history.stateId(), false});
                startNextJob();
            } catch (const std::bad_alloc&) {
                emit errorOccurred(
                    tr("The outgoing artwork could not be backed up: not enough memory."));
            }
        } else
            emit errorOccurred(tr(
                "The outgoing artwork could not be backed up: recovery storage is unavailable."));
    }
}

void ArtworkSession::adoptRecovery(const QString& path) {
    m_projectPath.clear();
    m_recoveryPath = path;
    m_history.markUnsaved();
    m_protectedRevision = m_history.stateId();
    m_autosaveStatus = "Protected";
    m_saveStatus = "Not saved";
    emit stateChanged();
}

void ArtworkSession::acceptedSave(const QString& path, std::uint64_t revision) {
    m_projectPath = path;
    m_history.markSaved(revision);
    m_saveStatus =
        m_history.isModified() ? QStringLiteral("Unsaved changes") : QStringLiteral("Saved");
    if (!m_history.isModified()) {
        m_timer.stop();
        m_protectedRevision = revision;
        m_discardEpochs.insert(m_epoch);
        if (!m_writer)
            removeRecovery(m_recoveryPath);
        m_autosaveStatus = "Idle";
    }
    emit stateChanged();
}

bool ArtworkSession::saveAsync(const QString& path) {
    if (m_manualPending || path.isEmpty() || m_strokeActive)
        return false;
    try {
        m_jobs.push_back(
            {snapshot(), path, m_recoveryPath, m_name, m_epoch, m_history.stateId(), true});
    } catch (const std::bad_alloc&) {
        emit errorOccurred(
            tr("Not enough memory to prepare Save. Your current artwork is still open."), true);
        return false;
    }
    m_manualPending = true;
    m_saveStatus = "Saving";
    emit stateChanged();
    startNextJob();
    return true;
}

void ArtworkSession::startNextJob() {
    if (m_writer || m_jobs.empty())
        return;
    const Job job = m_jobs.front();
    m_jobs.pop_front();
    if (!job.manual && m_discardEpochs.contains(job.epoch)) {
        removeRecovery(job.recoveryPath);
        startNextJob();
        return;
    }
    auto success = std::make_shared<bool>(false);
    auto* thread = QThread::create([job, success]() {
        try {
            *success = (job.manual || backupSnapshot(job.path)) &&
                       prism::io::ProjectIO::save(*job.snapshot, job.path);
        } catch (const std::exception&) {
            *success = false;
        }
    });
    m_writer = thread;
    connect(thread, &QThread::finished, this, [this, thread, job, success]() {
        if (m_writer == thread)
            m_writer = nullptr;
        thread->deleteLater();
        if (m_discardEpochs.contains(job.epoch))
            removeRecovery(job.recoveryPath);
        if (job.epoch == m_epoch) {
            if (job.manual) {
                m_manualPending = false;
                if (*success)
                    acceptedSave(job.path, job.revision);
                else
                    m_saveStatus = "Error";
                emit saveFinished(*success, job.path);
            } else if (!m_discardEpochs.contains(job.epoch)) {
                if (*success) {
                    m_protectedRevision = job.revision;
                    m_lastProtected = QDateTime::currentDateTime();
                    m_autosaveStatus = "Protected";
                    if (m_history.stateId() != m_protectedRevision && !m_timer.isActive())
                        m_timer.start();
                } else {
                    m_autosaveStatus = "Error";
                    m_timer.start();
                }
            }
        }
        if (!*success && !m_discardEpochs.contains(job.epoch))
            emit errorOccurred(tr("Could not %1 %2. Your existing files were kept; check free "
                                  "space and permissions, then retry.")
                                   .arg(job.manual ? tr("save") : tr("autosave"), job.name),
                               job.manual);
        emit stateChanged();
        emit recoveryChanged();
        startNextJob();
        if (m_protectionPending && !m_writer)
            protectNow();
    });
    thread->start();
}
