#include "output-dock.hpp"

#include "../version.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QCheckBox>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QToolBar>
#include <QToolButton>
#include <src/utils/color.hpp>
#include <src/utils/icon.hpp>
#include <util/config-file.h>
#include <util/platform.h>

void open_config_dialog(int tab, const char *create_type);

OutputDock::OutputDock(QWidget *parent) : QFrame(parent)
{

	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

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

	auto a = toolbar->addAction(QIcon(QString::fromUtf8(":/settings/images/settings/general.svg")),
				    QString::fromUtf8(obs_frontend_get_locale_string("Settings")),
				    [this] { open_config_dialog(2, nullptr); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("propertiesIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-gear");

	auto addMenu = new QMenu;

	a = toolbar->addAction(QIcon(":/res/images/plus.svg"), QString::fromUtf8(obs_module_text("AddOutput")),
			       [addMenu] { addMenu->exec(QCursor::pos()); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-plus");

	a = toolbar->addAction(
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStart, QIcon(":/res/images/media/media_play.svg")),
#else
		QIcon(":/res/images/media/media_play.svg"),
#endif
		QString::fromUtf8(obs_module_text("StartAll")));
	connect(a, &QAction::triggered, [this] {
		QMenu startMenu;
		auto a2 = startMenu.addAction(
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
			QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStart, QIcon(":/res/images/media/media_play.svg")),
#else
			QIcon(":/res/images/media/media_play.svg"),
#endif
			QString::fromUtf8(obs_module_text("StartAllOutputs")), [this]() { StartAll(false, false); });
		a2->setProperty("themeID", QVariant(QString::fromUtf8("playIcon")));
		a2->setProperty("class", "icon-media-play");
		if (mainStreamButton || std::find_if(outputWidgets.begin(), outputWidgets.end(),
						     [](OutputWidget *w) { return w->IsStream(); }) != outputWidgets.end()) {
			startMenu.addAction(QIcon(QString::fromUtf8(":/aitum/media/stream.svg")),
					    QString::fromUtf8(obs_module_text("StartAllStreams")),
					    [this]() { StartAll(true, false); });
		}
		if (mainRecordButton || std::find_if(outputWidgets.begin(), outputWidgets.end(),
						     [](OutputWidget *w) { return w->IsRecord(); }) != outputWidgets.end()) {
			startMenu.addAction(QIcon(QString::fromUtf8(":/aitum/media/record.svg")),
					    QString::fromUtf8(obs_module_text("StartAllRecordings")),
					    [this]() { StartAll(false, true); });
		}
		startMenu.exec(QCursor::pos());
	});
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("playIcon")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-media-play");
	a->setIconText(QString::fromUtf8(obs_module_text("StartAll")));
	((QToolButton *)toolbar->widgetForAction(a))->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	toolbar->widgetForAction(a)->setStyleSheet("QToolButton { min-width: 80px; max-width: none; }");

	a = toolbar->addAction(
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		QIcon::fromTheme(QIcon::ThemeIcon::MediaPlaybackStop, QIcon(":/res/images/media/media_stop.svg")),
#else
		QIcon(":/res/images/media/media_stop.svg"),
#endif
		QString::fromUtf8(obs_module_text("StopAllOutputs")), [this] { StopAll(); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("stopIcon")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-media-stop");

	a = addMenu->addAction(QIcon(QString::fromUtf8(":/aitum/media/stream.svg")), QString::fromUtf8(obs_module_text("Stream")),
			       [this] { open_config_dialog(2, "stream"); });

	a = addMenu->addAction(QIcon(QString::fromUtf8(":/aitum/media/record.svg")), QString::fromUtf8(obs_module_text("Record")),
			       [this] { open_config_dialog(2, "record"); });

	a = addMenu->addAction(QIcon(QString::fromUtf8(":/aitum/media/backtrack_off.svg")),
			       QString::fromUtf8(obs_module_text("Backtrack")), [this] { open_config_dialog(2, "backtrack"); });

	a = addMenu->addAction(QIcon(QString::fromUtf8(":/aitum/media/virtual_cam_off.svg")),
			       QString::fromUtf8(obs_module_text("VirtualCamera")),
			       [this] { open_config_dialog(2, "virtual_cam"); });

	layout->addWidget(toolbar);

	setLayout(layout);

	mainLayout = new QVBoxLayout;
	mainLayout->setContentsMargins(0, 0, 0, 0);
	t->setLayout(mainLayout);

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

	mainStreamGroup = new QFrame;
	mainStreamGroup->setLayout(l2);

	mainLayout->addWidget(mainStreamGroup);

	l2 = new QHBoxLayout;
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("BuiltinRecord"))), 1);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("MainCanvas"))), 1);
	mainRecordButton = new QPushButton;
	mainRecordButton->setObjectName(QStringLiteral("canvasRecord"));
	mainRecordButton->setMinimumHeight(30);
	mainRecordButton->setIcon(create2StateIcon(":/aitum/media/recording.svg", ":/aitum/media/record.svg"));
	mainRecordButton->setStyleSheet("QAbstractButton:checked{background: rgb(255,0,0);}");
	mainRecordButton->setCheckable(true);
	mainRecordButton->setChecked(false);
	QObject::connect(mainRecordButton, SIGNAL(clicked()), main_window, SLOT(RecordActionTriggered()));

	l2->addWidget(mainRecordButton);

	mainRecordGroup = new QFrame;
	mainRecordGroup->setLayout(l2);
	mainLayout->addWidget(mainRecordGroup);

	l2 = new QHBoxLayout;
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("BuiltinBacktrack"))), 1);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("MainCanvas"))), 1);

	//mainBacktrackCheckbox =

	mainBacktrackCheckboxButton = new QPushButton;
	mainBacktrackCheckboxButton->setCheckable(true);
	mainBacktrackCheckboxButton->setStyleSheet(QString::fromUtf8(
		"QPushButton:checked{background: rgb(26,87,255);} QPushButton{ border-top-right-radius: 0; border-bottom-right-radius: 0; width: 32px; padding-left: 0px; padding-right: 0px;}"));

	auto buttonLayout = new QHBoxLayout;
	mainBacktrackCheckboxButton->setLayout(buttonLayout);

	mainBacktrackCheckbox = new QCheckBox;
	buttonLayout->addWidget(mainBacktrackCheckbox);

	mainBacktrackButton = new QPushButton;
	mainBacktrackButton->setObjectName(QStringLiteral("canvasBacktrack"));
	mainBacktrackButton->setMinimumHeight(30);
	mainBacktrackButton->setIcon(create2StateIcon(":/aitum/media/backtrack_on.svg", ":/aitum/media/backtrack_off.svg"));
	mainBacktrackButton->setStyleSheet(
		"QPushButton:checked{background: rgb(26,87,255);} QPushButton{width: 32px; padding-left: 0px; padding-right: 0px; border-top-left-radius: 0; border-bottom-left-radius: 0;}");
	mainBacktrackButton->setCheckable(true);
	mainBacktrackButton->setChecked(false);
	QObject::connect(mainBacktrackButton, &QPushButton::clicked, [this] {
		if (obs_frontend_replay_buffer_active()) {
			obs_frontend_replay_buffer_save();
			mainBacktrackButton->setChecked(true);
		} else {
			mainBacktrackButton->setChecked(false);
		}
	});

	connect(mainBacktrackCheckboxButton, &QPushButton::clicked, [this] {
		bool enabled = mainBacktrackCheckboxButton->isChecked();
		if (enabled != mainBacktrackCheckbox->isChecked())
			mainBacktrackCheckbox->setChecked(enabled);
		if (enabled != mainBacktrackButton->isChecked())
			mainBacktrackButton->setChecked(enabled);
		if (enabled)
			obs_frontend_replay_buffer_start();
		else
			obs_frontend_replay_buffer_stop();
	});
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	connect(mainBacktrackCheckbox, &QCheckBox::checkStateChanged, [this] {
#else
	connect(mainBacktrackCheckbox, &QCheckBox::stateChanged, [this] {
#endif
		if (mainBacktrackCheckbox->isChecked() != mainBacktrackCheckboxButton->isChecked()) {
			mainBacktrackCheckboxButton->click();
		}
	});

	auto l3 = new QHBoxLayout;
	l3->setContentsMargins(0, 0, 0, 0);
	l3->setSpacing(0);
	l3->addWidget(mainBacktrackCheckboxButton);
	l3->addWidget(mainBacktrackButton);
	l2->addLayout(l3);

	mainBacktrackGroup = new QFrame;
	mainBacktrackGroup->setLayout(l2);
	mainLayout->addWidget(mainBacktrackGroup);

	l2 = new QHBoxLayout;
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("BuiltinVirtualCamera"))), 1);
	l2->addWidget(new QLabel(QString::fromUtf8(obs_module_text("MainCanvas"))), 1);
	mainVirtualCamButton = new QPushButton;
	mainVirtualCamButton->setObjectName(QStringLiteral("canvasVirtualCamera"));
	mainVirtualCamButton->setMinimumHeight(30);
	mainVirtualCamButton->setIcon(create2StateIcon(":/aitum/media/virtual_cam_on.svg", ":/aitum/media/virtual_cam_off.svg"));
	mainVirtualCamButton->setStyleSheet("QAbstractButton:checked{background: rgb(192,128,0);}");
	mainVirtualCamButton->setCheckable(true);
	mainVirtualCamButton->setChecked(false);

	connect(mainVirtualCamButton, &QPushButton::clicked, [this] {
		if (obs_frontend_virtualcam_active()) {
			obs_frontend_stop_virtualcam();
			mainVirtualCamButton->setChecked(false);
		} else {
			obs_frontend_start_virtualcam();
			mainVirtualCamButton->setChecked(true);
		}
	});

	l2->addWidget(mainVirtualCamButton);

	mainVirtualCamGroup = new QFrame;
	mainVirtualCamGroup->setLayout(l2);
	mainLayout->addWidget(mainVirtualCamGroup);

	connect(&videoCheckTimer, &QTimer::timeout, [this] {
		if (exiting)
			return;

		if (mainPlatformIconLabel) {
			auto service = obs_frontend_get_streaming_service();
			auto url = QString::fromUtf8(
				service ? obs_service_get_connect_info(service, OBS_SERVICE_CONNECT_INFO_SERVER_URL) : "");
			if (url != mainPlatformUrl) {
				mainPlatformUrl = url;
				mainPlatformIconLabel->setPixmap(
					getPlatformIconFromEndpoint(url).pixmap(outputPlatformIconSize, outputPlatformIconSize));
			}
		}

		if (mainStreamButton) {
			auto active = obs_frontend_streaming_active();
			if (mainStreamButton->isChecked() != active) {
				mainStreamButton->setChecked(active);
			}
		}

		if (mainRecordButton) {
			auto active = obs_frontend_recording_active();
			if (mainRecordButton->isChecked() != active) {
				mainRecordButton->setChecked(active);
			}
		}

		if (mainBacktrackCheckbox) {
			auto enabled = obs_frontend_replay_buffer_active();
			if (mainBacktrackCheckboxButton->isChecked() != enabled) {
				mainBacktrackCheckboxButton->setChecked(enabled);
			}
			if (mainBacktrackCheckbox->isChecked() != enabled) {
				mainBacktrackCheckbox->setChecked(enabled);
			}
			if (mainBacktrackButton->isChecked() != enabled) {
				mainBacktrackButton->setChecked(enabled);
			}
		}

		if (mainVirtualCamButton) {
			auto active = obs_frontend_virtualcam_active();
			if (mainVirtualCamButton->isChecked() != active) {
				mainVirtualCamButton->setChecked(active);
			}
		}

		for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
			(*it)->CheckActive();
		}
	});
	videoCheckTimer.start(500);
	obs_frontend_add_event_callback(frontend_event, this);

	StartStopHotkey = obs_hotkey_pair_register_frontend(
		"AitumStreamSuiteStartAllOutputs", obs_module_text("StartAllOutputs"), "AitumStreamSuiteStopAllOutputs",
		obs_module_text("StopAllOutputs"),
		[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed)
				return false;
			auto dock = static_cast<OutputDock *>(data);
			QMetaObject::invokeMethod(dock, [dock] { dock->StartAll(false, false); });
			return true;
		},
		[](void *data, obs_hotkey_pair_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed)
				return false;
			auto dock = static_cast<OutputDock *>(data);
			QMetaObject::invokeMethod(dock, [dock] { dock->StopAll(); });
			return true;
		},
		this, this);

	StartStreamHotkey = obs_hotkey_register_frontend(
		"AitumStreamSuiteStartAllStreams", obs_module_text("StartAllStreams"),
		[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed)
				return;
			auto dock = static_cast<OutputDock *>(data);
			QMetaObject::invokeMethod(dock, [dock] { dock->StartAll(true, false); });
		},
		this);

	StartRecordHotkey = obs_hotkey_register_frontend(
		"AitumStreamSuiteStartAllRecordings", obs_module_text("StartAllRecordings"),
		[](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
			UNUSED_PARAMETER(id);
			UNUSED_PARAMETER(hotkey);
			if (!pressed)
				return;
			auto dock = static_cast<OutputDock *>(data);
			QMetaObject::invokeMethod(dock, [dock] { dock->StartAll(false, true); });
		},
		this);
}

extern OutputDock *output_dock;
void RemoveWidget(QWidget *widget);

OutputDock::~OutputDock()
{
	if (StartStopHotkey != OBS_INVALID_HOTKEY_PAIR_ID)
		obs_hotkey_pair_unregister(StartStopHotkey);
	if (StartStreamHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(StartStreamHotkey);
	if (StartRecordHotkey != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(StartRecordHotkey);
	obs_frontend_add_event_callback(frontend_event, this);
	videoCheckTimer.stop();
	for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
		mainLayout->removeWidget(*it);
		RemoveWidget(*it);
	}
	outputWidgets.clear();
	output_dock = nullptr;
}

extern obs_data_t *current_profile_config;

void OutputDock::LoadSettings()
{
	auto enabled = obs_data_get_bool(current_profile_config, "main_stream_output_show");
	if (enabled != mainStreamGroup->isVisible()) {
		mainStreamGroup->setVisible(enabled);
	}
	enabled = obs_data_get_bool(current_profile_config, "main_record_output_show");
	if (enabled != mainRecordGroup->isVisible()) {
		mainRecordGroup->setVisible(enabled);
	}
	enabled = obs_data_get_bool(current_profile_config, "main_backtrack_output_show");
	if (enabled != mainBacktrackGroup->isVisible()) {
		mainBacktrackGroup->setVisible(enabled);
	}
	enabled = obs_data_get_bool(current_profile_config, "main_virtual_cam_output_show");
	if (enabled != mainVirtualCamGroup->isVisible()) {
		mainVirtualCamGroup->setVisible(enabled);
	}
	auto outputs2 = obs_data_get_array(current_profile_config, "outputs");
	for (auto it = outputWidgets.begin(); it != outputWidgets.end();) {
		bool found = false;
		auto count = obs_data_array_count(outputs2);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *data2 = obs_data_array_item(outputs2, i);
			auto name = QString::fromUtf8(obs_data_get_string(data2, "name"));
			obs_data_release(data2);
			if (name == (*it)->objectName()) {
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
			mainLayout->insertWidget((int)i + 4, outputWidget);
		}
		obs_data_release(data2);
	}

	obs_data_array_release(outputs2);

	QWidget *spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	mainLayout->addWidget(spacer);

	auto start_hotkey = obs_data_get_array(current_profile_config, "start_all_outputs_hotkey");
	auto stop_hotkey = obs_data_get_array(current_profile_config, "stop_all_outputs_hotkey");
	obs_hotkey_pair_load(StartStopHotkey, start_hotkey, stop_hotkey);
	obs_data_array_release(start_hotkey);
	obs_data_array_release(stop_hotkey);

	auto stream_hotkey = obs_data_get_array(current_profile_config, "start_all_streams_hotkey");
	obs_hotkey_load(StartStreamHotkey, stream_hotkey);
	obs_data_array_release(stream_hotkey);

	auto record_hotkey = obs_data_get_array(current_profile_config, "start_all_recordings_hotkey");
	obs_hotkey_load(StartRecordHotkey, record_hotkey);
	obs_data_array_release(record_hotkey);
}

void OutputDock::SaveSettings()
{
	for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
		(*it)->SaveSettings();
	}

	if (StartStopHotkey != OBS_INVALID_HOTKEY_PAIR_ID) {
		obs_data_array_t *start_hotkey = nullptr;
		obs_data_array_t *stop_hotkey = nullptr;
		obs_hotkey_pair_save(StartStopHotkey, &start_hotkey, &stop_hotkey);
		obs_data_set_array(current_profile_config, "start_all_outputs_hotkey", start_hotkey);
		obs_data_set_array(current_profile_config, "stop_all_outputs_hotkey", stop_hotkey);
		obs_data_array_release(start_hotkey);
		obs_data_array_release(stop_hotkey);
	}
	if (StartStreamHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *stream_hotkey = obs_hotkey_save(StartStreamHotkey);
		obs_data_set_array(current_profile_config, "start_all_streams_hotkey", stream_hotkey);
		obs_data_array_release(stream_hotkey);
	}
	if (StartRecordHotkey != OBS_INVALID_HOTKEY_ID) {
		obs_data_array_t *record_hotkey = obs_hotkey_save(StartRecordHotkey);
		obs_data_set_array(current_profile_config, "start_all_recordings_hotkey", record_hotkey);
		obs_data_array_release(record_hotkey);
	}
}

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

void OutputDock::UpdateMainRecordingStatus(bool active)
{
	mainRecordButton->setChecked(active);
}

void OutputDock::UpdateMainBacktrackStatus(bool active)
{
	mainBacktrackCheckboxButton->setChecked(active);
	mainBacktrackCheckbox->setChecked(active);
	mainBacktrackButton->setChecked(active);
}

void OutputDock::UpdateMainVirtualCameraStatus(bool active)
{
	mainVirtualCamButton->setChecked(active);
}

obs_data_array_t *OutputDock::GetOutputsArray()
{
	auto outputs2 = obs_data_array_create();
	for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
		auto data = obs_data_create();
		obs_data_set_string(data, "name", (*it)->objectName().toUtf8().constData());
		obs_data_set_bool(data, "active", obs_output_active((*it)->GetOutput()));
		obs_data_array_push_back(outputs2, data);
		obs_data_release(data);
	}
	return outputs2;
}

bool OutputDock::AddChapterToOutput(const char *output_name, const char *chapter_name)
{
	auto on = QString::fromUtf8(output_name);
	for (auto it = outputWidgets.begin(); it != outputWidgets.end(); it++) {
		if ((*it)->objectName() == on) {
			return (*it)->AddChapter(chapter_name);
		}
	}
	return false;
}

void OutputDock::StartNextOutput()
{
	if (outputsToStart.empty())
		return;
	if (outputStarting >= outputsToStart.size())
		outputStarting = 0;
	auto startedAt = outputStarting;
	while (true) {
		auto action = std::next(outputsToStart.begin(), outputStarting);
		if ((*action)([this] { QMetaObject::invokeMethod(this, [this] { StartNextOutput(); }, Qt::QueuedConnection); })) {
			outputsToStart.erase(action);
			break;
		}
		outputStarting++;
		if (outputStarting >= outputsToStart.size()) {
			outputStarting = 0;
		}
		if (outputStarting == startedAt) {
			blog(LOG_WARNING, "[Aitum Stream Suite] went through all outputs and %zu could not be started",
			     outputsToStart.size());
			outputsToStart.clear();
			break;
		}
	}
}

void OutputDock::frontend_event(enum obs_frontend_event event, void *private_data)
{
	auto dock = static_cast<OutputDock *>(private_data);
	if (!dock)
		return;
	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		if (dock->mainStreamOnStarted) {
			dock->mainStreamOnStarted();
			dock->mainStreamOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		if (dock->mainRecordOnStarted) {
			dock->mainRecordOnStarted();
			dock->mainRecordOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		if (dock->mainBacktrackOnStarted) {
			dock->mainBacktrackOnStarted();
			dock->mainBacktrackOnStarted = nullptr;
		}
		break;
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED:
	case OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED:
		if (dock->mainVirtualCamOnStarted) {
			dock->mainVirtualCamOnStarted();
			dock->mainVirtualCamOnStarted = nullptr;
		}
		break;
	default:
		break;
	}
}

void OutputDock::StartAll(bool streamOnly, bool recordOnly)
{
	outputsToStart.clear();
	if (mainStreamButton && !recordOnly) {
		bool warnBeforeStreamStart =
			config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStartingStream");
		if (warnBeforeStreamStart && isVisible()) {
			auto button = QMessageBox::question(this,
							    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Title")),
							    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStart.Text")),
							    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
			if (button == QMessageBox::No)
				return;
		}

		outputsToStart.push_back([this](std::function<void()> onStarted) {
			if (obs_frontend_streaming_active()) {
				onStarted();
				return true;
			}
			this->mainStreamOnStarted = onStarted;
			obs_frontend_streaming_start();
			auto output = obs_frontend_get_streaming_output();
			if (!output) {
				this->mainStreamOnStarted = nullptr;
				onStarted();
			}
			obs_output_release(output);
			return true;
		});
	}
	if (mainRecordButton && !streamOnly) {
		outputsToStart.push_back([this](std::function<void()> onStarted) {
			if (obs_frontend_recording_active()) {
				onStarted();
				return true;
			}
			this->mainRecordOnStarted = onStarted;
			obs_frontend_recording_start();
			auto output = obs_frontend_get_recording_output();
			if (!output) {
				this->mainRecordOnStarted = nullptr;
				onStarted();
			}
			obs_output_release(output);
			return true;
		});
	}
	if (mainBacktrackCheckboxButton && !streamOnly) {
		outputsToStart.push_back([this](std::function<void()> onStarted) {
			if (obs_frontend_replay_buffer_active()) {
				onStarted();
				return true;
			}
			this->mainBacktrackOnStarted = onStarted;
			obs_frontend_replay_buffer_start();
			auto output = obs_frontend_get_replay_buffer_output();
			if (!output) {
				this->mainBacktrackOnStarted = nullptr;
				onStarted();
			}
			obs_output_release(output);
			return true;
		});
	}
	if (mainVirtualCamButton && !streamOnly && !recordOnly) {
		outputsToStart.push_back([this](std::function<void()> onStarted) {
			if (obs_frontend_virtualcam_active()) {
				onStarted();
				return true;
			}
			this->mainVirtualCamOnStarted = onStarted;
			obs_frontend_start_virtualcam();
			auto output = obs_frontend_get_virtualcam_output();
			if (!output) {
				this->mainVirtualCamOnStarted = nullptr;
				onStarted();
			}
			obs_output_release(output);
			return true;
		});
	}
	for (auto &ow : outputWidgets) {
		if (streamOnly && !ow->IsStream())
			continue;
		if (recordOnly && !ow->IsRecord())
			continue;
		outputsToStart.push_back([this, ow](std::function<void()> onStarted) { return ow->StartOutput(onStarted); });
	}
	if (outputsToStart.empty())
		return;
	blog(LOG_INFO, "[Aitum Stream Suite] Starting %zu outputs", outputsToStart.size());
	StartNextOutput();
}

void OutputDock::StopAll()
{
	outputsToStart.clear();
	bool warnStream = config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStoppingStream");
	bool warnRecord = config_get_bool(obs_frontend_get_user_config(), "BasicWindow", "WarnBeforeStoppingRecord");
	if (warnStream && mainStreamButton && obs_frontend_streaming_active() && isVisible()) {
		auto button = QMessageBox::question(this, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Title")),
						    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStop.Text")),
						    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (button == QMessageBox::No)
			return;
	} else if (warnRecord && mainRecordButton && obs_frontend_recording_active() && isVisible()) {
		auto button = QMessageBox::question(this,
						    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStopRecord.Title")),
						    QString::fromUtf8(obs_frontend_get_locale_string("ConfirmStopRecord.Text")),
						    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (button == QMessageBox::No)
			return;
	}

	if (mainStreamButton && obs_frontend_streaming_active())
		obs_frontend_streaming_stop();
	if (mainRecordButton && obs_frontend_recording_active())
		obs_frontend_recording_stop();
	if (mainBacktrackCheckboxButton && obs_frontend_replay_buffer_active())
		obs_frontend_replay_buffer_stop();
	if (mainVirtualCamButton && obs_frontend_virtualcam_active())
		obs_frontend_stop_virtualcam();
	for (auto &ow : outputWidgets) {
		ow->StopOutput();
	}
}
