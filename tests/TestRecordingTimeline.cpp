#include "application/RecordingTimeline.h"
#include "application/SessionRecorder.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTemporaryDir>
#include <gtest/gtest.h>

TEST(RecordingTimeline, ReopenForkAndSharedSourceDeletion) {
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    const QString a = "11111111-1111-4111-8111-111111111111.prismrec",
                  b = "22222222-2222-4222-8222-222222222222.prismrec";
    const auto path = dir.filePath("art.prism");
    RecordingTimeline timeline(dir.path());
    ASSERT_TRUE(timeline.begin("art", a));
    ASSERT_TRUE(timeline.saved(path, "hash"));
    const auto first = timeline.activeId();
    ASSERT_TRUE(timeline.saved(dir.filePath("copy.prism"), "copy", b));
    EXPECT_NE(first, timeline.activeId());
    EXPECT_EQ(timeline.sessions(), QStringList({a, b}));
    ASSERT_TRUE(timeline.begin("art", b));
    ASSERT_TRUE(timeline.resolve(path, "hash"));
    EXPECT_EQ(first, timeline.activeId());
    ASSERT_TRUE(timeline.begin("replacement", a));
    EXPECT_FALSE(timeline.resolve(path, "different"));
    EXPECT_EQ(first, timeline.conflictId());
    EXPECT_TRUE(timeline.resolve(path, "different", true));
}
TEST(TimelapseExport, DurationSamplingKeepsEndsAndRejectsOverFourMinutes) {
    RecordedFrames input;
    input.count = 3;
    input.fetch = [](int i, QImage& image, qint64& ms) {
        image = QImage(32, 24, QImage::Format_RGB32);
        image.fill(i == 0 ? Qt::red : i == 1 ? Qt::green : Qt::blue);
        ms = i * 100000;
        return true;
    };
    TimelapseExportSettings settings;
    settings.seconds = 10;
    settings.endSeconds = 2;
    settings.maxSide = 1280;
    QString error;
    auto output = RecordExport::timelapse(input, settings, &error);
    ASSERT_EQ(output.count, 300);
    EXPECT_EQ(output.fps, 30);
    QImage image;
    qint64 ms = 0;
    ASSERT_TRUE(output.fetch(0, image, ms));
    EXPECT_EQ(image.pixelColor(0, 0), QColor(Qt::red));
    ASSERT_TRUE(output.fetch(299, image, ms));
    EXPECT_EQ(image.pixelColor(0, 0), QColor(Qt::blue));
    settings.inputFps = 1000;
    settings.endSeconds = 0;
    output = RecordExport::timelapse(input, settings, &error);
    ASSERT_EQ(output.count, 2);
    ASSERT_TRUE(output.fetch(0, image, ms));
    EXPECT_EQ(image.pixelColor(0, 0), QColor(Qt::red));
    ASSERT_TRUE(output.fetch(1, image, ms));
    EXPECT_EQ(image.pixelColor(0, 0), QColor(Qt::blue));
    settings.introSeconds = 2147483647;
    settings.endSeconds = 2147483647;
    EXPECT_FALSE(settings.valid());
    settings.introSeconds = settings.endSeconds = 0;
    settings.inputFps = .001;
    EXPECT_EQ(RecordExport::timelapse(input, settings, &error).count, 0);
    settings.seconds = 241;
    EXPECT_FALSE(settings.valid());
}

TEST(RecordingTimeline, BundleRoundTripPreservesSourcesAndRejectsTampering) {
    QTemporaryDir root, destination, extracted;
    ASSERT_TRUE(root.isValid());
    const auto previous = qgetenv("PRISM_RECOVERY_DIR");
    qputenv("PRISM_RECOVERY_DIR", root.path().toUtf8());
    QString a, b;
    QJsonObject manifest;
    {
        SessionRecorder recorder;
        QImage image(32, 24, QImage::Format_RGBA8888);
        image.fill(Qt::red);
        ASSERT_NE(recorder.capture(image, "first", true), 0U);
        recorder.markFinished();
        ASSERT_TRUE(recorder.flush());
        a = QFileInfo(recorder.spoolPath()).fileName();
        RecordingTimeline timeline(QFileInfo(recorder.spoolPath()).absolutePath());
        ASSERT_TRUE(timeline.begin("art", a));
        ASSERT_TRUE(timeline.saved(root.filePath("art.prism"), "saved"));
        recorder.restart();
        image.fill(Qt::blue);
        ASSERT_NE(recorder.capture(image, "second", true), 0U);
        recorder.markFinished();
        ASSERT_TRUE(recorder.flush());
        b = QFileInfo(recorder.spoolPath()).fileName();
        ASSERT_TRUE(timeline.saved(root.filePath("copy.prism"), "copy", b));
        manifest = timeline.manifest();
        const auto bundle = destination.filePath("process.prismrec");
        QString error;
        ASSERT_TRUE(RecordingTimeline::writeBundle(
            bundle, manifest, QFileInfo(recorder.spoolPath()).absolutePath(), {}, &error))
            << error.toStdString();
        int count = 0;
        ASSERT_TRUE(SessionRecorder::verify(bundle, &error, &count)) << error.toStdString();
        EXPECT_EQ(count, 2);
        QJsonObject restored;
        ASSERT_TRUE(RecordingTimeline::readBundle(bundle, extracted.path(), &restored, &error))
            << error.toStdString();
        EXPECT_EQ(restored["timelines"].toArray().size(), 2);
        EXPECT_EQ(RecordingTimeline::fingerprint(extracted.filePath(a)),
                  RecordingTimeline::fingerprint(QFileInfo(recorder.spoolPath()).absolutePath() +
                                                 "/" + a));
        QFile corrupt(bundle);
        ASSERT_TRUE(corrupt.open(QIODevice::ReadWrite));
        ASSERT_TRUE(corrupt.seek(corrupt.size() - 2));
        ASSERT_EQ(corrupt.write("!", 1), 1);
        corrupt.close();
        EXPECT_FALSE(SessionRecorder::verify(bundle, &error));
    }
    if (previous.isNull())
        qunsetenv("PRISM_RECOVERY_DIR");
    else
        qputenv("PRISM_RECOVERY_DIR", previous);
}

TEST(RecordingTimeline, ManifestRejectsTraversalCyclesAndInvalidReferences) {
    QJsonObject node{{"id", "11111111-1111-4111-8111-111111111111"},
                     {"name", "art"},
                     {"parent", ""},
                     {"sessions", QJsonArray{"../../outside.prismrec"}}};
    EXPECT_FALSE(
        RecordingTimeline::validManifest({{"version", 1}, {"timelines", QJsonArray{node}}}));
    node["sessions"] = QJsonArray{"22222222-2222-4222-8222-222222222222.prismrec"};
    node["parent"] = node["id"];
    EXPECT_FALSE(
        RecordingTimeline::validManifest({{"version", 1}, {"timelines", QJsonArray{node}}}));
    node["parent"] = "33333333-3333-4333-8333-333333333333";
    EXPECT_FALSE(
        RecordingTimeline::validManifest({{"version", 1}, {"timelines", QJsonArray{node}}}));
}

TEST(TimelapseExport, CancelledGifPreservesDestinationAndUsesAccurateDelays) {
    QTemporaryDir directory;
    const auto path = directory.filePath("existing.gif");
    QFile original(path);
    ASSERT_TRUE(original.open(QIODevice::WriteOnly));
    original.write("original");
    original.close();
    RecordedFrames frames;
    frames.count = 2;
    frames.fps = 30;
    frames.fetch = [](int i, QImage& image, qint64& ms) {
        image = QImage(32, 24, QImage::Format_RGB32);
        image.fill(i ? Qt::blue : Qt::red);
        ms = i;
        return true;
    };
    frames.progress = [](int, int) { return false; };
    QString error;
    EXPECT_FALSE(RecordExport::writeGif(path, frames, &error));
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    EXPECT_EQ(original.readAll(), QByteArray("original"));
    original.close();
    frames.progress = {};
    ASSERT_TRUE(RecordExport::writeGif(path, frames, &error));
    ASSERT_TRUE(original.open(QIODevice::ReadOnly));
    const auto bytes = original.readAll();
    const auto pos = bytes.indexOf(QByteArray::fromHex("21f904"));
    ASSERT_GE(pos, 0);
    EXPECT_EQ(static_cast<unsigned char>(bytes[pos + 4]), 3);
}
