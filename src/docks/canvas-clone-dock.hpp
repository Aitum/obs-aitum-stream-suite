#pragma once
#include "../utils/widgets/qt-display.hpp"
#include <obs.h>
#include <QFrame>
#include <QSplitter>
#include <QComboBox>

class CanvasCloneDock : public QFrame {
	Q_OBJECT
private:
	QSplitter *canvas_split = nullptr;
	OBSQTDisplay *preview;
	obs_canvas_t *canvas = nullptr;
	obs_weak_canvas_t *clone = nullptr;
	std::vector<std::pair<QComboBox *, QComboBox *>> replaceCombos;
	gs_vertbuffer_t *box = nullptr;
	obs_data_t *settings;
	uint32_t canvas_width;
	uint32_t canvas_height;
	float zoom = 1.0f;
	float scrollX = 0.5f;
	float scrollY = 0.5f;

	std::map<obs_source_t *, obs_weak_source_t *> replace_sources;

	obs_source_t *DuplicateSource(obs_source_t *source, obs_source_t *current);
	void DrawBackdrop(float cx, float cy);
	void LoadReplacements();
	void RemoveSource(QString source_name);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void Tick(void *param, float seconds);
	static void DuplicateSceneItem(obs_sceneitem_t *item, obs_sceneitem_t *item2);
	static bool AddSourceToCombos(void *param, obs_source_t *source);
	static void source_create(void *param, calldata_t *cd);
	static void source_remove(void *param, calldata_t *cd);
	static void source_rename(void *param, calldata_t *cd);
	static bool SceneDetectReplacedSource(obs_scene_t *scene, obs_sceneitem_t *item, void *param);

private slots:
	void LoadMode(int index);
	void SaveSettings(bool closing = false);

public:
	CanvasCloneDock(obs_data_t *settings, QWidget *parent = nullptr);
	~CanvasCloneDock();

	obs_canvas_t *GetCanvas() const { return canvas; }
	void UpdateSettings(obs_data_t *settings);

	uint32_t GetCanvasWidth() { return canvas_width; };
	uint32_t GetCanvasHeight() { return canvas_height; };

	void reset_live_state();
	void reset_build_state();
};
