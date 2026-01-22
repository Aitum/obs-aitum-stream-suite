
#include "../docks/browser-dock.hpp"
#include "../docks/canvas-clone-dock.hpp"
#include "../docks/canvas-dock.hpp"
#include "../docks/live-scenes-dock.hpp"
#include "../docks/output-dock.hpp"
#include "../version.h"
#include "obs-websocket-api.h"
#include <list>
#include <QDockWidget>
#include <QMainWindow>
#include <QTabBar>

obs_websocket_vendor vendor = nullptr;
extern std::list<CanvasDock *> canvas_docks;
extern std::list<CanvasCloneDock *> canvas_clone_docks;
extern OutputDock *output_dock;
extern LiveScenesDock *live_scenes_dock;
extern QTabBar *modesTabBar;

void vendor_request_version(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_set_string(response_data, "version", PROJECT_VERSION);
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_get_canvas(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	auto ca = obs_data_array_create();
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
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	for (const auto &it : canvas_docks) {
		if (canvas_name[0] == '\0' || strcmp(obs_canvas_get_name(it->GetCanvas()), canvas_name) == 0 ||
		    strcmp(obs_canvas_get_uuid(it->GetCanvas()), canvas_name) == 0)
			QMetaObject::invokeMethod(it, "SwitchScene", Q_ARG(QString, QString::fromUtf8(scene_name)));
	}

	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_current_scene(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;

		auto source = obs_canvas_get_channel(canvas, 0);
		if (source && obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION) {
			obs_source_release(source);
			source = obs_transition_get_active_source(source);
		}
		if (source) {
			obs_data_set_string(response_data, "scene", obs_source_get_name(source));
			obs_data_set_string(response_data, "scene_uuid", obs_source_get_uuid(source));
			obs_source_release(source);
		} else {
			obs_data_set_string(response_data, "scene", "");
			obs_data_set_string(response_data, "scene_uuid", "");
		}
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_get_scenes(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;

		auto sa = obs_data_array_create();
		obs_canvas_enum_scenes(
			canvas,
			[](void *param, obs_source_t *scene) {
				auto a = (obs_data_array_t *)param;
				auto s = obs_data_create();
				obs_data_set_string(s, "name", obs_source_get_name(scene));
				obs_data_set_string(s, "uuid", obs_source_get_uuid(scene));
				obs_data_array_push_back(a, s);
				obs_data_release(s);
				return true;
			},
			sa);
		obs_data_set_bool(response_data, "success", true);
		obs_data_set_array(response_data, "scenes", sa);
		obs_data_array_release(sa);
		return;
	}
	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_get_outputs(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto oa = output_dock->GetOutputsArray();
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
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StartAll", Q_ARG(bool, false), Q_ARG(bool, false));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_stop_all_outputs(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StopAll");
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_start_all_streams(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StartAll", Q_ARG(bool, true), Q_ARG(bool, false));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_start_all_recordings(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StartAll", Q_ARG(bool, false), Q_ARG(bool, true));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_add_chapter(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'output' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	bool result = output_dock->AddChapterToOutput(output_name, obs_data_get_string(request_data, "chapter_name"));

	obs_data_set_bool(response_data, "success", result);
}

void vendor_request_get_dock_modes(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!modesTabBar) {
		obs_data_set_string(response_data, "error", "Modes tab bar not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto modes = obs_data_array_create();
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
	obs_data_set_array(response_data, "modes", modes);
	obs_data_array_release(modes);
	auto index = modesTabBar->currentIndex();
	if (index >= 0) {
		auto d = modesTabBar->tabData(index);
		if (!d.isNull() && d.isValid() && !d.toString().isEmpty()) {
			obs_data_set_string(response_data, "current", d.toString().toUtf8().constData());
		} else {
			obs_data_set_string(response_data, "current", modesTabBar->tabText(index).toUtf8().constData());
		}
	}
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_switch_dock_mode(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	if (!modesTabBar) {
		obs_data_set_string(response_data, "error", "Modes tab bar not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto item = obs_data_item_byname(request_data, "mode");
	if (!item) {
		obs_data_set_string(response_data, "error", "'mode' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	if (obs_data_item_gettype(item) == OBS_DATA_NUMBER) {
		auto mode = obs_data_item_get_int(item);
		if (mode >= 0 && mode < modesTabBar->count()) {
			QMetaObject::invokeMethod(modesTabBar, "setCurrentIndex", Q_ARG(int, (int)mode));
			obs_data_item_release(&item);
			obs_data_set_bool(response_data, "success", true);
			return;
		}
	} else if (obs_data_item_gettype(item) == OBS_DATA_STRING) {
		auto mode = QString::fromUtf8(obs_data_item_get_string(item));
		for (int i = 0; i < modesTabBar->count(); i++) {
			auto d = modesTabBar->tabData(i);
			if (!d.isNull() && d.isValid() && d.toString() == mode) {
				if (modesTabBar->currentIndex() != i) {
					QMetaObject::invokeMethod(modesTabBar, "setCurrentIndex", Q_ARG(int, i));
				}
				obs_data_item_release(&item);
				obs_data_set_bool(response_data, "success", true);
				return;
			} else if (modesTabBar->tabText(i) == mode) {
				if (modesTabBar->currentIndex() != i) {
					QMetaObject::invokeMethod(modesTabBar, "setCurrentIndex", Q_ARG(int, i));
				}
				obs_data_item_release(&item);
				obs_data_set_bool(response_data, "success", true);
				return;
			}
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
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto docks = main_window->findChildren<QDockWidget *>();
	for (auto &dock : docks) {
		auto di = obs_data_create();
		obs_data_set_string(di, "name", dock->objectName().toUtf8().constData());
		obs_data_set_bool(di, "visible", dock->isVisible());
		obs_data_set_bool(di, "floating", dock->isFloating());
		obs_data_array_push_back(da, di);
		obs_data_release(di);
	}
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
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto docks = main_window->findChildren<QDockWidget *>();
	for (auto &dock : docks) {
		if (dock->objectName() == dn) {
			QMetaObject::invokeMethod(dock, "show");
			obs_data_set_bool(response_data, "success", true);
			return;
		}
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
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto docks = main_window->findChildren<QDockWidget *>();
	for (auto &dock : docks) {
		if (dock->objectName() == dn) {
			QMetaObject::invokeMethod(dock, "hide");
			obs_data_set_bool(response_data, "success", true);
			return;
		}
	}
	obs_data_set_string(response_data, "error", "'dock' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_get_live_scenes(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!live_scenes_dock) {
		obs_data_set_string(response_data, "error", "Live Scenes dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto sa = live_scenes_dock->GetLiveScenesArray();
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
	if (!live_scenes_dock) {
		obs_data_set_string(response_data, "error", "Live Scenes dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto sn = QString::fromUtf8(scene_name);
	QMetaObject::invokeMethod(live_scenes_dock, [sn] { live_scenes_dock->AddLiveScene(sn); });
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_live_scenes_remove(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *scene_name = obs_data_get_string(request_data, "scene");
	if (scene_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'scene' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	if (!live_scenes_dock) {
		obs_data_set_string(response_data, "error", "Live Scenes dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto sn = QString::fromUtf8(scene_name);
	QMetaObject::invokeMethod(live_scenes_dock, [sn] { live_scenes_dock->RemoveLiveScene(sn); });
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_dock_show_panel(obs_data_t *request_data, obs_data_t *response_data, void *)
{

	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *panel_name = obs_data_get_string(request_data, "panel");
	if (panel_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'panel' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, true); });
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	for (const auto &it : canvas_clone_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, true); });
		obs_data_set_bool(response_data, "success", true);
		return;
	}

	obs_data_set_string(response_data, "error", "'canvas' not found");
	obs_data_set_bool(response_data, "success", false);
}

void vendor_request_dock_hide_panel(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	const char *canvas_name = obs_data_get_string(request_data, "canvas");
	if (canvas_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'canvas' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	const char *panel_name = obs_data_get_string(request_data, "panel");
	if (panel_name[0] == '\0') {
		obs_data_set_string(response_data, "error", "'panel' not set");
		obs_data_set_bool(response_data, "success", false);
		return;
	}

	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, false); });
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	for (const auto &it : canvas_clone_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, false); });
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

	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;

		auto ta = obs_data_array_create();
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
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;

		it->SetSelectedTransition(QString::fromUtf8(transition_name));

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
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;
		it->AddTransition(transition_type, transition_name, settings);
		obs_data_set_bool(response_data, "success", true);
		obs_data_release(settings);
		return;
	}
	obs_data_release(settings);
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
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 && strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0)
			continue;
		it->RemoveTransition(transition_name);
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
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto bds = main_window->findChildren<BrowserDock *>();
	for (auto bd : bds) {
		if (bd->objectName() == pn) {
			bd->Refresh();
			obs_data_set_bool(response_data, "success", true);
			return;
		}
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
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto bds = main_window->findChildren<BrowserDock *>();
	for (auto bd : bds) {
		if (bd->objectName() == pn) {
			bd->Reset();
			obs_data_set_bool(response_data, "success", true);
			return;
		}
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
	obs_websocket_vendor_register_request(vendor, "start_all_recordings", vendor_request_start_all_recordings, nullptr);

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
