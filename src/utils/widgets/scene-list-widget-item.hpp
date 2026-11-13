#pragma once
#include <QListWidgetItem>

class SceneListWidgetItem : public QListWidgetItem {
private:
	int order;
public:
	SceneListWidgetItem(const QString &text, QListWidget *listview, int order);

	bool operator<(const QListWidgetItem &other) const override;
};
