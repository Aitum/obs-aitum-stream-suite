
#pragma once
#include "../utils/widgets/qt-display.hpp"
#include <obs.h>
#include <QFrame>
#include <QLayout>

class CanvasDock : public QFrame {
	Q_OBJECT
private:
	OBSQTDisplay *preview;
	static void DrawPreview(void *data, uint32_t cx, uint32_t cy);

public:
	CanvasDock(obs_data_t *settings, QWidget *parent = nullptr);
	~CanvasDock();
};
