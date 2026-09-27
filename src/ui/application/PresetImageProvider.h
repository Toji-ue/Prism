#pragma once

#include <QQuickImageProvider>

class AppController;

/// Serves brush-preset thumbnails to QML as image://preset/<index>.
/// Rendering happens in the canvas module (real brush engine), so the grid
/// shows what each preset will actually paint — including user presets.
class PresetImageProvider : public QQuickImageProvider {
public:
    explicit PresetImageProvider(AppController* controller);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    AppController* m_controller;
};

/// Serves downscaled layer previews as image://layerthumb/<index>/<size>.
class LayerThumbnailProvider : public QQuickImageProvider {
public:
    explicit LayerThumbnailProvider(AppController* controller);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    AppController* m_controller;
};

/// Serves the active selection as a tinted overlay: image://selection/<rev>.
class SelectionOverlayProvider : public QQuickImageProvider {
public:
    explicit SelectionOverlayProvider(AppController* controller);

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
    AppController* m_controller;
};
