#pragma once

#include <obs.h>
#include <QFrame>
#include <QGridLayout>
#include <QPushButton>
#include <QTimer>
#include <QLabel>

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
	bool StartOutput(obs_data_t *settings, QPushButton *streamButton);

	std::vector<std::tuple<std::string, obs_output_t *, QPushButton *>> outputs;

	static void stream_output_stop(void *data, calldata_t *calldata);
	static void stream_output_start(void *data, calldata_t *calldata);

	static bool EncoderAvailable(const char *encoder);

public:
	OutputDock(QWidget *parent = nullptr);
	~OutputDock();

	void UpdateMainStreamStatus(bool active);
	void Exiting() { exiting = true; }
	void LoadSettings();
};
