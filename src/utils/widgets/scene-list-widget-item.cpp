#include "scene-list-widget-item.hpp"

SceneListWidgetItem::SceneListWidgetItem(const QString &text, QListWidget *listview, int order)
	: QListWidgetItem(text, listview),
	  order(order)
{
}

bool SceneListWidgetItem::operator<(const QListWidgetItem &other) const
{
	const SceneListWidgetItem *otherItem = dynamic_cast<const SceneListWidgetItem *>(&other);
	if (otherItem) {
		return order < otherItem->order;
	}
	return QListWidgetItem::operator<(other);
}
