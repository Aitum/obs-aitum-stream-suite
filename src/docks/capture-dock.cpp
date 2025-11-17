#include "capture-dock.hpp"

#include <QScrollArea>
#include <src/utils/widgets/capture-widget.hpp>

void RemoveWidget(QWidget *widget);

CaptureDock::CaptureDock(QWidget *parent) : QFrame(parent)
{
	auto l = new QVBoxLayout;
	setLayout(l);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(0);
	auto scroll = new QScrollArea;
	scroll->setWidgetResizable(true);
	l->addWidget(scroll);

	auto w = new QWidget;
	w->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	scroll->setWidget(w);
	mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	w->setLayout(mainLayout);

	auto sh = obs_get_signal_handler();
	signal_handler_connect(sh, "source_create", source_create, this);
	signal_handler_connect(sh, "source_remove", source_remove, this);
}

CaptureDock::~CaptureDock()
{
	auto sh = obs_get_signal_handler();
	if (sh) {
		signal_handler_disconnect(sh, "source_create", source_create, this);
		signal_handler_disconnect(sh, "source_remove", source_remove, this);
	}
}

void CaptureDock::source_create(void *param, calldata_t *cd)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	const char *id = obs_source_get_unversioned_id(source);
	if (strcmp(id, "game_capture") != 0 && strcmp(id, "window_capture") != 0)
		return;
	auto dock = (CaptureDock *)param;
	QMetaObject::invokeMethod(dock, [dock, source] { dock->mainLayout->addWidget(new CaptureWidget(source)); });
}

void CaptureDock::source_remove(void *param, calldata_t *cd)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	const char *id = obs_source_get_unversioned_id(source);
	if (strcmp(id, "game_capture") != 0 && strcmp(id, "window_capture") != 0)
		return;
	auto dock = (CaptureDock *)param;
	QMetaObject::invokeMethod(dock, [dock, source] {
		auto c = dock->mainLayout->count();
		for (int i = c - 1; i >= 0; i--) {
			auto item = dock->mainLayout->itemAt(i);
			if (!item)
				continue;
			auto w = item->widget();
			if (!w)
				continue;
			auto cw = (CaptureWidget *)w;
			if (cw->IsSource(source)) {
				dock->mainLayout->removeWidget(cw);
				RemoveWidget(cw);
			}
		}
	});
}
