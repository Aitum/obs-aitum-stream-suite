#pragma once
#include "../utils/widgets/source-tree.hpp"
#include <obs.h>
#include <QFrame>
#include <QListWidget>

class SourcesDock : public QFrame {
	Q_OBJECT
private:
	friend class CanvasDock;

	obs_weak_source_t *scene = nullptr;
	SourceTree *sourceList;
	std::vector<obs_sceneitem_t *> hoveredPreviewItems;
	std::vector<obs_sceneitem_t *> selectedItems;
	std::mutex selectMutex;

	obs_sceneitem_t *GetCurrentSceneItem();
	int GetTopSelectedSourceItem();

	void ShowSourcesContextMenu(obs_sceneitem_t *item);

	static void SceneItemAdded(void *data, calldata_t *params);
	static void SceneReordered(void *data, calldata_t *params);
	static void SceneRefreshed(void *data, calldata_t *params);
private slots:
	void AddSceneItem(OBSSceneItem item, bool no_select = false);
	void RefreshSources(OBSScene scene);
	void ReorderSources(OBSScene scene);
	void AddSourceFromAction();
	void AddSourceToScene(OBSSource s);
	void OpenSourceProjector();
public:
	SourcesDock(QWidget *parent = nullptr);
	~SourcesDock();
	void SetScene(obs_source_t *scene);
};
