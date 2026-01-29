#include "output-widget.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QMessageBox>
#include <src/utils/color.hpp>
#include <src/utils/icon.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <src/utils/obs-websocket-api.h>

extern obs_data_t *current_profile_config;

OutputWidget::OutputWidget(obs_data_t *output_data, QWidget *parent) : QFrame(parent), settings(output_data)
{
	obs_data_addref(settings);
	auto nameChars = obs_data_get_string(settings, "name");
	auto name = QString::fromUtf8(nameChars);

	setObjectName(name);

	auto outputLayout = new QHBoxLayout;

	outputButton = new QPushButton;
	outputButton->setMinimumHeight(30);
	outputButton->setObjectName(QStringLiteral("canvasOutput"));

	auto output_type = obs_data_get_string(settings, "type");
	if (output_type[0] == '\0' || strcmp(output_type, "stream") == 0) {
		auto endpoint = QString::fromUtf8(obs_data_get_string(settings, "stream_server"));
		auto platformIconLabel = new QLabel;
		auto platformIcon = getPlatformIconFromEndpoint(endpoint);

		platformIconLabel->setPixmap(platformIcon.pixmap(outputPlatformIconSize, outputPlatformIconSize));

		outputLayout->addWidget(platformIconLabel);
	}
	outputLayout->addWidget(new QLabel(name), 1);

	if (strcmp(output_type, "record") == 0) {
		outputButton->setIcon(create2StateIcon(":/aitum/media/recording.svg", ":/aitum/media/record.svg"));
		outputButton->setStyleSheet("QAbstractButton:checked{background: rgb(255,0,0);}");
		outputButton->setToolTip(QString::fromUtf8(obs_module_text("Record")));

		std::string splitName = "AitumStreamSuiteSplit";
		splitName += nameChars;
		std::string splitDescription = obs_frontend_get_locale_string("Basic.Main.SplitFile");
		splitDescription = splitDescription + " " + nameChars;

		splitHotkey = obs_hotkey_register_frontend(
			splitName.c_str(), splitDescription.c_str(),
			[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
				UNUSED_PARAMETER(id);
				UNUSED_PARAMETER(hotkey);
				if (!pressed)
					return;
				auto this_ = (OutputWidget *)data;
				if (!this_->output)
					return;
				calldata_t cd = {0};
				proc_handler_t *ph = obs_output_get_proc_handler(this_->output);
				proc_handler_call(ph, "split", &cd);
				calldata_free(&cd);
			},
			this);

		auto split_hotkey = obs_data_get_array(settings, "split_hotkey");
		obs_hotkey_load(splitHotkey, split_hotkey);
		obs_data_array_release(split_hotkey);

		std::string chapterName = "AitumStreamSuiteChapter";
		chapterName += nameChars;
		std::string chapterDescription = obs_frontend_get_locale_string("Basic.Main.AddChapterMarker");
		chapterDescription = chapterDescription + " " + nameChars;

		chapterHotkey = obs_hotkey_register_frontend(
			chapterName.c_str(), chapterDescription.c_str(),
			[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
				UNUSED_PARAMETER(id);
				UNUSED_PARAMETER(hotkey);
				if (!pressed)
					return;
				auto this_ = (OutputWidget *)data;
				if (!this_->output)
					return;
				calldata_t cd = {0};
				proc_handler_t *ph = obs_output_get_proc_handler(this_->output);
				proc_handler_call(ph, "add_chapter", &cd);
				calldata_free(&cd);
			},
			this);

		auto chapter_hotkey = obs_data_get_array(settings, "chapter_hotkey");
		obs_hotkey_load(chapterHotkey, chapter_hotkey);
		obs_data_array_release(chapter_hotkey);

	} else if (strcmp(output_type, "backtrack") == 0) {
		outputButton->setStyleSheet(QString::fromUtf8(
			"QPushButton:checked{background: rgb(26,87,255);} QPushButton{ border-top-right-radius: 0; border-bottom-right-radius: 0; width: 32px; padding-left: 0px; padding-right: 0px;}"));

		auto replayEnable = new QCheckBox;
		auto testl = new QHBoxLayout;
		outputButton->setLayout(testl);
		testl->addWidget(replayEnable);
		outputButton->setToolTip(QString::fromUtf8(obs_module_text("Backtrack")));

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		connect(replayEnable, &QCheckBox::checkStateChanged, [this, replayEnable] {
#else
		connect(replayEnable, &QCheckBox::stateChanged, [this, replayEnable] {
#endif
			if (replayEnable->isChecked() != outputButton->isChecked()) {
				outputButton->click();
			}
		});

		extraButton = new QPushButton;
		extraButton->setCheckable(true);
		extraButton->setIcon(create2StateIcon(":/aitum/media/backtrack_on.svg", ":/aitum/media/backtrack_off.svg"));
		extraButton->setStyleSheet(QString::fromUtf8(
			"QPushButton:checked{background: rgb(26,87,255);} QPushButton{width: 32px; padding-left: 0px; padding-right: 0px; border-top-left-radius: 0; border-bottom-left-radius: 0;}"));
		extraButton->setToolTip(QString::fromUtf8(obs_module_text("SaveBacktrack")));

		connect(extraButton, &QPushButton::clicked, [this] {
			calldata_t cd = {0};
			proc_handler_t *ph = obs_output_get_proc_handler(this->output);
			proc_handler_call(ph, "save", &cd);
			calldata_free(&cd);
		});

		connect(outputButton, &QPushButton::toggled, [this, replayEnable] {
			bool enabled = outputButton->isChecked();
			replayEnable->setChecked(enabled);
			extraButton->setChecked(enabled);
		});

		std::string ebName = "AitumStreamSuiteSaveBacktrack";
		ebName += nameChars;
		std::string ebDescription = obs_module_text("SaveBacktrack");
		ebDescription = ebDescription + " " + nameChars;

		extraHotkey = obs_hotkey_register_frontend(
			ebName.c_str(), ebDescription.c_str(),
			[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
				UNUSED_PARAMETER(id);
				UNUSED_PARAMETER(hotkey);
				if (!pressed)
					return;
				auto this_ = (OutputWidget *)data;
				QMetaObject::invokeMethod(this_->extraButton, "click");
			},
			this);

		auto extra_hotkey = obs_data_get_array(settings, "extra_hotkey");
		obs_hotkey_load(extraHotkey, extra_hotkey);
		obs_data_array_release(extra_hotkey);

	} else if (strcmp(output_type, "virtual_cam") == 0) {
		outputButton->setIcon(create2StateIcon(":/aitum/media/virtual_cam_on.svg", ":/aitum/media/virtual_cam_off.svg"));
		outputButton->setStyleSheet("QAbstractButton:checked{background: rgb(192,128,0);}");
		outputButton->setToolTip(QString::fromUtf8(obs_module_text("VirtualCamera")));
	} else {
		outputButton->setIcon(create2StateIcon(":/aitum/media/streaming.svg", ":/aitum/media/stream.svg"));
		outputButton->setStyleSheet("QAbstractButton:checked{background: rgb(0,210,153);}");
		outputButton->setToolTip(QString::fromUtf8(obs_module_text("Stream")));
	}
	outputButton->setCheckable(true);
	outputButton->setChecked(false);

	connect(outputButton, &QPushButton::clicked, [this]() {
		auto output_type = obs_data_get_string(settings, "type");
		if (outputButton->isChecked()) {
			blog(LOG_INFO, "[Aitum Stream Suite] start %s output clicked '%s'", output_type,
			     obs_data_get_string(settings, "name"));
			if (!StartOutput())
				outputButton->setChecked(false);
		} else {
			bool stop = true;

			if (output_type[0] == '\0' || strcmp(output_type, "stream") == 0) {
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
			}
			if (stop) {
				blog(LOG_INFO, "[Aitum Stream Suite] stop %s output clicked '%s'", output_type,
				     obs_data_get_string(settings, "name"));

				obs_queue_task(
					OBS_TASK_GRAPHICS, [](void *param) { obs_output_stop((obs_output_t *)param); }, output,
					false);

			} else {
				outputButton->setChecked(true);
			}
		}
	});

	setProperty("class", "output-frame");

	canvasLabel = new QLabel;
	UpdateCanvas();
	outputLayout->addWidget(canvasLabel, 1);
	if (extraButton) {
		auto l3 = new QHBoxLayout;
		l3->setContentsMargins(0, 0, 0, 0);
		l3->setSpacing(0);
		l3->addWidget(outputButton);
		l3->addWidget(extraButton);
		outputLayout->addLayout(l3);
	} else {
		outputLayout->addWidget(outputButton);
	}

	setLayout(outputLayout);

	std::string startName = "AitumStreamSuiteStartOutput";
	startName += nameChars;
	std::string startDescription = obs_module_text("StartHotkey");
	startDescription = startDescription + " " + nameChars;
	std::string stopName = "AitumStreamSuiteStopOutput";
	stopName += nameChars;
	std::string stopDescription = obs_module_text("StopHotkey");
	stopDescription = stopDescription + " " + nameChars;

	StartStopHotkey = obs_hotkey_pair_register_frontend(
		startName.c_str(), startDescription.c_str(), stopName.c_str(), stopDescription.c_str(),
		[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed)
				return false;
			auto this_ = (OutputWidget *)data;
			if (!this_->output || !obs_output_active(this_->output)) {
				QMetaObject::invokeMethod(this_->outputButton, "click");
				return true;
			}
			return false;
		},
		[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed)
				return false;
			auto this_ = (OutputWidget *)data;
			if (this_->output && obs_output_active(this_->output)) {
				QMetaObject::invokeMethod(this_->outputButton, "click");
				return true;
			}
			return false;
		},
		this, this);

	auto start_hotkey = obs_data_get_array(output_data, "start_hotkey");
	auto stop_hotkey = obs_data_get_array(output_data, "stop_hotkey");
	obs_hotkey_pair_load(StartStopHotkey, start_hotkey, stop_hotkey);
	obs_data_array_release(start_hotkey);
	obs_data_array_release(stop_hotkey);
}

OutputWidget::~OutputWidget()
{
	if (StartStopHotkey != OBS_INVALID_HOTKEY_PAIR_ID)
		obs_hotkey_pair_unregister(StartStopHotkey);
	if (extraHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(extraHotkey);
	if (splitHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(splitHotkey);
	if (chapterHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(chapterHotkey);
	if (output) {
		signal_handler_t *signal = obs_output_get_signal_handler(output);
		signal_handler_disconnect(signal, "start", output_start, this);
		signal_handler_disconnect(signal, "stop", output_stop, this);
		if (strcmp(obs_output_get_id(output), "virtualcam_output") == 0) {
			obs_output_set_media(output, obs_get_video(), obs_get_audio());
		}
		obs_output_release(output);
		output = nullptr;
	}
	obs_data_release(settings);
}

void OutputWidget::output_start(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);
	auto this_ = (OutputWidget *)data;
	if (this_->onStarted) {
		this_->onStarted();
		this_->onStarted = nullptr;
	}
	if (this_->outputButton->isChecked())
		return;
	QMetaObject::invokeMethod(this_->outputButton, [this_] { this_->outputButton->setChecked(true); }, Qt::QueuedConnection);
}

extern obs_websocket_vendor vendor;

void OutputWidget::output_stop(void *data, calldata_t *calldata)
{
	UNUSED_PARAMETER(calldata);
	auto this_ = (OutputWidget *)data;
	if (this_->onStarted) {
		const char *last_error = (const char *)calldata_ptr(calldata, "last_error");
		if (last_error)
			blog(LOG_WARNING, "[Aitum Stream Suite] failed to start '%s': %s", this_->objectName().toUtf8().constData(),
			     last_error);
		else
			blog(LOG_WARNING, "[Aitum Stream Suite] failed to start '%s'", this_->objectName().toUtf8().constData());

		this_->onStarted();
		this_->onStarted = nullptr;
	}
	if (this_->outputButton->isChecked()) {
		QMetaObject::invokeMethod(
			this_->outputButton, [this_] { this_->outputButton->setChecked(false); }, Qt::QueuedConnection);
	}

	if (this_->output) {
		const char *error = (const char *)calldata_ptr(calldata, "last_error");
		std::string last_error;
		if (error)
			last_error = error;
		auto code = calldata_int(calldata, "code");
		QMetaObject::invokeMethod(
			this_->outputButton,
			[this_, last_error, code] {
				if (this_->output && strcmp(obs_output_get_id(this_->output), "virtualcam_output") == 0) {
					obs_output_set_media(this_->output, obs_get_video(), obs_get_audio());
				}
				obs_output_release(this_->output);
				this_->output = nullptr;
				if (vendor) {
					const auto d = obs_data_create();
					obs_data_set_string(d, "output", obs_output_get_name(this_->output));
					if (!last_error.empty())
						obs_data_set_string(d, "last_error", last_error.c_str());
					obs_data_set_int(d, "code", code);

					obs_websocket_vendor_emit_event(vendor, "stop_output", d);
					obs_data_release(d);
				}
			},
			Qt::QueuedConnection);
	}
}

bool OutputWidget::StartOutput(bool automated)
{
	if (!settings)
		return false;

	auto output_type = obs_data_get_string(settings, "type");
	if (!automated && (output_type[0] == '\0' || strcmp(output_type, "stream") == 0)) {

		bool warnBeforeStreamStart =
			config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStartingStream");
		if (warnBeforeStreamStart && isVisible()) {
			auto button = QMessageBox::question(this,
							    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Title")),
							    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Text")),
							    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
			if (button == QMessageBox::No)
				return false;
		}
	}

	const char *name = obs_data_get_string(settings, "name");
	if (output) {
		auto service = obs_output_get_service(output);
		if (obs_output_active(output)) {
			obs_output_force_stop(output);
		}
		obs_output_release(output);
		obs_service_release(service);
		output = nullptr;
	}

	if (strcmp(output_type, "virtual_cam") == 0) {
		const auto vco = obs_frontend_get_virtualcam_output();
		if (obs_output_active(vco)) {
			outputButton->setChecked(false);
			obs_output_release(vco);
			return false;
		}

		const char *canvas_name = obs_data_get_string(settings, "canvas");
		auto canvas = (!canvas_name || canvas_name[0] == '\0') ? obs_get_main_canvas()
								       : obs_get_canvas_by_name(canvas_name);
		if (!canvas)
			return false;

		signal_handler_t *signal = obs_output_get_signal_handler(vco);
		signal_handler_disconnect(signal, "start", output_start, this);
		signal_handler_disconnect(signal, "stop", output_stop, this);
		signal_handler_connect(signal, "start", output_start, this);
		signal_handler_connect(signal, "stop", output_stop, this);

		obs_output_set_media(vco, obs_canvas_get_video(canvas), obs_get_audio());
		if (!obs_output_start(vco)) {
			obs_canvas_release(canvas);
			return false;
		}
		output = vco;
		if (vendor) {
			const auto d = obs_data_create();
			obs_data_set_string(d, "output", name);
			obs_data_set_string(d, "canvas", obs_canvas_get_name(canvas));
			obs_websocket_vendor_emit_event(vendor, "start_output", d);
			obs_data_release(d);
		}
		obs_canvas_release(canvas);
		return true;
	}

	std::vector<obs_encoder_t *> vencs;
	std::vector<obs_encoder_t *> aencs;

	//obs_encoder_t *venc = nullptr;
	bool is_record = (strcmp(output_type, "record") == 0 || strcmp(output_type, "backtrack") == 0);
	auto advanced = obs_data_get_bool(settings, "advanced");
	auto video_encoders = obs_data_get_array(settings, "video_encoders");
	auto video_encoders_count = obs_data_array_count(video_encoders);
	if (video_encoders_count == 0) {
		auto venc = GetVideoEncoder(settings, advanced, is_record, name, automated);
		if (venc)
			vencs.push_back(venc);
	} else {
		for (size_t i = 0; i < video_encoders_count; i++) {
			auto item = obs_data_array_item(video_encoders, i);
			if (!item)
				continue;
			auto venc = GetVideoEncoder(item, advanced, is_record, name, automated);
			if (venc)
				vencs.push_back(venc);
			obs_data_release(item);
		}
	}
	obs_data_array_release(video_encoders);

	if (advanced) {
		auto aenc_name = obs_data_get_string(settings, "audio_encoder");
		if (!aenc_name || aenc_name[0] == '\0') {
			//use main encoder
			obs_output_t *main_output = nullptr;
			if (is_record) {
				main_output = obs_frontend_get_replay_buffer_output();
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
				if (!main_output)
					main_output = obs_frontend_get_recording_output();
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
			}
			if (!main_output)
				main_output = obs_frontend_get_streaming_output();

			if (!obs_output_active(main_output)) {
				obs_output_release(main_output);
				blog(LOG_WARNING, "[Aitum Stream Suite] failed to start output '%s' because main was not started",
				     name);
				if (!automated)
					QMessageBox::warning(this, QString::fromUtf8(obs_module_text("MainOutputNotActive")),
							     QString::fromUtf8(obs_module_text("MainOutputNotActive")));
				return false;
			}
			auto aenc = obs_output_get_audio_encoder(main_output, 0);
			obs_output_release(main_output);
			if (!aenc) {
				blog(LOG_WARNING,
				     "[Aitum Stream Suite] failed to start output '%s' because audio encoder was not found", name);
				if (!automated)
					QMessageBox::warning(this,
							     QString::fromUtf8(obs_module_text("MainOutputEncoderIndexNotFound")),
							     QString::fromUtf8(obs_module_text("MainOutputEncoderIndexNotFound")));
				return false;
			}
			aencs.push_back(aenc);
		} else {
			if (is_record) {
				auto tracks = obs_data_get_int(settings, "audio_tracks");
				if (!tracks)
					tracks = 1;

				for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
					if ((tracks & (1ll << i)) == 0)
						continue;
					obs_data_t *s = nullptr;
					auto aes = obs_data_get_obj(settings, "audio_encoder_settings");
					if (aes) {
						s = obs_data_create();
						obs_data_apply(s, aes);
						obs_data_release(aes);
					}

					std::string audio_encoder_name = "Aitum Stream Suite Audio ";
					audio_encoder_name += name;
					audio_encoder_name += " ";
					audio_encoder_name += std::to_string(i + 1);
					auto aenc = obs_audio_encoder_create(aenc_name, audio_encoder_name.c_str(), s, i, nullptr);
					obs_data_release(s);
					obs_encoder_set_audio(aenc, obs_get_audio());
					aencs.push_back(aenc);
				}
			} else {

				obs_data_t *s = nullptr;
				auto aes = obs_data_get_obj(settings, "audio_encoder_settings");
				if (aes) {
					s = obs_data_create();
					obs_data_apply(s, aes);
					obs_data_release(aes);
				}

				std::string audio_encoder_name = "Aitum Stream Suite Audio ";
				audio_encoder_name += name;
				auto audio_track = obs_data_get_int(settings, "audio_track");
				auto aenc =
					obs_audio_encoder_create(aenc_name, audio_encoder_name.c_str(), s, audio_track, nullptr);
				obs_data_release(s);
				obs_encoder_set_audio(aenc, obs_get_audio());
				aencs.push_back(aenc);
			}
		}
	} else {
		if (aencs.empty()) {
			obs_output_t *main_output = nullptr;
			if (is_record) {
				main_output = obs_frontend_get_replay_buffer_output();
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
				if (!main_output)
					main_output = obs_frontend_get_recording_output();
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
				if (!main_output)
					main_output = obs_frontend_get_streaming_output();

				for (size_t idx = 0; idx < MAX_OUTPUT_AUDIO_ENCODERS; idx++) {
					auto aenc = main_output ? obs_output_get_audio_encoder(main_output, idx) : nullptr;
					if (aenc)
						aencs.push_back(aenc);
				}
			} else {
				main_output = obs_frontend_get_streaming_output();
				auto aenc = main_output ? obs_output_get_audio_encoder(main_output, 0) : nullptr;
				if (aenc)
					aencs.push_back(aenc);
			}

			obs_output_release(main_output);
		}
		if (!vencs.empty() && aencs.empty()) {
			if (is_record) {
				for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
					std::string audio_encoder_name = "Aitum Stream Suite Audio ";
					audio_encoder_name += name;
					audio_encoder_name += " ";
					audio_encoder_name += std::to_string(i);
					auto aenc = obs_audio_encoder_create("ffmpeg_aac", audio_encoder_name.c_str(), nullptr, i,
									     nullptr);
					obs_encoder_set_audio(aenc, obs_get_audio());
					aencs.push_back(aenc);
				}
			} else {
				std::string audio_encoder_name = "Aitum Stream Suite Audio ";
				audio_encoder_name += name;
				auto aenc = obs_audio_encoder_create("ffmpeg_aac", audio_encoder_name.c_str(), nullptr, 0, nullptr);
				obs_encoder_set_audio(aenc, obs_get_audio());
				aencs.push_back(aenc);
			}
		}
	}

	if (vencs.empty()) {
		blog(LOG_WARNING, "[Aitum Stream Suite] Failed to start '%s', no video encoder found", name);
		if (!automated)
			QMessageBox::warning(this, QString::fromUtf8(obs_module_text("NoVideoEncoder")),
					     QString::fromUtf8(obs_module_text("NoVideoEncoder")));
		return false;
	}
	if (aencs.empty()) {
		blog(LOG_WARNING, "[Aitum Stream Suite] Failed to start '%s', no audio encoder found", name);
		if (!automated)
			QMessageBox::warning(this, QString::fromUtf8(obs_module_text("NoAudioEncoder")),
					     QString::fromUtf8(obs_module_text("NoAudioEncoder")));
		return false;
	}

	if (is_record) {
		std::string filenameFormat = obs_data_get_string(settings, "filename");
		if (filenameFormat.empty())
			filenameFormat = "%CCYY-%MM-%DD %hh-%mm-%ss";
		auto format = obs_data_get_string(settings, "format");
		if (format[0] == '\0')
			format = "hybrid_mp4";
		std::string ext = format;
		if (ext == "fragmented_mp4" || ext == "hybrid_mp4")
			ext = "mp4";
		else if (ext == "fragmented_mov" || ext == "hybrid_mov")
			ext = "mov";
		else if (ext == "hls")
			ext = "m3u8";
		else if (ext == "mpegts")
			ext = "ts";

		char *filename = os_generate_formatted_filename(ext.c_str(), true, filenameFormat.c_str());

		auto dir = obs_data_get_string(settings, "path");
		char path[512];
		if (dir[0] != '\0') {
			snprintf(path, 512, "%s/%s", dir, filename);
			ensure_directory(path);
		} else {
			snprintf(path, 512, "%s", filename);
		}
		bfree(filename);

		std::string output_name = "Aitum Stream Suite Output ";
		output_name += name;
		const char *output_id = "ffmpeg_muxer";
		if (strcmp(output_type, "backtrack") == 0)
			output_id = "replay_buffer";
		else if (strcmp(format, "hybrid_mp4") == 0)
			output_id = "mp4_output";
		else if (strcmp(format, "hybrid_mov") == 0)
			output_id = "mov_output";

		output = obs_output_create(output_id, output_name.c_str(), nullptr, nullptr);

		auto ps = obs_data_create();
		obs_data_set_string(ps, "path", path);
		obs_data_set_string(ps, "directory", dir);
		obs_data_set_string(ps, "format", filenameFormat.c_str());
		obs_data_set_string(ps, "extension", ext.c_str());
		obs_data_set_bool(ps, "split_file", true);
		obs_data_set_int(ps, "max_size_mb", obs_data_get_int(settings, "max_size_mb"));
		obs_data_set_int(ps, "max_time_sec", obs_data_get_int(settings, "max_time_sec"));
		obs_output_update(output, ps);
		obs_data_release(ps);
	} else {

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
		std::string output_name = "Aitum Stream Suite Output ";
		output_name += name;
		output = obs_output_create(type, output_name.c_str(), nullptr, nullptr);
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
	}

	signal_handler_t *signal = obs_output_get_signal_handler(output);
	signal_handler_disconnect(signal, "start", output_start, this);
	signal_handler_disconnect(signal, "stop", output_stop, this);
	signal_handler_connect(signal, "start", output_start, this);
	signal_handler_connect(signal, "stop", output_stop, this);

	for (size_t i = 0; i < vencs.size(); i++) {
		obs_output_set_video_encoder2(output, vencs[i], i);
	}

	for (size_t i = 0; i < aencs.size(); i++) {
		obs_output_set_audio_encoder(output, aencs[i], i);
	}

	if (!obs_output_start(output)) {
		obs_output_release(output);
		output = nullptr;
		return false;
	}
	if (vendor) {
		const auto d = obs_data_create();
		obs_data_set_string(d, "output", name);
		obs_websocket_vendor_emit_event(vendor, "start_output", d);
		obs_data_release(d);
	}
	return true;
}

obs_encoder_t *OutputWidget::GetVideoEncoder(obs_data_t *settings, bool advanced, bool is_record, const char *output_name,
					     bool automated)
{
	const char *canvas_name = obs_data_get_string(settings, "canvas");
	auto canvas = (!canvas_name || canvas_name[0] == '\0') ? obs_get_main_canvas() : obs_get_canvas_by_name(canvas_name);
	if (!canvas) {
		blog(LOG_WARNING, "[Aitum Stream Suite] canvas '%s' not found", canvas_name);
		if (!automated)
			QMessageBox::warning(this, QString::fromUtf8(obs_module_text("CanvasNotFound")),
					     QString::fromUtf8(obs_module_text("CanvasNotFound")));
		return nullptr;
	}
	auto main_canvas = obs_get_main_canvas();
	bool main = canvas == main_canvas;
	obs_canvas_release(main_canvas);
	obs_canvas_release(canvas);

	obs_encoder_t *venc = nullptr;
	if (advanced) {
		auto output_video_encoder_name = obs_data_get_string(settings, "output_video_encoder");
		if (output_video_encoder_name && output_video_encoder_name[0] != '\0' &&
		    (!main || strcmp(output_video_encoder_name, "MainEncoder") != 0)) {
			std::string other_output_name = "Aitum Stream Suite Output ";
			other_output_name += output_video_encoder_name;
			auto other_output = obs_get_output_by_name(other_output_name.c_str());
			if (other_output) {
				venc = obs_output_get_video_encoder(other_output);
				obs_output_release(other_output);
			}
			if (!venc) {
				blog(LOG_WARNING, "[Aitum Stream Suite] failed to start output '%s' because '%s' was not started",
				     output_name, output_video_encoder_name);
				if (!automated)
					QMessageBox::warning(this, QString::fromUtf8(obs_module_text("OtherOutputNotActive")),
							     QString::fromUtf8(obs_module_text("OtherOutputNotActive")));
				return nullptr;
			}

		} else {
			auto venc_name = obs_data_get_string(settings, "video_encoder");
			if (!venc_name || venc_name[0] == '\0') {
				if (main) {
					//use main encoder
					obs_output_t *main_output = nullptr;
					if (is_record) {
						main_output = obs_frontend_get_replay_buffer_output();
						if (main_output && !obs_output_active(main_output)) {
							obs_output_release(main_output);
							main_output = nullptr;
						}
						if (!main_output)
							main_output = obs_frontend_get_recording_output();
						if (main_output && !obs_output_active(main_output)) {
							obs_output_release(main_output);
							main_output = nullptr;
						}
					}
					if (!main_output)
						main_output = obs_frontend_get_streaming_output();

					if (!obs_output_active(main_output)) {
						obs_output_release(main_output);
						blog(LOG_WARNING,
						     "[Aitum Stream Suite] failed to start output '%s' because main was not started",
						     output_name);
						if (!automated)
							QMessageBox::warning(
								this, QString::fromUtf8(obs_module_text("MainOutputNotActive")),
								QString::fromUtf8(obs_module_text("MainOutputNotActive")));
						return nullptr;
					}
					auto vei = (int)obs_data_get_int(settings, "video_encoder_index");
					venc = obs_output_get_video_encoder2(main_output, vei);
					obs_output_release(main_output);
					if (!venc) {
						blog(LOG_WARNING,
						     "[Aitum Stream Suite] failed to start output '%s' because encoder index %d was not found",
						     output_name, vei);
						if (!automated)
							QMessageBox::warning(this,
									     QString::fromUtf8(obs_module_text(
										     "MainOutputEncoderIndexNotFound")),
									     QString::fromUtf8(obs_module_text(
										     "MainOutputEncoderIndexNotFound")));
						return nullptr;
					}
				} else {
					//default encoder
					std::pair<obs_encoder_t **, video_t *> d = {&venc, obs_canvas_get_video(canvas)};
					obs_enum_outputs(
						[](void *param, obs_output_t *output) {
							uint32_t has_flags = OBS_OUTPUT_VIDEO |
									     OBS_OUTPUT_ENCODED; //| OBS_OUTPUT_SERVICE;
							uint32_t flags = obs_output_get_flags(output);
							if ((flags & has_flags) != has_flags)
								return true;

							std::pair<obs_encoder_t **, video_t *> *d =
								(std::pair<obs_encoder_t **, video_t *> *)param;

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

					if (!venc) {
						auto videoEncoderIds = {"obs_nvenc_h264_tex", "jim_nvenc", "ffmpeg_nvenc",
									"obs_qsv11_v2", "h264_texture_amf"};
						const char *vencid = "obs_x264";
						foreach(auto videoEncoderId, videoEncoderIds)
						{
							if (EncoderAvailable(videoEncoderId)) {
								vencid = videoEncoderId;
								break;
							}
						}
						std::string venc_name = "Aitum Stream Suite Video ";
						venc_name += output_name;
						venc_name += " ";
						venc_name += obs_canvas_get_name(canvas);
						venc = obs_video_encoder_create(vencid, venc_name.c_str(), nullptr, nullptr);
						obs_encoder_set_video(venc, obs_canvas_get_video(canvas));
						auto video_settings = obs_data_create();
						obs_data_set_string(video_settings, "rate_control", "CBR");
						obs_data_set_int(video_settings, "bitrate", 6000);
						obs_encoder_update(venc, video_settings);
						obs_data_release(video_settings);
					}
				}
			} else {
				obs_data_t *s = nullptr;
				auto ves = obs_data_get_obj(settings, "video_encoder_settings");
				if (ves) {
					s = obs_data_create();
					obs_data_apply(s, ves);
					obs_data_release(ves);
				}
				std::string video_encoder_name = "Aitum Stream Suite Video ";
				video_encoder_name += output_name;
				video_encoder_name += " ";
				video_encoder_name += obs_canvas_get_name(canvas);
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
			obs_output_t *main_output = nullptr;
			if (is_record) {
				main_output = obs_frontend_get_replay_buffer_output();
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
				if (!main_output)
					main_output = obs_frontend_get_recording_output();
				if (main_output && !obs_output_active(main_output)) {
					obs_output_release(main_output);
					main_output = nullptr;
				}
			}
			if (!main_output)
				main_output = obs_frontend_get_streaming_output();
			venc = main_output ? obs_output_get_video_encoder(main_output) : nullptr;
			obs_output_release(main_output);
			if (!venc || !obs_output_active(main_output)) {
				blog(LOG_WARNING, "[Aitum Stream Suite] failed to start output '%s' because main was not started",
				     output_name);
				if (!automated)
					QMessageBox::warning(this, QString::fromUtf8(obs_module_text("MainOutputNotActive")),
							     QString::fromUtf8(obs_module_text("MainOutputNotActive")));
				return nullptr;
			}
		} else if (!venc) {
			auto videoEncoderIds = {"obs_nvenc_h264_tex", "jim_nvenc", "ffmpeg_nvenc", "obs_qsv11_v2",
						"h264_texture_amf"};
			const char *vencid = "obs_x264";
			foreach(auto videoEncoderId, videoEncoderIds)
			{
				if (EncoderAvailable(videoEncoderId)) {
					vencid = videoEncoderId;
					break;
				}
			}
			std::string venc_name = "Aitum Stream Suite Video ";
			venc_name += output_name;
			venc_name += " ";
			venc_name += obs_canvas_get_name(canvas);
			venc = obs_video_encoder_create(vencid, venc_name.c_str(), nullptr, nullptr);
			obs_encoder_set_video(venc, obs_canvas_get_video(canvas));
			auto video_settings = obs_data_create();
			obs_data_set_string(video_settings, "rate_control", "CBR");
			obs_data_set_int(video_settings, "bitrate", 6000);
			obs_encoder_update(venc, video_settings);
			obs_data_release(video_settings);
		}
	}

	return venc;
}

bool OutputWidget::EncoderAvailable(const char *encoder)
{
	const char *val;
	int i = 0;

	while (obs_enum_encoder_types(i++, &val))
		if (strcmp(val, encoder) == 0)
			return true;

	return false;
}

void OutputWidget::CheckActive()
{
	bool active = obs_output_active(output);
	if (outputButton->isChecked() != active)
		this->outputButton->setChecked(active);
}

void OutputWidget::SaveSettings()
{
	if (StartStopHotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_data_array_t *start_hotkey = nullptr;
		obs_data_array_t *stop_hotkey = nullptr;
		obs_hotkey_pair_save(StartStopHotkey, &start_hotkey, &stop_hotkey);
		obs_data_set_array(settings, "start_hotkey", start_hotkey);
		obs_data_set_array(settings, "stop_hotkey", stop_hotkey);
		obs_data_array_release(start_hotkey);
		obs_data_array_release(stop_hotkey);
	}
	if (extraHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *extra_hotkey = obs_hotkey_save(extraHotkey);
		obs_data_set_array(settings, "extra_hotkey", extra_hotkey);
		obs_data_array_release(extra_hotkey);
	}
	if (splitHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *split_hotkey = obs_hotkey_save(splitHotkey);
		obs_data_set_array(settings, "split_hotkey", split_hotkey);
		obs_data_array_release(split_hotkey);
	}
	if (chapterHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *chapter_hotkey = obs_hotkey_save(chapterHotkey);
		obs_data_set_array(settings, "chapter_hotkey", chapter_hotkey);
		obs_data_array_release(chapter_hotkey);
	}
}

void OutputWidget::UpdateSettings(obs_data_t *data)
{
	obs_data_release(settings);
	settings = data;
	obs_data_addref(settings);
	UpdateCanvas();
}

void OutputWidget::UpdateCanvas()
{
	auto canvas_name = obs_data_get_string(settings, "canvas");
	auto video_encoders = obs_data_get_array(settings, "video_encoders");
	auto count = obs_data_array_count(video_encoders);
	QSet<QString> canvases;
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(video_encoders, i);
		if (!item)
			continue;
		auto cn = obs_data_get_string(item, "canvas");
		canvases.insert(QString::fromUtf8(cn[0] == '\0' ? obs_module_text("MainCanvas") : cn));
		obs_data_release(item);
	}
	canvasLabel->setText(canvases.isEmpty()
				     ? QString::fromUtf8(canvas_name[0] == '\0' ? obs_module_text("MainCanvas") : canvas_name)
				     : canvases.values().join(", "));
	obs_data_array_release(video_encoders);

	auto canvas = obs_data_get_array(current_profile_config, "canvas");
	count = obs_data_array_count(canvas);
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(canvas, i);
		if (!item)
			continue;
		auto cn = obs_data_get_string(item, "name");
		if (cn[0] != '\0' && strcmp(cn, canvas_name) == 0) {
			auto c = color_from_int(obs_data_get_int(item, "color"));
			setStyleSheet(QString(".output-frame { border: 2px solid %1;}").arg(c.name(QColor::HexRgb)));
			obs_data_release(item);
			break;
		}
		obs_data_release(item);
	}
	obs_data_array_release(canvas);
}

void OutputWidget::ensure_directory(char *path)
{
#ifdef _WIN32
	char *backslash = strrchr(path, '\\');
	if (backslash)
		*backslash = '/';
#endif

	char *slash = strrchr(path, '/');
	if (slash) {
		*slash = 0;
		os_mkdirs(path);
		*slash = '/';
	}

#ifdef _WIN32
	if (backslash)
		*backslash = '\\';
#endif
}

bool OutputWidget::AddChapter(const char *chapter_name)
{
	if (!output)
		return false;
	proc_handler_t *ph = obs_output_get_proc_handler(output);
	calldata cd;
	calldata_init(&cd);
	calldata_set_string(&cd, "chapter_name", chapter_name);
	bool result = proc_handler_call(ph, "add_chapter", &cd);
	calldata_free(&cd);
	return result;
}

bool OutputWidget::StartOutput(std::function<void()> onStarted)
{
	if (output && obs_output_active(output)) {
		onStarted();
		return true;
	}
	auto name = this->objectName();
	this->onStarted = onStarted;
	auto starting = StartOutput(true);
	if (!starting)
		this->onStarted = nullptr;
	return starting;
}

void OutputWidget::StopOutput()
{
	if (!output || !obs_output_active(output))
		return;

	if (obs_output_get_active_delay(output) > 0) {
		obs_output_stop(output);
	} else {
		obs_output_force_stop(output);
	}
}

bool OutputWidget::IsStream() const
{
	const auto output_type = obs_data_get_string(settings, "type");
	return (output_type[0] == '\0' || strcmp(output_type, "stream") == 0);
}

bool OutputWidget::IsRecord() const
{
	const auto output_type = obs_data_get_string(settings, "type");
	return (strcmp(output_type, "record") == 0 || strcmp(output_type, "backtrack") == 0);
}

const char *OutputWidget::GetOutputType() const
{
	const auto output_type = obs_data_get_string(settings, "type");
	if (output_type[0] == '\0')
		return "stream";
	else
		return output_type;
}
