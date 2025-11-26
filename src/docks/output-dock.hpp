#pragma once

#include <obs.h>
#include <QCheckBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <src/utils/widgets/output-widget.hpp>

class OutputDock : public QFrame {
	Q_OBJECT

private:
	int outputPlatformIconSize = 36;
	QVBoxLayout *mainLayout = nullptr;
	QLabel *mainPlatformIconLabel = nullptr;
	QPushButton *mainStreamButton = nullptr;
	QPushButton *mainRecordButton = nullptr;
	QPushButton *mainBacktrackCheckboxButton = nullptr;
	QCheckBox *mainBacktrackCheckbox = nullptr;
	QPushButton *mainBacktrackButton = nullptr;
	QPushButton *mainVirtualCamButton = nullptr;
	QFrame *mainStreamGroup = nullptr;
	QFrame *mainRecordGroup = nullptr;
	QFrame *mainBacktrackGroup = nullptr;
	QFrame *mainVirtualCamGroup = nullptr;
	QString mainPlatformUrl;
	bool exiting = false;

	QTimer videoCheckTimer;

	std::vector<OutputWidget *> outputWidgets;

public:
	OutputDock(QWidget *parent = nullptr);
	~OutputDock();

	obs_data_array_t *GetOutputsArray();

	void Exiting() { exiting = true; }
	void LoadSettings();
	void SaveSettings();
	bool AddChapterToOutput(const char *output_name, const char *chapter_name);

public slots:
	void UpdateMainStreamStatus(bool active);
	void UpdateMainRecordingStatus(bool active);
	void UpdateMainBacktrackStatus(bool active);
	void UpdateMainVirtualCameraStatus(bool active);
};
