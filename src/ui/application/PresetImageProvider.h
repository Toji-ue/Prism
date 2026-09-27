#pragma once

#include <QQuickImageProvider>

/// Serves brush-preset thumbnails to QML as image://preset/<index>.
/// Rendering itself happens in the canvas module (real brush engine), so
/// the grid shows what each preset will actually paint.
class PresetImageProvider : public QQuickImageProvider {
public:
    PresetImageProvider();

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};
