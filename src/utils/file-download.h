#pragma once

#include <util/darray.h>

#ifdef __cplusplus
extern "C" {
#endif

struct download_info;
typedef struct download_info download_info_t;

struct file_download_data {
	const char *name;
	int version;

	DARRAY(uint8_t) buffer;
};

typedef bool (*confirm_file_callback_t)(void *param, struct file_download_data *file);

download_info_t *download_info_create_single(const char *log_prefix, const char *user_agent, const char *file_url,
					 confirm_file_callback_t confirm_callback, void *param);
void download_info_destroy(download_info_t *info);

#ifdef __cplusplus
}
#endif
