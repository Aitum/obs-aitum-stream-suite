#pragma once
#include <QFrame>
#include <QListWidget>
#include <obs.h>

class LiveScenesDock : public QFrame {
	Q_OBJECT
private:
	QListWidget *sceneList;

	void ChangeSceneIndex(bool relative, int offset, int invalidIdx);

	static void save_load(obs_data_t *save_data, bool saving, void *private_data);
	static void source_rename(void *data, calldata_t *call_data);
private slots:
	void MainSceneChanged();
public:
	LiveScenesDock(QWidget *parent = nullptr);
	~LiveScenesDock();

	obs_data_array_t *GetLiveScenesArray();
	bool AddLiveScene(const QString &name);
	bool RemoveLiveScene(const QString &name);
};
