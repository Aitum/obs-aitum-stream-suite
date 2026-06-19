
#include "../docks/browser-dock.hpp"
#include "../docks/canvas-clone-dock.hpp"
#include "../docks/canvas-dock.hpp"
#include "../docks/live-scenes-dock.hpp"
#include "../docks/output-dock.hpp"
#include "../version.h"
#include "obs-websocket-api.h"
#include <atomic>
#include <list>
#include <string>
#include <QDockWidget>
#include <QMainWindow>
#include <QTabBar>
#include <QThread>
#include <utility>

obs_websocket_vendor vendor = nullptr;
extern std::list<CanvasDock *> canvas_docks;
extern std::list<CanvasCloneDock *> canvas_clone_docks;
extern OutputDock *output_dock;
extern LiveScenesDock *live_scenes_dock;
extern QTabBar *modesTabBar;
extern std::atomic_bool stream_suite_unloading;

template<typename Fn>
static bool invoke_on_ui_thread(Fn &&fn)
{
	if (stream_suite_unloading.load())
		return false;

	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window)
		return false;

	if (QThread::currentThread() == main_window->thread()) {
		fn();
		return true;
	}

	return QMetaObject::invokeMethod(
		main_window,
		[callback = std::forward<Fn>(fn)]() mutable {
			if (!stream_suite_unloading.load())
				callback();
		},
		Qt::BlockingQueuedConnection);
}

void vendor_request_version(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_set_string(response_data, "version", PROJECT_VERSION);
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_get_canvas(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_array_t *ca = nullptr;
	if (!invoke_on_ui_thread([&] {
		ca = obs_data_array_create();
		for (const auto &it : canvas_docks) {
			auto c = obs_data_create();
			obs_data_set_string(c, "type", "extra");
			obs_data_set_string(c, "name", obs_canvas_get_name(it->GetCanvas()));
			obs_data_set_string(c, "uuid", obs_canvas_get_uuid(it->GetCanvas()));
			obs_video_info ovi;
			if (obs_canvas_get_video_info(it->GetCanvas(), &ovi)) {
				obs_data_set_int(c, "width", ovi.base_width);
				obs_data_set_int(c, "height", ovi.base_height);
			}
			obs_data_array_push_back(ca, c);
			obs_data_release(c);
		}
		for (const auto &it : canvas_clone_docks) {
			auto c = obs_data_create();
			obs_data_set_string(c, "type", "clone");
			obs_data_set_string(c, "name", obs_canvas_get_name(it->GetCanvas()));
			obs_data_set_string(c, "uuid", obs_canvas_get_uuid(it->GetCanvas()));
			obs_video_info ovi;
			if (obs_canvas_get_video_info(it->GetCanvas(), &ovi)) {
				obs_data_set_int(c, "width", ovi.base_width);
				obs_data_set_int(c, "height", ovi.base_height);
			}
			obs_data_array_push_back(ca, c);
			obs_data_release(c);
		}
	}) || !ca) {
		obs_data_set_string(response_data, "error", "UI is not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
	obs_data_set_array(response_data, "canvas", ca);
	obs_data_array_release(ca);
}

void vendor_request_switch_scene(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *scene_name = obs_data_get_string(request_data, "scene");
	if (scene_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'scene' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const auto canvas_name = QString::fromUtf8(obs_data_get_string(request_data, "canvas"));
	const auto scene = QString::fromUtf8(scene_name);
	bool handled = false;
	if (!invoke_on_ui_thread([&] {
		handled = true;
		for (const auto &it : canvas_docks) {
			auto canvas = it->GetCanvas();
			if (canvas_name.isEmpty() || canvas_name == QString::fromUtf8(obs_canvas_get_name(canvas)) ||
			    canvas_name == QString::fromUtf8(obs_canvas_get_uuid(canvas)))
				QMetaObject::invokeMethod(it, "SwitchScene", Qt::DirectConnection, Q_ARG(QString, scene));
		}
	}) || !handled) {
		obs_data_set_string(response_data, "error", "UI is not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_current_scene(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const auto canvas_name = QString::fromUtf8(obs_data_get_string(request_data, "canvas"));
	if (canvas_name.isEmpty()) {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	bool found = false;
	std::string scene_name;
	std::string scene_uuid;
	if (!invoke_on_ui_thread([&] {
		for (const auto &it : canvas_docks) {
			auto canvas = it->GetCanvas();
			if (canvas_name != QString::fromUtf8(obs_canvas_get_name(canvas)) &&
			    canvas_name != QString::fromUtf8(obs_canvas_get_uuid(canvas)))
				continue;

			auto source = obs_canvas_get_channel(canvas, 0);
			if (source && obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION) {
				auto active_source = obs_transition_get_active_source(source);
				obs_source_release(source);
				source = active_source;
			}
			if (source) {
				scene_name = obs_source_get_name(source);
				scene_uuid = obs_source_get_uuid(source);
				obs_source_release(source);
			}
			found = true;
			return;
		}
	}) || !found) {
		obs_data_set_string(response_data, "error", "'canvas' not found");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_string(response_data, "scene", scene_name.c_str());
	obs_data_set_string(response_data, "scene_uuid", scene_uuid.c_str());
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_get_scenes(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const auto canvas_name = QString::fromUtf8(obs_data_get_string(request_data, "canvas"));
	if (canvas_name.isEmpty()) {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	obs_data_array_t *sa = nullptr;
	if (!invoke_on_ui_thread([&] {
		for (const auto &it : canvas_docks) {
			auto canvas = it->GetCanvas();
			if (canvas_name != QString::fromUtf8(obs_canvas_get_name(canvas)) &&
			    canvas_name != QString::fromUtf8(obs_canvas_get_uuid(canvas)))
				continue;

			sa = obs_data_array_create();
			obs_canvas_enum_scenes(
				canvas,
				[](void *param, obs_source_t *scene) {
					auto a = static_cast<obs_data_array_t *>(param);
					auto s = obs_data_create();
					obs_data_set_string(s, "name", obs_source_get_name(scene));
					obs_data_set_string(s, "uuid", obs_source_get_uuid(scene));
					obs_data_array_push_back(a, s);
					obs_data_release(s);
					return true;
				},
				sa);
			return;
		}
	}) || !sa) {
		obs_data_set_string(response_data, "error", "'canvas' not found");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
	obs_data_set_array(response_data, "scenes", sa);
	obs_data_array_release(sa);
}

void vendor_request_get_outputs(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_array_t *oa = nullptr;
	if (!invoke_on_ui_thread([&] {
		if (output_dock)
			oa = output_dock->GetOutputsArray();
	}) || !oa) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
	obs_data_set_array(response_data, "outputs", oa);
	obs_data_array_release(oa);
}

void vendor_request_start_output(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'output' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	std::string startName = "AitumStreamSuiteStartOutput";
	startName += output_name;

	struct find_hotkey {
		obs_hotkey_t *hotkey;
		const char *name;
	};
	find_hotkey t = {};
	t.name = startName.c_str();
	obs_enum_hotkeys(
		[](void *param, obs_hotkey_id id, obs_hotkey_t *key) {
			UNUSED_PARAMETER(id);
			const auto hp = (struct find_hotkey *)param;
			const auto hn = obs_hotkey_get_name(key);
			if (strcmp(hp->name, hn) == 0)
				hp->hotkey = key;
			return true;
		},
		&t);
	if (t.hotkey) {
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), true);
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), false);
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'output' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_stop_output(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'output' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	std::string stopName = "AitumStreamSuiteStopOutput";
	stopName += output_name;

	struct find_hotkey {
		obs_hotkey_t *hotkey;
		const char *name;
	};
	find_hotkey t = {};
	t.name = stopName.c_str();
	obs_enum_hotkeys(
		[](void *param, obs_hotkey_id id, obs_hotkey_t *key) {
			UNUSED_PARAMETER(id);
			const auto hp = (struct find_hotkey *)param;
			const auto hn = obs_hotkey_get_name(key);
			if (strcmp(hp->name, hn) == 0)
				hp->hotkey = key;
			return true;
		},
		&t);
	if (t.hotkey) {
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), true);
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), false);
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'output' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_start_all_outputs(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool started = false;
	if (!invoke_on_ui_thread([&] {
		if (output_dock) {
			QMetaObject::invokeMethod(output_dock, "StartAll", Qt::DirectConnection, Q_ARG(bool, false),
						  Q_ARG(bool, false));
			started = true;
		}
	}) || !started) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_stop_all_outputs(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool stopped = false;
	if (!invoke_on_ui_thread([&] {
		if (output_dock) {
			QMetaObject::invokeMethod(output_dock, "StopAll", Qt::DirectConnection, Q_ARG(bool, false),
						  Q_ARG(bool, false));
			stopped = true;
		}
	}) || !stopped) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_start_all_streams(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool started = false;
	if (!invoke_on_ui_thread([&] {
		if (output_dock) {
			QMetaObject::invokeMethod(output_dock, "StartAll", Qt::DirectConnection, Q_ARG(bool, true),
						  Q_ARG(bool, false));
			started = true;
		}
	}) || !started) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_stop_all_streams(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool stopped = false;
	if (!invoke_on_ui_thread([&] {
		if (output_dock) {
			QMetaObject::invokeMethod(output_dock, "StopAll", Qt::DirectConnection, Q_ARG(bool, true),
						  Q_ARG(bool, false));
			stopped = true;
		}
	}) || !stopped) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_start_all_recordings(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool started = false;
	if (!invoke_on_ui_thread([&] {
		if (output_dock) {
			QMetaObject::invokeMethod(output_dock, "StartAll", Qt::DirectConnection, Q_ARG(bool, false),
						  Q_ARG(bool, true));
			started = true;
		}
	}) || !started) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_stop_all_recordings(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	bool stopped = false;
	if (!invoke_on_ui_thread([&] {
		if (output_dock) {
			QMetaObject::invokeMethod(output_dock, "StopAll", Qt::DirectConnection, Q_ARG(bool, false),
						  Q_ARG(bool, true));
			stopped = true;
		}
	}) || !stopped) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_save_backtrack(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'output' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	std::string saveName = "AitumStreamSuiteSaveBacktrack";
	saveName += output_name;

	struct find_hotkey {
		obs_hotkey_t *hotkey;
		const char *name;
	};
	find_hotkey t = {};
	t.name = saveName.c_str();
	obs_enum_hotkeys(
		[](void *param, obs_hotkey_id id, obs_hotkey_t *key) {
			UNUSED_PARAMETER(id);
			const auto hp = (struct find_hotkey *)param;
			const auto hn = obs_hotkey_get_name(key);
			if (strcmp(hp->name, hn) == 0)
				hp->hotkey = key;
			return true;
		},
		&t);
	if (t.hotkey) {
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), true);
		obs_hotkey_trigger_routed_callback(obs_hotkey_get_id(t.hotkey), false);
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'output' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_add_chapter(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'output' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const auto output = QString::fromUtf8(output_name);
	const auto chapter_name = QString::fromUtf8(obs_data_get_string(request_data, "chapter_name"));
	bool result = false;
	if (!invoke_on_ui_thread([&] {
		if (output_dock)
			result = output_dock->AddChapterToOutput(output.toUtf8().constData(), chapter_name.toUtf8().constData());
	})) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	obs_data_set_bool(response_data, "success", result);
}

void vendor_request_get_dock_modes(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_array_t *modes = nullptr;
	std::string current;
	if (!invoke_on_ui_thread([&] {
		if (!modesTabBar)
			return;
		modes = obs_data_array_create();
		for (int i = 0; i < modesTabBar->count(); i++) {
			auto mode = obs_data_create();
			auto d = modesTabBar->tabData(i);
			if (!d.isNull() && d.isValid() && !d.toString().isEmpty()) {
				obs_data_set_string(mode, "name", d.toString().toUtf8().constData());
				obs_data_set_bool(mode, "fixed", true);
			} else {
				obs_data_set_string(mode, "name", modesTabBar->tabText(i).toUtf8().constData());
				obs_data_set_bool(mode, "fixed", false);
			}
			obs_data_array_push_back(modes, mode);
			obs_data_release(mode);
		}
		auto index = modesTabBar->currentIndex();
		if (index >= 0) {
			auto d = modesTabBar->tabData(index);
			current = (!d.isNull() && d.isValid() && !d.toString().isEmpty())
					  ? d.toString().toStdString()
					  : modesTabBar->tabText(index).toStdString();
		}
	}) || !modes) {
		obs_data_set_string(response_data, "error", "Modes tab bar not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_array(response_data, "modes", modes);
	obs_data_array_release(modes);
	if (!current.empty())
		obs_data_set_string(response_data, "current", current.c_str());
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_switch_dock_mode(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	auto item = obs_data_item_byname(request_data, "mode");
	if (!item) {
		obs_data_set_string(response_data, "error", "'mode' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	if (obs_data_item_gettype(item) == OBS_DATA_NUMBER) {
		auto mode = obs_data_item_get_int(item);
		bool switched = false;
		if (invoke_on_ui_thread([&] {
			if (modesTabBar && mode >= 0 && mode < modesTabBar->count()) {
				modesTabBar->setCurrentIndex(static_cast<int>(mode));
				switched = true;
			}
		}) && switched) {
			obs_data_item_release(&item);
			obs_data_set_bool(response_data, "success", true);
			return;
		}
	} else if (obs_data_item_gettype(item) == OBS_DATA_STRING) {
		auto mode = QString::fromUtf8(obs_data_item_get_string(item));
		bool switched = false;
		if (invoke_on_ui_thread([&] {
			if (!modesTabBar)
				return;
			for (int i = 0; i < modesTabBar->count(); i++) {
				auto d = modesTabBar->tabData(i);
				if ((!d.isNull() && d.isValid() && d.toString() == mode) || modesTabBar->tabText(i) == mode) {
					modesTabBar->setCurrentIndex(i);
					switched = true;
					return;
				}
			}
		}) && switched) {
			obs_data_item_release(&item);
			obs_data_set_bool(response_data, "success", true);
			return;
		}
	}
	obs_data_set_string(response_data, "error", "'mode' invalid");
	obs_data_set_bool(response_data, "success", false);
	obs_data_item_release(&item);
}

void vendor_request_get_docks(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	auto da = obs_data_array_create();

	obs_queue_task(
		OBS_TASK_UI,
		[](void *param) {
			auto da = static_cast<obs_data_array_t *>(param);
			auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
			auto docks = main_window->findChildren<QDockWidget *>();
			for (auto &dock : docks) {
				auto di = obs_data_create();
				obs_data_set_string(di, "name", dock->objectName().toUtf8().constData());
				obs_data_set_string(di, "title", dock->windowTitle().toUtf8().constData());
				obs_data_set_bool(di, "visible", dock->isVisible());
				obs_data_set_bool(di, "floating", dock->isFloating());
				obs_data_array_push_back(da, di);
				obs_data_release(di);
			}
		},
		da, true);
	obs_data_set_array(response_data, "docks", da);
	obs_data_array_release(da);
}

void vendor_request_dock_show(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *dock_name = obs_data_get_string(request_data, "dock");
	if (dock_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'dock' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto dn = QString::fromUtf8(dock_name);
	bool shown = false;
	if (invoke_on_ui_thread([&] {
		auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		for (auto dock : main_window->findChildren<QDockWidget *>()) {
			if (dock->objectName() == dn) {
				dock->show();
				shown = true;
				return;
			}
		}
	}) && shown) {
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'dock' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_dock_hide(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *dock_name = obs_data_get_string(request_data, "dock");
	if (dock_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'dock' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto dn = QString::fromUtf8(dock_name);
	bool hidden = false;
	if (invoke_on_ui_thread([&] {
		auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		for (auto dock : main_window->findChildren<QDockWidget *>()) {
			if (dock->objectName() == dn) {
				dock->hide();
				hidden = true;
				return;
			}
		}
	}) && hidden) {
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'dock' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_get_live_scenes(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_array_t *sa = nullptr;
	if (!invoke_on_ui_thread([&] {
		if (live_scenes_dock)
			sa = live_scenes_dock->GetLiveScenesArray();
	}) || !sa) {
		obs_data_set_string(response_data, "error", "Live Scenes dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", true);
	obs_data_set_array(response_data, "live_scenes", sa);
	obs_data_array_release(sa);
}

void vendor_request_live_scenes_add(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *scene_name = obs_data_get_string(request_data, "scene");
	if (scene_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'scene' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto sn = QString::fromUtf8(scene_name);
	bool added = false;
	if (!invoke_on_ui_thread([&] {
		if (live_scenes_dock)
			added = live_scenes_dock->AddLiveScene(sn);
	})) {
		obs_data_set_string(response_data, "error", "Live Scenes dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", added);
}

void vendor_request_live_scenes_remove(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *scene_name = obs_data_get_string(request_data, "scene");
	if (scene_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'scene' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto sn = QString::fromUtf8(scene_name);
	bool removed = false;
	if (!invoke_on_ui_thread([&] {
		if (live_scenes_dock)
			removed = live_scenes_dock->RemoveLiveScene(sn);
	})) {
		obs_data_set_string(response_data, "error", "Live Scenes dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_set_bool(response_data, "success", removed);
}

void vendor_request_dock_show_panel(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const auto canvas_name = QString::fromUtf8(obs_data_get_string(request_data, "canvas"));
	if (canvas_name.isEmpty()) {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const auto panel_name = QString::fromUtf8(obs_data_get_string(request_data, "panel"));
	if (panel_name.isEmpty()) {
		obs_data_set_string(response_data, "error", "'panel' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	bool found = false;
	if (invoke_on_ui_thread([&] {
		for (const auto &it : canvas_docks) {
			auto canvas = it->GetCanvas();
			if (canvas_name == QString::fromUtf8(obs_canvas_get_name(canvas)) ||
			    canvas_name == QString::fromUtf8(obs_canvas_get_uuid(canvas))) {
				it->SetPanelVisible(panel_name, true);
				found = true;
				return;
			}
		}
		for (const auto &it : canvas_clone_docks) {
			auto canvas = it->GetCanvas();
			if (canvas_name == QString::fromUtf8(obs_canvas_get_name(canvas)) ||
			    canvas_name == QString::fromUtf8(obs_canvas_get_uuid(canvas))) {
				it->SetPanelVisible(panel_name, true);
				found = true;
				return;
			}
		}
	}) && found) {
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_dock_hide_panel(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const auto canvas_name = QString::fromUtf8(obs_data_get_string(request_data, "canvas"));
	if (canvas_name.isEmpty()) {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const auto panel_name = QString::fromUtf8(obs_data_get_string(request_data, "panel"));
	if (panel_name.isEmpty()) {
		obs_data_set_string(response_data, "error", "'panel' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	bool found = false;
	if (invoke_on_ui_thread([&] {
		for (const auto &it : canvas_docks) {
			auto canvas = it->GetCanvas();
			if (canvas_name == QString::fromUtf8(obs_canvas_get_name(canvas)) ||
			    canvas_name == QString::fromUtf8(obs_canvas_get_uuid(canvas))) {
				it->SetPanelVisible(panel_name, false);
				found = true;
				return;
			}
		}
		for (const auto &it : canvas_clone_docks) {
			auto canvas = it->GetCanvas();
			if (canvas_name == QString::fromUtf8(obs_canvas_get_name(canvas)) ||
			    canvas_name == QString::fromUtf8(obs_canvas_get_uuid(canvas))) {
				it->SetPanelVisible(panel_name, false);
				found = true;
				return;
			}
		}
	}) && found) {
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_get_transitions(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	// This handler runs on the obs-websocket worker thread. Both iterating the global
	// canvas_docks list (mutated on the UI thread) and calling CanvasDock::GetTransitions()
	// (UI-thread-owned state) are data races, so marshal the whole lookup + array build onto
	// the UI thread and block until done. By-ref captures are safe under the blocking
	// connection; the obs_data array is created and populated on the UI thread and returned to
	// the worker, which attaches it to response_data and releases it as before.
	bool found = false;
	obs_data_array_t *ta = nullptr;
	auto build = [&] {
		for (const auto &it : canvas_docks) {
			auto canvas = it->GetCanvas();
			if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
			    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
				continue;

			ta = obs_data_array_create();
			auto transitions = it->GetTransitions();
			for (const auto &t : transitions) {
				auto tr = obs_data_create();
				obs_data_set_string(tr, "name", obs_source_get_name(t));
				obs_data_set_string(tr, "uuid", obs_source_get_uuid(t));
				obs_data_set_string(tr, "type", obs_source_get_id(t));
				auto settings = obs_source_get_settings(t);
				if (settings) {
					obs_data_set_obj(tr, "settings", settings);
					obs_data_release(settings);
				}
				obs_data_array_push_back(ta, tr);
				obs_data_release(tr);
			}
			found = true;
			return;
		}
	};
	invoke_on_ui_thread(build);
	if (found) {
		obs_data_set_bool(response_data, "success", true);
		obs_data_set_array(response_data, "transitions", ta);
		obs_data_array_release(ta);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_switch_transition(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *transition_name = obs_data_get_string(request_data, "transition");
	if (transition_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'transition' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	// Runs on the worker thread: iterating canvas_docks and calling SetSelectedTransition()
	// (UI-thread state) are both races. Marshal the lookup + mutation onto the UI thread.
	// We use a blocking connection so we can report whether the canvas was found; by-ref
	// captures are valid because the worker blocks until the lambda returns.
	bool found = false;
	auto tn = QString::fromUtf8(transition_name);
	invoke_on_ui_thread(
		[&] {
			for (const auto &it : canvas_docks) {
				auto canvas = it->GetCanvas();
				if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
				    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
					continue;
				it->SetSelectedTransition(tn);
				found = true;
				return;
			}
		});
	if (found) {
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_transitions_add(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *transition_name = obs_data_get_string(request_data, "name");
	if (transition_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'name' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *transition_type = obs_data_get_string(request_data, "type");
	if (transition_type[0] == '\0') {
		obs_data_set_string(response_data, "error", "'type' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	obs_data_t *settings = obs_data_get_obj(request_data, "settings");
	// Runs on the worker thread: iterating canvas_docks and calling AddTransition() (UI-thread
	// state) are races, so marshal onto the UI thread with a blocking connection. `settings`
	// stays owned/alive on the worker (released below); the blocking connection guarantees the
	// lambda finishes using it before we release. By-ref captures are safe for the same reason.
	bool found = false;
	invoke_on_ui_thread(
		[&] {
			for (const auto &it : canvas_docks) {
				auto canvas = it->GetCanvas();
				if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
				    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
					continue;
				it->AddTransition(transition_type, transition_name, settings);
				found = true;
				return;
			}
		});
	obs_data_release(settings);
	if (found) {
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_transitions_remove(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *transition_name = obs_data_get_string(request_data, "name");
	if (transition_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'name' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	// Runs on the worker thread: iterating canvas_docks and calling RemoveTransition()
	// (UI-thread state) are races, so marshal the lookup + mutation onto the UI thread with a
	// blocking connection. By-ref captures are valid because the worker blocks until done.
	bool found = false;
	invoke_on_ui_thread(
		[&] {
			for (const auto &it : canvas_docks) {
				auto canvas = it->GetCanvas();
				if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
				    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
					continue;
				it->RemoveTransition(transition_name);
				found = true;
				return;
			}
		});
	if (found) {
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_refresh_browser_panel(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	auto panel_name = obs_data_get_string(request_data, "panel");
	if (panel_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'panel' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto pn = QString::fromUtf8(panel_name);
	bool refreshed = false;
	if (invoke_on_ui_thread([&] {
		auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		for (auto bd : main_window->findChildren<BrowserDock *>()) {
			if (bd->objectName() == pn) {
				bd->Refresh();
				refreshed = true;
				return;
			}
		}
	}) && refreshed) {
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'panel' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_reset_browser_panel(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	auto panel_name = obs_data_get_string(request_data, "panel");
	if (panel_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'panel' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto pn = QString::fromUtf8(panel_name);
	bool reset = false;
	if (invoke_on_ui_thread([&] {
		auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		for (auto bd : main_window->findChildren<BrowserDock *>()) {
			if (bd->objectName() == pn) {
				bd->Reset();
				reset = true;
				return;
			}
		}
	}) && reset) {
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_string(response_data, "error", "'panel' not found");
	obs_data_set_bool(response_data, "success", false);
}

void load_obs_websocket()
{
	vendor = obs_websocket_register_vendor("aitum-stream-suite");

	obs_websocket_vendor_register_request(vendor, "version", vendor_request_version, nullptr);
	obs_websocket_vendor_register_request(vendor, "get_canvas", vendor_request_get_canvas, nullptr);
	obs_websocket_vendor_register_request(vendor, "switch_scene", vendor_request_switch_scene, nullptr);
	obs_websocket_vendor_register_request(vendor, "current_scene", vendor_request_current_scene, nullptr);
	obs_websocket_vendor_register_request(vendor, "get_scenes", vendor_request_get_scenes, nullptr);

	obs_websocket_vendor_register_request(vendor, "get_outputs", vendor_request_get_outputs, nullptr);
	obs_websocket_vendor_register_request(vendor, "start_output", vendor_request_start_output, nullptr);
	obs_websocket_vendor_register_request(vendor, "stop_output", vendor_request_stop_output, nullptr);
	obs_websocket_vendor_register_request(vendor, "start_all_outputs", vendor_request_start_all_outputs, nullptr);
	obs_websocket_vendor_register_request(vendor, "stop_all_outputs", vendor_request_stop_all_outputs, nullptr);
	obs_websocket_vendor_register_request(vendor, "start_all_streams", vendor_request_start_all_streams, nullptr);
	obs_websocket_vendor_register_request(vendor, "stop_all_streams", vendor_request_stop_all_streams, nullptr);
	obs_websocket_vendor_register_request(vendor, "start_all_recordings", vendor_request_start_all_recordings, nullptr);
	obs_websocket_vendor_register_request(vendor, "stop_all_recordings", vendor_request_stop_all_recordings, nullptr);
	obs_websocket_vendor_register_request(vendor, "save_backtrack", vendor_request_save_backtrack, nullptr);

	obs_websocket_vendor_register_request(vendor, "add_chapter", vendor_request_add_chapter, nullptr);

	obs_websocket_vendor_register_request(vendor, "get_dock_modes", vendor_request_get_dock_modes, nullptr);
	obs_websocket_vendor_register_request(vendor, "switch_dock_mode", vendor_request_switch_dock_mode, nullptr);
	obs_websocket_vendor_register_request(vendor, "get_docks", vendor_request_get_docks, nullptr);
	obs_websocket_vendor_register_request(vendor, "dock_show", vendor_request_dock_show, nullptr);
	obs_websocket_vendor_register_request(vendor, "dock_hide", vendor_request_dock_hide, nullptr);

	obs_websocket_vendor_register_request(vendor, "get_live_scenes", vendor_request_get_live_scenes, nullptr);
	obs_websocket_vendor_register_request(vendor, "live_scenes_add", vendor_request_live_scenes_add, nullptr);
	obs_websocket_vendor_register_request(vendor, "live_scenes_remove", vendor_request_live_scenes_remove, nullptr);

	obs_websocket_vendor_register_request(vendor, "canvas_dock_show_panel", vendor_request_dock_show_panel, nullptr);
	obs_websocket_vendor_register_request(vendor, "canvas_dock_hide_panel", vendor_request_dock_hide_panel, nullptr);

	obs_websocket_vendor_register_request(vendor, "get_transitions", vendor_request_get_transitions, nullptr);
	obs_websocket_vendor_register_request(vendor, "switch_transition", vendor_request_switch_transition, nullptr);
	obs_websocket_vendor_register_request(vendor, "transitions_add", vendor_request_transitions_add, nullptr);
	obs_websocket_vendor_register_request(vendor, "transitions_remove", vendor_request_transitions_remove, nullptr);

	obs_websocket_vendor_register_request(vendor, "refresh_browser_panel", vendor_request_refresh_browser_panel, nullptr);
	obs_websocket_vendor_register_request(vendor, "reset_browser_panel", vendor_request_reset_browser_panel, nullptr);
}

void unload_obs_websocket()
{
	if (!vendor)
		return;

	const char *requests[] = {
		"version", "get_canvas", "switch_scene", "current_scene", "get_scenes",
		"get_outputs", "start_output", "stop_output", "start_all_outputs", "stop_all_outputs",
		"start_all_streams", "stop_all_streams", "start_all_recordings", "stop_all_recordings",
		"save_backtrack", "add_chapter", "get_dock_modes", "switch_dock_mode", "get_docks",
		"dock_show", "dock_hide", "get_live_scenes", "live_scenes_add", "live_scenes_remove",
		"canvas_dock_show_panel", "canvas_dock_hide_panel", "get_transitions", "switch_transition",
		"transitions_add", "transitions_remove", "refresh_browser_panel", "reset_browser_panel",
	};
	for (const auto *request : requests)
		obs_websocket_vendor_unregister_request(vendor, request);
	vendor = nullptr;
}
