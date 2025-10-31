#pragma once

#include <obs.h>
#include <QFrame>
#include <QGridLayout>
#include <QPushButton>
#include <QTimer>
#include <QLabel>
#include <src/utils/widgets/output-widget.hpp>

class OutputDock : public QFrame {
	Q_OBJECT

private:
	int outputPlatformIconSize = 36;
	QVBoxLayout *mainLayout = nullptr;
	QPushButton *mainStreamButton = nullptr;
	QLabel *mainPlatformIconLabel = nullptr;
	QString mainPlatformUrl;
	bool exiting = false;

	QTimer videoCheckTimer;

	void LoadOutput(obs_data_t *output_data);

	std::vector<OutputWidget *> outputWidgets;

public:
	OutputDock(QWidget *parent = nullptr);
	~OutputDock();

	void UpdateMainStreamStatus(bool active);
	void Exiting() { exiting = true; }
	void LoadSettings();
	void SaveSettings();
};
