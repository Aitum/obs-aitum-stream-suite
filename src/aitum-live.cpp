#include "docks/canvas-dock.hpp"
#include "utils/file-download.h"
#include "version.h"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QMainWindow>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Aitum");
OBS_MODULE_USE_DEFAULT_LOCALE("aitum-live", "en-US")

download_info_t *version_download_info = nullptr;

bool version_info_downloaded(void *param, struct file_download_data *file)
{
	UNUSED_PARAMETER(param);
	if (!file || !file->buffer.num)
		return true;

	if (version_download_info) {
		download_info_destroy(version_download_info);
		version_download_info = nullptr;
	}
	return true;
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Aitum Live] loaded version %s", PROJECT_VERSION);

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_add_dock_by_id("AitumLiveCanvasDock", obs_module_text("AitumLiveCanvas"),
				    new CanvasDock(nullptr, main_window));


	version_download_info = download_info_create_single("[Aitum Live]", "OBS", "https://api.aitum.tv/plugin/live",
							version_info_downloaded, nullptr);
	return true;
}

void obs_module_unload()
{
	if (version_download_info) {
		download_info_destroy(version_download_info);
		version_download_info = nullptr;
	}
}
