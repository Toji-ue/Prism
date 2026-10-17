#pragma once
#include "application/RecordExport.h"
#include <QJsonObject>
#include <QStringList>
#include <functional>

// Local links are separate from artwork and immutable session byte streams.
class RecordingTimeline {
public:
    explicit RecordingTimeline(QString root);
    bool begin(const QString& name, const QString& session);
    bool resolve(const QString& path, const QString& fingerprint, bool explicitLink = false);
    bool saved(const QString& path, const QString& fingerprint, const QString& nextSession = {});
    bool link(const QString& id, const QString& path, const QString& fingerprint);
    bool select(const QString& id);
    bool remove(const QString& id, const QStringList& protectedSessions);
    bool canDeleteSource(const QString& session) const;
    QString error() const { return m_error; }
    QString activeId() const { return m_active; }
    QString conflictId() const { return m_conflict; }
    QJsonObject active() const;
    QJsonObject manifest() const { return m_index; }
    QStringList sessions() const;
    bool merge(const QJsonObject& manifest);
    static bool validManifest(const QJsonObject& manifest);
    static QString fingerprint(const QString& path);
    static bool writeBundle(const QString& path, QJsonObject manifest, const QString& root,
                            const QJsonObject& byteLimits, QString* error,
                            const std::function<bool(int, int)>& progress = {});
    static bool readBundle(const QString& path, const QString& directory, QJsonObject* manifest,
                           QString* error, const std::function<bool(int, int)>& progress = {});
    static bool isBundle(const QString& path);

private:
    bool store(const QJsonObject& next);
    QString m_root, m_active, m_conflict, m_error;
    QJsonObject m_index;
    bool m_indexValid = true;
};
