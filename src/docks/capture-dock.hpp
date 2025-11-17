#pragma once
#include <QFrame>
#include <QGridLayout>
#include <obs.h>

class CaptureDock : public QFrame {
	Q_OBJECT

private:
	QVBoxLayout *mainLayout;

	static void source_create(void *param, calldata_t *cd);
	static void source_remove(void *param, calldata_t *cd);

public:
	CaptureDock(QWidget *parent = nullptr);
	~CaptureDock();
};
