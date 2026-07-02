#pragma once
#include <QFrame>
#include <QListWidget>
#include <obs.h>

class ScenesDock : public QFrame {
	Q_OBJECT
private:
	CanvasDock *canvasDock = nullptr;
	obs_weak_canvas_t *canvas = nullptr;
	obs_weak_source_t *transition = nullptr;
	QListWidget *sceneList;

	void ChangeSceneIndex(bool relative, int offset, int invalidIdx);
	void SwitchToCanvas(obs_canvas_t *new_canvas);

	void ShowScenesContextMenu(QListWidgetItem *widget_item);
	void SetGridMode(bool checked);
	bool IsGridMode();
	void AddScene(QString duplicate = "", bool ask_name = true);
	void RemoveCurrentScene();
	void UpdateCurrentScene();
	void UpdateLinkedScenes();

	static void scene_add(void *data, calldata_t *cd);
	static void scene_remove(void *data, calldata_t *cd);
	static void scene_rename(void *data, calldata_t *cd);
	static void channel_change(void *data, calldata_t *cd);
	static void transition_action(void *data, calldata_t *cd);

private slots:
	void handleFocusChange(QWidget *old, QWidget *now);
	void FinishedLoading();

public:
	ScenesDock(QWidget *parent = nullptr);
	~ScenesDock();
};
