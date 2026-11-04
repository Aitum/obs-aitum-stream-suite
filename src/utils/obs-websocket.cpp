
#include "../docks/browser-dock.hpp"
#include "../docks/canvas-clone-dock.hpp"
#include "../docks/canvas-dock.hpp"
#include "../docks/live-scenes-dock.hpp"
#include "../docks/output-dock.hpp"
#include "../version.h"
#include "obs-websocket-api.h"
#include "obs-websocket.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <list>
#include <memory>
#include <mutex>
#include <QCoreApplication>
#include <QDockWidget>
#include <QMainWindow>
#include <QTabBar>
#include <QThread>
#include <util/platform.h>

obs_websocket_vendor vendor = nullptr;
extern std::list<CanvasDock *> canvas_docks;
extern std::list<CanvasCloneDock *> canvas_clone_docks;
extern OutputDock *output_dock;
extern LiveScenesDock *live_scenes_dock;
extern QTabBar *modesTabBar;

// obs-websocket calls vendor requests on its own worker threads, but nearly all
// of them read or change Qt objects, the dock lists and the outputs those own,
// which are only safe on the UI thread. Rather than each handler arranging that
// for itself, register_ui_request() runs the whole handler there.
//
// The worker waits for the answer, with a timeout so that a stalled UI thread
// costs a failed request and never a hung one. A handler that has not started
// by then is cancelled, because response_data dies when the worker returns. One
// already running holds the mutex, so the worker cannot leave until it is done.
// Handlers must therefore not open dialogs directly; see start_all_outputs.

namespace {

struct UiCall {
	std::mutex mutex;
	std::condition_variable cv;
	bool finished = false;
	bool cancelled = false;
};

const std::chrono::milliseconds ui_call_timeout(2000);

template<typename Fn> bool run_on_ui(Fn fn)
{
	// Another plugin can call a vendor request from the UI thread itself, which
	// would otherwise wait out the timeout on an event loop it is blocking.
	if (QThread::currentThread() == QCoreApplication::instance()->thread()) {
		fn();
		return true;
	}

	auto call = std::make_shared<UiCall>();
	QMetaObject::invokeMethod(
		QCoreApplication::instance(),
		[call, fn] {
			std::lock_guard<std::mutex> lock(call->mutex);
			if (call->cancelled) {
				return;
			}
			fn();
			call->finished = true;
			call->cv.notify_all();
		},
		Qt::QueuedConnection);

	std::unique_lock<std::mutex> lock(call->mutex);
	if (!call->cv.wait_for(lock, ui_call_timeout, [&call] { return call->finished; })) {
		call->cancelled = true;
		return false;
	}
	return true;
}

void vendor_fail(obs_data_t *response_data, const char *error, const char *error_code)
{
	obs_data_set_string(response_data, "error", error);
	obs_data_set_string(response_data, "error_code", error_code);
	obs_data_set_bool(response_data, "success", false);
}

void ui_request(obs_data_t *request_data, obs_data_t *response_data, void *handler)
{
	auto fn = reinterpret_cast<obs_websocket_request_callback_function>(handler);
	if (!run_on_ui([fn, request_data, response_data] { fn(request_data, response_data, nullptr); })) {
		vendor_fail(response_data, "UI thread did not respond", "dock_unavailable");
		vendor_log(LOG_WARNING, "request", nullptr, "request timed out waiting for the UI thread");
	}
}

void register_ui_request(const char *request_type, obs_websocket_request_callback_function handler)
{
	obs_websocket_vendor_register_request(vendor, request_type, ui_request, reinterpret_cast<void *>(handler));
}

// Read from worker threads as well as the UI thread, hence the atomics.
std::atomic<bool> diag_log_events{true};
std::atomic<bool> diag_stats_events{true};
std::atomic<int> diag_stats_interval_ms{2000};
std::atomic<int> diag_min_level{LOG_DEBUG};

const char *diag_level_name(int level)
{
	switch (level) {
	case LOG_ERROR:
		return "error";
	case LOG_WARNING:
		return "warning";
	case LOG_INFO:
		return "info";
	default:
		return "debug";
	}
}

// Returns -1 when the name is not a level.
int diag_level_from_name(const char *name)
{
	for (int level : {LOG_ERROR, LOG_WARNING, LOG_INFO, LOG_DEBUG}) {
		if (strcmp(name, diag_level_name(level)) == 0) {
			return level;
		}
	}
	return -1;
}

void vendor_fill_logging(obs_data_t *response_data)
{
	obs_data_set_bool(response_data, "log_events", diag_log_events.load());
	obs_data_set_bool(response_data, "stats_events", diag_stats_events.load());
	obs_data_set_int(response_data, "stats_interval_ms", diag_stats_interval_ms.load());
	obs_data_set_string(response_data, "level", diag_level_name(diag_min_level.load()));
}

// Accepts a canvas name or uuid and returns its name, or "" if there is no such canvas.
std::string resolve_canvas_name(const char *canvas_name)
{
	std::string result;
	auto main_canvas = obs_get_main_canvas();
	if (strcmp(obs_canvas_get_name(main_canvas), canvas_name) == 0 ||
	    strcmp(obs_canvas_get_uuid(main_canvas), canvas_name) == 0) {
		result = obs_canvas_get_name(main_canvas);
	}
	obs_canvas_release(main_canvas);
	if (!result.empty()) {
		return result;
	}
	auto canvas = obs_get_canvas_by_name(canvas_name);
	if (!canvas) {
		canvas = obs_get_canvas_by_uuid(canvas_name);
	}
	if (canvas) {
		result = obs_canvas_get_name(canvas);
		obs_canvas_release(canvas);
	}
	return result;
}

const char *output_error_text(const char *error_code)
{
	if (strcmp(error_code, "already_starting_or_active") == 0) {
		return "Output is already starting or active";
	}
	if (strcmp(error_code, "output_not_running") == 0) {
		return "Output is not running";
	}
	return "Output failed to start";
}

} // namespace

void vendor_log(int level, const char *where, obs_data_t *extra, const char *fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	blog(level, "[Aitum Stream Suite] (%s) %s", where, buf);

	// obs log levels get larger as they get less severe.
	if (!vendor || !diag_log_events.load(std::memory_order_relaxed) ||
	    level > diag_min_level.load(std::memory_order_relaxed)) {
		return;
	}
	auto event = obs_data_create();
	if (extra) {
		obs_data_apply(event, extra);
	}
	obs_data_set_string(event, "level", diag_level_name(level));
	obs_data_set_string(event, "where", where);
	obs_data_set_string(event, "message", buf);
	obs_data_set_int(event, "ts_ms", (long long)(os_gettime_ns() / 1000000ULL));
	obs_websocket_vendor_emit_event(vendor, "log", event);
	obs_data_release(event);
}

void vendor_emit_output_state(const char *name, bool active, const char *last_error, long long stop_code)
{
	const bool has_error = last_error && last_error[0] != '\0';
	vendor_log(has_error ? LOG_WARNING : LOG_INFO, "output", nullptr, "output '%s' is now %s%s%s (stop code %lld)", name,
		   active ? "active" : "stopped", has_error ? ", last error: " : "", has_error ? last_error : "", stop_code);
	if (!vendor) {
		return;
	}
	auto event = obs_data_create();
	obs_data_set_string(event, "name", name);
	obs_data_set_bool(event, "active", active);
	if (has_error) {
		obs_data_set_string(event, "last_error", last_error);
	}
	if (!active) {
		obs_data_set_int(event, "stop_code", stop_code);
	}
	obs_websocket_vendor_emit_event(vendor, "output_state_changed", event);
	obs_data_release(event);
}

void vendor_fill_output_stats(obs_data_t *d, const char *name, obs_output_t *output, VendorStatsSamples *samples)
{
	obs_data_set_string(d, "name", name);
	obs_data_set_bool(d, "active", obs_output_active(output));
	obs_data_set_bool(d, "reconnecting", obs_output_reconnecting(output));
	obs_data_set_double(d, "congestion", (double)obs_output_get_congestion(output));
	obs_data_set_int(d, "connect_time_ms", obs_output_get_connect_time_ms(output));

	const int total = obs_output_get_total_frames(output);
	const int dropped = obs_output_get_frames_dropped(output);
	obs_data_set_int(d, "total_frames", total);
	obs_data_set_int(d, "dropped_frames", dropped);
	obs_data_set_double(d, "dropped_percent", total > 0 ? (double)dropped * 100.0 / (double)total : 0.0);

	const uint64_t bytes = obs_output_get_total_bytes(output);
	obs_data_set_int(d, "total_bytes", (long long)bytes);

	double kbps = 0.0;
	if (samples) {
		const uint64_t now = os_gettime_ns();
		auto prev = samples->find(name);
		if (prev != samples->end() && now > prev->second.second && bytes >= prev->second.first) {
			const double seconds = (double)(now - prev->second.second) / 1000000000.0;
			kbps = (double)(bytes - prev->second.first) * 8.0 / 1000.0 / seconds;
		}
		(*samples)[name] = {bytes, now};
	}
	obs_data_set_double(d, "bitrate_kbps", kbps);
}

bool vendor_stats_due()
{
	static uint64_t last_emit_ns = 0;
	if (!vendor || !diag_stats_events.load()) {
		return false;
	}
	const uint64_t now = os_gettime_ns();
	const uint64_t interval_ns = (uint64_t)diag_stats_interval_ms.load() * 1000000ULL;
	if (last_emit_ns && now - last_emit_ns < interval_ns) {
		return false;
	}
	last_emit_ns = now;
	return true;
}

void vendor_emit_output_stats(obs_data_t *stats)
{
	if (vendor) {
		obs_websocket_vendor_emit_event(vendor, "output_stats", stats);
	}
}

void vendor_request_version(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_set_string(response_data, "version", PROJECT_VERSION);
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_status(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	obs_data_set_string(response_data, "version", PROJECT_VERSION);
	obs_data_set_int(response_data, "api", 1);
	obs_data_set_bool(response_data, "main_active", obs_frontend_streaming_active());
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_set_logging(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	// Validated first so a rejected request changes nothing.
	int level = diag_min_level.load();
	if (obs_data_has_user_value(request_data, "level")) {
		level = diag_level_from_name(obs_data_get_string(request_data, "level"));
		if (level < 0) {
			vendor_fail(response_data, "'level' invalid", "invalid_level");
			return;
		}
	}
	diag_min_level.store(level);
	if (obs_data_has_user_value(request_data, "log_events")) {
		diag_log_events.store(obs_data_get_bool(request_data, "log_events"));
	}
	if (obs_data_has_user_value(request_data, "stats_events")) {
		diag_stats_events.store(obs_data_get_bool(request_data, "stats_events"));
	}
	if (obs_data_has_user_value(request_data, "stats_interval_ms")) {
		const long long interval = obs_data_get_int(request_data, "stats_interval_ms");
		diag_stats_interval_ms.store((int)std::min<long long>(std::max<long long>(interval, 250), 60000));
	}
	vendor_fill_logging(response_data);
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_get_logging(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	vendor_fill_logging(response_data);
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
	bool canvas_found = false;
	bool scene_found = false;
	for (const auto &it : canvas_docks) {
		auto canvas = it->GetCanvas();
		if (canvas_name[0] != '\0' && strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
		canvas_found = true;
		// SwitchScene() silently ignores a name it cannot use, so check here.
		auto scene = obs_canvas_get_source_by_name(canvas, scene_name);
		if (obs_source_is_scene(scene)) {
			scene_found = true;
			QMetaObject::invokeMethod(it, "SwitchScene", Q_ARG(QString, QString::fromUtf8(scene_name)));
		}
		obs_source_release(scene);
	}

	if (!canvas_found) {
		obs_data_set_string(response_data, "error", "'canvas' not found");
	} else if (!scene_found) {
		obs_data_set_string(response_data, "error", "'scene' not found");
	}
	obs_data_set_bool(response_data, "success", scene_found);
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
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}

		auto source = obs_canvas_get_channel(canvas, 0);
		if (source && obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION) {
			auto active_source = obs_transition_get_active_source(source);
			obs_source_release(source);
			source = active_source;
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
	obs_data_set_string(response_data, "error", "'canvas' not found");
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
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}

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
	if (!output_dock) {
		vendor_fail(response_data, "Output dock not available", "dock_unavailable");
		return;
	}
	const char *canvas_filter = obs_data_get_string(request_data, "canvas");
	std::string canvas_name;
	if (canvas_filter[0] != '\0') {
		canvas_name = resolve_canvas_name(canvas_filter);
		if (canvas_name.empty()) {
			vendor_fail(response_data, "'canvas' not found", "canvas_not_found");
			return;
		}
	}
	auto oa = output_dock->GetOutputsArray(canvas_name.empty() ? nullptr : canvas_name.c_str());
	obs_data_set_bool(response_data, "main_active", obs_frontend_streaming_active());
	obs_data_set_bool(response_data, "success", true);
	obs_data_set_array(response_data, "outputs", oa);
	obs_data_array_release(oa);
}

void vendor_request_get_stats(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	if (!output_dock) {
		vendor_fail(response_data, "Output dock not available", "dock_unavailable");
		return;
	}
	const char *output_name = obs_data_get_string(request_data, "output");
	const bool filtered = output_name[0] != '\0';
	if (filtered && !output_dock->FindOutput(output_name)) {
		vendor_fail(response_data, "'output' not found", "output_not_found");
		return;
	}
	auto stats = output_dock->GetStatsArray(filtered ? output_name : nullptr);
	obs_data_set_bool(response_data, "success", true);
	obs_data_set_array(response_data, "stats", stats);
	obs_data_array_release(stats);
}

static void vendor_output_action(obs_data_t *request_data, obs_data_t *response_data, const char *action,
				 const char *(OutputWidget::*remote)())
{
	const char *output_name = obs_data_get_string(request_data, "output");
	if (output_name[0] == '\0') {
		vendor_fail(response_data, "'output' not set", "name_not_set");
		return;
	}
	if (!output_dock) {
		vendor_fail(response_data, "Output dock not available", "dock_unavailable");
		return;
	}
	auto output = output_dock->FindOutput(output_name);
	if (!output) {
		vendor_fail(response_data, "'output' not found", "output_not_found");
		vendor_log(LOG_WARNING, "request", nullptr, "%s '%s' failed: output_not_found", action, output_name);
		return;
	}
	const char *error_code = (output->*remote)();
	if (error_code) {
		vendor_fail(response_data, output_error_text(error_code), error_code);
		vendor_log(LOG_WARNING, "request", nullptr, "%s '%s' failed: %s", action, output_name, error_code);
		return;
	}
	vendor_log(LOG_INFO, "request", nullptr, "%s '%s' accepted", action, output_name);
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_start_output(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	vendor_output_action(request_data, response_data, "start_output", &OutputWidget::RemoteStart);
}

void vendor_request_stop_output(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	vendor_output_action(request_data, response_data, "stop_output", &OutputWidget::RemoteStop);
}

// The *_all_* requests stay queued rather than running inside the handler because
// StartAll and StopAll can open a confirmation dialog, and the worker must not
// sit waiting on a person.
void vendor_request_start_all_outputs(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StartAll", Qt::QueuedConnection, Q_ARG(bool, false), Q_ARG(bool, false));
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
	QMetaObject::invokeMethod(output_dock, "StopAll", Qt::QueuedConnection, Q_ARG(bool, false), Q_ARG(bool, false));
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
	QMetaObject::invokeMethod(output_dock, "StartAll", Qt::QueuedConnection, Q_ARG(bool, true), Q_ARG(bool, false));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_stop_all_streams(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StopAll", Qt::QueuedConnection, Q_ARG(bool, true), Q_ARG(bool, false));
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
	QMetaObject::invokeMethod(output_dock, "StartAll", Qt::QueuedConnection, Q_ARG(bool, false), Q_ARG(bool, true));
	obs_data_set_bool(response_data, "success", true);
}

void vendor_request_stop_all_recordings(obs_data_t *request_data, obs_data_t *response_data, void *)
{
	UNUSED_PARAMETER(request_data);
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	QMetaObject::invokeMethod(output_dock, "StopAll", Qt::QueuedConnection, Q_ARG(bool, false), Q_ARG(bool, true));
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
			if (strcmp(hp->name, hn) == 0) {
				hp->hotkey = key;
			}
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
	if (!output_dock) {
		obs_data_set_string(response_data, "error", "Output dock not available");
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
	auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		obs_data_set_string(response_data, "error", "Main window not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto da = obs_data_array_create();
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
	obs_data_set_array(response_data, "docks", da);
	obs_data_array_release(da);
	obs_data_set_bool(response_data, "success", true);
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
	if (!main_window) {
		obs_data_set_string(response_data, "error", "Main window not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
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

void fill_central_widget();

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
	if (!main_window) {
		obs_data_set_string(response_data, "error", "Main window not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
	auto docks = main_window->findChildren<QDockWidget *>();
	for (auto &dock : docks) {
		if (dock->objectName() == dn) {
			QMetaObject::invokeMethod(dock, [dock] {
				dock->hide();
				fill_central_widget();
			});

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
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, true); });
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	for (const auto &it : canvas_clone_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
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
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
		auto pn = QString::fromUtf8(panel_name);
		QMetaObject::invokeMethod(it, [it, pn] { it->SetPanelVisible(pn, false); });
		obs_data_set_bool(response_data, "success", true);
		return;
	}
	for (const auto &it : canvas_clone_docks) {
		auto canvas = it->GetCanvas();
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
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
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}

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
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}

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
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
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
		if (strcmp(obs_canvas_get_name(canvas), canvas_name) != 0 &&
		    strcmp(obs_canvas_get_uuid(canvas), canvas_name) != 0) {
			continue;
		}
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
	if (!main_window) {
		obs_data_set_string(response_data, "error", "Main window not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
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
	if (!main_window) {
		obs_data_set_string(response_data, "error", "Main window not available");
		obs_data_set_bool(response_data, "success", false);
		return;
	}
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
	if (!vendor) {
		return;
	}

	// These never touch Qt, so diagnostics keep answering if the UI thread is stuck.
	obs_websocket_vendor_register_request(vendor, "version", vendor_request_version, nullptr);
	obs_websocket_vendor_register_request(vendor, "set_logging", vendor_request_set_logging, nullptr);
	obs_websocket_vendor_register_request(vendor, "get_logging", vendor_request_get_logging, nullptr);

	register_ui_request("status", vendor_request_status);
	register_ui_request("get_canvas", vendor_request_get_canvas);
	register_ui_request("switch_scene", vendor_request_switch_scene);
	register_ui_request("current_scene", vendor_request_current_scene);
	register_ui_request("get_scenes", vendor_request_get_scenes);

	register_ui_request("get_outputs", vendor_request_get_outputs);
	register_ui_request("get_stats", vendor_request_get_stats);
	register_ui_request("start_output", vendor_request_start_output);
	register_ui_request("stop_output", vendor_request_stop_output);
	register_ui_request("start_all_outputs", vendor_request_start_all_outputs);
	register_ui_request("stop_all_outputs", vendor_request_stop_all_outputs);
	register_ui_request("start_all_streams", vendor_request_start_all_streams);
	register_ui_request("stop_all_streams", vendor_request_stop_all_streams);
	register_ui_request("start_all_recordings", vendor_request_start_all_recordings);
	register_ui_request("stop_all_recordings", vendor_request_stop_all_recordings);
	register_ui_request("save_backtrack", vendor_request_save_backtrack);

	register_ui_request("add_chapter", vendor_request_add_chapter);

	register_ui_request("get_dock_modes", vendor_request_get_dock_modes);
	register_ui_request("switch_dock_mode", vendor_request_switch_dock_mode);
	register_ui_request("get_docks", vendor_request_get_docks);
	register_ui_request("dock_show", vendor_request_dock_show);
	register_ui_request("dock_hide", vendor_request_dock_hide);

	register_ui_request("get_live_scenes", vendor_request_get_live_scenes);
	register_ui_request("live_scenes_add", vendor_request_live_scenes_add);
	register_ui_request("live_scenes_remove", vendor_request_live_scenes_remove);

	register_ui_request("canvas_dock_show_panel", vendor_request_dock_show_panel);
	register_ui_request("canvas_dock_hide_panel", vendor_request_dock_hide_panel);

	register_ui_request("get_transitions", vendor_request_get_transitions);
	register_ui_request("switch_transition", vendor_request_switch_transition);
	register_ui_request("transitions_add", vendor_request_transitions_add);
	register_ui_request("transitions_remove", vendor_request_transitions_remove);

	register_ui_request("refresh_browser_panel", vendor_request_refresh_browser_panel);
	register_ui_request("reset_browser_panel", vendor_request_reset_browser_panel);
}

void unload_obs_websocket()
{
	if (!vendor) {
		return;
	}
	// obs-websocket-api.h caches a pointer into obs-websocket, and module unload
	// order is arbitrary. free_module() unlinks a module once it has unloaded, so
	// this is what tells us that pointer is still good.
	if (!obs_get_module("obs-websocket")) {
		vendor = nullptr;
		return;
	}
	obs_websocket_vendor_unregister_request(vendor, "version");
	obs_websocket_vendor_unregister_request(vendor, "set_logging");
	obs_websocket_vendor_unregister_request(vendor, "get_logging");
	obs_websocket_vendor_unregister_request(vendor, "status");
	obs_websocket_vendor_unregister_request(vendor, "get_canvas");
	obs_websocket_vendor_unregister_request(vendor, "switch_scene");
	obs_websocket_vendor_unregister_request(vendor, "current_scene");
	obs_websocket_vendor_unregister_request(vendor, "get_scenes");

	obs_websocket_vendor_unregister_request(vendor, "get_outputs");
	obs_websocket_vendor_unregister_request(vendor, "get_stats");
	obs_websocket_vendor_unregister_request(vendor, "start_output");
	obs_websocket_vendor_unregister_request(vendor, "stop_output");
	obs_websocket_vendor_unregister_request(vendor, "start_all_outputs");
	obs_websocket_vendor_unregister_request(vendor, "stop_all_outputs");
	obs_websocket_vendor_unregister_request(vendor, "start_all_streams");
	obs_websocket_vendor_unregister_request(vendor, "stop_all_streams");
	obs_websocket_vendor_unregister_request(vendor, "start_all_recordings");
	obs_websocket_vendor_unregister_request(vendor, "stop_all_recordings");
	obs_websocket_vendor_unregister_request(vendor, "save_backtrack");

	obs_websocket_vendor_unregister_request(vendor, "add_chapter");

	obs_websocket_vendor_unregister_request(vendor, "get_dock_modes");
	obs_websocket_vendor_unregister_request(vendor, "switch_dock_mode");
	obs_websocket_vendor_unregister_request(vendor, "get_docks");
	obs_websocket_vendor_unregister_request(vendor, "dock_show");
	obs_websocket_vendor_unregister_request(vendor, "dock_hide");

	obs_websocket_vendor_unregister_request(vendor, "get_live_scenes");
	obs_websocket_vendor_unregister_request(vendor, "live_scenes_add");
	obs_websocket_vendor_unregister_request(vendor, "live_scenes_remove");

	obs_websocket_vendor_unregister_request(vendor, "canvas_dock_show_panel");
	obs_websocket_vendor_unregister_request(vendor, "canvas_dock_hide_panel");

	obs_websocket_vendor_unregister_request(vendor, "get_transitions");
	obs_websocket_vendor_unregister_request(vendor, "switch_transition");
	obs_websocket_vendor_unregister_request(vendor, "transitions_add");
	obs_websocket_vendor_unregister_request(vendor, "transitions_remove");

	obs_websocket_vendor_unregister_request(vendor, "refresh_browser_panel");
	obs_websocket_vendor_unregister_request(vendor, "reset_browser_panel");

	// Docks are destroyed after this, and must not emit through a vendor that
	// outlives obs-websocket once it unloads.
	vendor = nullptr;
}
