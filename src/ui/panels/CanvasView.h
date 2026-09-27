#pragma once

#include <QQuickPaintedItem>

class AppController;

/// Displays the compositor's flattened canvas. QQuickPaintedItem for now —
/// fast enough at this stage; the RHI direct-render path replaces it later
/// without touching the controller.
class CanvasView : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(AppController* controller READ controller WRITE setController
                   NOTIFY controllerChanged)

public:
    explicit CanvasView(QQuickItem* parent = nullptr);

    AppController* controller() const { return m_controller; }
    void setController(AppController* controller);

    void paint(QPainter* painter) override;

signals:
    void controllerChanged();

private slots:
    void onCanvasChanged(int x, int y, int w, int h);
    void onViewChanged();

private:
    AppController* m_controller = nullptr;
};
