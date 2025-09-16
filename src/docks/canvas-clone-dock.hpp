#include "../utils/widgets/qt-display.hpp"
#include <obs.h>
#include <QFrame>

class CanvasCloneDock : public QFrame {
	Q_OBJECT
private:
	OBSQTDisplay *preview;
	obs_canvas_t *canvas = nullptr;
	obs_weak_canvas_t *clone = nullptr;
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
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void Tick(void *param, float seconds);
	static void DuplicateSceneItem(obs_sceneitem_t *item, obs_sceneitem_t *item2);

public:
	CanvasCloneDock(obs_data_t *settings, QWidget *parent = nullptr);
	~CanvasCloneDock();
};
