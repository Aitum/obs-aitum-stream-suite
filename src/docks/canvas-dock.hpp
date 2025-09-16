
#pragma once
#include "../utils/widgets/qt-display.hpp"
#include "../utils/widgets/source-tree.hpp"
#include <mutex>
#include <obs.h>
#include <QFrame>
#include <QLayout>
#include <QSplitter>
#include <QListWidget>
#include <QComboBox>
#include <util/config-file.h>

class CanvasDock : public QFrame {
	Q_OBJECT
private:
	friend class CanvasCloneDock;
	std::string canvas_name;
	QSplitter *canvas_split;
	QSplitter *panel_split;
	OBSQTDisplay *preview;
	obs_data_t *settings;

	OBSSourceAutoRelease spacerLabel[4];
	int spacerPx[4] = {0};
	gs_vertbuffer_t *box = nullptr;
	gs_vertbuffer_t *rectFill = nullptr;
	gs_vertbuffer_t *circleFill = nullptr;

	obs_scene_t *scene = nullptr;
	//obs_view_t *view = nullptr;
	obs_canvas_t *canvas = nullptr;

	bool locked = false;
	bool selectionBox = false;

	std::vector<obs_sceneitem_t *> hoveredPreviewItems;
	std::vector<obs_sceneitem_t *> selectedItems;
	std::mutex selectMutex;
	bool drawSpacingHelpers = true;

	vec2 startPos{};
	vec2 mousePos{};

	uint32_t canvas_width;
	uint32_t canvas_height;

	float zoom = 1.0f;
	float scrollX = 0.5f;
	float scrollY = 0.5f;
	float groupRot = 0.0f;

	SourceTree *sourceList = nullptr;
	QListWidget *sceneList = nullptr;
	bool hideScenes = true;
	QString currentSceneName;
	OBSWeakSource source;
	std::vector<OBSSource> transitions;
	QComboBox *transition;

	obs_scene_item *GetSelectedItem(obs_scene_t *scene = nullptr);
	QColor GetSelectionColor() const;
	QColor GetCropColor() const;
	QColor GetHoverColor() const;

	void SaveSettings();
	void DrawBackdrop(float cx, float cy);
	void DrawSpacingHelpers(obs_scene_t *scene, float x, float y, float cx, float cy, float scale, float sourceX,
				float sourceY);
	bool DrawSelectionBox(float x1, float y1, float x2, float y2, gs_vertbuffer_t *rectFill);
	void DrawSpacingLine(vec3 &start, vec3 &end, vec3 &viewport, float pixelRatio);
	void SetLabelText(int sourceIndex, int px);
	void RenderSpacingHelper(int sourceIndex, vec3 &start, vec3 &end, vec3 &viewport, float pixelRatio);
	float GetDevicePixelRatio();

	obs_sceneitem_t *GetCurrentSceneItem();
	int GetTopSelectedSourceItem();
	void ChangeSceneIndex(bool relative, int offset, int invalidIdx);
	QListWidget *GetGlobalScenesList();
	void AddScene(QString duplicate = "", bool ask_name = true);
	void RemoveScene(const QString &sceneName);
	void SetLinkedScene(obs_source_t *scene, const QString &linkedScene);
	void SwitchScene(const QString &scene_name, bool transition = true);
	obs_source_t *GetTransition(const char *transition_name);
	bool SwapTransition(obs_source_t *transition);
	void ShowSourcesContextMenu(obs_sceneitem_t *item);
	void AddSceneItemMenuItems(QMenu *popup, OBSSceneItem sceneItem);
	QMenu *CreateVisibilityTransitionMenu(bool visible, obs_sceneitem_t *sceneItem);

	QMenu *CreateAddSourcePopupMenu();
	void LoadSourceTypeMenu(QMenu *menu, const char *type);
	void AddSourceToScene(obs_source_t *source);
	void ShowScenesContextMenu(QListWidgetItem *widget_item);
	void SetGridMode(bool checked);
	bool IsGridMode();

	QIcon GetIconFromType(enum obs_icon_type icon_type) const;
	QIcon GetGroupIcon() const;
	QIcon GetSceneIcon() const;

	enum class CenterType {
		Scene,
		Vertical,
		Horizontal,
	};
	void CenterSelectedItems(CenterType centerType);
	void AddProjectorMenuMonitors(QMenu *parent, QObject *target, const char *slot);

	static bool FindSelected(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static bool DrawSelectedItem(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static void DrawLabel(OBSSource source, vec3 &pos, vec3 &viewport);
	static void DrawLine(float x1, float y1, float x2, float y2, float thickness, vec2 scale);
	static void DrawStripedLine(float x1, float y1, float x2, float y2, float thickness, vec2 scale);
	static void DrawRect(float thickness, vec2 scale);
	static void DrawSquareAtPos(float x, float y, float pixelRatio);
	static void DrawRotationHandle(gs_vertbuffer_t *circle, float rot, float pixelRatio);
	static void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY, int &x, int &y, float &scale);
	static vec2 GetItemSize(obs_sceneitem_t *item);
	static vec3 GetTransformedPos(float x, float y, const matrix4 &mat);
	static obs_source_t *CreateLabel(float pixelRatio, int i);
	static config_t *GetUserConfig(void);
	static inline QColor color_from_int(long long val);
	static bool SceneItemHasVideo(obs_sceneitem_t *item);
	static bool CloseFloat(float a, float b, float epsilon = 0.01f);
	static inline bool crop_enabled(const obs_sceneitem_crop *crop);

	static void SceneItemAdded(void *data, calldata_t *params);
	static void SceneReordered(void *data, calldata_t *params);
	static void SceneRefreshed(void *data, calldata_t *params);
	static void transition_override_stop(void *data, calldata_t *);
	static bool add_sources_of_type_to_menu(void *param, obs_source_t *source);
	static bool selected_items(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static bool RotateSelectedSources(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static vec3 GetItemTL(obs_sceneitem_t *item);
	static void SetItemTL(obs_sceneitem_t *item, const vec3 &tl);
	static void GetItemBox(obs_sceneitem_t *item, vec3 &tl, vec3 &br);
	static bool MultiplySelectedItemScale(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static bool CenterAlignSelectedItems(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static bool GetSelectedItemsWithSize(obs_scene_t *scene, obs_sceneitem_t *item, void *param);

public:
	CanvasDock(obs_data_t *settings, QWidget *parent = nullptr);
	~CanvasDock();
};
