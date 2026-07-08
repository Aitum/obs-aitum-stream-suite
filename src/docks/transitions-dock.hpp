#pragma once
#include "canvas-dock.hpp"
#include <obs.h>
#include <QComboBox>
#include <QDockWidget>
#include <QFrame>
#include <QSpinBox>

class TransitionsDock : public QFrame {
	Q_OBJECT
private:
	QComboBox *transition = nullptr;
	QSpinBox *duration = nullptr;
	CanvasDock *canvasDock = nullptr;
	obs_weak_canvas_t *canvas = nullptr;

	obs_source_t *GetTransition(const char *transition_name);
private slots:
	void TransitionDurationChanged();
	void TransitionChanged();
	void TransitionListChanged();
public:
	TransitionsDock(QWidget *parent = nullptr);
	~TransitionsDock();
	void SetCanvas(obs_canvas_t *new_canvas, CanvasDock *new_canvasDock);
	void UpdateCurrentTransition();
};
