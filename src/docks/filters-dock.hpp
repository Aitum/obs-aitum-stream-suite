#pragma once

#include <QDockWidget>
#include <QFrame>
#include <QListWidget>
#include <QMenu>
#include <obs.h>
#include <obs.hpp>

class FiltersDock : public QFrame {
	Q_OBJECT
private:
	QListWidget *filtersList = nullptr;
	obs_weak_source_t *source = nullptr;

	void AddFilterMenu(QMenu *addFilterMEnu);
	void ChangeFilterIndex(QListWidgetItem *widget_item, enum obs_order_movement movement);
	void ShowFiltersContextMenu(QListWidgetItem *widget_item);
	void RemoveFilter(QListWidgetItem *widget_item);
	void RenameFilter(QListWidgetItem *widget_item);

	static bool filter_compatible(uint32_t sourceFlags, uint32_t filterFlags);
	static void filter_add(void *param, calldata_t *cd);
	static void filter_remove(void *param, calldata_t *cd);
	static void source_remove(void *param, calldata_t *cd);
	static void filter_reorder(void *param, calldata_t *cd);

private slots:
	void SourceChanged(OBSSource source);
	void SourceDeselected(OBSSource source);
	void Reorder();
public:
	FiltersDock(QWidget *parent = nullptr);
	~FiltersDock();
};
