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
    };
    Q_ENUM(Role)

    explicit LayerListModel(QObject* parent = nullptr);

    void setDocument(prism::core::Document* doc);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Refetch from the document and emit the standard change signals.
    void refresh();

private:
    prism::core::Document* m_doc = nullptr;
};
