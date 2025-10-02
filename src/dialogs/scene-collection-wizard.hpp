#pragma once
#include <QWizard>
#include <QListWidget>
#include <QComboBox>
#include <obs.hpp>
#include <src/utils/widgets/qt-display.hpp>

class SceneCollectionWizard : public QWizard {
	Q_OBJECT

private:
	obs_data_t *scene_collection_data = nullptr;
	std::list<obs_data_t *> cams;
	std::list<obs_data_t *> mics;

	void CheckSource(obs_data_t *item);

public:
	enum { Page_SceneCollection, Page_Cam, Page_Mic, Page_Conclusion };

	SceneCollectionWizard(QWidget *parent = nullptr);
	~SceneCollectionWizard();

	int nextId() const override;

	void LoadSceneCollectionFile(QString path);

	obs_data_t *GetCam();
	obs_data_t *GetMic();
	bool SaveSceneCollection();
};

class SceneCollectionPage : public QWizardPage {
	Q_OBJECT

private:
	QListWidget *scl;

public:
	SceneCollectionPage(QWidget *parent = nullptr);

	bool validatePage() override;
};

class CamPage : public QWizardPage {
	Q_OBJECT
private:
	QComboBox *cc;
	OBSSourceAutoRelease cam;
	OBSQTDisplay *preview;
	gs_vertbuffer_t *box = nullptr;

#ifdef WIN32
	const char *id = "dshow_input";
	const char *setting_name = "video_device_id";
#elif __APPLE__
	const char *id = "av_capture_input";
	const char *setting_name = "device";
#else
	const char *id = "v4l2_input";
	const char *setting_name = "input";
#endif

	void DrawBackdrop(float cx, float cy);

	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY, int &x, int &y, float &scale);

public:
	CamPage(QWidget *parent = nullptr);
	~CamPage();

	void initializePage() override;
	bool validatePage() override;
};

class MicPage : public QWizardPage {
	Q_OBJECT
private:
	QComboBox *mc;
	OBSSourceAutoRelease mic;

#ifdef WIN32
	const char *id = "wasapi_input_capture";
	const char *setting_name = "device_id";
#elif __APPLE__
	const char *id = "coreaudio_input_capture";
	const char *setting_name = "device_id";
#else
	const char *id = "pulse_input_capture";
	const char *setting_name = "device_id";
#endif

public:
	MicPage(QWidget *parent = nullptr);

	bool validatePage() override;
};

class ConclusionPage : public QWizardPage {
	Q_OBJECT
public:
	ConclusionPage(QWidget *parent = nullptr);

	bool validatePage() override;
};
