#pragma once

#include <obs.h>
#include <obs.hpp>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <obs-frontend-api.h>

class PropertiesDock : public QFrame {
	Q_OBJECT
private:
	QLabel *sourceLabel = nullptr;

	obs_weak_source_t *current_source = nullptr;
	obs_weak_source_t *current_properties = nullptr;

	obs_properties_t *properties = nullptr;

	QFormLayout *layout = nullptr;

	std::map<obs_property_t *, QWidget *> property_widgets;

	void AddProperty(obs_properties_t *properties, obs_property_t *property, obs_data_t *settings, QFormLayout *layout);
	void RefreshProperties(obs_properties_t *properties, QFormLayout *layout);

	static void canvas_create(void *param, calldata_t *cd);
	static void canvas_channel_change(void *param, calldata_t *cd);
	static void transition_start(void *param, calldata_t *cd);
	static void transition_stop(void *param, calldata_t *cd);
	static void scene_item_select(void *param, calldata_t *cd);
	static void scene_item_deselect(void *param, calldata_t *cd);
	static void scene_item_remove(void *param, calldata_t *cd);
	static void scene_item_add(void *param, calldata_t *cd);
	static void scene_item_transform(void *param, calldata_t *cd);
	static void scene_item_locked(void *param, calldata_t *cd);
	static void source_remove(void *param, calldata_t *cd);
	static void properties_remove(void *param, calldata_t *cd);
	static void update_properties(void *param, calldata_t *cd);

private slots:
	void SceneChanged(OBSSource scene);
	void TransitionChanged(OBSSource transition);
	void SourceChanged(OBSSource source);
	void SourceDeselected(OBSSource source);
	void LoadProperties(OBSSource source);
public:
	PropertiesDock(QWidget *parent = nullptr);
	~PropertiesDock();
};
