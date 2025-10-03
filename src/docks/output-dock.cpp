#include "output-dock.hpp"

#include "../version.h"
#include <obs-module.h>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <util/config-file.h>
#include <obs-frontend-api.h>
#include <src/utils/color.hpp>

QIcon create2StateIcon(QString fileOn, QString fileOff);

QIcon getPlatformIconFromEndpoint(QString endpoint)
{

	if (endpoint.contains(QString::fromUtf8("ingest.global-contribute.live-video.net")) ||
	    endpoint.contains(QString::fromUtf8(".contribute.live-video.net")) ||
	    endpoint.contains(QString::fromUtf8(".twitch.tv"))) { // twitch
		return QIcon(":/aitum/media/twitch.png");
	} else if (endpoint.contains(QString::fromUtf8(".youtube.com"))) { // youtube
		return QIcon(":/aitum/media/youtube.png");
	} else if (endpoint.contains(QString::fromUtf8("fa723fc1b171.global-contribute.live-video.net"))) { // kick
		return QIcon(":/aitum/media/kick.png");
	} else if (endpoint.contains(QString::fromUtf8(".tiktokcdn"))) { // tiktok
		return QIcon(":/aitum/media/tiktok.png");
	} else if (endpoint.contains(QString::fromUtf8(".pscp.tv"))) { // twitter
		return QIcon(":/aitum/media/twitter.png");
	} else if (endpoint.contains(QString::fromUtf8("livepush.trovo.live"))) { // trovo
		return QIcon(":/aitum/media/trovo.png");
	} else if (endpoint.contains(QString::fromUtf8(".facebook.com")) ||
		   endpoint.contains(QString::fromUtf8(".fbcdn.net"))) { // facebook
		return QIcon(":/aitum/media/facebook.png");
	} else { // unknown
		return QIcon(":/aitum/media/unknown.png");
	}
}

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

		int idx = 0;
		while (auto item = mainLayout->itemAt(idx++)) {
			auto streamGroup = item->widget();
			if (idx == 1) {
				auto active = obs_frontend_streaming_active();
				foreach(QObject * c, streamGroup->children())
				{
					std::string cn = c->metaObject()->className();
					if (cn == "QPushButton") {
						auto pb = (QPushButton *)c;
						if (pb->isChecked() != active) {
							pb->setChecked(active);
						}
					}
				}
				continue;
			}
			std::string name = streamGroup->objectName().toUtf8().constData();
			if (name.empty())
				continue;
			for (auto it = outputs.begin(); it != outputs.end(); it++) {
				if (std::get<std::string>(*it) != name)
					continue;

				auto active = obs_output_active(std::get<obs_output_t *>(*it));
				foreach(QObject * c, streamGroup->children())
				{
					std::string cn = c->metaObject()->className();
					if (cn == "QPushButton") {
						auto pb = (QPushButton *)c;
						if (pb->isChecked() != active) {
							pb->setChecked(active);
						}
					}
				}
			}
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
	int idx = 1;
	while (auto item = mainLayout->itemAt(idx)) {
		auto streamGroup = item->widget();
		mainLayout->removeWidget(streamGroup);
		RemoveWidget(streamGroup);
	}

	obs_data_array_enum(
		outputs2,
		[](obs_data_t *data2, void *param) {
			auto d = (OutputDock *)param;
			d->LoadOutput(data2);
		},
		this);
	obs_data_array_release(outputs2);

	QWidget *spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	mainLayout->addWidget(spacer);
}

void OutputDock::LoadOutput(obs_data_t *output_data)
{
	auto nameChars = obs_data_get_string(output_data, "name");
	auto name = QString::fromUtf8(nameChars);

	for (int i = 1; i < mainLayout->count(); i++) {
		auto item = mainLayout->itemAt(i);
		auto oName = item->widget()->objectName();
		if (oName == name) {
			return;
		}
	}

	auto streamButton = new QPushButton;
	for (auto it = outputs.begin(); it != outputs.end(); it++) {
		if (std::get<std::string>(*it) != nameChars)
			continue;
		if (obs_data_get_bool(output_data, "advanced")) {
			auto output = std::get<obs_output_t *>(*it);
			auto video_encoder = obs_output_get_video_encoder(output);
			if (video_encoder &&
			    strcmp(obs_encoder_get_id(video_encoder), obs_data_get_string(output_data, "video_encoder")) == 0) {
				auto ves = obs_data_get_obj(output_data, "video_encoder_settings");
				obs_encoder_update(video_encoder, ves);
				obs_data_release(ves);
			}
		}
		std::get<QPushButton *>(*it) = streamButton;
	}
	auto streamGroup = new QFrame;
	streamGroup->setProperty("class", "stream-group");

	auto canvas = obs_data_get_array(current_profile_config, "canvas");
	auto count = obs_data_array_count(canvas);
	auto fcn = obs_data_get_string(output_data, "canvas");
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(canvas,i);
		if (!item)
			continue;
		auto cn = obs_data_get_string(item, "name");
		if (cn[0] != '\0' && strcmp(cn, fcn) == 0) {
			auto c = color_from_int(obs_data_get_int(item, "color"));
			streamGroup->setStyleSheet(QString(".stream-group { border: 2px solid %1;}").arg(c.name(QColor::HexRgb)));
			obs_data_release(item);
			break;
		}
		obs_data_release(item);
	}
	obs_data_array_release(canvas);
	
	streamGroup->setObjectName(name);
	auto streamLayout = new QVBoxLayout;

	auto l2 = new QHBoxLayout;

	auto endpoint = QString::fromUtf8(obs_data_get_string(output_data, "stream_server"));
	auto platformIconLabel = new QLabel;
	auto platformIcon = getPlatformIconFromEndpoint(endpoint);

	platformIconLabel->setPixmap(platformIcon.pixmap(outputPlatformIconSize, outputPlatformIconSize));

	l2->addWidget(platformIconLabel);

	l2->addWidget(new QLabel(name), 1);

	streamButton->setMinimumHeight(30);
	streamButton->setObjectName(QStringLiteral("canvasStream"));
	streamButton->setIcon(create2StateIcon(":/aitum/media/streaming.svg", ":/aitum/media/stream.svg"));
	streamButton->setStyleSheet("QAbstractButton:checked{background: rgb(0,210,153);}");
	streamButton->setCheckable(true);
	streamButton->setChecked(false);

	connect(streamButton, &QPushButton::clicked, [this, streamButton, output_data] {
		if (streamButton->isChecked()) {
			blog(LOG_INFO, "[Aitum Stream Suite] start stream clicked '%s'", obs_data_get_string(output_data, "name"));
			if (!StartOutput(output_data, streamButton))
				streamButton->setChecked(false);
		} else {
			bool stop = true;
			bool warnBeforeStreamStop =
				config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStoppingStream");
			if (warnBeforeStreamStop && isVisible()) {
				auto button = QMessageBox::question(
					this, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Title")),
					QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Text")),
					QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
				if (button == QMessageBox::No)
					stop = false;
			}
			if (stop) {
				blog(LOG_INFO, "[Aitum Stream Suite] stop stream clicked '%s'",
				     obs_data_get_string(output_data, "name"));
				const char *name2 = obs_data_get_string(output_data, "name");
				for (auto it = outputs.begin(); it != outputs.end(); it++) {
					if (std::get<std::string>(*it) != name2)
						continue;

					obs_queue_task(
						OBS_TASK_GRAPHICS, [](void *param) { obs_output_stop((obs_output_t *)param); },
						std::get<obs_output *>(*it), false);
				}
			} else {
				streamButton->setChecked(true);
			}
		}
	});

	//streamButton->setSizePolicy(sp2);
	streamButton->setToolTip(QString::fromUtf8(obs_module_text("Stream")));
	l2->addWidget(streamButton);
	streamLayout->addLayout(l2);

	streamGroup->setLayout(streamLayout);

	mainLayout->addWidget(streamGroup);
}

void OutputDock::stream_output_start(void *data, calldata_t *calldata)
{
	auto md = (OutputDock *)data;
	auto output = (obs_output_t *)calldata_ptr(calldata, "output");
	for (auto it = md->outputs.begin(); it != md->outputs.end(); it++) {
		if (std::get<obs_output_t *>(*it) != output)
			continue;
		auto button = std::get<QPushButton *>(*it);
		if (!button->isChecked()) {
			QMetaObject::invokeMethod(button, [button, md] { button->setChecked(true); }, Qt::QueuedConnection);
		}
	}
}

void OutputDock::stream_output_stop(void *data, calldata_t *calldata)
{
	auto md = (OutputDock *)data;
	auto output = (obs_output_t *)calldata_ptr(calldata, "output");
	for (auto it = md->outputs.begin(); it != md->outputs.end(); it++) {
		if (std::get<obs_output_t *>(*it) != output)
			continue;
		auto button = std::get<QPushButton *>(*it);
		if (button->isChecked()) {
			QMetaObject::invokeMethod(button, [button, md] { button->setChecked(false); }, Qt::QueuedConnection);
		}
		if (!md->exiting)
			QMetaObject::invokeMethod(button, [output] { obs_output_release(output); }, Qt::QueuedConnection);
		md->outputs.erase(it);
		break;
	}
	//const char *last_error = (const char *)calldata_ptr(calldata, "last_error");
}

bool OutputDock::StartOutput(obs_data_t *settings, QPushButton *streamButton)
{
	if (!settings)
		return false;

	bool warnBeforeStreamStart = config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStartingStream");
	if (warnBeforeStreamStart && isVisible()) {
		auto button = QMessageBox::question(this, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Title")),
						    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Text")),
						    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (button == QMessageBox::No)
			return false;
	}

	const char *name = obs_data_get_string(settings, "name");
	for (auto it = outputs.begin(); it != outputs.end(); it++) {
		if (std::get<std::string>(*it) != name)
			continue;
		auto old = std::get<obs_output_t *>(*it);
		auto service = obs_output_get_service(old);
		if (obs_output_active(old)) {
			obs_output_force_stop(old);
		}
		obs_output_release(old);
		obs_service_release(service);
		outputs.erase(it);
		break;
	}
	obs_canvas_t *canvas = nullptr;
	obs_canvas_t *main_canvas = obs_get_main_canvas();
	const char *canvas_name = obs_data_get_string(settings, "canvas");
	if (!canvas_name || canvas_name[0] == '\0') {
		canvas = obs_get_main_canvas();
	} else {
		canvas = obs_get_canvas_by_name(canvas_name);
	}
	obs_canvas_release(main_canvas);
	bool main = canvas == main_canvas;
	if (!canvas) {
		blog(LOG_WARNING, "[Aitum Stream Suite] failed to start stream '%s' because canvas '%s' was not found", name,
		     canvas_name);
		QMessageBox::warning(this, QString::fromUtf8(obs_module_text("CanvasNotFound")),
				     QString::fromUtf8(obs_module_text("CanvasNotFound")));
		return false;
	}

	obs_canvas_release(canvas);
	obs_encoder_t *venc = nullptr;
	obs_encoder_t *aenc = nullptr;
	auto advanced = obs_data_get_bool(settings, "advanced");
	if (advanced) {
		auto output_video_encoder_name = obs_data_get_string(settings, "output_video_encoder");
		if (output_video_encoder_name && output_video_encoder_name[0] != '\0') {
			for (auto it = outputs.begin(); it != outputs.end(); it++) {
				if (std::get<std::string>(*it) != output_video_encoder_name)
					continue;
				venc = obs_output_get_video_encoder(std::get<obs_output_t *>(*it));
				break;
			}
			if (!venc) {
				blog(LOG_WARNING, "[Aitum Stream Suite] failed to start stream '%s' because '%s' was not started",
				     obs_data_get_string(settings, "name"), output_video_encoder_name);
				QMessageBox::warning(this, QString::fromUtf8(obs_module_text("OtherOutputNotActive")),
						     QString::fromUtf8(obs_module_text("OtherOutputNotActive")));
				return false;
			}

		} else {
			auto venc_name = obs_data_get_string(settings, "video_encoder");
			if (!venc_name || venc_name[0] == '\0') {
				if (main) {
					//use main encoder
					auto main_output = obs_frontend_get_streaming_output();
					if (!obs_output_active(main_output)) {
						obs_output_release(main_output);
						blog(LOG_WARNING,
						     "[Aitum Stream Suite] failed to start stream '%s' because main was not started",
						     obs_data_get_string(settings, "name"));
						QMessageBox::warning(this,
								     QString::fromUtf8(obs_module_text("MainOutputNotActive")),
								     QString::fromUtf8(obs_module_text("MainOutputNotActive")));
						return false;
					}
					auto vei = (int)obs_data_get_int(settings, "video_encoder_index");
					venc = obs_output_get_video_encoder2(main_output, vei);
					obs_output_release(main_output);
					if (!venc) {
						blog(LOG_WARNING,
						     "[Aitum Stream Suite] failed to start stream '%s' because encoder index %d was not found",
						     obs_data_get_string(settings, "name"), vei);
						QMessageBox::warning(
							this, QString::fromUtf8(obs_module_text("MainOutputEncoderIndexNotFound")),
							QString::fromUtf8(obs_module_text("MainOutputEncoderIndexNotFound")));
						return false;
					}
				} else {
					return false;
				}
			} else {
				obs_data_t *s = nullptr;
				auto ves = obs_data_get_obj(settings, "video_encoder_settings");
				if (ves) {
					s = obs_data_create();
					obs_data_apply(s, ves);
					obs_data_release(ves);
				}
				std::string video_encoder_name = "aitum_stream_suite_video_encoder_";
				video_encoder_name += name;
				venc = obs_video_encoder_create(venc_name, video_encoder_name.c_str(), s, nullptr);
				obs_data_release(s);
				obs_encoder_set_video(venc, obs_canvas_get_video(canvas));
				auto divisor = obs_data_get_int(settings, "frame_rate_divisor");
				if (divisor > 1)
					obs_encoder_set_frame_rate_divisor(venc, (uint32_t)divisor);

				bool scale = obs_data_get_bool(settings, "scale");
				if (scale) {
					obs_encoder_set_scaled_size(venc, (uint32_t)obs_data_get_int(settings, "width"),
								    (uint32_t)obs_data_get_int(settings, "height"));
					obs_encoder_set_gpu_scale_type(venc,
								       (obs_scale_type)obs_data_get_int(settings, "scale_type"));
				}
			}
		}
		auto aenc_name = obs_data_get_string(settings, "audio_encoder");
		if (!aenc_name || aenc_name[0] == '\0') {
			//use main encoder
			auto main_output = obs_frontend_get_streaming_output();
			if (!obs_output_active(main_output)) {
				obs_output_release(main_output);
				blog(LOG_WARNING, "[Aitum Stream Suite] failed to start stream '%s' because main was not started",
				     obs_data_get_string(settings, "name"));
				QMessageBox::warning(this, QString::fromUtf8(obs_module_text("MainOutputNotActive")),
						     QString::fromUtf8(obs_module_text("MainOutputNotActive")));
				return false;
			}
			auto aei = (int)obs_data_get_int(settings, "audio_encoder_index");
			aenc = obs_output_get_audio_encoder(main_output, aei);
			obs_output_release(main_output);
			if (!aenc) {
				blog(LOG_WARNING,
				     "[Aitum Stream Suite] failed to start stream '%s' because encoder index %d was not found",
				     obs_data_get_string(settings, "name"), aei);
				QMessageBox::warning(this, QString::fromUtf8(obs_module_text("MainOutputEncoderIndexNotFound")),
						     QString::fromUtf8(obs_module_text("MainOutputEncoderIndexNotFound")));
				return false;
			}
		} else {
			obs_data_t *s = nullptr;
			auto aes = obs_data_get_obj(settings, "audio_encoder_settings");
			if (aes) {
				s = obs_data_create();
				obs_data_apply(s, aes);
				obs_data_release(aes);
			}
			std::string audio_encoder_name = "aitum_stream_suite_audio_encoder_";
			audio_encoder_name += name;
			aenc = obs_audio_encoder_create(aenc_name, audio_encoder_name.c_str(), s,
							obs_data_get_int(settings, "audio_track"), nullptr);
			obs_data_release(s);
			obs_encoder_set_audio(aenc, obs_get_audio());
		}
	} else {
		std::pair<obs_encoder_t **, video_t *> d = {&venc, obs_canvas_get_video(canvas)};
		obs_enum_outputs(
			[](void *param, obs_output_t *output) {
				uint32_t has_flags = OBS_OUTPUT_VIDEO | OBS_OUTPUT_ENCODED; //| OBS_OUTPUT_SERVICE;
				uint32_t flags = obs_output_get_flags(output);
				if ((flags & has_flags) != has_flags)
					return true;

				std::pair<obs_encoder_t **, video_t *> *d = (std::pair<obs_encoder_t **, video_t *> *)param;

				for (size_t idx = 0; idx < MAX_OUTPUT_VIDEO_ENCODERS; idx++) {
					auto enc = obs_output_get_video_encoder2(output, idx);
					if (enc && obs_encoder_video(enc) == d->second) {
						*d->first = enc;
						return false;
					}
				}
				return true;
			},
			&d);

		if (!venc && main) {
			auto main_output = obs_frontend_get_streaming_output();
			venc = main_output ? obs_output_get_video_encoder(main_output) : nullptr;
			obs_output_release(main_output);
			if (!venc || !obs_output_active(main_output)) {
				blog(LOG_WARNING, "[Aitum Stream Suite] failed to start stream '%s' because main was not started",
				     obs_data_get_string(settings, "name"));
				QMessageBox::warning(this, QString::fromUtf8(obs_module_text("MainOutputNotActive")),
						     QString::fromUtf8(obs_module_text("MainOutputNotActive")));
				return false;
			}
		}
		if (!aenc && main) {
			auto main_output = obs_frontend_get_streaming_output();
			aenc = main_output ? obs_output_get_audio_encoder(main_output, 0) : nullptr;
			obs_output_release(main_output);
			if (!aenc || !obs_output_active(main_output)) {
				blog(LOG_WARNING, "[Aitum Stream Suite] failed to start stream '%s' because main was not started",
				     obs_data_get_string(settings, "name"));
				QMessageBox::warning(this, QString::fromUtf8(obs_module_text("MainOutputNotActive")),
						     QString::fromUtf8(obs_module_text("MainOutputNotActive")));
				return false;
			}
		}
		if (venc && !aenc) {
			std::string audio_encoder_name = "aitum_stream_suite_audio_encoder_";
			audio_encoder_name += name;
			aenc = obs_audio_encoder_create("ffmpeg_aac", audio_encoder_name.c_str(), nullptr, 0, nullptr);
		}
	}

	if (!aenc || !venc) {
		return false;
	}
	auto server = obs_data_get_string(settings, "stream_server");
	if (!server || !strlen(server)) {
		server = obs_data_get_string(settings, "server");
		if (server && strlen(server))
			obs_data_set_string(settings, "stream_server", server);
	}
	bool whip = strstr(server, "whip") != nullptr;
	auto s = obs_data_create();
	obs_data_set_string(s, "server", server);
	auto key = obs_data_get_string(settings, "stream_key");
	if (!key || !strlen(key)) {
		key = obs_data_get_string(settings, "key");
		if (key && strlen(key))
			obs_data_set_string(settings, "stream_key", key);
	}
	if (whip) {
		obs_data_set_string(s, "bearer_token", key);
	} else {
		obs_data_set_string(s, "key", key);
	}
	//use_auth
	//username
	//password
	std::string service_name = "aitum_stream_suite_service_";
	service_name += name;
	auto service = obs_service_create(whip ? "whip_custom" : "rtmp_custom", service_name.c_str(), s, nullptr);
	obs_data_release(s);

	const char *type = obs_service_get_preferred_output_type(service);
	if (!type) {
		type = "rtmp_output";
		if (strncmp(server, "ftl", 3) == 0) {
			type = "ftl_output";
		} else if (strncmp(server, "rtmp", 4) != 0) {
			type = "ffmpeg_mpegts_muxer";
		}
	}
	std::string output_name = "aitum_stream_suite_output_";
	output_name += name;
	auto output = obs_output_create(type, output_name.c_str(), nullptr, nullptr);
	obs_output_set_service(output, service);

	config_t *config = obs_frontend_get_profile_config();
	if (config) {
		obs_data_t *output_settings = obs_data_create();
		obs_data_set_string(output_settings, "bind_ip", config_get_string(config, "Output", "BindIP"));
		obs_data_set_string(output_settings, "ip_family", config_get_string(config, "Output", "IPFamily"));
		obs_output_update(output, output_settings);
		obs_data_release(output_settings);

		bool useDelay = config_get_bool(config, "Output", "DelayEnable");
		auto delaySec = (uint32_t)config_get_int(config, "Output", "DelaySec");
		bool preserveDelay = config_get_bool(config, "Output", "DelayPreserve");
		obs_output_set_delay(output, useDelay ? delaySec : 0, preserveDelay ? OBS_OUTPUT_DELAY_PRESERVE : 0);
	}

	signal_handler_t *signal = obs_output_get_signal_handler(output);
	signal_handler_disconnect(signal, "start", stream_output_start, this);
	signal_handler_disconnect(signal, "stop", stream_output_stop, this);
	signal_handler_connect(signal, "start", stream_output_start, this);
	signal_handler_connect(signal, "stop", stream_output_stop, this);

	//for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
	//auto venc = obs_output_get_video_encoder2(main_output, 0);
	//for (size_t i = 0; i < MAX_OUTPUT_AUDIO_ENCODERS; i++) {
	//obs_output_get_audio_encoder(main_output, 0);

	obs_output_set_video_encoder(output, venc);
	obs_output_set_audio_encoder(output, aenc, 0);

	obs_output_start(output);

	outputs.push_back({obs_data_get_string(settings, "name"), output, streamButton});

	return true;
}

void OutputDock::UpdateMainStreamStatus(bool active)
{
	mainStreamButton->setChecked(active);
}
