#include "browser-dock.hpp"
#include <QVBoxLayout>
#include <obs-frontend-api.h>
#include <QEventLoop>
#include <QThread>
#include <random>

QCef *cef = nullptr;
QCefCookieManager *panel_cookies = nullptr;

bool load_cef()
{
	if (cef) {
		return true;
	}

	obs_module_t *browserModule = obs_get_module("obs-browser");
	QCef *(*create_qcef)(void) = nullptr;
	if (browserModule) {
		create_qcef = (decltype(create_qcef))os_dlsym(obs_get_module_lib(browserModule), "obs_browser_create_qcef");
		if (create_qcef) {
			cef = create_qcef();
		}
	}
	return cef != nullptr;
}

static std::string GenId()
{
	std::random_device rd;
	std::mt19937_64 e2(rd());
	std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFF);

	uint64_t id = dist(e2);

	char id_str[20];
	snprintf(id_str, sizeof(id_str), "%016llX", (unsigned long long)id);
	return std::string(id_str);
}

class QuickThread : public QThread {
public:
	explicit inline QuickThread(std::function<void()> func_) : func(func_) {}

private:
	virtual void run() override { func(); }

	std::function<void()> func;
};

BrowserDock::BrowserDock(const char *name, const char *url_, QWidget *parent) : QWidget(parent), url(url_)
{
	setMinimumSize(200, 100);
	setObjectName(QString::fromUtf8(name));

	load_cef();
	if (!panel_cookies && cef) {
		if (!cef->init_browser()) {
			QEventLoop eventLoop;
			auto t = new QuickThread([&] {
				cef->wait_for_browser_init();
				QMetaObject::invokeMethod(&eventLoop, &QEventLoop::quit, Qt::QueuedConnection);
			});
			t->start();
			eventLoop.exec();
			t->wait();
			t->deleteLater();
		}
		const char *cookie_id = config_get_string(obs_frontend_get_profile_config(), "Panels", "CookieId");
		if (!cookie_id || cookie_id[0] == '\0') {
			config_set_string(obs_frontend_get_profile_config(), "Panels", "CookieId", GenId().c_str());
			cookie_id = config_get_string(obs_frontend_get_profile_config(), "Panels", "CookieId");
		}
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

void BrowserDock::Refresh()
{
	if (cefWidget) {
		cefWidget->reloadPage();
	}
}

void BrowserDock::Reset()
{
	if (cefWidget) {
		cefWidget->setURL(url);
	}
}

void DestroyPanelCookieManager()
{
	if (!panel_cookies) {
		return;
	}
	panel_cookies->FlushStore();
	delete panel_cookies;
	panel_cookies = nullptr;
}
