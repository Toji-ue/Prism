#include "application/LayerListModel.h"

#include "document/Document.h"
#include "layer/Layer.h"

#include <array>

LayerListModel::LayerListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void LayerListModel::setDocument(prism::core::Document* doc)
{
    beginResetModel();
    m_doc = doc;
    endResetModel();
}

int LayerListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || !m_doc)
        return 0;
    return static_cast<int>(m_doc->layers().size());
}

QVariant LayerListModel::data(const QModelIndex& index, int role) const
{
    if (!m_doc || !index.isValid() || index.row() >= rowCount())
        return {};
    const auto& layer = *m_doc->layers()[static_cast<std::size_t>(index.row())];
    switch (role) {
    case NameRole: return QString::fromStdString(layer.name());
    case VisibleRole: return layer.isVisible();
    case OpacityRole: return static_cast<int>(layer.opacity() * 100.0F + 0.5F);
    case BlendModeRole: return static_cast<int>(layer.blendMode());
    case TypeRole: return static_cast<int>(layer.type());
    case LockedRole: return layer.isLocked();
    }
    return {};
}

QHash<int, QByteArray> LayerListModel::roleNames() const
{
    return {
        { NameRole, "name" },
        { VisibleRole, "visible" },
        { OpacityRole, "opacity" },
        { BlendModeRole, "blendMode" },
        { TypeRole, "type" },
        { LockedRole, "locked" },
    };
}

void LayerListModel::refresh()
{
    if (!m_doc)
        return;
    // The row COUNT may have changed (add / delete / duplicate). Emitting only
    // dataChanged left the view showing a stale list — new layers never
    // appeared. A reset is the correct notification for a structural change.
    beginResetModel();
    endResetModel();
}

void LayerListModel::notifyRow(int row, const QList<int>& roles)
{
    if (!m_doc || row < 0 || row >= rowCount())
        return;
    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, roles);
}
