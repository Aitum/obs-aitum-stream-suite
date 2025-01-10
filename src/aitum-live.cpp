#include <obs-module.h>
#include "version.h"
#include "utils/file-updater.h"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Aitum");
OBS_MODULE_USE_DEFAULT_LOCALE("aitum-live", "en-US")

update_info_t *version_update_info = nullptr;

bool version_info_downloaded(void *param, struct file_download_data *file)
{
	UNUSED_PARAMETER(param);
	if (!file || !file->buffer.num)
		return true;

	

	return true;
}

bool obs_module_load(void)
{
	blog(LOG_INFO, "[Aitum Live] loaded version %s", PROJECT_VERSION);

	version_update_info = update_info_create_single("[Aitum Live]", "OBS", "https://api.aitum.tv/plugin/live",
							version_info_downloaded, nullptr);
	return true;
}
