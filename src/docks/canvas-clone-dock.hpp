#pragma once
#include "../utils/widgets/projector.hpp"
#include "../utils/widgets/qt-display.hpp"
#include <obs.h>
#include <QComboBox>
#include <QFrame>
#include <QSplitter>
#include <util/threading.h>

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
	std::vector<OBSProjector *> projectors;

	std::map<obs_source_t *, obs_weak_source_t *> replace_sources;
	pthread_mutex_t replace_sources_mutex;

	obs_source_t *DuplicateSource(obs_source_t *source, obs_source_t *current);
	void DuplicateSceneItem(obs_sceneitem_t *item, obs_sceneitem_t *item2);
	void DrawBackdrop(float cx, float cy);
	void LoadReplacements();
	void RemoveSource(QString source_name);
	void DeleteProjector(OBSProjector *projector);
	OBSProjector *OpenProjector(int monitor);
	static void AddProjectorMenuMonitors(QMenu *parent, QObject *target, const char *slot);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void Tick(void *param, float seconds);
	static bool AddSourceToCombos(void *param, obs_source_t *source);
	static void source_create(void *param, calldata_t *cd);
	static void source_remove(void *param, calldata_t *cd);
	static void source_rename(void *param, calldata_t *cd);
	static bool SceneDetectReplacedSource(obs_scene_t *scene, obs_sceneitem_t *item, void *param);

private slots:
	void LoadMode(QString mode);
	void SaveSettings(bool closing = false, QString mode = "");
	void OpenPreviewProjector();

public:
	CanvasCloneDock(obs_data_t *settings, QWidget *parent = nullptr);
	~CanvasCloneDock();

	obs_canvas_t *GetCanvas() const { return canvas; }
	void UpdateSettings(obs_data_t *settings);

	uint32_t GetCanvasWidth() { return canvas_width; };
	uint32_t GetCanvasHeight() { return canvas_height; };

	void reset_live_state();
	void reset_build_state();

	void SetPanelVisible(const QString &panel_name, bool visible);
};
