
#pragma once
#include "../utils/event-filter.hpp"
#include "../utils/widgets/qt-display.hpp"
#include "../utils/widgets/source-tree.hpp"
#include "../utils/widgets/projector.hpp"
#include <mutex>
#include <obs.h>
#include <QComboBox>
#include <QFrame>
#include <QKeyEvent>
#include <QLayout>
#include <QListWidget>
#include <QMouseEvent>
#include <QSplitter>
#include <QWheelEvent>
#include <util/config-file.h>
#include <graphics/matrix4.h>
#include <graphics/vec2.h>

#define ITEM_LEFT (1 << 0)
#define ITEM_RIGHT (1 << 1)
#define ITEM_TOP (1 << 2)
#define ITEM_BOTTOM (1 << 3)
#define ITEM_ROT (1 << 4)

enum class ItemHandle : uint32_t {
	None = 0,
	TopLeft = ITEM_TOP | ITEM_LEFT,
	TopCenter = ITEM_TOP,
	TopRight = ITEM_TOP | ITEM_RIGHT,
	CenterLeft = ITEM_LEFT,
	CenterRight = ITEM_RIGHT,
	BottomLeft = ITEM_BOTTOM | ITEM_LEFT,
	BottomCenter = ITEM_BOTTOM,
	BottomRight = ITEM_BOTTOM | ITEM_RIGHT,
	Rot = ITEM_ROT
};

#define HANDLE_RADIUS 4.0f
#define HANDLE_SEL_RADIUS (HANDLE_RADIUS * 1.5f)

class OBSProjector;

class CanvasDock : public QFrame {
	Q_OBJECT
private:
	friend class CanvasCloneDock;
	friend class OBSProjector;
	std::string canvas_name;
	QSplitter *canvas_split = nullptr;
	QSplitter *panel_split = nullptr;
	OBSQTDisplay *preview;
	obs_data_t *settings = nullptr;

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
	float previewScale = 1.0f;

	SourceTree *sourceList = nullptr;
	QListWidget *sceneList = nullptr;
	QComboBox *sceneCombo = nullptr;
	bool hideScenes = true;
	QString currentSceneName;
	OBSWeakSource source;
	std::vector<OBSSource> transitions;
	QComboBox *transition = nullptr;
	std::vector<OBSProjector *> projectors;

	bool mouseDown = false;
	bool mouseMoved = false;
	bool scrollMode = false;
	bool fixedScaling = false;
	bool cropping = false;
	bool mouseOverItems = false;
	bool preview_disabled = false;
	QFrame *previewDisabledWidget;
	vec2 scrollingFrom{};
	vec2 scrollingOffset{};
	vec2 lastMoveOffset{};
	float rotateAngle{};
	vec2 rotatePoint{};
	vec2 offsetPoint{};
	vec2 stretchItemSize{};
	matrix4 screenToItem{};
	matrix4 itemToScreen{};
	obs_sceneitem_crop startCrop{};
	vec2 startItemPos{};
	vec2 cropSize{};
	OBSSceneItem stretchGroup;
	OBSSceneItem stretchItem;
	ItemHandle stretchHandle = ItemHandle::None;
	matrix4 invGroupTransform{};
	inline bool IsFixedScaling() const { return fixedScaling; }
	vec2 GetMouseEventPos(QMouseEvent *event);
	bool SelectedAtPos(obs_scene_t *scene, const vec2 &pos);

	std::unique_ptr<OBSEventFilter> eventFilter;
	OBSEventFilter *BuildEventFilter();
	void LoadUI();
	bool HandleMousePressEvent(QMouseEvent *event);
	bool HandleMouseReleaseEvent(QMouseEvent *event);
	bool HandleMouseMoveEvent(QMouseEvent *event);
	bool HandleMouseLeaveEvent(QMouseEvent *event);
	bool HandleMouseWheelEvent(QWheelEvent *event);
	bool HandleKeyPressEvent(QKeyEvent *event);
	bool HandleKeyReleaseEvent(QKeyEvent *event);
	void UpdateCursor(uint32_t &flags);
	void ProcessClick(const vec2 &pos);
	void DoCtrlSelect(const vec2 &pos);
	void DoSelect(const vec2 &pos);
	OBSSceneItem GetItemAtPos(const vec2 &pos, bool selectBelow);
	void RotateItem(const vec2 &pos);
	void CropItem(const vec2 &pos);
	void StretchItem(const vec2 &pos);
	void SnapStretchingToScreen(vec3 &tl, vec3 &br);
	vec3 GetSnapOffset(const vec3 &tl, const vec3 &br);
	void MoveItems(const vec2 &pos);
	void SnapItemMovement(vec2 &offset);
	void BoxItems(const vec2 &startPos, const vec2 &pos);
	void GetStretchHandleData(const vec2 &pos, bool ignoreGroup);
	void ClampAspect(vec3 &tl, vec3 &br, vec2 &size, const vec2 &baseSize);
	vec3 CalculateStretchPos(const vec3 &tl, const vec3 &br);

	obs_scene_item *GetSelectedItem(obs_scene_t *scene = nullptr);
	QColor GetSelectionColor() const;
	QColor GetCropColor() const;
	QColor GetHoverColor() const;

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
	void LoadScenes();
	void LoadTransitions();
	void UpdateLinkedScenes();
	void AddScene(QString duplicate = "", bool ask_name = true);
	void RemoveScene(const QString &sceneName);
	void SetLinkedScene(obs_source_t *scene, const QString &linkedScene);
	obs_source_t *GetTransition(const char *transition_name);
	bool SwapTransition(obs_source_t *transition);
	void ShowSourcesContextMenu(obs_sceneitem_t *item);
	void AddSceneItemMenuItems(QMenu *popup, OBSSceneItem sceneItem);
	QMenu *CreateVisibilityTransitionMenu(bool visible, obs_sceneitem_t *sceneItem);

	QMenu *CreateAddSourcePopupMenu();
	void LoadSourceTypeMenu(QMenu *menu, const char *type);
	void AddSourceToScene(obs_source_t *source);
	void AddSourceTypeToMenu(QMenu *popup, const char *source_type, const char *name);
	void ShowScenesContextMenu(QListWidgetItem *widget_item);
	void SetGridMode(bool checked);
	bool IsGridMode();

	enum class MoveDir { Up, Down, Left, Right };
	void Nudge(int dist, MoveDir dir);

	void LogScenes();

	enum class CenterType {
		Scene,
		Vertical,
		Horizontal,
	};
	void CenterSelectedItems(CenterType centerType);
	void DeleteProjector(OBSProjector *projector);
	OBSProjector *OpenProjector(int monitor);
	static void AddProjectorMenuMonitors(QMenu *parent, QObject *target, const char *slot);

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
	static void source_add(void *p, calldata_t *calldata);
	static void source_rename(void *p, calldata_t *calldata);
	static void source_remove(void *p, calldata_t *calldata);
	static bool select_one(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static bool CheckItemSelected(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static bool FindItemAtPos(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static void RotatePos(vec2 *pos, float rot);
	static bool move_items(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static bool FindItemsInBox(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static bool IntersectBox(matrix4 transform, float x1, float x2, float y1, float y2);
	static bool IntersectLine(float x1, float x2, float x3, float x4, float y1, float y2, float y3, float y4);
	static bool CounterClockwise(float x1, float x2, float x3, float y1, float y2, float y3);
	static bool AddItemBounds(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static bool GetSourceSnapOffset(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static bool FindHandleAtPos(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	static void save_load(obs_data_t *save_data, bool saving, void *private_data);

	static bool LogSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *v_val);
	static void LogFilter(obs_source_t *, obs_source_t *filter, void *v_val);
	static void check_descendant(obs_source_t *parent, obs_source_t *child, void *param);
	static bool nudge_callback(obs_scene_t *, obs_sceneitem_t *item, void *param);

private slots:
	void AddSourceFromAction();
	void SceneAdded(const QString name);
	void SceneRemoved(const QString name);
	void AddSceneItem(OBSSceneItem item);
	void RefreshSources(OBSScene scene);
	void ReorderSources(OBSScene scene);
	void SwitchBackToSelectedTransition();
	void SwitchScene(const QString &scene_name, bool transition = true);
	void MainSceneChanged();
	void LoadMode(int index);
	void SaveSettings(bool closing = false);
	void OpenPreviewProjector();
	void OpenSourceProjector();

public:
	CanvasDock(obs_data_t *settings, QWidget *parent = nullptr);
	CanvasDock(const char* canvas_name, QWidget *parent = nullptr);
	~CanvasDock();

	obs_canvas_t *GetCanvas() const { return canvas; }
	void UpdateSettings(obs_data_t *settings);

	uint32_t GetCanvasWidth() { return canvas_width; };
	uint32_t GetCanvasHeight() { return canvas_height; };

	void reset_live_state();
	void reset_build_state();
};

struct SelectedItemBounds {
	bool first = true;
	vec3 tl, br;
};

struct OffsetData {
	float clampDist;
	vec3 tl, br, offset;
};

struct HandleFindData {
	const vec2 &pos;
	const float radius;
	matrix4 parent_xform;

	OBSSceneItem item;
	ItemHandle handle = ItemHandle::None;
	float angle = 0.0f;
	vec2 rotatePoint;
	vec2 offsetPoint;

	float angleOffset = 0.0f;

	HandleFindData(const HandleFindData &) = delete;
	HandleFindData(HandleFindData &&) = delete;
	HandleFindData &operator=(const HandleFindData &) = delete;
	HandleFindData &operator=(HandleFindData &&) = delete;

	inline HandleFindData(const vec2 &pos_, float scale) : pos(pos_), radius(HANDLE_SEL_RADIUS / scale)
	{
		matrix4_identity(&parent_xform);
	}

	inline HandleFindData(const HandleFindData &hfd, obs_sceneitem_t *parent)
		: pos(hfd.pos),
		  radius(hfd.radius),
		  item(hfd.item),
		  handle(hfd.handle),
		  angle(hfd.angle),
		  rotatePoint(hfd.rotatePoint),
		  offsetPoint(hfd.offsetPoint)
	{
		obs_sceneitem_get_draw_transform(parent, &parent_xform);
	}
};

struct descendant_info {
	bool exists;
	obs_weak_source_t *target;
	obs_source_t *target2;
};
