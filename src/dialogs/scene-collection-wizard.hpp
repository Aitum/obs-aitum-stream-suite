#pragma once
#include <QWizard>
#include <QListWidget>
#include <QComboBox>
#include <obs.hpp>

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
	OBSDisplay display;
public:
	CamPage(QWidget *parent = nullptr);

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
