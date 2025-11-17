#include "capture-widget.hpp"
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMenu>
#include <obs-module.h>

CaptureWidget::CaptureWidget(obs_source_t *s, QWidget *parent) : QFrame(parent), source(obs_source_get_weak_source(s))
{
	auto sh = obs_source_get_signal_handler(s);
	signal_handler_connect(sh, "rename", rename, this);
	signal_handler_connect(sh, "hooked", hooked, this);
	signal_handler_connect(sh, "unhooked", unhooked, this);

	auto l = new QVBoxLayout;
	setLayout(l);

	auto l2 = new QHBoxLayout;

	l->addLayout(l2);

	auto action = new QAction(QIcon(":/res/images/cogs.svg"), obs_module_text("Config"));
	auto button = new QToolButton();
	button->setIcon(QIcon(":/res/images/cogs.svg"));
	button->setText(QString::fromUtf8(obs_module_text("Config")));
	button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	button->setDefaultAction(action);
	button->setProperty("themeID", "cogsIcon");
	button->setProperty("class", "icon-gear");

	connect(button, &QToolButton::triggered, [this] {
		auto s = obs_weak_source_get_source(source);
		if (!s)
			return;
		auto props = obs_source_properties(s);
		obs_source_release(s);
		if (!props)
			return;

		QMenu menu;

		auto window = obs_properties_get(props, "window");
		auto format = obs_property_list_format(window);
		auto count = obs_property_list_item_count(window);
		for (size_t i = 0; i < count; i++) {
			auto desc = obs_property_list_item_name(window, i);
			if (!desc || desc[0] == '\0')
				continue;
			if (format == OBS_COMBO_FORMAT_STRING) {
				auto val = obs_property_list_item_string(window, i);
				menu.addAction(QString::fromUtf8(desc), [this, val] {
					auto s2 = obs_weak_source_get_source(source);
					if (!s2)
						return;
					auto d = obs_data_create();
					obs_data_set_string(d, "window", val);
					obs_source_update(s2, d);
					obs_data_release(d);
					obs_source_release(s2);
				});
			} else if (format == OBS_COMBO_FORMAT_INT) {
				auto val = obs_property_list_item_int(window, i);
				menu.addAction(QString::fromUtf8(desc), [this, val] {
					auto s2 = obs_weak_source_get_source(source);
					if (!s2)
						return;
					auto d = obs_data_create();
					obs_data_set_int(d, "window", val);
					obs_source_update(s2, d);
					obs_data_release(d);
					obs_source_release(s2);
				});
			}
		}

		menu.exec(QCursor::pos());
		obs_properties_destroy(props);
	});

	l2->addWidget(button);
	sourceLabel = new QLabel(QString::fromUtf8(obs_source_get_name(s)));

	l2->addWidget(sourceLabel);

	captureLabel = new QLabel;
	l->addWidget(captureLabel);
}

CaptureWidget::~CaptureWidget()
{
	auto s = obs_weak_source_get_source(source);
	if (!s)
		return;
	auto sh = obs_source_get_signal_handler(s);
	if (sh) {
		signal_handler_disconnect(sh, "rename", rename, this);
		signal_handler_disconnect(sh, "hooked", hooked, this);
		signal_handler_disconnect(sh, "unhooked", unhooked, this);
	}
	obs_source_release(s);
	obs_weak_source_release(source);
}

void CaptureWidget::hooked(void *param, calldata_t *cd)
{
	const char *hook_title = calldata_string(cd, "title");
	const char *hook_executable = calldata_string(cd, "executable");
	QString capture;
	if (hook_executable && hook_executable[0] != '\0')
		capture += "[" + QString::fromUtf8(hook_executable) + "]: ";
	if (hook_title && hook_title[0] != '\0')
		capture += QString::fromUtf8(hook_title);

	auto widget = (CaptureWidget *)param;
	QMetaObject::invokeMethod(widget, [widget, capture] { widget->captureLabel->setText(capture); });
}

void CaptureWidget::unhooked(void *param, calldata_t *cd)
{
	UNUSED_PARAMETER(cd);
	auto widget = (CaptureWidget *)param;
	QMetaObject::invokeMethod(widget, [widget] { widget->captureLabel->setText(""); });
}

void CaptureWidget::rename(void *param, calldata_t *cd)
{
	auto widget = (CaptureWidget *)param;
	auto name = QString::fromUtf8(calldata_string(cd, "new_name"));
	QMetaObject::invokeMethod(widget, [widget, name] { widget->sourceLabel->setText(name); });
}
