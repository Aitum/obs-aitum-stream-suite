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
	QComboBox *canvasCombo = nullptr;
	QLabel *sourceLabel = nullptr;
	QComboBox *filterCombo = nullptr;

	obs_weak_canvas_t *current_canvas = nullptr;
	obs_weak_source_t *current_transition = nullptr;
	obs_weak_source_t *current_scene = nullptr;
	obs_weak_source_t *current_source = nullptr;
	obs_weak_source_t *current_properties = nullptr;

	obs_properties_t *properties = nullptr;

	QFormLayout *layout = nullptr;

	std::map<obs_property_t *, QWidget *> property_widgets;

	void AddProperty(obs_properties_t *properties, obs_property_t *property, obs_data_t *settings, QFormLayout *layout);
	void RefreshProperties(obs_properties_t *properties, QFormLayout *layout);

	static void canvas_create(void *param, calldata_t *cd);
	static void canvas_destroy(void *param, calldata_t *cd);
	static void canvas_channel_change(void *param, calldata_t *cd);
	static void transition_start(void *param, calldata_t *cd);
	static void transition_stop(void *param, calldata_t *cd);
	static void scene_item_select(void *param, calldata_t *cd);
	static void scene_item_deselect(void *param, calldata_t *cd);
	static void filter_add(void *param, calldata_t *cd);
	static void filter_remove(void *param, calldata_t *cd);
	static void source_remove(void *param, calldata_t *cd);
	static void frontend_event(enum obs_frontend_event event, void *param);

private slots:
	void SceneChanged(OBSSource scene);
	void TransitionChanged(OBSSource transition);
	void SourceChanged(OBSSource source);
	void LoadProperties(OBSSource source);

public:
	PropertiesDock(QWidget *parent = nullptr);
	~PropertiesDock();
};
