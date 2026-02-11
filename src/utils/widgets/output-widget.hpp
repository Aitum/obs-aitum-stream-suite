#pragma once

#include <obs.h>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>

class OutputWidget : public QFrame {
	Q_OBJECT
private:
	obs_data_t *settings = nullptr;
	int outputPlatformIconSize = 36;

	QPushButton *outputButton = nullptr;
	QPushButton *extraButton = nullptr;
	QLabel *canvasLabel = nullptr;
	obs_output_t *output = nullptr;

	std::function<void()> onStarted = nullptr;

	QTimer activeTimer;
	QDateTime startTime;

	obs_hotkey_pair_id StartStopHotkey = OBS_INVALID_HOTKEY_PAIR_ID;
	obs_hotkey_id extraHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id splitHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id chapterHotkey = OBS_INVALID_HOTKEY_ID;

	bool StartOutput(bool automated = false);
	void UpdateCanvas();
	obs_encoder_t *GetVideoEncoder(obs_data_t *settings, bool advanced, bool is_record, const char *output_name,
				       bool automated);

	static void output_stop(void *data, calldata_t *calldata);
	static void output_start(void *data, calldata_t *calldata);
	static void replay_saved(void *data, calldata_t *calldata);
	static bool EncoderAvailable(const char *encoder);
	static void ensure_directory(char *path);

public:
	OutputWidget(obs_data_t *output_data, QWidget *parent = nullptr);
	~OutputWidget();

	obs_output_t *GetOutput() const { return output; }

	void CheckActive();
	void SaveSettings();
	void UpdateSettings(obs_data_t *data);
	bool AddChapter(const char *chapter_name);
	bool StartOutput(std::function<void()> onStarted);
	void StopOutput();
	bool IsStream() const;
	bool IsRecord() const;
	const char* GetOutputType() const;
};
