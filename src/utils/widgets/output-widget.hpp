#pragma once

#include <obs.h>
#include <QFrame>
#include <QLabel>
#include <QPushButton>

class OutputWidget : public QFrame {
	Q_OBJECT
private:
	obs_data_t *settings = nullptr;
	int outputPlatformIconSize = 36;
	std::string name;

	QPushButton *outputButton = nullptr;
	QPushButton *extraButton = nullptr;
	QLabel *canvasLabel = nullptr;
	obs_output_t *output = nullptr;

	obs_hotkey_pair_id StartStopHotkey = OBS_INVALID_HOTKEY_PAIR_ID;
	obs_hotkey_id extraHotkey = OBS_INVALID_HOTKEY_ID;

	bool StartOutput();
	void UpdateCanvas();

	static void output_stop(void *data, calldata_t *calldata);
	static void output_start(void *data, calldata_t *calldata);
	static bool EncoderAvailable(const char *encoder);
	static void ensure_directory(char *path);

public:
	OutputWidget(obs_data_t *output_data, QWidget *parent = nullptr);
	~OutputWidget();

	obs_output_t *GetOutput() const { return output; }

	void CheckActive();
	void SaveSettings();
	void UpdateSettings(obs_data_t *data);
};
