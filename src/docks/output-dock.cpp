#include "output-dock.hpp"

#include "../version.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QCheckBox>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolBar>
#include <src/utils/color.hpp>
#include <src/utils/icon.hpp>
#include <util/config-file.h>
#include <util/platform.h>

void open_config_dialog(int tab);

OutputDock::OutputDock(QWidget *parent) : QFrame(parent)
{
	auto t = new QWidget;

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidget(t);
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);
	//scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	auto layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(scrollArea);

	auto toolbar = new QToolBar();
	toolbar->setObjectName(QStringLiteral("outputsToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);
	auto a = toolbar->addAction(QIcon(QString::fromUtf8(":/res/images/plus.svg")),
				    QString::fromUtf8(obs_frontend_get_locale_string("Add")), [this] { open_config_dialog(2); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-plus");
	layout->addWidget(toolbar);

	setLayout(layout);

	mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	t->setLayout(mainLayout);

	// blank because we're pulling settings through from bis later
	mainPlatformIconLabel = new QLabel;
	mainPlatformIconLabel->setPixmap(
		getPlatformIconFromEndpoint(QString::fromUtf8("")).pixmap(outputPlatformIconSize, outputPlatformIconSize));

	auto l2 = new QHBoxLayout;

	l2->addWidget(mainPlatformIconLabel);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("BuiltinStream"))), 1);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("MainCanvas"))), 1);
	mainStreamButton = new QPushButton;
	mainStreamButton->setObjectName(QStringLiteral("canvasStream"));
	mainStreamButton->setMinimumHeight(30);
	mainStreamButton->setIcon(create2StateIcon(":/aitum/media/streaming.svg", ":/aitum/media/stream.svg"));
	mainStreamButton->setStyleSheet("QAbstractButton:checked{background: rgb(0,210,153);}");
	mainStreamButton->setCheckable(true);
	mainStreamButton->setChecked(false);

	connect(mainStreamButton, &QPushButton::clicked, [this] {
		const auto config = obs_frontend_get_user_config();
		if (obs_frontend_streaming_active()) {
			bool stop = true;
			bool warnBeforeStreamStop = config_get_bool(config, "BasicWindow", "WarnBeforeStoppingStream");
			if (warnBeforeStreamStop && isVisible()) {
				auto button = QMessageBox::question(
					this, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Title")),
					QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Text")),
					QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
				if (button == QMessageBox::No)
					stop = false;
			}
			if (stop) {
				obs_frontend_streaming_stop();
				mainStreamButton->setChecked(false);
			} else {
				mainStreamButton->setChecked(true);
			}
		} else {
			bool warnBeforeStreamStart = config_get_bool(config, "BasicWindow", "WarnBeforeStartingStream");
			if (warnBeforeStreamStart && isVisible()) {
				auto button = QMessageBox::question(
					this, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Title")),
					QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Text")),
					QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
				if (button == QMessageBox::No) {
					mainStreamButton->setChecked(false);
				} else {
					obs_frontend_streaming_start();
				}
			} else {
				obs_frontend_streaming_start();
			}
			mainStreamButton->setChecked(true);
		}
	});
	//streamButton->setSizePolicy(sp2);
	mainStreamButton->setToolTip(QString::fromUtf8(obs_module_text("Stream")));
	l2->addWidget(mainStreamButton);
	//mainLayout->addLayout(l2);

	auto streamGroup = new QFrame;
	streamGroup->setLayout(l2);

	mainLayout->addWidget(streamGroup);

	QWidget *spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	mainLayout->addWidget(spacer);

	connect(&videoCheckTimer, &QTimer::timeout, [this] {
		if (exiting)
			return;
		/* if (obs_get_video() != mainVideo) {
			oldVideo.push_back(mainVideo);
			mainVideo = obs_get_video();
			for (auto it = outputs.begin(); it != outputs.end(); it++) {
				auto venc = obs_output_get_video_encoder(std::get<obs_output_t *>(*it));
				if (venc && !obs_encoder_active(venc))
					obs_encoder_set_video(venc, mainVideo);
			}
		}*/

		auto service = obs_frontend_get_streaming_service();
		auto url = QString::fromUtf8(service ? obs_service_get_connect_info(service, OBS_SERVICE_CONNECT_INFO_SERVER_URL)
						     : "");
		if (url != mainPlatformUrl) {
			mainPlatformUrl = url;
			mainPlatformIconLabel->setPixmap(
				getPlatformIconFromEndpoint(url).pixmap(outputPlatformIconSize, outputPlatformIconSize));
		}

		auto active = obs_frontend_streaming_active();
		if (mainStreamButton->isChecked() != active) {
			mainStreamButton->setChecked(active);
		}

		for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
			(*it)->CheckActive();
		}
	});
	videoCheckTimer.start(500);
}

extern OutputDock *output_dock;

OutputDock::~OutputDock()
{
	/*
	videoCheckTimer.stop();
	for (auto it = outputs.begin(); it != outputs.end(); it++) {
		auto old = std::get<obs_output_t *>(*it);
		signal_handler_t *signal = obs_output_get_signal_handler(old);
		signal_handler_disconnect(signal, "start", stream_output_start, this);
		signal_handler_disconnect(signal, "stop", stream_output_stop, this);
		auto service = obs_output_get_service(old);
		if (obs_output_active(old)) {
			obs_output_force_stop(old);
		}
		if (!exiting)
			obs_output_release(old);
		obs_service_release(service);
	}
	outputs.clear();
	
	obs_frontend_remove_event_callback(frontend_event, this);*/
	output_dock = nullptr;
}

extern obs_data_t *current_profile_config;
void RemoveWidget(QWidget *widget);

void OutputDock::LoadSettings()
{
	auto outputs2 = obs_data_get_array(current_profile_config, "outputs");
	for (auto it = outputWidgets.begin(); it != outputWidgets.end();) {
		bool found = false;
		auto count = obs_data_array_count(outputs2);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *data2 = obs_data_array_item(outputs2, i);
			auto name = QString::fromUtf8(obs_data_get_string(data2, "name"));
			obs_data_release(data2);
			if (name == objectName()) {
				(*it)->UpdateSettings(data2);
				found = true;
				break;
			}
		}
		if (!found) {
			mainLayout->removeWidget(*it);
			RemoveWidget(*it);
			it = outputWidgets.erase(it);
		} else {
			it++;
		}
	}

	auto count = obs_data_array_count(outputs2);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *data2 = obs_data_array_item(outputs2, i);
		auto name = QString::fromUtf8(obs_data_get_string(data2, "name"));
		if (std::find_if(outputWidgets.begin(), outputWidgets.end(),
				 [&name](OutputWidget *ow) { return ow->objectName() == name; }) == outputWidgets.end()) {
			auto outputWidget = new OutputWidget(data2, this);
			outputWidgets.push_back(outputWidget);
			mainLayout->insertWidget((int)i + 1, outputWidget);
		}
		obs_data_release(data2);
	}

	obs_data_array_release(outputs2);

	QWidget *spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	mainLayout->addWidget(spacer);
}

void OutputDock::SaveSettings()
{
	for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
		(*it)->SaveSettings();
	}
}

void OutputDock::LoadOutput(obs_data_t *output_data) {}

void OutputDock::UpdateMainStreamStatus(bool active)
{
	mainStreamButton->setChecked(active);
	if (!active)
		return;
	if (!current_profile_config)
		return;
	struct obs_video_info ovi = {0};
	obs_get_video_info(&ovi);
	double fps = ovi.fps_den > 0 ? (double)ovi.fps_num / (double)ovi.fps_den : 0.0;
	auto output = obs_frontend_get_streaming_output();
	bool found = false;
	for (auto i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
		auto encoder = obs_output_get_video_encoder2(output, i);
		QString settingName = QString::fromUtf8("video_encoder_description") + QString::number(i);
		if (encoder) {
			found = true;
			auto mainEncoderDescription = QString::number(obs_encoder_get_width(encoder)) + "x" +
						      QString::number(obs_encoder_get_height(encoder));
			auto divisor = obs_encoder_get_frame_rate_divisor(encoder);
			if (divisor > 0)
				mainEncoderDescription +=
					QString::fromUtf8(" ") + QString::number(fps / divisor, 'g', 4) + QString::fromUtf8("fps");

			auto settings = obs_encoder_get_settings(encoder);
			auto bitrate = settings ? obs_data_get_int(settings, "bitrate") : 0;
			if (bitrate > 0)
				mainEncoderDescription +=
					QString::fromUtf8(" ") + QString::number(bitrate) + QString::fromUtf8("Kbps");
			obs_data_release(settings);

			obs_data_set_string(current_profile_config, settingName.toUtf8().constData(),
					    mainEncoderDescription.toUtf8().constData());

		} else if (found && obs_data_has_user_value(current_profile_config, settingName.toUtf8().constData())) {
			obs_data_unset_user_value(current_profile_config, settingName.toUtf8().constData());
		}
	}
	obs_output_release(output);
}
