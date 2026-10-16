#include "application/AppController.h"
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>
#include <gtest/gtest.h>

namespace {
bool waitFor(const std::function<bool()>& predicate, int timeout = 5000) {
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeout) {
        QCoreApplication::processEvents();
        QThread::msleep(5);
    }
    return predicate();
}
void paint(AppController& app, float y = 10) {
    app.beginStroke(5, y);
    app.extendStroke(25, y);
    app.endStroke();
}
} // namespace

TEST(ArtworkSession, AutosaveProtectsWorkWithoutMarkingProjectSaved) {
    qputenv("PRISM_AUTOSAVE_INTERVAL_MS", "30");
    AppController app;
    app.newDocument(40, 32);
    paint(app);
    ASSERT_TRUE(app.hasUnsavedChanges());
    ASSERT_TRUE(waitFor([&] { return app.autosaveStatus() == "Protected"; }));
    EXPECT_TRUE(app.hasUnsavedChanges());
    QTemporaryDir target;
    ASSERT_TRUE(app.saveProject(QUrl::fromLocalFile(target.filePath("art.prism"))));
    EXPECT_FALSE(app.hasUnsavedChanges());
    app.undo();
    EXPECT_TRUE(app.hasUnsavedChanges());
    app.redo();
    EXPECT_FALSE(app.hasUnsavedChanges());
    qunsetenv("PRISM_AUTOSAVE_INTERVAL_MS");
}

TEST(ArtworkSession, ContinuedEditsCannotPostponeAutosaveDeadline) {
    qputenv("PRISM_AUTOSAVE_INTERVAL_MS", "60");
    AppController app;
    app.newDocument(40, 32);
    QElapsedTimer drawing;
    drawing.start();
    bool protectedWhileDrawing = false;
    while (drawing.elapsed() < 500) {
        paint(app, 8 + static_cast<float>(drawing.elapsed() % 12));
        QCoreApplication::processEvents();
        if (app.autosaveStatus() == "Protected")
            protectedWhileDrawing = true;
        QThread::msleep(10);
    }
    EXPECT_TRUE(protectedWhileDrawing);
    EXPECT_TRUE(app.hasUnsavedChanges());
    qunsetenv("PRISM_AUTOSAVE_INTERVAL_MS");
}

TEST(ArtworkSession, CancellingCloseKeepsEditableArtwork) {
    AppController app;
    app.newDocument(40, 32);
    paint(app);
    const QImage before = app.canvasImage();
    QSignalSpy close(&app, &AppController::closeReady);
    ASSERT_TRUE(app.requestDocumentAction("close"));
    EXPECT_TRUE(app.documentDecisionPending());
    app.resolveUnsavedChanges("cancel");
    EXPECT_FALSE(app.documentDecisionPending());
    EXPECT_TRUE(app.hasUnsavedChanges());
    EXPECT_EQ(app.canvasImage(), before);
    EXPECT_EQ(close.count(), 0);
}

TEST(ArtworkSession, FailedSaveKeepsArtworkAndPendingDecision) {
    AppController app;
    app.newDocument(40, 32);
    paint(app);
    const QImage before = app.canvasImage();
    QTemporaryDir target;
    QSignalSpy decision(&app, &AppController::unsavedChangesDecisionRequested);
    ASSERT_TRUE(app.requestDocumentAction("new", {{"width", 20}, {"height", 15}}));
    app.resolveUnsavedChanges("save");
    ASSERT_TRUE(app.saveProjectAsync(QUrl::fromLocalFile(target.filePath("missing/art.prism"))));
    ASSERT_TRUE(waitFor([&] { return !app.saving(); }));
    EXPECT_TRUE(app.documentDecisionPending());
    EXPECT_TRUE(app.hasUnsavedChanges());
    EXPECT_EQ(app.canvasImage(), before);
    EXPECT_GE(decision.count(), 2);
    app.cancelDocumentAction();
}

TEST(ArtworkSession, SaveAcknowledgesCapturedRevisionOnly) {
    AppController app;
    app.newDocument(40, 32);
    paint(app);
    QTemporaryDir target;
    ASSERT_TRUE(app.saveProjectAsync(QUrl::fromLocalFile(target.filePath("art.prism"))));
    paint(app, 20);
    ASSERT_TRUE(waitFor([&] { return !app.saving(); }));
    EXPECT_TRUE(app.hasUnsavedChanges());
    app.undo();
    EXPECT_FALSE(app.hasUnsavedChanges());
}

TEST(ArtworkSession, NewDocumentUsesRealDpiAndClipboardPixels) {
    AppController app;
    ASSERT_TRUE(app.requestDocumentAction("new", {{"width", 32},
                                                  {"height", 24},
                                                  {"dpi", 300},
                                                  {"layerCount", 2},
                                                  {"background", "solid"},
                                                  {"backgroundColor", QColor(Qt::white)}}));
    ASSERT_TRUE(waitFor([&] { return app.canvasDpi() == 300; }));
    EXPECT_EQ(app.canvasDpi(), 300);
    EXPECT_TRUE(app.hasUnsavedChanges());
    QImage image(16, 10, QImage::Format_RGBA8888);
    image.fill(QColor(50, 90, 130, 120));
    QGuiApplication::clipboard()->setImage(image);
    ASSERT_TRUE(app.requestDocumentAction("new", {{"clipboard", true}, {"dpi", 72}}));
    app.resolveUnsavedChanges("discard");
    ASSERT_TRUE(waitFor([&] { return app.canvasWidth() == 16; }));
    EXPECT_EQ(app.canvasImage(), image);
    EXPECT_EQ(app.canvasDpi(), 72);
}

TEST(ArtworkSession, LiveLayerPreviewsCannotBeSavedAsCommittedHistory) {
    AppController app;
    app.newDocument(40, 32);
    paint(app);
    QTemporaryDir target;
    app.previewLayerOpacity(0, 35);
    EXPECT_TRUE(app.documentEditing());
    EXPECT_FALSE(app.saveProjectAsync(QUrl::fromLocalFile(target.filePath("opacity.prism"))));
    app.commitLayerOpacity(0, 35, 100);
    EXPECT_FALSE(app.documentEditing());
    ASSERT_TRUE(app.saveProjectAsync(QUrl::fromLocalFile(target.filePath("opacity.prism"))));
    ASSERT_TRUE(waitFor([&] { return !app.saving(); }));
    EXPECT_FALSE(app.hasUnsavedChanges());
    app.undo();
    EXPECT_TRUE(app.hasUnsavedChanges());
    app.beginLayerMove();
    app.previewLayerMove(3, 2);
    EXPECT_TRUE(app.documentEditing());
    EXPECT_FALSE(app.saveProjectAsync(QUrl::fromLocalFile(target.filePath("move.prism"))));
    app.endLayerMove();
    EXPECT_FALSE(app.documentEditing());
}

TEST(ArtworkSession, DocumentDecisionWaitsForStrokeAndLayerCommit) {
    AppController app;
    app.newDocument(40, 32);
    app.beginStroke(5, 10);
    ASSERT_TRUE(app.requestDocumentAction("close"));
    EXPECT_FALSE(app.documentDecisionPending());
    EXPECT_TRUE(app.strokeActive());
    app.extendStroke(25, 10);
    app.endStroke();
    ASSERT_TRUE(waitFor([&] { return app.documentDecisionPending(); }));
    app.resolveUnsavedChanges("cancel");
    const QImage committed = app.canvasImage();
    app.previewLayerOpacity(0, 35);
    ASSERT_TRUE(app.requestDocumentAction("new", {{"width", 20}, {"height", 15}}));
    EXPECT_FALSE(app.documentDecisionPending());
    app.commitLayerOpacity(0, 35, 100);
    ASSERT_TRUE(waitFor([&] { return app.documentDecisionPending(); }));
    app.resolveUnsavedChanges("cancel");
    app.undo();
    EXPECT_EQ(app.canvasImage(), committed);
}

TEST(ArtworkSession, CancelFailedRecordingCloseRetainsProtectedArtwork) {
    qputenv("PRISM_AUTOSAVE_INTERVAL_MS", "30");
    qputenv("PRISM_TEST_RECORD_FAULT", "encode");
    AppController app;
    app.newDocument(40, 32);
    qunsetenv("PRISM_TEST_RECORD_FAULT");
    paint(app);
    ASSERT_TRUE(waitFor(
        [&] { return app.autosaveStatus() == "Protected" && !app.recordingError().isEmpty(); }));
    const auto recovery = app.recoverableArtworks();
    QTemporaryDir output;
    EXPECT_FALSE(app.exportRecording(QUrl::fromLocalFile(output.filePath("incomplete.prismrec"))));
    EXPECT_FALSE(QFileInfo::exists(output.filePath("incomplete.prismrec")));
    ASSERT_FALSE(recovery.isEmpty());
    ASSERT_TRUE(app.requestDocumentAction("close"));
    app.resolveUnsavedChanges("discard");
    app.confirmCloseRecordingError(false);
    EXPECT_TRUE(app.hasUnsavedChanges());
    EXPECT_EQ(app.autosaveStatus(), "Protected");
    for (const auto& entry : recovery)
        EXPECT_TRUE(QFileInfo::exists(QUrl(entry.toMap().value("url").toString()).toLocalFile()));
    qunsetenv("PRISM_AUTOSAVE_INTERVAL_MS");
}

TEST(ArtistUx, BrushIdentitySurvivesTemporaryEditsAndAchromaticColours) {
    AppController app;
    app.setSelfTestMode(true);
    const QString id = app.property("activePresetId").toString();
    const QString name = app.property("activePresetName").toString();
    EXPECT_FALSE(id.isEmpty());
    EXPECT_FALSE(name.isEmpty());
    app.setBrushSize(37);
    EXPECT_TRUE(app.property("brushModified").toBool());
    EXPECT_EQ(app.property("activePresetId").toString(), id);
    EXPECT_EQ(app.property("activePresetName").toString(), name);
    app.resetBrushToPreset();
    EXPECT_FALSE(app.property("brushModified").toBool());
    app.setBrushColor(QColor::fromHsvF(0.6, 0.8, 0.9));
    app.setBrushColor(Qt::black);
    EXPECT_NEAR(app.property("brushHue").toDouble(), 0.6, 0.001);
    app.setBrushColor(Qt::white);
    EXPECT_NEAR(app.property("brushHue").toDouble(), 0.6, 0.001);
    app.setLastError("Unclassified test error");
    EXPECT_EQ(app.property("errorContext").toString(), "general");
    app.dismissError();
    EXPECT_EQ(app.lastError(), QString());
}

TEST(ArtistUx, HsvValidationAndRecentRemovalKeepExistingData) {
    AppController app;
    app.newDocument(20, 16);
    app.setBrushColor(QColor::fromRgbF(0, 0, 0, 0.5));
    app.setBrushHsv(0.25, 0, 0);
    EXPECT_NEAR(app.brushHue(), 0.25, 0.001);
    app.setBrushHsv(0.25, 0.8, 0.9);
    const QColor before = app.brushColor();
    app.setBrushHsv(-1, 0.8, 0.9);
    app.setBrushHsv(0.5, 1.2, 0.9);
    EXPECT_EQ(app.brushColor(), before);
    QTemporaryDir output;
    const auto path = output.filePath("recent.prism");
    ASSERT_TRUE(app.saveProject(QUrl::fromLocalFile(path)));
    app.removeRecentFile(QUrl::fromLocalFile(path));
    EXPECT_TRUE(QFileInfo::exists(path));
    for (const auto& entry : app.recentFiles())
        EXPECT_NE(entry.toMap().value("path").toString(), path);
}

TEST(ArtistUx, SaveFailureHasAnArtworkActionContext) {
    AppController app;
    app.newDocument(20, 16);
    paint(app);
    QTemporaryDir output;
    QFile blocker(output.filePath("blocker"));
    ASSERT_TRUE(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    ASSERT_TRUE(app.saveProjectAsync(QUrl::fromLocalFile(blocker.fileName() + "/art.prism")));
    ASSERT_TRUE(waitFor([&] { return !app.saving() && !app.lastError().isEmpty(); }));
    EXPECT_EQ(app.errorContext(), "artwork-save");
    EXPECT_TRUE(app.hasUnsavedChanges());
}

TEST(ArtistUx, ImportSelectsStoredBrushSettingsAndKeepsIndependentIdentity) {
    AppController app;
    app.setSelfTestMode(true);
    app.setBrushSize(77);
    app.setBrushFlow(0.3F);
    app.addPreset();
    const QString originalId = app.activePresetId();
    QTemporaryDir output;
    const auto url = QUrl::fromLocalFile(output.filePath("brush.prismbrush"));
    ASSERT_TRUE(app.exportBrush(url, app.activePresetIndex()));
    app.setBrushSize(12);
    app.setBrushColor(QColor("#cc33aa"));
    ASSERT_TRUE(app.importBrush(url));
    EXPECT_EQ(app.brushSize(), 77);
    EXPECT_NEAR(app.brushFlow(), 0.3, 0.001);
    EXPECT_NE(app.activePresetId(), originalId);
    EXPECT_FALSE(app.brushModified());
    EXPECT_EQ(app.brushColor().name(), "#cc33aa");
    app.selectPreset(0);
    EXPECT_EQ(app.brushColor().name(), "#cc33aa");
    app.setBrushSize(25);
    app.resetBrushToPreset();
    EXPECT_EQ(app.brushColor().name(), "#cc33aa");
}

TEST(ArtistUx, PresetDeletionRetainsTheSelectedIdentityOrAppliesItsReplacement) {
    AppController app;
    app.setSelfTestMode(true);
    app.setBrushSize(10);
    app.addPreset();
    const int first = app.activePresetIndex();
    app.setBrushSize(20);
    app.addPreset();
    const QString second = app.activePresetId();
    app.removePreset(first);
    EXPECT_EQ(app.activePresetId(), second);
    EXPECT_EQ(app.brushSize(), 20);
    app.removePreset(app.activePresetIndex());
    const float replacement = app.brushSize();
    EXPECT_NE(app.activePresetId(), second);
    EXPECT_FALSE(app.brushModified());
    app.resetBrushToPreset();
    EXPECT_EQ(app.brushSize(), replacement);
}
