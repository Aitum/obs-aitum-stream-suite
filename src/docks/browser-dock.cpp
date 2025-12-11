#include "browser-dock.hpp"
#include <QVBoxLayout>
#include <obs-frontend-api.h>

QCef *cef = nullptr;
QCefCookieManager *panel_cookies = nullptr;

BrowserDock::BrowserDock(const char *url, QWidget *parent) : QWidget(parent)
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

	if (!panel_cookies && cef) {
		const char *cookie_id = config_get_string(obs_frontend_get_profile_config(), "Panels", "CookieId");
		if (cookie_id && cookie_id[0] != '\0') {
			std::string sub_path;
			sub_path += "obs_profile_cookies/";
			sub_path += cookie_id;
			panel_cookies = cef->create_cookie_manager(sub_path);
		}
	}

	layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);
	if (cef) {
		cefWidget = cef->create_widget(this, url, panel_cookies);
		layout->addWidget(cefWidget);
	}
}

BrowserDock::~BrowserDock()
{
	layout->removeWidget(cefWidget);
	cefWidget->setParent(nullptr);
	cefWidget->deleteLater();
}

void DestroyPanelCookieManager()
{
	if (!panel_cookies)
		return;
	panel_cookies->FlushStore();
	delete panel_cookies;
	panel_cookies = nullptr;
}
