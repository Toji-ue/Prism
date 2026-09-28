#pragma once

#include <QAbstractListModel>

#include <vector>

namespace prism::core {
class Document;
}

/// Exposes a Document's layer stack to QML as a ListView model.
/// Roles: name, visible, opacity (0..100), blendMode (int), type.
class LayerListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        VisibleRole,
        OpacityRole,
        BlendModeRole,
        TypeRole,
        LockedRole,
    };
    Q_ENUM(Role)

    explicit LayerListModel(QObject* parent = nullptr);

    void setDocument(prism::core::Document* doc);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Refetch from the document and emit the standard change signals.
    void refresh();

    /// Notify only one row's roles (live opacity drag). Cheaper than refresh(),
    /// which resets the whole model, and it keeps the row's own text in sync
    /// while the user is still dragging.
    void notifyRow(int row, const QList<int>& roles = {});

private:
    prism::core::Document* m_doc = nullptr;
};
