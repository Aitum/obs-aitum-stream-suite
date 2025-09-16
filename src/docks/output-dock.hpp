#pragma once

#include <QFrame>
#include <QGridLayout>

class OutputDock : public QFrame {
	Q_OBJECT

private:
	int outputPlatformIconSize = 36;
	QGridLayout *mainLayout = nullptr;

public:
	OutputDock(QWidget *parent = nullptr);
	~OutputDock();
};
