#pragma once
#include "document/Document.h"
#include "history/History.h"
#include <QDateTime>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <deque>
#include <memory>
#include <set>

/// Project-save checkpoints and recovery snapshots have independent lifetimes.
/// Jobs own immutable documents, so replacing the canvas cannot change a write.
class ArtworkSession : public QObject {
    Q_OBJECT
public:
    ArtworkSession(prism::core::Document& document, prism::core::History& history,
                   QString recoveryRoot, QObject* parent = nullptr);
    ~ArtworkSession() override;
    void edited();
    void setStrokeActive(bool active);
    void setEnabled(bool enabled) { m_enabled = enabled; }
    void beginDocument(const QString& path, const QString& name, bool initialContents = false);
    void endDocument(bool discard);
    void adoptRecovery(const QString& path);
    void protectNow();
    bool saveAsync(const QString& path);
    void acceptedSave(const QString& path, std::uint64_t revision);
    [[nodiscard]] QString projectPath() const { return m_projectPath; }
    [[nodiscard]] QString saveStatus() const { return m_saveStatus; }
    [[nodiscard]] QString autosaveStatus() const { return m_autosaveStatus; }
    [[nodiscard]] QString lastProtectedTime() const {
        return m_lastProtected.toString(Qt::ISODate);
    }
    [[nodiscard]] bool isSaving() const { return m_manualPending; }
    [[nodiscard]] bool hasUnsavedChanges() const { return m_history.isModified(); }
    [[nodiscard]] bool isWriting() const { return m_writer != nullptr; }
signals:
    void stateChanged();
    void recoveryChanged();
    void errorOccurred(const QString& message);
    void saveFinished(bool success, const QString& path);

private:
    struct Job {
        std::shared_ptr<const prism::core::Document> snapshot;
        QString path;
        QString recoveryPath;
        QString name;
        std::uint64_t epoch = 0;
        std::uint64_t revision = 0;
        bool manual = false;
    };
    std::shared_ptr<const prism::core::Document> snapshot() const;
    void ensureRecoveryPath();
    void startNextJob();
    void removeRecovery(const QString& path);
    prism::core::Document& m_document;
    prism::core::History& m_history;
    QString m_root;
    QString m_projectPath;
    QString m_name;
    QString m_recoveryPath;
    QString m_saveStatus = "Not saved";
    QString m_autosaveStatus = "Idle";
    QDateTime m_lastProtected;
    QTimer m_timer;
    QThread* m_writer = nullptr;
    std::deque<Job> m_jobs;
    std::set<std::uint64_t> m_discardEpochs;
    std::uint64_t m_epoch = 1;
    std::uint64_t m_protectedRevision = 0;
    bool m_enabled = true;
    bool m_strokeActive = false;
    bool m_protectionPending = false;
    bool m_manualPending = false;
};
