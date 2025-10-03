#include "dialogs/config-dialog.hpp"
#include "docks/browser-dock.hpp"
#include "docks/canvas-clone-dock.hpp"
#include "docks/canvas-dock.hpp"
#include "docks/output-dock.hpp"
#include "docks/properties-dock.hpp"
#include "utils/file-download.h"
#include "version.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QApplication>
#include <QDesktopServices>
#include <QDockWidget>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QToolBar>
#include <util/dstr.h>
#include "dialogs/scene-collection-wizard.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Aitum");
OBS_MODULE_USE_DEFAULT_LOCALE("aitum-stream-suite", "en-US")

download_info_t *version_download_info = nullptr;
obs_data_t *current_profile_config = nullptr;
QTabBar *modesTabBar = nullptr;
int modesTab = -1;

QAction *streamAction = nullptr;
QAction *recordAction = nullptr;
QAction *studioModeAction = nullptr;
QAction *virtualCameraAction = nullptr;

OBSBasicSettings *configDialog = nullptr;
OutputDock *output_dock = nullptr;

QIcon create2StateIcon(QString fileOn, QString fileOff)
{
	QIcon icon = QIcon(fileOff);
	icon.addFile(fileOn, QSize(), QIcon::Normal, QIcon::On);
	return icon;
}

extern QWidget *aitumSettingsWidget;

bool version_info_downloaded(void *param, struct file_download_data *file)
{
	UNUSED_PARAMETER(param);
	if (!file || !file->buffer.num)
		return true;

	auto d = obs_data_create_from_json((const char *)file->buffer.array);
	if (d) {
		auto data_obj = obs_data_get_obj(d, "data");
		obs_data_release(d);
		if (data_obj) {

			auto version = obs_data_get_string(data_obj, "version");
			int major;
			int minor;
			int patch;
			if (sscanf(version, "%d.%d.%d", &major, &minor, &patch) == 3) {
				auto sv = MAKE_SEMANTIC_VERSION(major, minor, patch);
				if (sv >
				    MAKE_SEMANTIC_VERSION(PROJECT_VERSION_MAJOR, PROJECT_VERSION_MINOR, PROJECT_VERSION_PATCH)) {
					QMetaObject::invokeMethod(aitumSettingsWidget, [] {
						aitumSettingsWidget->setStyleSheet(
							QString::fromUtf8("background: rgb(192,128,0);"));
					});
				}
			}
			obs_data_release(data_obj);
		}

	}


	QMetaObject::invokeMethod(output_dock, "ApiInfo", Q_ARG(QString, QString::fromUtf8((const char *)file->buffer.array)));
	

	if (version_download_info) {
		download_info_destroy(version_download_info);
		version_download_info = nullptr;
	}
	return true;
}

void save_dock_state(int index)
{
	if (index < 0)
		return;
	if (!current_profile_config)
		return;
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto state = main_window->saveState();
	auto b64 = state.toBase64();
	auto state_chars = b64.constData();
	if (index == 0) {
		obs_data_set_string(current_profile_config, "dock_state_live", state_chars);
	} else if (index == 1) {
		obs_data_set_string(current_profile_config, "dock_state_build", state_chars);
	} else if (index == 2) {
		obs_data_set_string(current_profile_config, "dock_state_design", state_chars);
	}
}

void reset_live_dock_state()
{
	//Shows activity feeds, chat (multi-chat), Game capture change dock, main scenes quick switch dock, canvas previews, multi-stream dock. Hides scene list or sources or anything related to actually making a stream setup and not actually being live
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QMetaObject::invokeMethod(main_window, "on_resetDocks_triggered", Qt::BlockingQueuedConnection, Q_ARG(bool, true));
	auto d = main_window->findChild<QDockWidget *>(QStringLiteral("controlsDock"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("sourcesDock"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteChat"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteActivity"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteInfo"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteOutput"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
	}
}

void reset_build_dock_state()
{
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QMetaObject::invokeMethod(main_window, "on_resetDocks_triggered", Qt::BlockingQueuedConnection, Q_ARG(bool, true));
	auto d = main_window->findChild<QDockWidget *>(QStringLiteral("controlsDock"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("sourcesDock"));
	if (d)
		d->setVisible(true);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteChat"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteActivity"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteInfo"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteOutput"));
	if (d)
		d->setVisible(false);
}

void reset_design_dock_state()
{
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QMetaObject::invokeMethod(main_window, "on_resetDocks_triggered", Qt::BlockingQueuedConnection, Q_ARG(bool, true));
	auto d = main_window->findChild<QDockWidget *>(QStringLiteral("controlsDock"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("sourcesDock"));
	if (d)
		d->setVisible(true);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteChat"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteActivity"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteInfo"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteOutput"));
	if (d)
		d->setVisible(false);
}

void load_dock_state(int index)
{
	if (!current_profile_config)
		return;
	const char *state = nullptr;
	if (index == 0) {
		state = obs_data_get_string(current_profile_config, "dock_state_live");
		if (!state || state[0] == '\0') {
			reset_live_dock_state();
			return;
		}
	} else if (index == 1) {
		state = obs_data_get_string(current_profile_config, "dock_state_build");
		if (!state || state[0] == '\0') {
			reset_build_dock_state();
			return;
		}
	} else if (index == 2) {
		state = obs_data_get_string(current_profile_config, "dock_state_design");
		if (!state || state[0] == '\0') {
			reset_design_dock_state();
			return;
		}
	}
	if (state && strlen(state) > 0) {
		auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		main_window->restoreState(QByteArray::fromBase64(state));
	}
}

void load_current_profile_config()
{
	obs_data_release(current_profile_config);
	current_profile_config = nullptr;

	char *profile_path = obs_frontend_get_current_profile_path();
	if (!profile_path)
		return;

	struct dstr path;
	dstr_init_copy(&path, profile_path);
	bfree(profile_path);

	if (!dstr_is_empty(&path) && dstr_end(&path) != '/')
		dstr_cat_ch(&path, '/');
	dstr_cat(&path, "aitum.json");

	current_profile_config = obs_data_create_from_json_file_safe(path.array, "bak");
	dstr_free(&path);
	if (!current_profile_config) {
		current_profile_config = obs_data_create();
		blog(LOG_WARNING, "[Aitum Stream Suite] No configuration file loaded");
	} else {
		blog(LOG_INFO, "[Aitum Stream Suite] Loaded configuration file");
	}

	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto canvas = obs_data_get_array(current_profile_config, "canvas");
	if (!canvas) {
		canvas = obs_data_array_create();
		auto new_canvas = obs_data_create();
		obs_data_set_string(new_canvas, "name", "Vertical");
		obs_data_array_push_back(canvas, new_canvas);
		obs_data_release(new_canvas);
		obs_data_set_array(current_profile_config, "canvas", canvas);
	}
	auto canvas_count = obs_data_array_count(canvas);
	for (size_t i = 0; i < canvas_count;) {
		obs_data_t *t = obs_data_array_item(canvas, i);
		if (!t) {
			i++;
			continue;
		}
		if (obs_data_get_bool(t, "delete")) {
			const char *canvas_name = obs_data_get_string(t, "name");
			obs_frontend_remove_dock(canvas_name);

			obs_canvas_t *c = obs_get_canvas_by_uuid(obs_data_get_string(t, "uuid"));
			if (!c)
				c = obs_get_canvas_by_name(canvas_name);
			if (c) {
				obs_frontend_remove_canvas(c);
				obs_canvas_remove(c);
				obs_canvas_release(c);
			}
			obs_data_array_erase(canvas, i);
			obs_data_release(t);
			canvas_count--;
		} else if (strcmp(obs_data_get_string(t, "type"), "clone") == 0) {
			obs_frontend_add_dock_by_id(obs_data_get_string(t, "name"), obs_data_get_string(t, "name"),
						    new CanvasCloneDock(t, main_window));
			i++;
		} else {
			obs_frontend_add_dock_by_id(obs_data_get_string(t, "name"), obs_data_get_string(t, "name"),
						    new CanvasDock(t, main_window));
			i++;
		}
	}
	obs_data_array_release(canvas);

	auto index = obs_data_get_int(current_profile_config, "dock_state_mode");
	if (modesTabBar->currentIndex() == index) {
		load_dock_state(index);
	} else {
		modesTab = -1;
		modesTabBar->setCurrentIndex(index);
	}

	QMetaObject::invokeMethod(
		output_dock,
		[] {
			if (output_dock)
				output_dock->LoadSettings();
		},
		Qt::QueuedConnection);
}

void save_current_profile_config()
{
	if (!current_profile_config)
		return;
	char *profile_path = obs_frontend_get_current_profile_path();
	if (!profile_path)
		return;

	struct dstr path;
	dstr_init_copy(&path, profile_path);
	bfree(profile_path);

	if (!dstr_is_empty(&path) && dstr_end(&path) != '/')
		dstr_cat_ch(&path, '/');
	dstr_cat(&path, "aitum.json");

	auto index = modesTabBar->currentIndex();
	save_dock_state(index);
	obs_data_set_int(current_profile_config, "dock_state_mode", index);

	if (!obs_data_save_json_safe(current_profile_config, path.array, "tmp", "bak")) {
		blog(LOG_WARNING, "[Aitum Stream Suite] Failed to save configuration file");
	} else {
		blog(LOG_INFO, "[Aitum Stream Suite] Saved configuration file");
	}

	dstr_free(&path);
}

static bool restart = false;

static void frontend_event(enum obs_frontend_event event, void *private_data)
{
	UNUSED_PARAMETER(private_data);
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		if (restart) {
			const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			QMetaObject::invokeMethod(main_window, [main_window] { main_window->close(); }, Qt::QueuedConnection);
			return;
		}
		size_t scene_count = 0;
		obs_enum_scenes(
			[](void *param, obs_source_t *) {
				size_t *scene_count = (size_t *)param;
				(*scene_count)++;
				return true;
			},
			&scene_count);
		if (scene_count <= 1) {
			obs_source_t *ss = obs_frontend_get_current_scene();
			obs_scene_t *scene = obs_scene_from_source(ss);
			bool has_items = false;
			obs_scene_enum_items(
				scene,
				[](obs_scene_t *, obs_sceneitem_t *, void *param) {
					bool *has_items = (bool *)param;
					*has_items = true;
					return false;
				},
				&has_items);
			if (!has_items) {
				SceneCollectionWizard wizard;
				wizard.exec();
			}
			obs_source_release(ss);
		}
		load_current_profile_config();
	} else if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
		load_current_profile_config();
	} else if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGING) {
		save_current_profile_config();
	} else if (event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		if (current_profile_config) {
			save_current_profile_config();
			obs_data_release(current_profile_config);
			current_profile_config = nullptr;
		}
		if (output_dock)
			output_dock->Exiting();
	} else if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) {
		if (!studioModeAction->isChecked()) {
			studioModeAction->setChecked(true);
		}
	} else if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED) {
		if (studioModeAction->isChecked()) {
			studioModeAction->setChecked(false);
		}
	} else if (event == OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED) {
		if (!virtualCameraAction->isChecked()) {
			virtualCameraAction->setChecked(true);
		}
	} else if (event == OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED) {
		if (virtualCameraAction->isChecked()) {
			virtualCameraAction->setChecked(false);
		}
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STARTING || event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		if (!streamAction->isChecked()) {
			streamAction->setChecked(true);
		}
		output_dock->UpdateMainStreamStatus(true);
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPING || event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		if (streamAction->isChecked()) {
			streamAction->setChecked(false);
		}
		output_dock->UpdateMainStreamStatus(false);
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTING || event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
		if (!recordAction->isChecked()) {
			recordAction->setChecked(true);
		}
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPING || event == OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
		if (recordAction->isChecked()) {
			recordAction->setChecked(false);
		}
	}

	//OBS_FRONTEND_EVENT_PROFILE_RENAMED
}

class TabToolBar : public QToolBar {
private:
	QTabBar *tabs;

	void checkOrientation() const;

public:
	TabToolBar(QTabBar *tabs_);
	//QSize minimumSizeHint() const override;

protected:
	virtual void resizeEvent(QResizeEvent *event) override;
};

QIcon generateEmojiQIcon(QString emoji)
{
	QPixmap pixmap(32, 32);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	QFont font = painter.font();
	font.setPixelSize(32);
	painter.setFont(font);
	painter.drawText(pixmap.rect(), Qt::AlignCenter, emoji);

	return QIcon(pixmap);
}

QWidget *aitumSettingsWidget = nullptr;

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Aitum Stream Suite] loaded version %s", PROJECT_VERSION);

	QFontDatabase::addApplicationFont(":/aitum/media/Roboto.ttf");
	QFontDatabase::addApplicationFont(":/aitum/media/Roboto-Italic.ttf");
	QFontDatabase::addApplicationFont(":/aitum/media/RobotoCondensed.ttf");
	QFontDatabase::addApplicationFont(":/aitum/media/RobotoCondensed-Italic.ttf");

	obs_frontend_add_tools_menu_item(
		obs_module_text("SceneCollectionWizard"),
		[](void *) {
			SceneCollectionWizard wizard;
			wizard.exec();
		},
		nullptr);

	obs_frontend_add_event_callback(frontend_event, nullptr);

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto user_config = obs_frontend_get_user_config();
	if (user_config) {
		if (!config_get_bool(user_config, "Aitum", "ThemeSet")) {
			config_set_string(user_config, "Appearance", "Theme", "com.obsproject.Aitum.Original");
			config_set_bool(user_config, "Aitum", "ThemeSet", true);
			config_save_safe(user_config, "tmp", "bak");
			restart = true;
		}
		const char *theme = config_get_string(user_config, "Appearance", "Theme");
		if (theme && strcmp(theme, "com.obsproject.Aitum.Original") == 0) {
			main_window->setContentsMargins(10, 10, 10, 10);
			main_window->findChild<QWidget *>("centralwidget")->setContentsMargins(0, 0, 0, 0);
		}
	}

	modesTabBar = new QTabBar();
	auto tb = new TabToolBar(modesTabBar);
	main_window->addToolBar(tb);
	tb->setFloatable(false);
	//tb->setMovable(false);
	//tb->setAllowedAreas(Qt::ToolBarArea::TopToolBarArea);

	modesTabBar->addTab(QString::fromUtf8(obs_module_text("Live")));
	modesTabBar->addTab(QString::fromUtf8(obs_module_text("Build")));
	modesTabBar->addTab(QString::fromUtf8(obs_module_text("Design")));
	tb->addWidget(modesTabBar);
	tb->addSeparator();

	QObject::connect(modesTabBar, &QTabBar::currentChanged, [](int index) {
		save_dock_state(modesTab);
		modesTab = index;
		load_dock_state(index);
	});

	auto aitumSettingsAction = tb->addAction(QString::fromUtf8(obs_module_text("Settings")));
	aitumSettingsAction->setProperty("themeID", "configIconSmall");
	aitumSettingsAction->setProperty("class", "icon-gear");
	aitumSettingsWidget = tb->widgetForAction(aitumSettingsAction);
	aitumSettingsWidget->setProperty("themeID", "configIconSmall");
	aitumSettingsWidget->setProperty("class", "icon-gear");
	aitumSettingsWidget->setObjectName("AitumStreamSuiteSettingsButton");
	QObject::connect(aitumSettingsAction, &QAction::triggered, [] {
		if (!configDialog)
			configDialog = new OBSBasicSettings((QMainWindow *)obs_frontend_get_main_window());
		auto settings = obs_data_create();
		if (current_profile_config)
			obs_data_apply(settings, current_profile_config);

		configDialog->LoadSettings(settings);

		if (configDialog->exec() == QDialog::Accepted) {
			if (current_profile_config) {
				obs_data_apply(current_profile_config, settings);
				obs_data_release(settings);
			} else {
				current_profile_config = settings;
			}
			save_current_profile_config();
			load_current_profile_config();
		} else {
			obs_data_release(settings);
		}
	});

	// Contribute Button
	auto contributeButton = tb->addAction(generateEmojiQIcon("❤️"), QString::fromUtf8(obs_module_text("Donate")));
	contributeButton->setProperty("themeID", "icon-aitum-donate");
	contributeButton->setProperty("class", "icon-aitum-donate");
	tb->widgetForAction(contributeButton)->setProperty("themeID", "icon-aitum-donate");
	tb->widgetForAction(contributeButton)->setProperty("class", "icon-aitum-donate");
	contributeButton->setToolTip(QString::fromUtf8(obs_module_text("AitumStreamSuiteDonate")));
	QAction::connect(contributeButton, &QAction::triggered,
			 [] { QDesktopServices::openUrl(QUrl("https://aitum.tv/contribute")); });

	// Aitum Button
	auto aitumButton = tb->addAction(QIcon(":/aitum/media/aitum.png"), QString::fromUtf8(obs_module_text("Aitum")));
	aitumButton->setProperty("themeID", "icon-aitum");
	aitumButton->setProperty("class", "icon-aitum");
	tb->widgetForAction(aitumButton)->setProperty("themeID", "icon-aitum");
	tb->widgetForAction(aitumButton)->setProperty("class", "icon-aitum");
	aitumButton->setToolTip(QString::fromUtf8("https://aitum.tv"));
	QAction::connect(aitumButton, &QAction::triggered, [] { QDesktopServices::openUrl(QUrl("https://aitum.tv")); });

	//tb->addAction(QString::fromUtf8(obs_module_text("Reset")));
	//tb->layout()->addItem(new QSpacerItem(0, 0));
	QWidget *spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	tb->addWidget(spacer);

	//auto controlsToolBar = main_window->addToolBar(QString::fromUtf8(obs_module_text("Controls")));
	auto controlsToolBar = tb;

	streamAction = controlsToolBar->addAction(QString::fromUtf8(obs_module_text("Stream")));
	streamAction->setCheckable(true);
	streamAction->setIcon(create2StateIcon(":/aitum/media/streaming.svg", ":/aitum/media/stream.svg"));
	controlsToolBar->widgetForAction(streamAction)->setStyleSheet("QAbstractButton:checked{background: rgb(0,210,153);}");
	QObject::connect(streamAction, SIGNAL(triggered()), main_window, SLOT(StreamActionTriggered()));
	//QMenu *m = new QMenu();
	//m->addAction(QString::fromUtf8("test"));
	//streamButton->setMenu(m);

	recordAction = controlsToolBar->addAction(QString::fromUtf8(obs_module_text("Record")));
	recordAction->setCheckable(true);
	recordAction->setIcon(create2StateIcon(":/aitum/media/recording.svg", ":/aitum/media/record.svg"));
	controlsToolBar->widgetForAction(recordAction)->setStyleSheet("QAbstractButton:checked{background: rgb(255,0,0);}");
	QObject::connect(recordAction, SIGNAL(triggered()), main_window, SLOT(RecordActionTriggered()));
	//recordButton->setMenu(new QMenu());

	auto replayButton = controlsToolBar->addAction(QString::fromUtf8(obs_module_text("Backtrack")));
	replayButton->setCheckable(true);
	replayButton->setIcon(create2StateIcon(":/aitum/media/backtrack_on.svg", ":/aitum/media/backtrack_off.svg"));
	controlsToolBar->widgetForAction(replayButton)->setStyleSheet("QAbstractButton:checked{background: rgb(26,87,255);}");
	QObject::connect(replayButton, SIGNAL(triggered()), main_window, SLOT(ReplayBufferSave()));

	virtualCameraAction = controlsToolBar->addAction(QString::fromUtf8(obs_module_text("VirtualCamera")));
	virtualCameraAction->setCheckable(true);
	virtualCameraAction->setIcon(create2StateIcon(":/aitum/media/virtual_cam_on.svg", ":/aitum/media/virtual_cam_off.svg"));
	controlsToolBar->widgetForAction(virtualCameraAction)->setStyleSheet("QAbstractButton:checked{background: rgb(192,128,0);}");
	QObject::connect(virtualCameraAction, SIGNAL(triggered()), main_window, SLOT(VirtualCamActionTriggered()));

	studioModeAction = controlsToolBar->addAction(QString::fromUtf8(obs_module_text("StudioMode")));
	studioModeAction->setCheckable(true);
	studioModeAction->setIcon(create2StateIcon(":/aitum/media/studio_mode_on.svg", ":/aitum/media/studio_mode_off.svg"));
	controlsToolBar->widgetForAction(studioModeAction)->setStyleSheet("QAbstractButton:checked{background: rgb(158,0,89);}");
	QObject::connect(studioModeAction, SIGNAL(triggered()), main_window, SLOT(TogglePreviewProgramMode()));

	//https://coolors.co/00d299-ff0000-1a57ff-c08000-9e0059

	auto settingsButton = controlsToolBar->addAction(QString::fromUtf8(obs_module_text("Settings")));
	settingsButton->setProperty("themeID", "configIconSmall");
	settingsButton->setProperty("class", "icon-gear");
	controlsToolBar->widgetForAction(settingsButton)->setProperty("themeID", "configIconSmall");
	controlsToolBar->widgetForAction(settingsButton)->setProperty("class", "icon-gear");
	QObject::connect(settingsButton, SIGNAL(triggered()), main_window, SLOT(on_action_Settings_triggered()));

	//auto action = controlsToolBar->addAction("");
	//action->setCheckable(true);
	//action->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::InsertLink));
	//action->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::CameraWeb));
	//action->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditUndo));

	output_dock = new OutputDock(main_window);
	obs_frontend_add_dock_by_id("AitumStreamSuiteOutput", obs_module_text("AitumStreamSuiteOutput"), output_dock);
	obs_frontend_add_dock_by_id("AitumStreamSuiteProperties", obs_module_text("AitumStreamSuiteProperties"),
				    new PropertiesDock(main_window));

	version_download_info = download_info_create_single(
		"[Aitum Stream Suite]", "OBS", "https://api.aitum.tv/plugin/streamsuite", version_info_downloaded, nullptr);
	return true;
}

void TabToolBar::checkOrientation() const
{
	const auto main_window = static_cast<QMainWindow *>(parent());
	auto area = main_window->toolBarArea(this);
	if (orientation() == Qt::Orientation::Vertical) {
		if (area == Qt::RightToolBarArea) {
			if (tabs->shape() != QTabBar::Shape::RoundedEast)
				tabs->setShape(QTabBar::Shape::RoundedEast);
		} else {
			if (tabs->shape() != QTabBar::Shape::RoundedWest)
				tabs->setShape(QTabBar::Shape::RoundedWest);
		}
	} else {
		if (area == Qt::BottomToolBarArea) {
			if (tabs->shape() != QTabBar::Shape::RoundedSouth)
				tabs->setShape(QTabBar::Shape::RoundedSouth);
		} else {
			if (tabs->shape() != QTabBar::Shape::RoundedNorth)
				tabs->setShape(QTabBar::Shape::RoundedNorth);
		}
	}
}

void TabToolBar::resizeEvent(QResizeEvent *event)
{
	checkOrientation();
	QToolBar::resizeEvent(event);
}

TabToolBar::TabToolBar(QTabBar *tabs_) : QToolBar(), tabs(tabs_)
{
	connect(this, &QToolBar::orientationChanged, [&] { checkOrientation(); });
}

/*
QSize TabToolBar::minimumSizeHint() const{
	checkOrientation();
	auto size = QToolBar::minimumSizeHint();
	if (orientation() == Qt::Orientation::Vertical) {
		auto t = tabs->minimumSizeHint();
		int i = 0;
	}
	//auto size2 = QToolBar::minimumSizeHint();
	return size;
}*/

void obs_module_post_load()
{
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_add_dock_by_id("AitumStreamSuiteChat", obs_module_text("AitumStreamSuiteChat"),
				    new BrowserDock(obs_module_text("AitumStreamSuiteChat"), "https://chat.aitumsuite.tv/chat",
						    main_window));
	obs_frontend_add_dock_by_id("AitumStreamSuiteActivity", obs_module_text("AitumStreamSuiteActivity"),
				    new BrowserDock(obs_module_text("AitumStreamSuiteActivity"),
						    "https://chat.aitumsuite.tv/activity", main_window));
	obs_frontend_add_dock_by_id("AitumStreamSuiteInfo", obs_module_text("AitumStreamSuiteInfo"),
				    new BrowserDock(obs_module_text("AitumStreamSuiteInfo"), "https://chat.aitumsuite.tv/info",
						    main_window));
}

void obs_module_unload()
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	if (version_download_info) {
		download_info_destroy(version_download_info);
		version_download_info = nullptr;
	}
	if (current_profile_config) {
		obs_data_release(current_profile_config);
		current_profile_config = nullptr;
	}
	if (output_dock) {
		delete output_dock;
	}
}

const char *obs_module_name(void)
{
	return obs_module_text("AitumStreamSuite");
}
