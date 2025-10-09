#pragma once
#include "browser-panel.hpp"
#include <QWidget>

class BrowserDock : public QWidget {
	Q_OBJECT

private:
	QCefWidget *cefWidget = nullptr;
public:
	BrowserDock(const char *url, QWidget *parent = nullptr);
	~BrowserDock() { delete cefWidget; }
};

