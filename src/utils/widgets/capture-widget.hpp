#pragma once

#include <obs.h>
#include <QFrame>
#include <QLabel>

class CaptureWidget : public QFrame {
	Q_OBJECT
private:
	obs_weak_source_t *source;
	QLabel *sourceLabel;
	QLabel *captureLabel;
	static void hooked(void *param, calldata_t *cd);
	static void unhooked(void *param, calldata_t *cd);
	static void rename(void *param, calldata_t *cd);

public:
	CaptureWidget(obs_source_t *s, QWidget *parent = nullptr);
	~CaptureWidget();

	bool IsSource(obs_source_t *s) { return obs_weak_source_references_source(source, s); }
};
