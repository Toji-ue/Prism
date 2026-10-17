#include "application/RecordingTimeline.h"
#include "application/SessionRecorder.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <cmath>

namespace {
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString canonical(const QString& path) {
    const QFileInfo info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty())
        result = info.absoluteFilePath();
#ifdef Q_OS_WIN
    result = result.toCaseFolded();
#endif
    return result;
}
bool safeName(const QString& name) {
    return name.endsWith(".prismrec") && !QUuid(name.chopped(9)).isNull();
}
QByteArray hashPrefix(QFile& file, qint64 bytes, const std::function<bool()>& keepGoing = {}) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    file.seek(0);
    while (bytes > 0) {
        if (keepGoing && !keepGoing())
            return {};
        auto chunk = file.read(std::min<qint64>(bytes, 1024 * 1024));
        if (chunk.isEmpty())
            return {};
        hash.addData(chunk);
        bytes -= chunk.size();
    }
    return hash.result().toHex();
}
} // namespace
RecordingTimeline::RecordingTimeline(QString root) : m_root(std::move(root)) {
    m_index = {{"version", 1}, {"timelines", QJsonArray()}};
    QFile file(m_root + "/timelines.json");
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly) || file.size() > 8 * 1024 * 1024) {
        m_indexValid = false;
        m_error = "Cannot read recording timeline index.";
        return;
    }
    const auto parsed = QJsonDocument::fromJson(file.readAll()).object();
    if (!validManifest(parsed)) {
        m_indexValid = false;
        m_error = "Recording timeline index is damaged. It was kept unchanged.";
        return;
    }
    m_index = parsed;
}
bool RecordingTimeline::canDeleteSource(const QString& session) const {
    if (!m_indexValid)
        return false;
    for (const auto& row : m_index["timelines"].toArray())
        if (row.toObject()["sessions"].toArray().contains(session))
            return false;
    return true;
}
bool RecordingTimeline::validManifest(const QJsonObject& manifest) {
    const auto rows = manifest[QLatin1String("timelines")].toArray();
    if (manifest[QLatin1String("version")].toInt() != 1 ||
        !manifest[QLatin1String("timelines")].isArray() || rows.size() > 4096)
        return false;
    QHash<QString, QString> parents;
    for (const auto& row : rows) {
        const auto r = row.toObject();
        const auto id = r[QLatin1String("id")].toString();
        if (QUuid(id).isNull() || parents.contains(id) ||
            r[QLatin1String("name")].toString().size() > 1024 ||
            !r[QLatin1String("sessions")].isArray() ||
            r[QLatin1String("sessions")].toArray().size() > 4096)
            return false;
        parents[id] = r[QLatin1String("parent")].toString();
        QSet<QString> seen;
        for (const auto& value : r[QLatin1String("sessions")].toArray()) {
            const auto name = value.toString();
            if (!safeName(name) || seen.contains(name))
                return false;
            seen.insert(name);
        }
    }
    for (auto it = parents.begin(); it != parents.end(); ++it) {
        QSet<QString> seen;
        QString id = it.key();
        while (!id.isEmpty()) {
            if (!parents.contains(id) || seen.contains(id))
                return false;
            seen.insert(id);
            id = parents[id];
        }
    }
    return true;
}
bool RecordingTimeline::store(const QJsonObject& next) {
    if (!m_indexValid)
        return false;
    if (!validManifest(next)) {
        m_error =
            "Invalid recording timeline or local index limit reached. Original index was kept.";
        return false;
    }
    m_error.clear();
    QSaveFile file(m_root + "/timelines.json");
    const auto bytes = QJsonDocument(next).toJson(QJsonDocument::Compact);
    if (bytes.size() > 8 * 1024 * 1024) {
        m_error = "Recording timeline index exceeds its safe size limit. Original index was kept.";
        return false;
    }
    if (!QDir().mkpath(m_root) || !file.open(QIODevice::WriteOnly) ||
        file.write(bytes) != bytes.size() || !file.commit()) {
        m_error = "Could not save recording timeline links. Recording frames were kept.";
        return false;
    }
    m_index = next;
    return true;
}
QJsonObject RecordingTimeline::active() const {
    for (const auto& row : m_index[QLatin1String("timelines")].toArray())
        if (row[QLatin1String("id")].toString() == m_active)
            return row.toObject();
    return {};
}
QStringList RecordingTimeline::sessions() const {
    QStringList result;
    for (const auto& value : active()[QLatin1String("sessions")].toArray())
        result << value.toString();
    return result;
}
bool RecordingTimeline::begin(const QString& name, const QString& session) {
    if (!safeName(session))
        return false;
    const QString id = uuid();
    auto next = m_index;
    auto rows = next[QLatin1String("timelines")].toArray();
    rows.append(
        QJsonObject{{"id", id}, {"name", name}, {"sessions", QJsonArray{session}}, {"parent", ""}});
    next[QLatin1String("timelines")] = rows;
    if (!store(next))
        return false;
    m_active = id;
    m_conflict.clear();
    return true;
}
bool RecordingTimeline::resolve(const QString& path, const QString& fingerprint,
                                bool explicitLink) {
    if (path.isEmpty() || fingerprint.isEmpty())
        return false;
    const auto own = active();
    if (own.isEmpty())
        return false;
    auto next = m_index;
    auto rows = next[QLatin1String("timelines")].toArray();
    int match = -1;
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i][QLatin1String("path")].toString() == canonical(path) &&
            rows[i][QLatin1String("id")].toString() != m_active) {
            match = i;
            break;
        }
    if (match >= 0) {
        auto target = rows[match].toObject();
        if (!explicitLink && target[QLatin1String("fingerprint")].toString() != fingerprint) {
            m_conflict = target[QLatin1String("id")].toString();
            return false;
        }
        auto sources = target[QLatin1String("sessions")].toArray();
        for (const auto& value : own[QLatin1String("sessions")].toArray())
            if (!sources.contains(value))
                sources.append(value);
        target[QLatin1String("sessions")] = sources;
        target[QLatin1String("fingerprint")] = fingerprint;
        rows[match] = target;
        for (int i = 0; i < rows.size(); ++i)
            if (rows[i][QLatin1String("id")].toString() == m_active) {
                rows.removeAt(i);
                break;
            }
        next[QLatin1String("timelines")] = rows;
        if (!store(next))
            return false;
        m_active = target[QLatin1String("id")].toString();
        m_conflict.clear();
        return true;
    }
    return saved(path, fingerprint);
}
bool RecordingTimeline::saved(const QString& path, const QString& fingerprint,
                              const QString& nextSession) {
    auto own = active();
    if (own.isEmpty() || fingerprint.isEmpty())
        return false;
    auto next = m_index;
    auto rows = next[QLatin1String("timelines")].toArray();
    const bool fork = !own[QLatin1String("path")].toString().isEmpty() &&
                      own[QLatin1String("path")].toString() != canonical(path);
    if (fork && !safeName(nextSession))
        return false;
    if (fork) {
        own[QLatin1String("parent")] = m_active;
        own[QLatin1String("id")] = uuid();
        auto sources = own[QLatin1String("sessions")].toArray();
        sources.append(nextSession);
        own[QLatin1String("sessions")] = sources;
    }
    own[QLatin1String("path")] = canonical(path);
    own[QLatin1String("fingerprint")] = fingerprint;
    own[QLatin1String("name")] = QFileInfo(path).fileName();
    // A path belongs to only one active binding, even after overwriting via Save As.
    for (int i = 0; i < rows.size(); ++i) {
        auto r = rows[i].toObject();
        if (r[QLatin1String("id")].toString() == own[QLatin1String("id")].toString()) {
            rows[i] = own;
            continue;
        }
        if (r[QLatin1String("path")].toString() == canonical(path)) {
            r.remove("path");
            r.remove("fingerprint");
            rows[i] = r;
        }
    }
    if (fork)
        rows.append(own);
    next[QLatin1String("timelines")] = rows;
    if (!store(next))
        return false;
    m_active = own[QLatin1String("id")].toString();
    m_conflict.clear();
    return true;
}
bool RecordingTimeline::link(const QString& id, const QString& path, const QString& fingerprint) {
    if (path.isEmpty() || fingerprint.isEmpty())
        return false;
    auto next = m_index;
    auto rows = next["timelines"].toArray();
    bool found = false;
    for (int i = 0; i < rows.size(); ++i) {
        auto row = rows[i].toObject();
        if (row["id"].toString() == id) {
            row["path"] = canonical(path);
            row["fingerprint"] = fingerprint;
            found = true;
        } else if (row["path"].toString() == canonical(path)) {
            row.remove("path");
            row.remove("fingerprint");
        }
        rows[i] = row;
    }
    next["timelines"] = rows;
    return found && store(next);
}
bool RecordingTimeline::select(const QString& id) {
    for (const auto& row : m_index[QLatin1String("timelines")].toArray())
        if (row[QLatin1String("id")].toString() == id) {
            m_active = id;
            return true;
        }
    return false;
}
bool RecordingTimeline::remove(const QString& id, const QStringList& protectedSessions) {
    if (id == m_active)
        return false;
    auto next = m_index;
    auto rows = next[QLatin1String("timelines")].toArray();
    QStringList removed;
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i][QLatin1String("id")].toString() == id) {
            for (const auto& s : rows[i][QLatin1String("sessions")].toArray()) {
                if (protectedSessions.contains(s.toString()))
                    return false;
                removed << s.toString();
            }
            const QString parent = rows[i][QLatin1String("parent")].toString();
            rows.removeAt(i);
            for (int j = 0; j < rows.size(); ++j)
                if (rows[j][QLatin1String("parent")].toString() == id) {
                    auto child = rows[j].toObject();
                    child[QLatin1String("parent")] = parent;
                    rows[j] = child;
                }
            break;
        }
    next[QLatin1String("timelines")] = rows;
    if (!store(next))
        return false;
    QSet<QString> used;
    for (const auto& r : rows)
        for (const auto& s : r[QLatin1String("sessions")].toArray())
            used.insert(s.toString());
    bool removedAll = true;
    for (const auto& name : removed)
        if (!used.contains(name) && !protectedSessions.contains(name)) {
            for (const auto& suffix : QStringList{QString(), QString(".meta.json")}) {
                const auto path = m_root + "/" + name + suffix;
                if (QFileInfo::exists(path) && !QFile::remove(path))
                    removedAll = false;
            }
        }
    if (!removedAll)
        m_error = "Timeline link removed, but some unshared files could not be deleted. Check "
                  "permissions in Storage.";
    return removedAll;
}
bool RecordingTimeline::merge(const QJsonObject& manifest) {
    if (!validManifest(manifest))
        return false;
    auto next = m_index;
    auto rows = next[QLatin1String("timelines")].toArray();
    QSet<QString> ids;
    for (const auto& r : rows)
        ids.insert(r[QLatin1String("id")].toString());
    QHash<QString, QString> remap;
    for (const auto& r : manifest[QLatin1String("timelines")].toArray())
        remap[r[QLatin1String("id")].toString()] = uuid();
    for (const auto& r : manifest[QLatin1String("timelines")].toArray()) {
        auto row = r.toObject();
        row[QLatin1String("id")] = remap[row[QLatin1String("id")].toString()];
        row[QLatin1String("parent")] = remap.value(row[QLatin1String("parent")].toString());
        row.remove("path");
        row.remove("fingerprint");
        rows.append(row);
    }
    next[QLatin1String("timelines")] = rows;
    return store(next);
}
QString RecordingTimeline::fingerprint(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLatin1(hashPrefix(file, file.size()));
}
bool RecordingTimeline::isBundle(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return QJsonDocument::fromJson(file.readLine(8 * 1024 * 1024 + 1))
               .object()[QLatin1String("version")]
               .toInt() == 3;
}
bool RecordingTimeline::writeBundle(const QString& path, QJsonObject manifest, const QString& root,
                                    const QJsonObject& limits, QString* error,
                                    const std::function<bool(int, int)>& progress) {
    if (!validManifest(manifest)) {
        if (error)
            *error = "Invalid recording timeline.";
        return false;
    }
    auto rows = manifest["timelines"].toArray();
    for (int i = 0; i < rows.size(); ++i) {
        auto row = rows[i].toObject();
        row.remove("path");
        row.remove("fingerprint");
        rows[i] = row;
    }
    manifest["timelines"] = rows;
    QSet<QString> names;
    for (const auto& row : manifest[QLatin1String("timelines")].toArray())
        for (const auto& s : row[QLatin1String("sessions")].toArray())
            names.insert(s.toString());
    QJsonArray entries;
    for (const auto& name : names) {
        QFile file(root + "/" + name);
        if (QFileInfo(file).isSymLink() || !file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = "A recording session is missing.";
            return false;
        }
        const qint64 size = limits.contains(name) ? qint64(limits[name].toDouble()) : file.size();
        if (size <= 0 || size > file.size()) {
            if (error)
                *error = "Recording has no acknowledged frames.";
            return false;
        }
        QFile metadata(file.fileName() + ".meta.json");
        QJsonObject meta;
        if (metadata.open(QIODevice::ReadOnly))
            meta = QJsonDocument::fromJson(metadata.readAll()).object();
        if (limits["_active"].toString() == name) {
            meta["state"] = "completed";
            meta["hasGaps"] = limits["_partial"].toBool();
        }
        const auto sha =
            hashPrefix(file, size, [&]() { return !progress || progress(0, names.size()); });
        if (sha.size() != 64) {
            if (error)
                *error = "Could not read recording or export was cancelled.";
            return false;
        }
        entries.append(QJsonObject{{"name", name},
                                   {"bytes", double(size)},
                                   {"sha256", QString::fromLatin1(sha)},
                                   {"metadata", meta}});
    }
    const QByteArray header = QJsonDocument(QJsonObject{{"format", "prism-record"},
                                                        {"version", 3},
                                                        {"manifest", manifest},
                                                        {"entries", entries}})
                                  .toJson(QJsonDocument::Compact) +
                              "\n";
    if (header.size() > 8 * 1024 * 1024) {
        if (error)
            *error = "Recording manifest is too large.";
        return false;
    }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly) || output.write(header) != header.size())
        return false;
    int index = 0;
    for (const auto& e : entries) {
        QFile input(root + "/" + e[QLatin1String("name")].toString());
        if (!input.open(QIODevice::ReadOnly))
            return false;
        qint64 remaining = qint64(e[QLatin1String("bytes")].toDouble());
        while (remaining > 0) {
            if (progress && !progress(index, entries.size()))
                return false;
            const auto chunk = input.read(std::min<qint64>(remaining, 1024 * 1024));
            if (chunk.isEmpty() || output.write(chunk) != chunk.size())
                return false;
            remaining -= chunk.size();
        }
        ++index;
    }
    return (!progress || progress(index, index)) && output.commit();
}
bool RecordingTimeline::readBundle(const QString& path, const QString& directory,
                                   QJsonObject* manifest, QString* error,
                                   const std::function<bool(int, int)>& progress) {
    const auto fail = [&]() {
        if (error && error->isEmpty())
            *error = "Invalid or damaged recording bundle.";
        return false;
    };
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly))
        return fail();
    const auto line = input.readLine(8 * 1024 * 1024 + 1);
    if (!line.endsWith('\n') || line.size() > 8 * 1024 * 1024)
        return fail();
    const auto header = QJsonDocument::fromJson(line).object();
    const auto entries = header[QLatin1String("entries")].toArray();
    auto data = header[QLatin1String("manifest")].toObject();
    if (header[QLatin1String("format")].toString() != "prism-record" ||
        header[QLatin1String("version")].toInt() != 3 || !validManifest(data) ||
        entries.isEmpty() || entries.size() > 4096)
        return fail();
    QSet<QString> names;
    qint64 total = 0;
    for (const auto& entry : entries) {
        const QString name = entry[QLatin1String("name")].toString();
        const double n = entry[QLatin1String("bytes")].toDouble(-1);
        const auto sha = entry[QLatin1String("sha256")].toString();
        if (!safeName(name) || names.contains(name) || !std::isfinite(n) || n <= 0 ||
            n > double(1LL << 40) || std::floor(n) != n || sha.size() != 64 ||
            QByteArray::fromHex(sha.toLatin1()).size() != 32 ||
            qint64(n) > input.size() - input.pos() - total)
            return fail();
        names.insert(name);
        total += qint64(n);
    }
    if (total != input.size() - input.pos())
        return fail();
    QSet<QString> referenced;
    for (const auto& row : data[QLatin1String("timelines")].toArray())
        for (const auto& s : row[QLatin1String("sessions")].toArray()) {
            if (!names.contains(s.toString()))
                return fail();
            referenced.insert(s.toString());
        }
    if (referenced != names || !QDir().mkpath(directory))
        return fail();
    int index = 0;
    for (const auto& entry : entries) {
        const QString target = directory + "/" + entry[QLatin1String("name")].toString();
        if (QFileInfo::exists(target))
            return fail();
        QSaveFile output(target);
        if (!output.open(QIODevice::WriteOnly))
            return fail();
        QCryptographicHash hash(QCryptographicHash::Sha256);
        qint64 remaining = qint64(entry[QLatin1String("bytes")].toDouble());
        while (remaining > 0) {
            if (progress && !progress(index, entries.size()))
                return fail();
            const auto chunk = input.read(std::min<qint64>(remaining, 1024 * 1024));
            if (chunk.isEmpty() || output.write(chunk) != chunk.size())
                return fail();
            hash.addData(chunk);
            remaining -= chunk.size();
        }
        if (QString::fromLatin1(hash.result().toHex()) !=
                entry[QLatin1String("sha256")].toString() ||
            !output.commit() ||
            (isBundle(target) || !SessionRecorder::verify(target, error, nullptr, [&]() {
                 return !progress || progress(index, entries.size());
             })))
            return fail();
        QSaveFile meta(target + ".meta.json");
        auto info = entry[QLatin1String("metadata")].toObject();
        const auto bytes = QJsonDocument(info).toJson(QJsonDocument::Compact);
        if (!meta.open(QIODevice::WriteOnly) || meta.write(bytes) != bytes.size() || !meta.commit())
            return fail();
        ++index;
    }
    if (progress && !progress(index, index))
        return fail();
    if (manifest)
        *manifest = data;
    return true;
}
