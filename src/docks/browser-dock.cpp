#include "browser-dock.hpp"
#include <QVBoxLayout>

QCef *cef = nullptr;

BrowserDock::BrowserDock(const char *url, QWidget *parent)
	: QWidget(parent)
{

	setMinimumSize(200, 100);

	if (!cef) {
		obs_module_t *browserModule = obs_get_module("obs-browser");
		QCef *(*create_qcef)(void) = nullptr;
		if (browserModule) {
			create_qcef = (decltype(create_qcef))os_dlsym(obs_get_module_lib(browserModule), "obs_browser_create_qcef");
			if (create_qcef) {
				cef = create_qcef();
			}
		}
	}

	auto l = new QVBoxLayout(this);
	setLayout(l);
	if (cef)
		cefWidget = cef->create_widget(nullptr, url);
	l->addWidget(cefWidget);
}
