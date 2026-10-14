#include "application/SessionRecorder.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <gtest/gtest.h>

namespace {
class SessionRecorderTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!QCoreApplication::instance())
            application = std::make_unique<QCoreApplication>(argc, argv);
        previousRoot = qgetenv("PRISM_RECOVERY_DIR");
        rootWasSet = qEnvironmentVariableIsSet("PRISM_RECOVERY_DIR");
        ASSERT_TRUE(directory.isValid());
        qputenv("PRISM_RECOVERY_DIR", directory.path().toUtf8());
        previousFault = qgetenv("PRISM_TEST_RECORD_FAULT");
        previousFailAfter = qgetenv("PRISM_TEST_RECORD_FAIL_AFTER");
        faultWasSet = qEnvironmentVariableIsSet("PRISM_TEST_RECORD_FAULT");
        failAfterWasSet = qEnvironmentVariableIsSet("PRISM_TEST_RECORD_FAIL_AFTER");
        qunsetenv("PRISM_TEST_RECORD_FAULT");
        qunsetenv("PRISM_TEST_RECORD_FAIL_AFTER");
    }

    void TearDown() override {
        if (rootWasSet)
            qputenv("PRISM_RECOVERY_DIR", previousRoot);
        else
            qunsetenv("PRISM_RECOVERY_DIR");
        if (faultWasSet)
            qputenv("PRISM_TEST_RECORD_FAULT", previousFault);
        else
            qunsetenv("PRISM_TEST_RECORD_FAULT");
        if (failAfterWasSet)
            qputenv("PRISM_TEST_RECORD_FAIL_AFTER", previousFailAfter);
        else
            qunsetenv("PRISM_TEST_RECORD_FAIL_AFTER");
    }

    QImage image(Qt::GlobalColor color = Qt::red) {
        QImage result(32, 24, QImage::Format_RGBA8888);
        result.fill(color);
        return result;
    }

    QJsonObject metadata(const SessionRecorder& recorder) {
        QFile file(recorder.spoolPath() + ".meta.json");
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QJsonDocument::fromJson(file.readAll()).object();
    }

    QTemporaryDir directory;
    QByteArray previousRoot;
    bool rootWasSet = false;
    QByteArray previousFault;
    QByteArray previousFailAfter;
    bool faultWasSet = false;
    bool failAfterWasSet = false;
    int argc = 1;
    char name[18] = "recorder-tests";
    char* argv[2] = {name, nullptr};
    std::unique_ptr<QCoreApplication> application;
};
} // namespace

TEST_F(SessionRecorderTest, BlockedStorageReportsFailureAndRetryAcknowledgesFrames) {
    const QString blocked = directory.filePath("blocked");
    QFile blocker(blocked);
    ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    qputenv("PRISM_RECOVERY_DIR", blocked.toUtf8());

    SessionRecorder recorder;
    recorder.capture(image(), "attempt", true);
    EXPECT_FALSE(recorder.flush());
    EXPECT_TRUE(recorder.hasError());
    EXPECT_FALSE(recorder.lastError().isEmpty());
    EXPECT_FALSE(recorder.isActive());
    EXPECT_TRUE(recorder.hasGaps());
    EXPECT_EQ(recorder.lastStoredCaptureId(), 0U);
    EXPECT_EQ(recorder.capture(image(), "blocked", true), 0U);

    ASSERT_TRUE(QFile::remove(blocked));
    ASSERT_TRUE(QDir().mkpath(blocked));
    ASSERT_TRUE(recorder.retry());
    const quint64 finalId = recorder.capture(image(Qt::green), "final", true);
    ASSERT_NE(finalId, 0U);
    ASSERT_TRUE(recorder.flush());
    EXPECT_EQ(recorder.lastStoredCaptureId(), finalId);
    EXPECT_FALSE(recorder.hasError());
    EXPECT_TRUE(recorder.hasGaps());
    EXPECT_TRUE(SessionRecorder::verify(recorder.spoolPath()));
}

TEST_F(SessionRecorderTest, AsyncRetryKeepsEventsMovingAndRejectsSessionReplacement) {
    qputenv("PRISM_TEST_RECORD_FAULT", "write");
    SessionRecorder recorder;
    recorder.capture(image(), "fault", true);
    ASSERT_FALSE(recorder.flush());
    qunsetenv("PRISM_TEST_RECORD_FAULT");
    const QString source = recorder.spoolPath();
    QSignalSpy finished(&recorder, &SessionRecorder::retryFinished);
    int events = 0;
    QTimer heartbeat;
    heartbeat.setInterval(1);
    QObject::connect(&heartbeat, &QTimer::timeout, [&]() { ++events; });
    heartbeat.start();
    ASSERT_TRUE(recorder.retryAsync());
    EXPECT_TRUE(recorder.isRepairing());
    EXPECT_FALSE(recorder.retryAsync());
    recorder.restart();
    EXPECT_EQ(recorder.spoolPath(), source);
    EXPECT_EQ(recorder.capture(image(), "while repairing", true), 0U);
    QElapsedTimer limit;
    limit.start();
    while (finished.isEmpty() && limit.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    ASSERT_EQ(finished.count(), 1);
    EXPECT_TRUE(finished[0][0].toBool());
    EXPECT_FALSE(recorder.isRepairing());
    EXPECT_GT(events, 0);
    EXPECT_TRUE(recorder.hasGaps());
    ASSERT_NE(recorder.capture(image(Qt::green), "final", true), 0U);
    ASSERT_TRUE(recorder.flush());
    EXPECT_TRUE(SessionRecorder::verify(source));
}

TEST_F(SessionRecorderTest, AsyncRecoveryMarksCleanCrashPrefixIncompleteAndKeepsMetadata) {
    SessionRecorder source;
    source.capture(image(), "last complete frame", true);
    ASSERT_TRUE(source.flush());
    const QString path = source.spoolPath();
    QFile metadataFile(path + ".meta.json");
    ASSERT_TRUE(metadataFile.open(QIODevice::ReadOnly));
    const QByteArray original = metadataFile.readAll();
    metadataFile.close();
    SessionRecorder recovered;
    QSignalSpy ready(&recovered, &SessionRecorder::archiveLoaded);
    ASSERT_TRUE(recovered.adoptRecoveredAsync(path));
    EXPECT_TRUE(recovered.isRepairing());
    EXPECT_FALSE(recovered.adoptRecoveredAsync(path));
    QElapsedTimer timer;
    timer.start();
    while (ready.isEmpty() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    ASSERT_EQ(ready.count(), 1);
    ASSERT_TRUE(ready[0][0].toBool());
    EXPECT_TRUE(recovered.hasGaps());
    EXPECT_FALSE(recovered.isActive());
    ASSERT_TRUE(metadataFile.open(QIODevice::ReadOnly));
    EXPECT_EQ(metadataFile.readAll(), original);
    metadataFile.close();
    ASSERT_TRUE(QFile::remove(metadataFile.fileName()));
    SessionRecorder withoutMetadata;
    ASSERT_TRUE(withoutMetadata.adoptRecovered(path));
    EXPECT_TRUE(withoutMetadata.hasGaps());
}

TEST_F(SessionRecorderTest, ExportPrefixEndsAtAcknowledgedCaptureAndSurvivesNewSession) {
    SessionRecorder recorder;
    const quint64 first = recorder.capture(image(Qt::red), "first", true);
    ASSERT_NE(first, 0U);
    ASSERT_TRUE(recorder.flush());
    const quint64 final = recorder.capture(image(Qt::green), "final", true);
    ASSERT_GT(final, first);
    ASSERT_TRUE(recorder.flush());
    ASSERT_NE(recorder.capture(image(Qt::blue), "later edit", true), 0U);
    ASSERT_TRUE(recorder.flush());

    auto frames = recorder.recordedFrames(0, final);
    ASSERT_EQ(frames.count, 2);
    EXPECT_EQ(recorder.frameCountThroughCaptureId(final), 2);
    EXPECT_EQ(recorder.frameCountThroughCaptureId(final + 100), 0);
    const qint64 prefixBytes = recorder.validBytesThroughCaptureId(final);
    ASSERT_GT(prefixBytes, 0);
    ASSERT_LT(prefixBytes, recorder.validBytes());
    const QString exported = directory.filePath("prefix.prismrec");
    ASSERT_TRUE(SessionRecorder::saveSnapshot(exported, recorder.spoolPath(), prefixBytes));
    int count = 0;
    ASSERT_TRUE(SessionRecorder::verify(exported, nullptr, &count));
    EXPECT_EQ(count, 2);
    recorder.restart();
    QImage decoded;
    qint64 ms = -1;
    ASSERT_TRUE(frames.fetch(1, decoded, ms));
    EXPECT_EQ(decoded.pixelColor(0, 0), QColor(Qt::green));
}

TEST_F(SessionRecorderTest, MetadataDistinguishesFinishedAndImmutableRecoveredSessions) {
    SessionRecorder recorder;
    recorder.setDocumentName("Comic page 7");
    ASSERT_NE(recorder.capture(image(), "ink", true), 0U);
    ASSERT_TRUE(recorder.flush());
    auto info = metadata(recorder);
    EXPECT_EQ(info["documentName"].toString(), "Comic page 7");
    EXPECT_EQ(info["state"].toString(), "recording");
    EXPECT_FALSE(info["created"].toString().isEmpty());
    EXPECT_FALSE(info["updated"].toString().isEmpty());
    EXPECT_FALSE(info["hasGaps"].toBool());
    recorder.markFinished();
    ASSERT_TRUE(recorder.flush());
    EXPECT_FALSE(recorder.isActive());
    EXPECT_EQ(metadata(recorder)["state"].toString(), "completed");
    EXPECT_EQ(recorder.capture(image(Qt::blue), "after finish", true), 0U);

    QFile original(recorder.spoolPath());
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    const QByteArray bytes = original.readAll();
    original.close();
    SessionRecorder recovered;
    ASSERT_TRUE(recovered.adoptRecovered(recorder.spoolPath()));
    EXPECT_FALSE(recovered.isActive());
    EXPECT_FALSE(recovered.retry());
    EXPECT_EQ(recovered.capture(image(Qt::green), "archive", true), 0U);
    recovered.markFinished();
    EXPECT_EQ(metadata(recovered)["state"].toString(), "completed");
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    EXPECT_EQ(original.readAll(), bytes);
    recovered.restart();
    ASSERT_TRUE(original.seek(0));
    EXPECT_EQ(original.readAll(), bytes);
}

TEST_F(SessionRecorderTest, InterruptedFrameRetryPreservesPrefixHashTimeAndStickyGap) {
    qputenv("PRISM_TEST_RECORD_FAULT", "write");
    qputenv("PRISM_TEST_RECORD_FAIL_AFTER", "1");
    SessionRecorder recorder;
    const quint64 first = recorder.capture(image(Qt::red), "first", true);
    ASSERT_TRUE(recorder.flush());
    QFile spool(recorder.spoolPath());
    ASSERT_TRUE(spool.open(QIODevice::ReadOnly));
    const QByteArray validPrefix = spool.readAll();
    spool.close();
    const quint64 missing = recorder.capture(image(Qt::green), "interrupted", true);
    ASSERT_GT(missing, first);
    EXPECT_FALSE(recorder.flush());
    EXPECT_EQ(recorder.lastStoredCaptureId(), first);
    EXPECT_EQ(recorder.validBytesThroughCaptureId(missing), 0);
    EXPECT_TRUE(recorder.hasError());
    EXPECT_TRUE(recorder.hasGaps());
    EXPECT_EQ(metadata(recorder)["state"].toString(), "error");
    EXPECT_TRUE(metadata(recorder)["hasGaps"].toBool());
    const QString refused = directory.filePath("normal.prismrec");
    EXPECT_FALSE(recorder.save(refused, QSize(32, 24)));
    EXPECT_FALSE(QFile::exists(refused));

    qunsetenv("PRISM_TEST_RECORD_FAULT");
    qunsetenv("PRISM_TEST_RECORD_FAIL_AFTER");
    ASSERT_TRUE(recorder.retry());
    ASSERT_TRUE(spool.open(QIODevice::ReadOnly));
    EXPECT_EQ(spool.readAll(), validPrefix);
    spool.close();
    const quint64 final = recorder.capture(image(Qt::blue), "final", true);
    ASSERT_GT(final, missing);
    ASSERT_TRUE(recorder.flush());
    EXPECT_EQ(recorder.lastStoredCaptureId(), final);
    EXPECT_TRUE(recorder.hasGaps());
    EXPECT_TRUE(metadata(recorder)["hasGaps"].toBool());
    ASSERT_TRUE(spool.open(QIODevice::ReadOnly));
    const auto lines = spool.readAll().split('\n');
    EXPECT_GT(QJsonDocument::fromJson(lines[2]).object()["ms"].toDouble(),
              QJsonDocument::fromJson(lines[1]).object()["ms"].toDouble());
    int count = 0;
    EXPECT_TRUE(SessionRecorder::verify(recorder.spoolPath(), nullptr, &count));
    EXPECT_EQ(count, 2);
    EXPECT_EQ(recorder.frameCountThroughCaptureId(final), 2);
}

TEST_F(SessionRecorderTest, EncodeAndFlushFailuresCannotAcknowledgeTheFinalFrame) {
    for (const QByteArray fault : {QByteArray("encode"), QByteArray("flush")}) {
        SCOPED_TRACE(fault.constData());
        qputenv("PRISM_TEST_RECORD_FAULT", fault);
        qputenv("PRISM_TEST_RECORD_FAIL_AFTER", "1");
        SessionRecorder recorder;
        const quint64 first = recorder.capture(image(Qt::red), "first", true);
        ASSERT_TRUE(recorder.flush());
        const quint64 final = recorder.capture(image(Qt::green), "failed final", true);
        ASSERT_GT(final, first);
        EXPECT_FALSE(recorder.flush());
        EXPECT_EQ(recorder.lastStoredCaptureId(), first);
        EXPECT_EQ(recorder.frameCountThroughCaptureId(final), 0);
        EXPECT_TRUE(recorder.hasError());
        EXPECT_TRUE(recorder.hasGaps());
        EXPECT_FALSE(recorder.lastError().isEmpty());
        qunsetenv("PRISM_TEST_RECORD_FAULT");
        qunsetenv("PRISM_TEST_RECORD_FAIL_AFTER");
        ASSERT_TRUE(recorder.retry());
        const quint64 retried = recorder.capture(image(Qt::blue), "retried final", true);
        ASSERT_GT(retried, final);
        ASSERT_TRUE(recorder.flush());
        EXPECT_EQ(recorder.lastStoredCaptureId(), retried);
        EXPECT_TRUE(recorder.hasGaps());
        EXPECT_TRUE(SessionRecorder::verify(recorder.spoolPath()));
        QImage last;
        qint64 ms = 0;
        ASSERT_TRUE(recorder.frameAt(recorder.frameCount() - 1, last, ms));
        EXPECT_EQ(last.pixelColor(0, 0), QColor(Qt::blue));
    }
}

TEST_F(SessionRecorderTest, RetryNeverTruncatesAnAcknowledgedDamagedPrefix) {
    SessionRecorder recorder;
    ASSERT_NE(recorder.capture(image(), "first", true), 0U);
    ASSERT_TRUE(recorder.flush());
    QFile file(recorder.spoolPath());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    auto lines = file.readAll().split('\n');
    file.close();
    QJsonObject frame = QJsonDocument::fromJson(lines[1]).object();
    frame["hash"] = QString(64, '0');
    lines[1] = QJsonDocument(frame).toJson(QJsonDocument::Compact);
    const QByteArray damaged = lines.join('\n');
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    ASSERT_EQ(file.write(damaged), damaged.size());
    file.close();
    EXPECT_FALSE(recorder.retry());
    EXPECT_TRUE(recorder.hasError());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), damaged);
    file.close();
    ASSERT_TRUE(QFile::remove(file.fileName()));
    EXPECT_FALSE(recorder.retry());
    EXPECT_FALSE(QFile::exists(file.fileName()));
}

TEST_F(SessionRecorderTest, InactiveTruncatedAndLegacyExportsNeverEditTheirSources) {
    SessionRecorder recorder;
    ASSERT_NE(recorder.capture(image(Qt::red), "first", true), 0U);
    ASSERT_TRUE(recorder.flush());
    recorder.markFinished();
    ASSERT_TRUE(recorder.flush());
    QFile spool(recorder.spoolPath());
    ASSERT_TRUE(spool.open(QIODevice::ReadOnly));
    const QByteArray complete = spool.readAll();
    spool.close();
    ASSERT_TRUE(spool.open(QIODevice::Append));
    ASSERT_EQ(spool.write("{\"ms\":2000,\"png\":\"cut"), 21);
    spool.close();
    ASSERT_TRUE(spool.open(QIODevice::ReadOnly));
    const QByteArray truncated = spool.readAll();
    spool.close();
    QFile sidecar(recorder.spoolPath() + ".meta.json");
    ASSERT_TRUE(sidecar.open(QIODevice::ReadOnly));
    const QByteArray originalMetadata = sidecar.readAll();
    sidecar.close();

    const QString output = directory.filePath("recovered-export.prismrec");
    ASSERT_TRUE(SessionRecorder::exportRecovered(recorder.spoolPath(), output));
    EXPECT_TRUE(SessionRecorder::verify(output));
    ASSERT_TRUE(spool.open(QIODevice::ReadOnly));
    EXPECT_EQ(spool.readAll(), truncated);
    spool.close();
    ASSERT_TRUE(sidecar.open(QIODevice::ReadOnly));
    EXPECT_EQ(sidecar.readAll(), originalMetadata);

    const auto lines = complete.split('\n');
    QJsonObject legacy;
    legacy["format"] = "prism-record";
    legacy["version"] = 1;
    legacy["frames"] = QJsonArray{QJsonDocument::fromJson(lines[1]).object()};
    QFile old(directory.filePath("legacy.prismrec"));
    ASSERT_TRUE(old.open(QIODevice::WriteOnly));
    const QByteArray legacyBytes = QJsonDocument(legacy).toJson(QJsonDocument::Compact);
    ASSERT_EQ(old.write(legacyBytes), legacyBytes.size());
    old.close();
    const QString copied = directory.filePath("legacy-copy.prismrec");
    ASSERT_TRUE(SessionRecorder::exportRecovered(old.fileName(), copied));
    EXPECT_TRUE(SessionRecorder::verify(copied));
    QFile copy(copied);
    ASSERT_TRUE(copy.open(QIODevice::ReadOnly));
    EXPECT_EQ(copy.readAll(), legacyBytes);
}

TEST_F(SessionRecorderTest, RetryRepairsAnInterruptedHeaderBeforeTheFirstFrame) {
    qputenv("PRISM_TEST_RECORD_FAULT", "header");
    SessionRecorder recorder;
    recorder.capture(image(), "interrupted header", true);
    EXPECT_FALSE(recorder.flush());
    EXPECT_TRUE(recorder.hasError());
    EXPECT_EQ(recorder.lastStoredCaptureId(), 0U);
    qunsetenv("PRISM_TEST_RECORD_FAULT");
    ASSERT_TRUE(recorder.retry());
    const quint64 final = recorder.capture(image(Qt::green), "first stored", true);
    ASSERT_NE(final, 0U);
    ASSERT_TRUE(recorder.flush());
    EXPECT_EQ(recorder.lastStoredCaptureId(), final);
    EXPECT_TRUE(recorder.hasGaps());
    EXPECT_TRUE(SessionRecorder::verify(recorder.spoolPath()));
}
