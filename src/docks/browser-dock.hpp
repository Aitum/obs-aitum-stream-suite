#pragma once
#include "browser-panel.hpp"
#include <QWidget>
#include <QVBoxLayout>

class BrowserDock : public QWidget {
	Q_OBJECT

private:
	QCefWidget *cefWidget = nullptr;
	QVBoxLayout *layout = nullptr;
	std::string url;
public:
	BrowserDock(const char* name, const char *url, QWidget *parent = nullptr);
	~BrowserDock();

	void Refresh();
	void Reset();
};

