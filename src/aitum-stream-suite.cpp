#include "dialogs/config-dialog.hpp"
#include "dialogs/name-dialog.hpp"
#include "docks/browser-dock.hpp"
#include "docks/canvas-clone-dock.hpp"
#include "docks/canvas-dock.hpp"
#include "docks/capture-dock.hpp"
#include "docks/filters-dock.hpp"
#include "docks/live-scenes-dock.hpp"
#include "docks/output-dock.hpp"
#include "docks/properties-dock.hpp"
#include "utils/file-download.h"
#include "utils/icon.hpp"
#include "utils/obs-websocket-api.h"
#include "utils/widgets/pixmap-label.hpp"
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

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Aitum");
OBS_MODULE_USE_DEFAULT_LOCALE("aitum-stream-suite", "en-US")

download_info_t *version_download_info = nullptr;
obs_data_t *current_profile_config = nullptr;
QTabBar *modesTabBar = nullptr;
QToolBar *toolbar = nullptr;
QString modesTab;

QList<QAction *> partnerBlockActions;
QAction *studioModeAction = nullptr;

OBSBasicSettings *configDialog = nullptr;
OutputDock *output_dock = nullptr;
PropertiesDock *properties_dock = nullptr;
FiltersDock *filters_dock = nullptr;
LiveScenesDock *live_scenes_dock = nullptr;
CanvasDock *component_dock = nullptr;

QString newer_version_available;

QTimer load_dock_state_timer;
QList<QString> loaded_docks;

extern std::list<CanvasDock *> canvas_docks;
extern std::list<CanvasCloneDock *> canvas_clone_docks;
extern obs_websocket_vendor vendor;

std::list<QFrame *> empty_docks;

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
					newer_version_available = QString::fromUtf8(version);
					QMetaObject::invokeMethod(aitumSettingsWidget, [] {
						aitumSettingsWidget->setStyleSheet(
							QString::fromUtf8("background: rgb(192,128,0);"));
					});
				}
			}

			obs_data_array_t *blocks = obs_data_get_array(data_obj, "partnerBlocks");
			if (obs_data_array_count(blocks) > 0) {
				time_t current_time = time(nullptr);
				auto partnerBlockTime =
					(time_t)config_get_int(obs_frontend_get_user_config(), "Aitum", "partner_block");
				if (current_time < partnerBlockTime || current_time - partnerBlockTime > 1209600) {
					QMetaObject::invokeMethod(toolbar, [blocks] {
						auto before = studioModeAction;
						size_t count = obs_data_array_count(blocks);
						for (size_t i = 0; i < count; i++) {
							obs_data_t *block = obs_data_array_item(blocks, i);
							auto block_type = obs_data_get_string(block, "type");
							if (strcmp(block_type, "LINK") == 0) {
								auto button = new QPushButton(
									QString::fromUtf8(obs_data_get_string(block, "label")));
								button->setStyleSheet(
									QString::fromUtf8(obs_data_get_string(block, "qss")));
								auto url = QString::fromUtf8(obs_data_get_string(block, "data"));
								button->connect(button, &QPushButton::clicked,
										[url] { QDesktopServices::openUrl(QUrl(url)); });
								partnerBlockActions.append(toolbar->insertWidget(before, button));
							} else if (strcmp(block_type, "IMAGE") == 0) {
								auto image_data =
									QString::fromUtf8(obs_data_get_string(block, "data"));
								if (image_data.startsWith("data:image/")) {
									auto pos = image_data.indexOf(";");
									auto format = image_data.mid(11, pos - 11);
									QImage image;
									if (image.loadFromData(
										    QByteArray::fromBase64(image_data.mid(pos + 7)
														   .toUtf8()
														   .constData()),
										    format.toUtf8().constData())) {
										auto label = new AspectRatioPixmapLabel;
										label->setPixmap(QPixmap::fromImage(image));
										label->setAlignment(Qt::AlignCenter);
										label->setStyleSheet(QString::fromUtf8(
											obs_data_get_string(block, "qss")));
										partnerBlockActions.append(
											toolbar->insertWidget(before, label));
									}
								}
							} else if (strcmp(block_type, "LABEL") == 0) {
								auto label = new QLabel(
									QString::fromUtf8(obs_data_get_string(block, "label")));
								label->setOpenExternalLinks(true);
								label->setStyleSheet(
									QString::fromUtf8(obs_data_get_string(block, "qss")));
								partnerBlockActions.append(toolbar->insertWidget(before, label));
							}
							obs_data_release(block);
						}
						auto close = new QAction("x");
						close->setToolTip(QString::fromUtf8(obs_module_text("ClosePartnerBlock")));
						close->connect(close, &QAction::triggered, [] {
							foreach(auto &a, partnerBlockActions)
							{
								toolbar->removeAction(a);
							}
							partnerBlockActions.clear();
							config_set_int(obs_frontend_get_user_config(), "Aitum", "partner_block",
								       (int64_t)time(nullptr));
						});
						toolbar->insertAction(before, close);
						partnerBlockActions.append(close);
						QWidget *spacer = new QWidget();
						spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
						partnerBlockActions.append(toolbar->insertWidget(before, spacer));
					});
				}
			}
			obs_data_array_release(blocks);
			obs_data_release(data_obj);
		}
	}

	if (version_download_info) {
		download_info_destroy(version_download_info);
		version_download_info = nullptr;
	}
	return true;
}

void transition_start(void *, calldata_t *)
{
	QMetaObject::invokeMethod(live_scenes_dock, "MainSceneChanged", Qt::QueuedConnection);
	for (const auto &it : canvas_docks) {
		QMetaObject::invokeMethod(it, "MainSceneChanged", Qt::QueuedConnection);
	}
}

void save_dock_state(QString mode)
{
	if (mode.isEmpty())
		return;
	if (!current_profile_config)
		return;
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto state = main_window->saveState();
	auto b64 = state.toBase64();
	auto state_chars = b64.constData();
	std::string setting_name = "dock_state_" + mode.toStdString();
	obs_data_set_string(current_profile_config, setting_name.c_str(), state_chars);

	for (const auto &it : canvas_docks) {
		QMetaObject::invokeMethod(it, "SaveSettings", Q_ARG(bool, false), Q_ARG(QString, mode));
	}
	for (const auto &it : canvas_clone_docks) {
		QMetaObject::invokeMethod(it, "SaveSettings", Q_ARG(bool, false), Q_ARG(QString, mode));
	}
	if (component_dock)
		QMetaObject::invokeMethod(component_dock, "SaveSettings", Q_ARG(bool, false), Q_ARG(QString, mode));
}

void reset_live_dock_state()
{
	//Shows activity feeds, chat (multi-chat), Game capture change dock, main scenes quick switch dock, canvas previews, multi-stream dock. Hides scene list or sources or anything related to actually making a stream setup and not actually being live
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QMetaObject::invokeMethod(main_window, "on_resetDocks_triggered", Q_ARG(bool, true));
	QMetaObject::invokeMethod(main_window, "on_sideDocks_toggled", Q_ARG(bool, true));
	auto d = main_window->findChild<QDockWidget *>(QStringLiteral("controlsDock"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("sourcesDock"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("scenesDock"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("transitionsDock"));
	if (d)
		d->setVisible(false);

	QList<QDockWidget *> right_docks;
	QList<int> right_dock_sizes;

	auto chat = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteChat"));
	if (chat) {
		chat->setVisible(true);
		chat->setFloating(false);
		main_window->addDockWidget(Qt::RightDockWidgetArea, chat);
		right_docks.append(chat);
		right_dock_sizes.append(3);
	}

	auto activity = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteActivity"));
	if (activity) {
		activity->setVisible(true);
		activity->setFloating(false);
		main_window->addDockWidget(Qt::RightDockWidgetArea, activity);
		right_docks.append(activity);
		right_dock_sizes.append(3);
		if (chat)
			main_window->splitDockWidget(chat, activity, Qt::Horizontal);
	}

	auto info = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteInfo"));
	if (info) {
		info->setVisible(true);
		info->setFloating(false);
		main_window->addDockWidget(Qt::RightDockWidgetArea, info);
		right_docks.append(info);
		right_dock_sizes.append(1);
	}

	auto portal = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuitePortal"));
	if (portal) {
		portal->setVisible(true);
		portal->setFloating(false);
		main_window->addDockWidget(Qt::RightDockWidgetArea, portal);
		right_docks.append(portal);
		right_dock_sizes.append(1);
		if (info)
			main_window->splitDockWidget(portal, info, Qt::Horizontal);
	}

	QList<QDockWidget *> bottom_docks;
	QList<int> bottom_dock_sizes;

	d = main_window->findChild<QDockWidget *>(QStringLiteral("mixerDock"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::BottomDockWidgetArea, d);
		bottom_docks.append(d);
		bottom_dock_sizes.append(1);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteOutput"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::BottomDockWidgetArea, d);
		bottom_docks.append(d);
		bottom_dock_sizes.append(1);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteProperties"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteFilters"));
	if (d)
		d->setVisible(false);

	QList<QDockWidget *> left_docks;
	QList<int> left_dock_sizes;

	foreach(auto &canvas_dock, canvas_docks)
	{
		d = (QDockWidget *)canvas_dock->parentWidget();
		canvas_dock->reset_live_state();
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::LeftDockWidgetArea, d);
		left_docks.append(d);
		left_dock_sizes.append(1);
	}

	foreach(auto &canvas_clone_dock, canvas_clone_docks)
	{
		d = (QDockWidget *)canvas_clone_dock->parentWidget();
		canvas_clone_dock->reset_live_state();
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::LeftDockWidgetArea, d);
		left_docks.append(d);
		left_dock_sizes.append(1);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteLiveScenes"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::LeftDockWidgetArea, d);
		left_docks.append(d);
		left_dock_sizes.append(1);
	}

	main_window->resizeDocks(left_docks, left_dock_sizes, Qt::Vertical);
	main_window->resizeDocks(right_docks, right_dock_sizes, Qt::Vertical);
	main_window->resizeDocks(bottom_docks, bottom_dock_sizes, Qt::Horizontal);
}

void reset_build_dock_state()
{
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QMetaObject::invokeMethod(main_window, "on_resetDocks_triggered", Q_ARG(bool, true));
	QMetaObject::invokeMethod(main_window, "on_sideDocks_toggled", Q_ARG(bool, true));
	auto d = main_window->findChild<QDockWidget *>(QStringLiteral("controlsDock"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("scenesDock"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::LeftDockWidgetArea, d);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("sourcesDock"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::LeftDockWidgetArea, d);
	}

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

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteFilters"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::LeftDockWidgetArea, d);
	}

	QList<QDockWidget *> bottom_docks;
	QList<int> bottom_dock_sizes;

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteProperties"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::BottomDockWidgetArea, d);
		bottom_docks.append(d);
		bottom_dock_sizes.append(2);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("mixerDock"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::BottomDockWidgetArea, d);
		bottom_docks.append(d);
		bottom_dock_sizes.append(1);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("SceneNotesDock"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::BottomDockWidgetArea, d);
		bottom_docks.append(d);
		bottom_dock_sizes.append(1);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("transitionsDock"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::LeftDockWidgetArea, d);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteLiveScenes"));
	if (d)
		d->setVisible(false);

	QList<QDockWidget *> right_docks;
	QList<int> right_dock_sizes;

	foreach(auto &canvas_dock, canvas_docks)
	{
		d = (QDockWidget *)canvas_dock->parentWidget();
		canvas_dock->reset_build_state();
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::RightDockWidgetArea, d);
		right_docks.append(d);
		right_dock_sizes.append(1);
	}

	foreach(auto &canvas_clone_dock, canvas_clone_docks)
	{
		d = (QDockWidget *)canvas_clone_dock->parentWidget();
		canvas_clone_dock->reset_build_state();
		d->setVisible(true);
		d->setFloating(false);
		main_window->addDockWidget(Qt::RightDockWidgetArea, d);
		right_docks.append(d);
		right_dock_sizes.append(1);
	}

	main_window->resizeDocks(right_docks, right_dock_sizes, Qt::Vertical);
	main_window->resizeDocks(bottom_docks, bottom_dock_sizes, Qt::Horizontal);
}

void reset_design_dock_state()
{
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	QMetaObject::invokeMethod(main_window, "on_resetDocks_triggered", Q_ARG(bool, true));
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

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteProperties"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteFilters"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
	}

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteLiveScenes"));
	if (d)
		d->setVisible(false);

	d = main_window->findChild<QDockWidget *>(QStringLiteral("AitumStreamSuiteComponent"));
	if (d) {
		d->setVisible(true);
		d->setFloating(false);
	}
}

std::vector<std::pair<std::string, void (*)(void)>> fixed_tabs = {{"Live", reset_live_dock_state},
								  {"Build", reset_build_dock_state}};
//,{"Design", reset_design_dock_state}};

static bool scene_collection_changing = false;

void load_dock_state(QString mode)
{
	if (!current_profile_config)
		return;
	scene_collection_changing = false;
	std::string state;
	std::string setting_name = "dock_state_" + mode.toStdString();
	state = obs_data_get_string(current_profile_config, setting_name.c_str());
	if (state.empty()) {
		setting_name = "dock_state_" + mode.toLower().toStdString();
		state = obs_data_get_string(current_profile_config, setting_name.c_str());
	}
	if (state.empty()) {
		for (auto it = fixed_tabs.begin(); it != fixed_tabs.end(); ++it) {
			auto translated = obs_module_text(it->first.c_str());
			if ((translated && mode == translated) || mode == QString::fromStdString(it->first)) {
				it->second();
				return;
			}
		}
	}
	loaded_docks.clear();
	if (!state.empty()) {
		auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		main_window->restoreState(QByteArray::fromBase64(state.c_str()));

		auto docks = main_window->findChildren<QDockWidget *>();
		for (auto &dock : docks) {
			if (dock->isVisible())
				loaded_docks.append(dock->objectName());
		}
	}
	for (const auto &it : canvas_docks) {
		QMetaObject::invokeMethod(it, "LoadMode", Qt::QueuedConnection, Q_ARG(QString, mode));
	}
	for (const auto &it : canvas_clone_docks) {
		QMetaObject::invokeMethod(it, "LoadMode", Qt::QueuedConnection, Q_ARG(QString, mode));
	}
	if (component_dock)
		QMetaObject::invokeMethod(component_dock, "LoadMode", Qt::QueuedConnection, Q_ARG(QString, mode));
}

void load_outputs()
{
	QMetaObject::invokeMethod(
		output_dock,
		[] {
			if (output_dock)
				output_dock->LoadSettings();
		},
		Qt::QueuedConnection);
}

void load_canvas()
{
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto canvas = obs_data_get_array(current_profile_config, "canvas");
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
			auto uuid = obs_data_get_string(t, "uuid");
			for (const auto &it : canvas_docks) {
				if (strcmp(obs_canvas_get_uuid(it->GetCanvas()), uuid) == 0) {
					obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					break;
				}
			}
			for (const auto &it : canvas_clone_docks) {
				if (strcmp(obs_canvas_get_uuid(it->GetCanvas()), uuid) == 0) {
					obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					break;
				}
			}
			obs_canvas_t *c = obs_get_canvas_by_uuid(uuid);
			if (c && obs_canvas_removed(c)) {
				obs_canvas_release(c);
				c = nullptr;
			}
			if (!c)
				c = obs_get_canvas_by_name(canvas_name);
			if (c && obs_canvas_removed(c)) {
				obs_canvas_release(c);
				c = nullptr;
			}
			if (c) {
				obs_frontend_remove_canvas(c);
				obs_canvas_remove(c);
				obs_canvas_release(c);
			}
			obs_data_array_erase(canvas, i);
			obs_data_release(t);
			canvas_count--;
		} else if (strcmp(obs_data_get_string(t, "type"), "clone") == 0) {
			auto uuid = obs_data_get_string(t, "uuid");
			auto name = obs_data_get_string(t, "name");
			for (const auto &it : canvas_docks) {
				if (strcmp(obs_canvas_get_uuid(it->GetCanvas()), uuid) == 0) {
					obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					break;
				}
			}
			CanvasCloneDock *ccd = nullptr;
			for (const auto &it : canvas_clone_docks) {
				auto cuuid = obs_canvas_get_uuid(it->GetCanvas());
				if (strcmp(cuuid, uuid) == 0) {
					if (obs_canvas_removed(it->GetCanvas()) ||
					    strcmp(it->parentWidget()->objectName().toUtf8().constData(), name) != 0) {
						// canvas name changed, remove old dock and create a new one
						obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					} else {
						ccd = it;
					}
					break;
				} else if (strcmp(it->parentWidget()->objectName().toUtf8().constData(), name) == 0) {
					if (obs_canvas_removed(it->GetCanvas())) {
						obs_frontend_remove_dock(name);
						break;
					}
					obs_data_set_string(t, "uuid", cuuid);
					ccd = it;
					break;
				}
			}
			if (ccd) {
				ccd->UpdateSettings(t);
			} else {
				ccd = new CanvasCloneDock(t, main_window);
				if (obs_frontend_add_dock_by_id(name, name, ccd)) {
					canvas_clone_docks.push_back(ccd);
					if (!obs_data_get_bool(t, "has_loaded")) {
						ccd->parentWidget()->show();
						obs_data_set_bool(t, "has_loaded", true);
					}
				} else {
					delete ccd;
				}
			}
			i++;
		} else {
			auto uuid = obs_data_get_string(t, "uuid");
			auto name = obs_data_get_string(t, "name");
			for (const auto &it : canvas_clone_docks) {
				if (strcmp(obs_canvas_get_uuid(it->GetCanvas()), uuid) == 0) {
					obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					break;
				}
			}
			CanvasDock *cd = nullptr;
			for (const auto &it : canvas_docks) {
				auto cuuid = obs_canvas_get_uuid(it->GetCanvas());
				if (strcmp(cuuid, uuid) == 0) {
					if (obs_canvas_removed(it->GetCanvas()) ||
					    strcmp(it->parentWidget()->objectName().toUtf8().constData(), name) != 0) {
						// canvas name changed, remove old dock and create a new one
						obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
					} else {
						cd = it;
					}
					break;
				} else if (strcmp(it->parentWidget()->objectName().toUtf8().constData(), name) == 0) {
					if (obs_canvas_removed(it->GetCanvas())) {
						obs_frontend_remove_dock(name);
						break;
					}
					obs_data_set_string(t, "uuid", cuuid);
					cd = it;
					break;
				}
			}

			if (cd) {
				cd->UpdateSettings(t);
			} else {
				cd = new CanvasDock(t, main_window);
				if (obs_frontend_add_dock_by_id(name, name, cd)) {
					canvas_docks.push_back(cd);
					if (!obs_data_get_bool(t, "has_loaded")) {
						cd->parentWidget()->show();
						obs_data_set_bool(t, "has_loaded", true);
					}
				} else {
					delete cd;
				}
			}
			i++;
		}
	}
	obs_data_array_release(canvas);
	for (const auto &it : canvas_clone_docks) {
		it->UpdateSettings(nullptr);
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
		obs_data_set_bool(current_profile_config, "main_stream_output_show", true);
		obs_data_set_bool(current_profile_config, "main_record_output_show", true);
		obs_data_set_bool(current_profile_config, "main_backtrack_output_show", true);
		obs_data_set_bool(current_profile_config, "main_virtual_cam_output_show", true);
		blog(LOG_WARNING, "[Aitum Stream Suite] No configuration file loaded");
	} else {
		blog(LOG_INFO, "[Aitum Stream Suite] Loaded configuration file");
	}
	for (const auto &it : empty_docks) {
		obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
	}
	empty_docks.clear();
	auto ed = obs_data_get_array(current_profile_config, "empty_docks");
	if (ed) {
		obs_data_array_enum(
			ed,
			[](obs_data_t *data, void *param) {
				UNUSED_PARAMETER(param);
				auto empty_dock = new QFrame;
				if (obs_frontend_add_dock_by_id(obs_data_get_string(data, "name"),
								obs_data_get_string(data, "name"), empty_dock)) {
					empty_docks.push_back(empty_dock);
				} else {
					delete empty_dock;
				}
			},
			nullptr);
		obs_data_array_release(ed);
	}

	auto canvas = obs_data_get_array(current_profile_config, "canvas");
	if (!canvas) {
		canvas = obs_data_array_create();
		const char *canvas_name = "Vertical";
		auto vertical_canvas = obs_get_canvas_by_name("Aitum Vertical");
		if (vertical_canvas) {
			obs_canvas_release(vertical_canvas);
			canvas_name = "Aitum Vertical";
		}
		auto new_canvas = obs_data_create();
		obs_data_set_string(new_canvas, "name", canvas_name);
		obs_data_set_int(new_canvas, "color", 0x1F1A17);
		obs_data_array_push_back(canvas, new_canvas);
		obs_data_release(new_canvas);

		obs_data_set_array(current_profile_config, "canvas", canvas);

		auto outputs2 = obs_data_get_array(current_profile_config, "outputs");
		if (!outputs2) {
			outputs2 = obs_data_array_create();
			obs_data_set_array(current_profile_config, "outputs", outputs2);
		}
		if (obs_data_array_count(outputs2) < 1) {
			auto new_output = obs_data_create();
			obs_data_set_bool(new_output, "enabled", true);
			obs_data_set_string(new_output, "type", "stream");
			obs_data_set_string(new_output, "name", "Vertical Stream");
			obs_data_set_string(new_output, "canvas", canvas_name);
			obs_data_array_push_back(outputs2, new_output);
			obs_data_release(new_output);

			new_output = obs_data_create();
			obs_data_set_bool(new_output, "enabled", true);
			obs_data_set_string(new_output, "type", "backtrack");
			obs_data_set_string(new_output, "name", "Vertical Backtrack");
			obs_data_set_string(new_output, "canvas", canvas_name);
			obs_data_array_push_back(outputs2, new_output);
			obs_data_release(new_output);
		}
		obs_data_array_release(outputs2);
	} else {
		obs_data_array_release(canvas);
	}
	auto dsm = obs_data_item_byname(current_profile_config, "dock_state_mode");
	if (obs_data_item_gettype(dsm) == OBS_DATA_STRING) {
		auto mode = QString::fromUtf8(obs_data_item_get_string(dsm));
		for (int i = 0; i < modesTabBar->count(); i++) {
			auto d = modesTabBar->tabData(i);
			if (!d.isNull() && d.isValid() && d.toString() == mode) {
				if (modesTabBar->currentIndex() != i) {
					modesTab = "";
					modesTabBar->setCurrentIndex(i);
				}
				break;
			} else if (modesTabBar->tabText(i) == mode) {
				if (modesTabBar->currentIndex() != i) {
					modesTab = "";
					modesTabBar->setCurrentIndex(i);
				}
				break;
			}
		}
	} else {
		auto index = obs_data_item_get_int(dsm);
		if (modesTabBar->currentIndex() != index) {
			modesTab = "";
			modesTabBar->setCurrentIndex(index);
		}
	}
	obs_data_item_release(&dsm);
	load_canvas();

	load_outputs();

	QMetaObject::invokeMethod(
		modesTabBar,
		[] {
			auto index = modesTabBar->currentIndex();
			if (index >= 0) {
				auto d = modesTabBar->tabData(index);
				if (!d.isNull() && d.isValid() && !d.toString().isEmpty()) {
					load_dock_state(d.toString());
				} else {
					load_dock_state(modesTabBar->tabText(index));
				}
			}
		},
		Qt::QueuedConnection);
}

void save_current_profile_config(bool save_docks)
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

	if (save_docks) {
		auto index = modesTabBar->currentIndex();
		if (index >= 0) {
			QString tn;
			auto d = modesTabBar->tabData(index);
			if (!d.isNull() && d.isValid() && !d.toString().isEmpty()) {
				tn = d.toString();
			} else {
				tn = modesTabBar->tabText(index);
			}
			save_dock_state(tn);
			obs_data_set_string(current_profile_config, "dock_state_mode", tn.toUtf8().constData());
		}
	}
	if (output_dock)
		output_dock->SaveSettings();

	if (!obs_data_save_json_safe(current_profile_config, path.array, "tmp", "bak")) {
		blog(LOG_WARNING, "[Aitum Stream Suite] Failed to save configuration file");
	} else {
		blog(LOG_INFO, "[Aitum Stream Suite] Saved configuration file");
	}

	dstr_free(&path);
}

void load_browser_panels()
{
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_add_dock_by_id("AitumStreamSuiteChat", obs_module_text("AitumStreamSuiteChat"),
				    new BrowserDock("chat", "https://chat.aitumsuite.tv/chat", main_window));
	obs_frontend_add_dock_by_id("AitumStreamSuiteActivity", obs_module_text("AitumStreamSuiteActivity"),
				    new BrowserDock("activity", "https://chat.aitumsuite.tv/activity", main_window));
	obs_frontend_add_dock_by_id("AitumStreamSuiteInfo", obs_module_text("AitumStreamSuiteInfo"),
				    new BrowserDock("info", "https://chat.aitumsuite.tv/info", main_window));
	obs_frontend_add_dock_by_id("AitumStreamSuitePortal", obs_module_text("AitumStreamSuitePortal"),
				    new BrowserDock("portal", "https://chat.aitumsuite.tv/portal", main_window));
}

void unload_browser_panels()
{
	obs_frontend_remove_dock("AitumStreamSuiteChat");
	obs_frontend_remove_dock("AitumStreamSuiteActivity");
	obs_frontend_remove_dock("AitumStreamSuiteInfo");
	obs_frontend_remove_dock("AitumStreamSuitePortal");
}

struct QCef;
extern QCef *cef;
void DestroyPanelCookieManager();
static bool restart = false;

static void frontend_event(enum obs_frontend_event event, void *private_data)
{
	static bool finished_loading = false;
	UNUSED_PARAMETER(private_data);
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		finished_loading = true;
		if (restart) {
			const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			QTimer::singleShot(200, main_window, [main_window] { main_window->close(); });
			return;
		}
		load_browser_panels();
		struct obs_frontend_source_list transitions = {};
		obs_frontend_get_transitions(&transitions);
		for (size_t i = 0; i < transitions.sources.num; i++) {
			auto sh = obs_source_get_signal_handler(transitions.sources.array[i]);
			signal_handler_connect(sh, "transition_start", transition_start, nullptr);
		}
		obs_frontend_source_list_free(&transitions);

		load_current_profile_config();
	} else if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGED) {
		DestroyPanelCookieManager();
		load_browser_panels();
		load_current_profile_config();
	} else if (event == OBS_FRONTEND_EVENT_PROFILE_CHANGING) {
		save_current_profile_config(true);
		unload_browser_panels();
	} else if (event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		if (current_profile_config) {
			obs_data_release(current_profile_config);
			current_profile_config = nullptr;
		}
		if (output_dock)
			output_dock->Exiting();
		for (auto &it : empty_docks) {
			obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
		}
		empty_docks.clear();
		obs_queue_task(
			OBS_TASK_GRAPHICS,
			[](void *) {
				obs_queue_task(
					OBS_TASK_UI,
					[](void *) {
						unload_browser_panels();
						DestroyPanelCookieManager();
						if (cef) {
							delete cef;
							cef = nullptr;
						}
					},
					nullptr, false);
			},
			nullptr, false);
	} else if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED) {
		if (!studioModeAction->isChecked()) {
			studioModeAction->setChecked(true);
		}
	} else if (event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED) {
		if (studioModeAction->isChecked()) {
			studioModeAction->setChecked(false);
		}
	} else if (event == OBS_FRONTEND_EVENT_VIRTUALCAM_STARTED) {
		output_dock->UpdateMainVirtualCameraStatus(true);
	} else if (event == OBS_FRONTEND_EVENT_VIRTUALCAM_STOPPED) {
		output_dock->UpdateMainVirtualCameraStatus(false);
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STARTING || event == OBS_FRONTEND_EVENT_STREAMING_STARTED) {
		output_dock->UpdateMainStreamStatus(true);
	} else if (event == OBS_FRONTEND_EVENT_STREAMING_STOPPING || event == OBS_FRONTEND_EVENT_STREAMING_STOPPED) {
		output_dock->UpdateMainStreamStatus(false);
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STARTING || event == OBS_FRONTEND_EVENT_RECORDING_STARTED) {
		output_dock->UpdateMainRecordingStatus(true);
	} else if (event == OBS_FRONTEND_EVENT_RECORDING_STOPPING || event == OBS_FRONTEND_EVENT_RECORDING_STOPPED) {
		output_dock->UpdateMainRecordingStatus(false);
	} else if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING || event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED) {
		output_dock->UpdateMainBacktrackStatus(true);
	} else if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING || event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED) {
		output_dock->UpdateMainBacktrackStatus(false);
	} else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED) {
		QMetaObject::invokeMethod(live_scenes_dock, "MainSceneChanged", Qt::QueuedConnection);
		for (const auto &it : canvas_docks) {
			QMetaObject::invokeMethod(it, "MainSceneChanged", Qt::QueuedConnection);
		}
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		if (finished_loading) {
			struct obs_frontend_source_list transitions = {};
			obs_frontend_get_transitions(&transitions);
			for (size_t i = 0; i < transitions.sources.num; i++) {
				auto sh = obs_source_get_signal_handler(transitions.sources.array[i]);
				signal_handler_connect(sh, "transition_start", transition_start, nullptr);
			}
			obs_frontend_source_list_free(&transitions);
			load_current_profile_config();
		}
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING) {
		scene_collection_changing = true;
	} else if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP) {
		for (auto i = canvas_clone_docks.size(); i > 0; i--) {
			auto it = canvas_clone_docks.begin();
			std::advance(it, i - 1);
			auto dock = (*it)->parentWidget();
			if (dock)
				obs_frontend_remove_dock(dock->objectName().toUtf8().constData());
		}

		for (auto i = canvas_docks.size(); i > 0; i--) {
			auto it = canvas_docks.begin();
			std::advance(it, i - 1);
			auto dock = (*it)->parentWidget();
			if (dock)
				obs_frontend_remove_dock(dock->objectName().toUtf8().constData());
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

bool obs_data_array_equal(obs_data_array_t *a, obs_data_array_t *b)
{
	size_t a_count = obs_data_array_count(a);
	size_t b_count = obs_data_array_count(b);
	if (a_count != b_count)
		return false;
	for (size_t i = 0; i < a_count; i++) {
		obs_data_t *a_item = obs_data_array_item(a, i);
		obs_data_t *b_item = obs_data_array_item(b, i);
		const char *a_json = obs_data_get_json(a_item);
		const char *b_json = obs_data_get_json(b_item);
		bool equal = (strcmp(a_json, b_json) == 0);
		obs_data_release(a_item);
		obs_data_release(b_item);
		if (!equal)
			return false;
	}
	return true;
}

void open_config_dialog(int tab, const char *create_type)
{
	if (!configDialog) {
		configDialog = new OBSBasicSettings((QMainWindow *)obs_frontend_get_main_window());
		QObject::connect(configDialog, &OBSBasicSettings::accepted, [] {

		});
	}
	auto settings = obs_data_create();
	if (current_profile_config) {
		const char *geom = obs_data_get_string(current_profile_config, "config_geometry");
		if (geom && strlen(geom)) {
			QByteArray ba = QByteArray::fromBase64(QByteArray(geom));
			configDialog->restoreGeometry(ba);
		}
		obs_data_apply(settings, current_profile_config);
	}

	configDialog->LoadSettings(settings);
	configDialog->SetNewerVersion(newer_version_available);
	if (tab > 0)
		configDialog->ShowTab(tab);
	configDialog->show();
	configDialog->SetCreateType(create_type);

	if (configDialog->exec() == QDialog::Accepted) {
		bool canvas_changed = false;
		bool outputs_changed = false;
		if (current_profile_config) {
			auto show = obs_data_get_bool(settings, "main_stream_output_show");
			if (show != obs_data_get_bool(current_profile_config, "main_stream_output_show")) {
				obs_data_set_bool(current_profile_config, "main_stream_output_show", show);
				outputs_changed = true;
			}
			show = obs_data_get_bool(settings, "main_record_output_show");
			if (show != obs_data_get_bool(current_profile_config, "main_record_output_show")) {
				obs_data_set_bool(current_profile_config, "main_record_output_show", show);
				outputs_changed = true;
			}
			show = obs_data_get_bool(settings, "main_backtrack_output_show");
			if (show != obs_data_get_bool(current_profile_config, "main_backtrack_output_show")) {
				obs_data_set_bool(current_profile_config, "main_backtrack_output_show", show);
				outputs_changed = true;
			}
			show = obs_data_get_bool(settings, "main_virtual_cam_output_show");
			if (show != obs_data_get_bool(current_profile_config, "main_virtual_cam_output_show")) {
				obs_data_set_bool(current_profile_config, "main_virtual_cam_output_show", show);
				outputs_changed = true;
			}
			obs_data_array_t *a = obs_data_get_array(current_profile_config, "canvas");
			obs_data_array_t *b = obs_data_get_array(settings, "canvas");
			if (!obs_data_array_equal(a, b)) {
				canvas_changed = true;
				obs_data_set_array(current_profile_config, "canvas", b);
			}
			obs_data_array_release(a);
			obs_data_array_release(b);
			a = obs_data_get_array(current_profile_config, "outputs");
			b = obs_data_get_array(settings, "outputs");
			if (!obs_data_array_equal(a, b)) {
				outputs_changed = true;
				obs_data_set_array(current_profile_config, "outputs", b);
			}
			obs_data_array_release(a);
			obs_data_array_release(b);
			obs_data_release(settings);
		} else {
			canvas_changed = true;
			outputs_changed = true;
			current_profile_config = settings;
		}
		obs_data_set_string(current_profile_config, "config_geometry", configDialog->saveGeometry().toBase64().constData());
		configDialog->SaveHotkeys();

		save_current_profile_config(true);
		if (canvas_changed) {
			load_canvas();
			if (vendor)
				obs_websocket_vendor_emit_event(vendor, "canvas_changed", nullptr);
		}

		if (outputs_changed) {
			load_outputs();
			if (vendor)
				obs_websocket_vendor_emit_event(vendor, "outputs_changed", nullptr);
		}
	} else {
		obs_data_release(settings);
	}
}
extern "C" const struct obs_source_info component_info;

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Aitum Stream Suite] loaded version %s", PROJECT_VERSION);

	obs_register_source(&component_info);

	QFontDatabase::addApplicationFont(":/aitum/media/Roboto.ttf");
	QFontDatabase::addApplicationFont(":/aitum/media/Roboto-Italic.ttf");
	QFontDatabase::addApplicationFont(":/aitum/media/RobotoCondensed.ttf");
	QFontDatabase::addApplicationFont(":/aitum/media/RobotoCondensed-Italic.ttf");

	obs_frontend_add_event_callback(frontend_event, nullptr);

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto user_config = obs_frontend_get_user_config();
	if (user_config) {
		if (!config_get_bool(user_config, "Aitum", "ThemeSet")) {
			auto theme = config_get_string(user_config, "Appearance", "Theme");
			if (!theme || strcmp(theme, "com.obsproject.Aitum.Original") != 0) {
				config_set_string(user_config, "Appearance", "Theme", "com.obsproject.Aitum.Original");
				restart = true;
			}
			config_set_bool(user_config, "BasicWindow", "VerticalVolControl", true);
			config_set_bool(user_config, "Aitum", "ThemeSet", true);
			config_save_safe(user_config, "tmp", "bak");
		}
		const char *theme = config_get_string(user_config, "Appearance", "Theme");
		if (theme && strcmp(theme, "com.obsproject.Aitum.Original") == 0) {
			main_window->setContentsMargins(10, 10, 10, 10);
			main_window->findChild<QWidget *>("centralwidget")->setContentsMargins(0, 0, 0, 0);
		}
	}

	modesTabBar = new QTabBar();
	modesTabBar->setContextMenuPolicy(Qt::CustomContextMenu);
	toolbar = new TabToolBar(modesTabBar);
	toolbar->setObjectName(QStringLiteral("AitumToolbar"));
	main_window->addToolBar(toolbar);
	toolbar->setFloatable(false);
	//tb->setMovable(false);
	//tb->setAllowedAreas(Qt::ToolBarArea::TopToolBarArea);

	for (auto it : fixed_tabs) {
		auto index = modesTabBar->addTab(QString::fromUtf8(obs_module_text(it.first.c_str())));
		modesTabBar->setTabData(index, QString::fromUtf8(it.first.c_str()));
	}
	toolbar->addWidget(modesTabBar);
	toolbar->addSeparator();

	QObject::connect(modesTabBar, &QTabBar::currentChanged, [](int index) {
		save_dock_state(modesTab);
		auto d = modesTabBar->tabData(index);
		if (!d.isNull() && d.isValid() && !d.toString().isEmpty()) {
			modesTab = d.toString();
			load_dock_state(d.toString());
		} else {
			modesTab = modesTabBar->tabText(index);
			load_dock_state(modesTabBar->tabText(index));
		}
	});

	QObject::connect(modesTabBar, &QTabBar::customContextMenuRequested, [] {
		QMenu menu;
		auto index = modesTabBar->currentIndex();
		auto d = modesTabBar->tabData(index);
		if (!d.isNull() && d.isValid() && !d.toString().isEmpty()) {
			menu.addAction(QString::fromUtf8(obs_module_text("Reset")), [d] {
				for (auto it : fixed_tabs) {
					if (it.first == d.toString().toUtf8().constData()) {
						it.second();
						return;
					}
				}
			});
		}
		menu.addSeparator();
		menu.addAction(QString::fromUtf8(obs_module_text("AddEmptyDock")), [] {
			const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			std::string name = obs_module_text("EmptyDock");
			if (NameDialog::AskForName(main_window, QString::fromUtf8(obs_module_text("EmptyDockName")), name)) {
				//break;
				auto empty_dock = new QFrame;
				if (obs_frontend_add_dock_by_id(name.c_str(), name.c_str(), empty_dock)) {
					empty_dock->parentWidget()->show();
					auto ed = obs_data_get_array(current_profile_config, "empty_docks");
					if (!ed) {
						ed = obs_data_array_create();
						obs_data_set_array(current_profile_config, "empty_docks", ed);
					}
					auto edd = obs_data_create();
					obs_data_set_string(edd, "name", name.c_str());
					obs_data_array_push_back(ed, edd);
					obs_data_release(edd);
					empty_docks.push_back(empty_dock);
				} else {
					delete empty_dock;
				}
			}
		});
		if (!empty_docks.empty()) {
			auto removeMenu = menu.addMenu(QString::fromUtf8(obs_module_text("RemoveEmptyDock")));
			for (const auto &it : empty_docks) {
				QFrame *w = it;
				removeMenu->addAction(it->parentWidget()->objectName(), [w] {
					std::string name = w->parentWidget()->objectName().toUtf8().constData();
					obs_frontend_remove_dock(name.c_str());
					empty_docks.remove(w);
					auto ed = obs_data_get_array(current_profile_config, "empty_docks");
					auto count = obs_data_array_count(ed);
					for (size_t i = count; i > 0; i--) {
						auto item = obs_data_array_item(ed, i - 1);
						if (!item)
							continue;
						if (strcmp(name.c_str(), obs_data_get_string(item, "name")) == 0) {
							obs_data_array_erase(ed, i - 1);
						}
					}
				});
			}
		}
		menu.exec(QCursor::pos());
	});

	auto aitumSettingsAction = toolbar->addAction(QString::fromUtf8(obs_module_text("Settings")));
	aitumSettingsAction->setProperty("themeID", "configIconSmall");
	aitumSettingsAction->setProperty("class", "icon-gear");
	aitumSettingsWidget = toolbar->widgetForAction(aitumSettingsAction);
	aitumSettingsWidget->setProperty("themeID", "configIconSmall");
	aitumSettingsWidget->setProperty("class", "icon-gear");
	aitumSettingsWidget->setObjectName("AitumStreamSuiteSettingsButton");
	QObject::connect(aitumSettingsAction, &QAction::triggered, [] { open_config_dialog(0, nullptr); });

	// Contribute Button
	auto contributeButton = toolbar->addAction(generateEmojiQIcon("❤️"), QString::fromUtf8(obs_module_text("Donate")));
	contributeButton->setProperty("themeID", "icon-aitum-donate");
	contributeButton->setProperty("class", "icon-aitum-donate");
	toolbar->widgetForAction(contributeButton)->setProperty("themeID", "icon-aitum-donate");
	toolbar->widgetForAction(contributeButton)->setProperty("class", "icon-aitum-donate");
	contributeButton->setToolTip(QString::fromUtf8(obs_module_text("AitumStreamSuiteDonate")));
	QAction::connect(contributeButton, &QAction::triggered,
			 [] { QDesktopServices::openUrl(QUrl("https://aitum.tv/contribute")); });

	// Aitum Button
	auto aitumButton = toolbar->addAction(QIcon(":/aitum/media/aitum.png"), QString::fromUtf8(obs_module_text("Aitum")));
	aitumButton->setProperty("themeID", "icon-aitum");
	aitumButton->setProperty("class", "icon-aitum");
	toolbar->widgetForAction(aitumButton)->setProperty("themeID", "icon-aitum");
	toolbar->widgetForAction(aitumButton)->setProperty("class", "icon-aitum");
	aitumButton->setToolTip(QString::fromUtf8("https://aitum.tv"));
	QAction::connect(aitumButton, &QAction::triggered, [] { QDesktopServices::openUrl(QUrl("https://aitum.tv")); });

	auto addCanvas = toolbar->addAction(QString::fromUtf8(obs_module_text("AddCanvas")));
	QAction::connect(addCanvas, &QAction::triggered, [] { open_config_dialog(1, nullptr); });

	//tb->addAction(QString::fromUtf8(obs_module_text("Reset")));
	//tb->layout()->addItem(new QSpacerItem(0, 0));
	QWidget *spacer = new QWidget();
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	toolbar->addWidget(spacer);

	//auto controlsToolBar = main_window->addToolBar(QString::fromUtf8(obs_module_text("Controls")));
	auto controlsToolBar = toolbar;

	studioModeAction = controlsToolBar->addAction(QString::fromUtf8(obs_module_text("StudioMode")));

	studioModeAction->setCheckable(true);
	studioModeAction->setIcon(create2StateIcon(":/aitum/media/studio_mode_on.svg", ":/aitum/media/studio_mode_off.svg"));
	controlsToolBar->widgetForAction(studioModeAction)->setStyleSheet("QAbstractButton:checked{background: rgb(158,0,89);}");
	((QToolButton *)controlsToolBar->widgetForAction(studioModeAction))->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	QObject::connect(studioModeAction, SIGNAL(triggered()), main_window, SLOT(TogglePreviewProgramMode()));

	//https://coolors.co/00d299-ff0000-1a57ff-c08000-9e0059

	auto settingsButton = controlsToolBar->addAction(QString::fromUtf8(obs_module_text("Settings")));
	settingsButton->setProperty("themeID", "configIconSmall");
	settingsButton->setProperty("class", "icon-gear");
	controlsToolBar->widgetForAction(settingsButton)->setProperty("themeID", "configIconSmall");
	controlsToolBar->widgetForAction(settingsButton)->setProperty("class", "icon-gear");
	((QToolButton *)controlsToolBar->widgetForAction(settingsButton))->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	QObject::connect(settingsButton, SIGNAL(triggered()), main_window, SLOT(on_action_Settings_triggered()));

	//auto action = controlsToolBar->addAction("");
	//action->setCheckable(true);
	//action->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::InsertLink));
	//action->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::CameraWeb));
	//action->setIcon(QIcon::fromTheme(QIcon::ThemeIcon::EditUndo));

	output_dock = new OutputDock(main_window);
	obs_frontend_add_dock_by_id("AitumStreamSuiteOutput", obs_module_text("AitumStreamSuiteOutput"), output_dock);
	//component_dock = new CanvasDock("Components", main_window);
	//obs_frontend_add_dock_by_id("AitumStreamSuiteComponent", obs_module_text("AitumStreamSuiteComponent"), component_dock);
	properties_dock = new PropertiesDock(main_window);
	obs_frontend_add_dock_by_id("AitumStreamSuiteProperties", obs_module_text("AitumStreamSuiteProperties"), properties_dock);
	filters_dock = new FiltersDock(main_window);
	obs_frontend_add_dock_by_id("AitumStreamSuiteFilters", obs_module_text("AitumStreamSuiteFilters"), filters_dock);
	live_scenes_dock = new LiveScenesDock(main_window);
	obs_frontend_add_dock_by_id("AitumStreamSuiteLiveScenes", obs_module_text("AitumStreamSuiteLiveScenes"), live_scenes_dock);
	auto capture_dock = new CaptureDock(main_window);
	obs_frontend_add_dock_by_id("AitumStreamSuiteCapture", obs_module_text("AitumStreamSuiteCapture"), capture_dock);

	std::string url = "https://api.aitum.tv/plugin/streamsuite";
	const char *pguid = config_get_string(obs_frontend_get_app_config(), "General", "InstallGUID");
	if (pguid) {
		url += "?uuid=";
		url += pguid;
	}

	version_download_info =
		download_info_create_single("[Aitum Stream Suite]", "OBS", url.c_str(), version_info_downloaded, nullptr);
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
	load_dock_state_timer.stop();
	if (!isFloating()) {
		auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		auto docks = main_window->findChildren<QDockWidget *>();
		QList<QString> current_docks;
		for (auto &dock : docks) {
			if (dock->isVisible())
				current_docks.append(dock->objectName());
		}
		if (current_docks == loaded_docks) {
			load_dock_state_timer.start();
		} else if (main_window->isVisible()) {
			auto index = modesTabBar->currentIndex();
			if (index >= 0) {
				auto d = modesTabBar->tabData(index);
				if (!d.isNull() && d.isValid() && !d.toString().isEmpty()) {
					save_dock_state(d.toString());
				} else {
					save_dock_state(modesTabBar->tabText(index));
				}
			}
			loaded_docks = current_docks;
		}
	}
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

static void save_load(obs_data_t *save_data, bool saving, void *private_data)
{
	UNUSED_PARAMETER(save_data);
	UNUSED_PARAMETER(private_data);
	if (saving)
		save_current_profile_config(!scene_collection_changing);
}

void load_obs_websocket();

void obs_module_post_load()
{
	load_dock_state_timer.setInterval(100);
	load_dock_state_timer.setSingleShot(true);
	QObject::connect(&load_dock_state_timer, &QTimer::timeout, []() {
		auto index = modesTabBar->currentIndex();
		if (index >= 0) {
			auto d = modesTabBar->tabData(index);
			if (!d.isNull() && d.isValid() && !d.toString().isEmpty()) {
				load_dock_state(d.toString());
			} else {
				load_dock_state(modesTabBar->tabText(index));
			}
		}
	});

	obs_frontend_add_save_callback(save_load, nullptr);

	load_obs_websocket();
}

void obs_module_unload()
{
	obs_frontend_remove_save_callback(save_load, nullptr);
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
	for (auto &it : empty_docks) {
		obs_frontend_remove_dock(it->parentWidget()->objectName().toUtf8().constData());
	}
	empty_docks.clear();

	DestroyPanelCookieManager();
	if (cef) {
		delete cef;
		cef = nullptr;
	}
}

const char *obs_module_name(void)
{
	return obs_module_text("AitumStreamSuite");
}

extern "C" void show_component_editor(const char *name);

void show_component_editor(const char *name)
{
	if (!component_dock)
		return;
	QMetaObject::invokeMethod(component_dock, "SwitchScene", Q_ARG(QString, QString::fromUtf8(name)));
	QMetaObject::invokeMethod(component_dock, [] {
		auto window = QApplication::activeWindow();
		if (window->objectName() == "OBSBasicProperties") {
			window->close();
		}
		component_dock->parentWidget()->show();
		component_dock->parentWidget()->raise();
		component_dock->parentWidget()->setFocus();
	});
}
