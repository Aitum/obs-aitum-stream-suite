#pragma once
#include <QWizard>
#include <QListWidget>
#include <QComboBox>
#include <obs.hpp>
#include <src/utils/widgets/qt-display.hpp>

class SceneCollectionWizard : public QWizard {
	Q_OBJECT

public:
	enum { Page_SceneCollection, Page_Cam, Page_Mic, Page_Conclusion };

	SceneCollectionWizard(QWidget *parent = nullptr);

	int nextId() const override;
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

	void DrawBackdrop(float cx, float cy);

	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);
	static void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY, int &x, int &y, float &scale);

public:
	CamPage(QWidget *parent = nullptr);
	~CamPage();

	void initializePage() override;
};

class MicPage : public QWizardPage {
	Q_OBJECT
public:
	MicPage(QWidget *parent = nullptr);
};

class ConclusionPage : public QWizardPage {
	Q_OBJECT
public:
	ConclusionPage(QWidget *parent = nullptr);
};
