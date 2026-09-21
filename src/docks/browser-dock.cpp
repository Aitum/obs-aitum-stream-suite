#include "browser-dock.hpp"
#include <obs-frontend-api.h>
#include <QEventLoop>
#include <QThread>
#include <QVBoxLayout>
#include <random>
#include <src/utils/obs-websocket-api.h>

QCef *cef = nullptr;
QCefCookieManager *panel_cookies = nullptr;

#if defined(WIN32)
#define CEF_CALLBACK __stdcall
#else
#define CEF_CALLBACK
#endif

extern "C" {
typedef struct _cef_string_utf8_t {
	char *str;
	size_t length;
	void (*dtor)(char *str);
} cef_string_utf8_t;

typedef struct _cef_string_utf16_t {
	char16_t *str;
	size_t length;
	void (*dtor)(char16_t *str);
} cef_string_utf16_t;

typedef cef_string_utf16_t cef_string_t;
typedef struct _cef_string_multimap_t *cef_string_multimap_t;
typedef cef_string_utf8_t *cef_string_userfree_utf8_t;
typedef cef_string_utf16_t *cef_string_userfree_utf16_t;
typedef cef_string_userfree_utf16_t cef_string_userfree_t;

typedef struct _cef_base_ref_counted_t {
	size_t size;
	void(CEF_CALLBACK *add_ref)(struct _cef_base_ref_counted_t *self);
	int(CEF_CALLBACK *release)(struct _cef_base_ref_counted_t *self);
	int(CEF_CALLBACK *has_one_ref)(struct _cef_base_ref_counted_t *self);
	int(CEF_CALLBACK *has_at_least_one_ref)(struct _cef_base_ref_counted_t *self);
} cef_base_ref_counted_t;

typedef struct _cef_stream_reader_t {
	cef_base_ref_counted_t base;
	size_t(CEF_CALLBACK *read)(struct _cef_stream_reader_t *self, void *ptr, size_t size, size_t n);
	int(CEF_CALLBACK *seek)(struct _cef_stream_reader_t *self, int64_t offset, int whence);
	int64_t(CEF_CALLBACK *tell)(struct _cef_stream_reader_t *self);
	int(CEF_CALLBACK *eof)(struct _cef_stream_reader_t *self);
	int(CEF_CALLBACK *may_block)(struct _cef_stream_reader_t *self);
} cef_stream_reader_t;

typedef struct _cef_resource_handler_t {
	cef_base_ref_counted_t base;
	int(CEF_CALLBACK *open)(struct _cef_resource_handler_t *self, struct _cef_request_t *request, int *handle_request,
				struct _cef_callback_t *callback);
	int(CEF_CALLBACK *process_request)(struct _cef_resource_handler_t *self, struct _cef_request_t *request,
					   struct _cef_callback_t *callback);
	void(CEF_CALLBACK *get_response_headers)(struct _cef_resource_handler_t *self, struct _cef_response_t *response,
						 int64_t *response_length, cef_string_t *redirectUrl);
	int(CEF_CALLBACK *skip)(struct _cef_resource_handler_t *self, int64_t bytes_to_skip, int64_t *bytes_skipped,
				struct _cef_resource_skip_callback_t *callback);
	int(CEF_CALLBACK *read)(struct _cef_resource_handler_t *self, void *data_out, int bytes_to_read, int *bytes_read,
				struct _cef_resource_read_callback_t *callback);
	int(CEF_CALLBACK *read_response)(struct _cef_resource_handler_t *self, void *data_out, int bytes_to_read, int *bytes_read,
					 struct _cef_callback_t *callback);
	void(CEF_CALLBACK *cancel)(struct _cef_resource_handler_t *self);
} cef_resource_handler_t;

typedef struct _file_cef_resource_handler_t {
	cef_resource_handler_t resource_handler;
	cef_string_userfree_t mimetype;
	cef_stream_reader_t *reader;
	size_t refcount;
} file_cef_resource_handler_t;

typedef struct _obs_websocket_cef_resource_handler_t {
	cef_resource_handler_t resource_handler;
	char *request_type;
	struct obs_websocket_request_response *response;
	size_t response_data_pos;
	size_t refcount;
} obs_websocket_cef_resource_handler_t;

typedef struct _cef_scheme_handler_factory_t {
	cef_base_ref_counted_t base;
	struct _cef_resource_handler_t *(CEF_CALLBACK *create)(struct _cef_scheme_handler_factory_t *self,
							       struct _cef_browser_t *browser, struct _cef_frame_t *frame,
							       const cef_string_t *scheme_name, struct _cef_request_t *request);
} cef_scheme_handler_factory_t;

typedef enum {
	REFERRER_POLICY_CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
	REFERRER_POLICY_DEFAULT = REFERRER_POLICY_CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
	REFERRER_POLICY_REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN,
	REFERRER_POLICY_ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN,
	REFERRER_POLICY_NEVER_CLEAR_REFERRER,
	REFERRER_POLICY_ORIGIN,
	REFERRER_POLICY_CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN,
	REFERRER_POLICY_ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE,
	REFERRER_POLICY_NO_REFERRER,
	REFERRER_POLICY_NUM_VALUES,
} cef_referrer_policy_t;

typedef enum {
	RT_MAIN_FRAME = 0,
	RT_SUB_FRAME,
	RT_STYLESHEET,
	RT_SCRIPT,
	RT_IMAGE,
	RT_FONT_RESOURCE,
	RT_SUB_RESOURCE,
	RT_OBJECT,
	RT_MEDIA,
	RT_WORKER,
	RT_SHARED_WORKER,
	RT_PREFETCH,
	RT_FAVICON,
	RT_XHR,
	RT_PING,
	RT_SERVICE_WORKER,
	RT_CSP_REPORT,
	RT_PLUGIN_RESOURCE,
	RT_NAVIGATION_PRELOAD_MAIN_FRAME = 19,
	RT_NAVIGATION_PRELOAD_SUB_FRAME,
	RT_NUM_VALUES,
} cef_resource_type_t;

typedef enum {
	TT_LINK,
	TT_EXPLICIT,
	TT_AUTO_BOOKMARK,
	TT_AUTO_SUBFRAME,
	TT_MANUAL_SUBFRAME,
	TT_GENERATED,
	TT_AUTO_TOPLEVEL,
	TT_FORM_SUBMIT,
	TT_RELOAD,
	TT_KEYWORD,
	TT_KEYWORD_GENERATED,
	TT_NUM_VALUES,
	TT_SOURCE_MASK = 0xFF,
	TT_BLOCKED_FLAG = 0x00800000,
	TT_FORWARD_BACK_FLAG = 0x01000000,
	TT_DIRECT_LOAD_FLAG = 0x02000000,
	TT_HOME_PAGE_FLAG = 0x04000000,
	TT_FROM_API_FLAG = 0x08000000,
	TT_CHAIN_START_FLAG = 0x10000000,
	TT_CHAIN_END_FLAG = 0x20000000,
	TT_CLIENT_REDIRECT_FLAG = 0x40000000,
	TT_SERVER_REDIRECT_FLAG = 0x80000000,
	TT_IS_REDIRECT_MASK = 0xC0000000,
	TT_QUALIFIER_MASK = 0xFFFFFF00,
} cef_transition_type_t;

typedef enum {
	ERR_NONE = 0,
} cef_errorcode_t;

typedef enum {
	UU_NONE = 0,
	UU_NORMAL = 1 << 0,
	UU_SPACES = 1 << 1,
	UU_PATH_SEPARATORS = 1 << 2,
	UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS = 1 << 3,
	UU_REPLACE_PLUS_WITH_SPACE = 1 << 4,
} cef_uri_unescape_rule_t;

typedef enum {
	PDE_TYPE_EMPTY = 0,
	PDE_TYPE_BYTES,
	PDE_TYPE_FILE,
	PDE_TYPE_NUM_VALUES,
} cef_postdataelement_type_t;

typedef struct _cef_post_data_element_t {
	cef_base_ref_counted_t base;
	int(CEF_CALLBACK *is_read_only)(struct _cef_post_data_element_t *self);
	void(CEF_CALLBACK *set_to_empty)(struct _cef_post_data_element_t *self);
	void(CEF_CALLBACK *set_to_file)(struct _cef_post_data_element_t *self, const cef_string_t *fileName);
	void(CEF_CALLBACK *set_to_bytes)(struct _cef_post_data_element_t *self, size_t size, const void *bytes);
	cef_postdataelement_type_t(CEF_CALLBACK *get_type)(struct _cef_post_data_element_t *self);
	cef_string_userfree_t(CEF_CALLBACK *get_file)(struct _cef_post_data_element_t *self);
	size_t(CEF_CALLBACK *get_bytes_count)(struct _cef_post_data_element_t *self);
	size_t(CEF_CALLBACK *get_bytes)(struct _cef_post_data_element_t *self, size_t size, void *bytes);
} cef_post_data_element_t;

typedef struct _cef_post_data_t {
	cef_base_ref_counted_t base;
	int(CEF_CALLBACK *is_read_only)(struct _cef_post_data_t *self);
	int(CEF_CALLBACK *has_excluded_elements)(struct _cef_post_data_t *self);
	size_t(CEF_CALLBACK *get_element_count)(struct _cef_post_data_t *self);
	void(CEF_CALLBACK *get_elements)(struct _cef_post_data_t *self, size_t *elementsCount,
					 struct _cef_post_data_element_t **elements);
	int(CEF_CALLBACK *remove_element)(struct _cef_post_data_t *self, struct _cef_post_data_element_t *element);
	int(CEF_CALLBACK *add_element)(struct _cef_post_data_t *self, struct _cef_post_data_element_t *element);
	void(CEF_CALLBACK *remove_elements)(struct _cef_post_data_t *self);
} cef_post_data_t;

typedef struct _cef_request_t {
	cef_base_ref_counted_t base;
	int(CEF_CALLBACK *is_read_only)(struct _cef_request_t *self);
	cef_string_userfree_t(CEF_CALLBACK *get_url)(struct _cef_request_t *self);
	void(CEF_CALLBACK *set_url)(struct _cef_request_t *self, const cef_string_t *url);
	cef_string_userfree_t(CEF_CALLBACK *get_method)(struct _cef_request_t *self);
	void(CEF_CALLBACK *set_method)(struct _cef_request_t *self, const cef_string_t *method);
	void(CEF_CALLBACK *set_referrer)(struct _cef_request_t *self, const cef_string_t *referrer_url,
					 cef_referrer_policy_t policy);
	cef_string_userfree_t(CEF_CALLBACK *get_referrer_url)(struct _cef_request_t *self);
	cef_referrer_policy_t(CEF_CALLBACK *get_referrer_policy)(struct _cef_request_t *self);
	struct _cef_post_data_t *(CEF_CALLBACK *get_post_data)(struct _cef_request_t *self);
	void(CEF_CALLBACK *set_post_data)(struct _cef_request_t *self, struct _cef_post_data_t *postData);
	void(CEF_CALLBACK *get_header_map)(struct _cef_request_t *self, cef_string_multimap_t headerMap);
	void(CEF_CALLBACK *set_header_map)(struct _cef_request_t *self, cef_string_multimap_t headerMap);
	cef_string_userfree_t(CEF_CALLBACK *get_header_by_name)(struct _cef_request_t *self, const cef_string_t *name);
	void(CEF_CALLBACK *set_header_by_name)(struct _cef_request_t *self, const cef_string_t *name, const cef_string_t *value,
					       int overwrite);
	void(CEF_CALLBACK *set)(struct _cef_request_t *self, const cef_string_t *url, const cef_string_t *method,
				struct _cef_post_data_t *postData, cef_string_multimap_t headerMap);
	int(CEF_CALLBACK *get_flags)(struct _cef_request_t *self);
	void(CEF_CALLBACK *set_flags)(struct _cef_request_t *self, int flags);
	cef_string_userfree_t(CEF_CALLBACK *get_first_party_for_cookies)(struct _cef_request_t *self);
	void(CEF_CALLBACK *set_first_party_for_cookies)(struct _cef_request_t *self, const cef_string_t *url);
	cef_resource_type_t(CEF_CALLBACK *get_resource_type)(struct _cef_request_t *self);
	cef_transition_type_t(CEF_CALLBACK *get_transition_type)(struct _cef_request_t *self);
	uint64_t(CEF_CALLBACK *get_identifier)(struct _cef_request_t *self);
} cef_request_t;

typedef struct _cef_urlparts_t {
	size_t size;
	cef_string_t spec;
	cef_string_t scheme;
	cef_string_t username;
	cef_string_t password;
	cef_string_t host;
	cef_string_t port;
	cef_string_t origin;
	cef_string_t path;
	cef_string_t query;
	cef_string_t fragment;
} cef_urlparts_t;

typedef struct _cef_callback_t {
	cef_base_ref_counted_t base;
	void(CEF_CALLBACK *cont)(struct _cef_callback_t *self);
	void(CEF_CALLBACK *cancel)(struct _cef_callback_t *self);
} cef_callback_t;

typedef struct _cef_response_t {
	cef_base_ref_counted_t base;
	int(CEF_CALLBACK *is_read_only)(struct _cef_response_t *self);
	cef_errorcode_t(CEF_CALLBACK *get_error)(struct _cef_response_t *self);
	void(CEF_CALLBACK *set_error)(struct _cef_response_t *self, cef_errorcode_t error);
	int(CEF_CALLBACK *get_status)(struct _cef_response_t *self);
	void(CEF_CALLBACK *set_status)(struct _cef_response_t *self, int status);
	cef_string_userfree_t(CEF_CALLBACK *get_status_text)(struct _cef_response_t *self);
	void(CEF_CALLBACK *set_status_text)(struct _cef_response_t *self, const cef_string_t *statusText);
	cef_string_userfree_t(CEF_CALLBACK *get_mime_type)(struct _cef_response_t *self);
	void(CEF_CALLBACK *set_mime_type)(struct _cef_response_t *self, const cef_string_t *mimeType);
	cef_string_userfree_t(CEF_CALLBACK *get_charset)(struct _cef_response_t *self);
	void(CEF_CALLBACK *set_charset)(struct _cef_response_t *self, const cef_string_t *charset);
	cef_string_userfree_t(CEF_CALLBACK *get_header_by_name)(struct _cef_response_t *self, const cef_string_t *name);
	void(CEF_CALLBACK *set_header_by_name)(struct _cef_response_t *self, const cef_string_t *name, const cef_string_t *value,
					       int overwrite);
	void(CEF_CALLBACK *get_header_map)(struct _cef_response_t *self, cef_string_multimap_t headerMap);
	void(CEF_CALLBACK *set_header_map)(struct _cef_response_t *self, cef_string_multimap_t headerMap);
	cef_string_userfree_t(CEF_CALLBACK *get_url)(struct _cef_response_t *self);
	void(CEF_CALLBACK *set_url)(struct _cef_response_t *self, const cef_string_t *url);
} cef_response_t;

typedef struct _cef_resource_skip_callback_t {
	cef_base_ref_counted_t base;
	void(CEF_CALLBACK *cont)(struct _cef_resource_skip_callback_t *self, int64_t bytes_skipped);
} cef_resource_skip_callback_t;

typedef struct _cef_resource_read_callback_t {
	cef_base_ref_counted_t base;
	void(CEF_CALLBACK *cont)(struct _cef_resource_read_callback_t *self, int bytes_read);
} cef_resource_read_callback_t;

cef_string_t scheme{};
cef_string_t domain{};
cef_scheme_handler_factory_t factory{};

int (*cef_parse_url)(const cef_string_t *url, struct _cef_urlparts_t *parts) = nullptr;
void (*cef_string_userfree_free)(cef_string_userfree_t str) = nullptr;
cef_string_userfree_utf8_t (*cef_string_userfree_utf8_alloc)(void) = nullptr;
int (*cef_string_utf16_to_utf8)(const char16_t *src, size_t src_len, cef_string_utf8_t *output) = nullptr;
void (*cef_string_userfree_utf8_free)(cef_string_userfree_utf8_t str) = nullptr;
cef_string_userfree_t (*cef_uridecode)(cef_string_t *text, int convert_to_utf8, cef_uri_unescape_rule_t unescape_rule) = nullptr;
cef_stream_reader_t *(*cef_stream_reader_create_for_file)(const cef_string_t *fileName) = nullptr;
cef_string_userfree_t (*cef_get_mime_type)(const cef_string_t *extension) = nullptr;
int (*cef_string_set)(const char16_t *, size_t, cef_string_t *) = nullptr;
int (*cef_string_utf8_to_utf16)(const char *src, size_t src_len, cef_string_utf16_t *output) = nullptr;
cef_string_userfree_utf16_t (*cef_string_userfree_utf16_alloc)(void) = nullptr;
}

cef_resource_handler_t *scheme_factory(struct _cef_scheme_handler_factory_t *self, struct _cef_browser_t *browser,
				       struct _cef_frame_t *frame, const cef_string_t *scheme_name, struct _cef_request_t *request)
{
	UNUSED_PARAMETER(self);
	UNUSED_PARAMETER(browser);
	UNUSED_PARAMETER(frame);
	UNUSED_PARAMETER(scheme_name);
	if (!request) {
		return nullptr;
	}

	cef_urlparts_t parts{};
	parts.size = sizeof(cef_urlparts_t);
	/* auto referrer = request->get_referrer_url(request);
	if (referrer) {
		auto r = cef_parse_url(referrer, &parts);
		cef_string_userfree_free(referrer);
		if (r == 0) {
			return nullptr;
		}
		if (parts.host.length) {
			auto host = cef_string_userfree_utf8_alloc();
			cef_string_utf16_to_utf8(parts.host.str, parts.host.length, host);
			if (strcmp(host->str, "chat.aitumsuite.tv") != 0) {
				cef_string_userfree_utf8_free(host);
				return nullptr;
			}
			cef_string_userfree_utf8_free(host);
		}
	}*/
	auto url = request->get_url(request);

	auto r = cef_parse_url(url, &parts);
	cef_string_userfree_free(url);
	if (r == 0) {
		return nullptr;
	}

	auto path = cef_uridecode(&parts.path, 1, UU_SPACES);
	auto path2 = cef_uridecode(path, 1, UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);
	cef_string_userfree_free(path);

	auto path3 = cef_string_userfree_utf8_alloc();
	cef_string_utf16_to_utf8(path2->str, path2->length, path3);
	cef_string_userfree_free(path2);

	std::string filePath = path3->str;

	cef_string_userfree_utf8_free(path3);
	filePath = filePath.substr(1);

	if (filePath.find(".") == std::string::npos && filePath.find("/") == std::string::npos &&
	    filePath.find("\\") == std::string::npos) {
		auto owrh = (obs_websocket_cef_resource_handler_t *)bzalloc(sizeof(obs_websocket_cef_resource_handler_t));
		owrh->request_type = bstrdup(filePath.c_str());
		auto resource_handler = &owrh->resource_handler;
		owrh->refcount = 1;
		resource_handler->base.size = sizeof(cef_resource_handler_t);
		resource_handler->base.add_ref = [](cef_base_ref_counted_t *self) {
			((obs_websocket_cef_resource_handler_t *)self)->refcount++;
		};
		resource_handler->base.release = [](cef_base_ref_counted_t *self) -> int {
			auto s = (obs_websocket_cef_resource_handler_t *)self;
			s->refcount--;
			if (!s->refcount) {
				obs_websocket_request_response_free(s->response);
				bfree(s->request_type);
				bfree(self);
			}
			return 1;
		};
		resource_handler->base.has_at_least_one_ref = [](cef_base_ref_counted_t *self) -> int {
			return ((obs_websocket_cef_resource_handler_t *)self)->refcount >= 1 ? 1 : 0;
		};
		resource_handler->base.has_one_ref = [](cef_base_ref_counted_t *self) -> int {
			return ((obs_websocket_cef_resource_handler_t *)self)->refcount == 1 ? 1 : 0;
		};
		resource_handler->open = [](struct _cef_resource_handler_t *self, struct _cef_request_t *request,
					    int *handle_request, struct _cef_callback_t *callback) -> int {
			auto s = (obs_websocket_cef_resource_handler_t *)self;

			auto post_data = request->get_post_data(request);
			if (post_data) {
				size_t count = post_data->get_element_count(post_data);
				if (count) {
					cef_post_data_element_t *elements = nullptr;
					post_data->get_elements(post_data, &count, &elements);
					for (size_t i = 0; i < count; i++) {
						if (s->response) {
							elements[i].base.release(&elements[i].base);
							continue;
						}
						auto bc = elements[i].get_bytes_count(&elements[i]);
						if (bc) {
							auto bytes = (char *)bmalloc(bc + 1);
							elements[i].get_bytes(&elements[i], bc, bytes);
							bytes[bc] = '\0';

							auto json_data = obs_data_create_from_json(bytes);
							if (json_data) {
								s->response =
									obs_websocket_call_request(s->request_type, json_data);
								obs_data_release(json_data);
							}
							bfree(bytes);
						}
						elements[i].base.release(&elements[i].base);
					}
				} else {
					s->response = obs_websocket_call_request(s->request_type);
				}
				post_data->base.release(&post_data->base);
			} else {
				s->response = obs_websocket_call_request(s->request_type);
			}
			if (s->response) {
				*handle_request = 1;
				callback->cont(callback);
				return 1;
			} else {
				*handle_request = 1;
				callback->cancel(callback);
				return 0;
			}
		};
		resource_handler->process_request = [](struct _cef_resource_handler_t *self, struct _cef_request_t *request,
						       struct _cef_callback_t *callback) -> int {
			UNUSED_PARAMETER(self);
			UNUSED_PARAMETER(request);
			callback->cont(callback);
			return 1;
		};
		resource_handler->get_response_headers = [](struct _cef_resource_handler_t *self, struct _cef_response_t *response,
							    int64_t *response_length, cef_string_t *redirectUrl) {
			UNUSED_PARAMETER(redirectUrl);
			auto s = (obs_websocket_cef_resource_handler_t *)self;
			if (s->response->status_code != 100) {
				response->set_status(response, 400);
				if (s->response->comment) {
					auto comment = cef_string_userfree_utf16_alloc();
					cef_string_utf8_to_utf16(s->response->comment, strlen(s->response->comment), comment);
					response->set_status_text(response, comment);
					cef_string_userfree_free(comment);
				}
			}
			if (s->response->response_data) {
				*response_length = strlen(s->response->response_data);
				auto mime_type = cef_string_userfree_utf16_alloc();
				cef_string_set(u"application/json", 16, mime_type);
				response->set_mime_type(response, mime_type);
				cef_string_userfree_free(mime_type);

				auto origin = cef_string_userfree_utf16_alloc();
				cef_string_set(u"Access-Control-Allow-Origin", 27, origin);
				auto val = cef_string_userfree_utf16_alloc();
				cef_string_set(u"*", 1, val);
				response->set_header_by_name(response, origin, val, 0);
				cef_string_userfree_free(origin);
				cef_string_userfree_free(val);
			} else {
				*response_length = 0;
			}
		};
		resource_handler->skip = [](struct _cef_resource_handler_t *self, int64_t bytes_to_skip, int64_t *bytes_skipped,
					    struct _cef_resource_skip_callback_t *callback) -> int {
			auto s = (obs_websocket_cef_resource_handler_t *)self;
			s->response_data_pos += bytes_to_skip;
			*bytes_skipped = bytes_to_skip;
			callback->cont(callback, *bytes_skipped);
			return 1;
		};
		resource_handler->read = [](struct _cef_resource_handler_t *self, void *data_out, int bytes_to_read,
					    int *bytes_read, struct _cef_resource_read_callback_t *callback) -> int {
			auto s = (obs_websocket_cef_resource_handler_t *)self;
			if (s->response->response_data) {
				auto len = strlen(s->response->response_data);
				if (s->response_data_pos >= len) {
					*bytes_read = 0;
				} else {
					size_t bytes_available = len - s->response_data_pos;
					if (bytes_available > (size_t)bytes_to_read) {
						*bytes_read = (int)bytes_to_read;
					} else {
						*bytes_read = (int)bytes_available;
					}
					memcpy(data_out, s->response->response_data + s->response_data_pos, *bytes_read);
					s->response_data_pos += *bytes_read;
				}
			} else {
				*bytes_read = 0;
			}
			callback->cont(callback, *bytes_read);
			return 1;
		};
		resource_handler->read_response = [](struct _cef_resource_handler_t *self, void *data_out, int bytes_to_read,
						     int *bytes_read, struct _cef_callback_t *callback) -> int {
			auto s = (obs_websocket_cef_resource_handler_t *)self;
			if (s->response->response_data) {
				auto len = strlen(s->response->response_data);
				if (s->response_data_pos >= len) {
					*bytes_read = 0;
				} else {
					size_t bytes_available = len - s->response_data_pos;
					if (bytes_available > (size_t)bytes_to_read) {
						*bytes_read = (int)bytes_to_read;
					} else {
						*bytes_read = (int)bytes_available;
					}
					memcpy(data_out, s->response->response_data + s->response_data_pos, *bytes_read);
					s->response_data_pos += *bytes_read;
				}
			} else {
				*bytes_read = 0;
			}
			callback->cont(callback);
			return 1;
		};
		resource_handler->cancel = [](struct _cef_resource_handler_t *self) {
			UNUSED_PARAMETER(self);
		};

		return resource_handler;
	}

	auto path4 = cef_string_userfree_utf16_alloc();
	cef_string_utf8_to_utf16(filePath.c_str(), filePath.length(), path4);

	std::string fileExtension = filePath.substr(filePath.find_last_of(".") + 1);

	for (char &ch : fileExtension) {
		ch = (char)tolower(ch);
	}
	if (fileExtension.compare("woff2") == 0) {
		fileExtension = "woff";
	}

	auto reader = cef_stream_reader_create_for_file(path4);
	cef_string_userfree_free(path4);
	if (!reader) {
		return nullptr;
	}

	auto frh = (file_cef_resource_handler_t *)bzalloc(sizeof(file_cef_resource_handler_t));
	auto resource_handler = &frh->resource_handler;
	frh->reader = reader;
	frh->refcount = 1;
	auto extension = cef_string_userfree_utf16_alloc();
	cef_string_utf8_to_utf16(fileExtension.c_str(), fileExtension.length(), extension);
	frh->mimetype = cef_get_mime_type(extension);
	cef_string_userfree_free(extension);

	resource_handler->base.size = sizeof(cef_resource_handler_t);
	resource_handler->base.add_ref = [](cef_base_ref_counted_t *self) {
		((file_cef_resource_handler_t *)self)->refcount++;
	};
	resource_handler->base.release = [](cef_base_ref_counted_t *self) -> int {
		auto s = (file_cef_resource_handler_t *)self;
		s->refcount--;
		if (!s->refcount) {
			if (s->reader) {
				s->reader->base.release(&s->reader->base);
				s->reader = nullptr;
			}
			if (s->mimetype) {
				cef_string_userfree_free(s->mimetype);
				s->mimetype = nullptr;
			}
			bfree(self);
		}
		return 1;
	};
	resource_handler->base.has_at_least_one_ref = [](cef_base_ref_counted_t *self) -> int {
		return ((file_cef_resource_handler_t *)self)->refcount >= 1 ? 1 : 0;
	};
	resource_handler->base.has_one_ref = [](cef_base_ref_counted_t *self) -> int {
		return ((file_cef_resource_handler_t *)self)->refcount == 1 ? 1 : 0;
	};
	resource_handler->open = [](struct _cef_resource_handler_t *self, struct _cef_request_t *request, int *handle_request,
				    struct _cef_callback_t *callback) -> int {
		UNUSED_PARAMETER(self);
		UNUSED_PARAMETER(request);
		*handle_request = 1;
		callback->cont(callback);
		return 1;
	};
	resource_handler->process_request = [](struct _cef_resource_handler_t *self, struct _cef_request_t *request,
					       struct _cef_callback_t *callback) -> int {
		UNUSED_PARAMETER(self);
		UNUSED_PARAMETER(request);
		callback->cont(callback);
		return 1;
	};
	resource_handler->get_response_headers = [](struct _cef_resource_handler_t *self, struct _cef_response_t *response,
						    int64_t *response_length, cef_string_t *redirectUrl) {
		UNUSED_PARAMETER(redirectUrl);
		*response_length = -1;
		auto s = (file_cef_resource_handler_t *)self;
		if (s->mimetype) {
			response->set_mime_type(response, s->mimetype);
		}
		auto origin = cef_string_userfree_utf16_alloc();
		cef_string_set(u"Access-Control-Allow-Origin", 27, origin);
		auto val = cef_string_userfree_utf16_alloc();
		cef_string_set(u"*", 1, val);
		response->set_header_by_name(response, origin, val, 0);
		cef_string_userfree_free(origin);
		cef_string_userfree_free(val);
	};
	resource_handler->skip = [](struct _cef_resource_handler_t *self, int64_t bytes_to_skip, int64_t *bytes_skipped,
				    struct _cef_resource_skip_callback_t *callback) -> int {
		auto reader = ((file_cef_resource_handler_t *)self)->reader;
		if (reader) {
			reader->seek(reader, bytes_to_skip, SEEK_CUR);
		} else {
			*bytes_skipped = 0;
		}
		callback->cont(callback, *bytes_skipped);
		return 1;
	};
	resource_handler->read = [](struct _cef_resource_handler_t *self, void *data_out, int bytes_to_read, int *bytes_read,
				    struct _cef_resource_read_callback_t *callback) -> int {
		auto s = (file_cef_resource_handler_t *)self;
		auto reader = s->reader;
		if (reader) {
			*bytes_read = (int)s->reader->read(reader, data_out, 1, bytes_to_read);
		} else {
			*bytes_read = 0;
		}
		callback->cont(callback, *bytes_read);
		return 1;
	};
	resource_handler->read_response = [](struct _cef_resource_handler_t *self, void *data_out, int bytes_to_read,
					     int *bytes_read, struct _cef_callback_t *callback) -> int {
		auto s = (file_cef_resource_handler_t *)self;
		auto reader = s->reader;
		if (reader) {
			*bytes_read = (int)s->reader->read(reader, data_out, 1, bytes_to_read);
		} else {
			*bytes_read = 0;
		}
		callback->cont(callback);
		return 1;
	};
	resource_handler->cancel = [](struct _cef_resource_handler_t *self) {
		auto s = (file_cef_resource_handler_t *)self;
		if (s->reader) {
			s->reader->base.release(&s->reader->base);
			s->reader = nullptr;
		}
	};
	return resource_handler;
}

bool load_cef()
{
	if (cef) {
		return true;
	}

	obs_module_t *browserModule = obs_get_module("obs-browser");
	QCef *(*create_qcef)(void) = nullptr;
	if (browserModule) {
		auto lib = obs_get_module_lib(browserModule);
		create_qcef = (decltype(create_qcef))os_dlsym(lib, "obs_browser_create_qcef");
		if (create_qcef) {
			cef = create_qcef();

			auto libcef = os_dlopen("libcef");
			if (libcef) {
				cef_parse_url = (decltype(cef_parse_url))os_dlsym(libcef, "cef_parse_url");
				cef_string_userfree_free =
					(decltype(cef_string_userfree_free))os_dlsym(libcef, "cef_string_userfree_utf16_free");
				cef_string_userfree_utf8_alloc = (decltype(cef_string_userfree_utf8_alloc))os_dlsym(
					libcef, "cef_string_userfree_utf8_alloc");
				cef_string_utf16_to_utf8 =
					(decltype(cef_string_utf16_to_utf8))os_dlsym(libcef, "cef_string_utf16_to_utf8");
				cef_string_userfree_utf8_free =
					(decltype(cef_string_userfree_utf8_free))os_dlsym(libcef, "cef_string_userfree_utf8_free");
				cef_uridecode = (decltype(cef_uridecode))os_dlsym(libcef, "cef_uridecode");
				cef_stream_reader_create_for_file = (decltype(cef_stream_reader_create_for_file))os_dlsym(
					libcef, "cef_stream_reader_create_for_file");
				cef_get_mime_type = (decltype(cef_get_mime_type))os_dlsym(libcef, "cef_get_mime_type");
				cef_string_set = (decltype(cef_string_set))os_dlsym(libcef, "cef_string_utf16_set");
				cef_string_utf8_to_utf16 =
					(decltype(cef_string_utf8_to_utf16))os_dlsym(libcef, "cef_string_utf8_to_utf16");
				cef_string_userfree_utf16_alloc = (decltype(cef_string_userfree_utf16_alloc))os_dlsym(
					libcef, "cef_string_userfree_utf16_alloc");

				cef_string_set(u"https", 5, &scheme);
				cef_string_set(u"local.aitumsuite.tv", 19, &domain);
				auto t = (int (*)(const cef_string_t *, const cef_string_t *,
						  cef_scheme_handler_factory_t *))os_dlsym(libcef,
											   "cef_register_scheme_handler_factory");
				factory.create = scheme_factory;
				factory.base.size = sizeof(cef_scheme_handler_factory_t);
				factory.base.add_ref = [](cef_base_ref_counted_t *self) {
					UNUSED_PARAMETER(self);
				};
				factory.base.release = [](cef_base_ref_counted_t *self) -> int {
					UNUSED_PARAMETER(self);
					return 1;
				};
				factory.base.has_at_least_one_ref = [](cef_base_ref_counted_t *self) -> int {
					UNUSED_PARAMETER(self);
					return 1;
				};
				factory.base.has_one_ref = [](cef_base_ref_counted_t *self) -> int {
					UNUSED_PARAMETER(self);
					return 1;
				};
				t(&scheme, &domain, &factory);
			}
		}
	}
	return cef != nullptr;
}

static std::string GenId()
{
	std::random_device rd;
	std::mt19937_64 e2(rd());
	std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFF);

	uint64_t id = dist(e2);

	char id_str[20];
	snprintf(id_str, sizeof(id_str), "%016llX", (unsigned long long)id);
	return std::string(id_str);
}

class QuickThread : public QThread {
public:
	explicit inline QuickThread(std::function<void()> func_) : func(func_) {}

private:
	virtual void run() override { func(); }

	std::function<void()> func;
};

BrowserDock::BrowserDock(const char *name, const char *url_, QWidget *parent) : QWidget(parent), url(url_)
{
	setMinimumSize(200, 100);
	setObjectName(QString::fromUtf8(name));

	load_cef();
	if (!panel_cookies && cef) {
		if (!cef->init_browser()) {
			QEventLoop eventLoop;
			auto t = new QuickThread([&] {
				cef->wait_for_browser_init();
				QMetaObject::invokeMethod(&eventLoop, &QEventLoop::quit, Qt::QueuedConnection);
			});
			t->start();
			eventLoop.exec();
			t->wait();
			t->deleteLater();
		}
		const char *cookie_id = config_get_string(obs_frontend_get_profile_config(), "Panels", "CookieId");
		if (!cookie_id || cookie_id[0] == '\0') {
			config_set_string(obs_frontend_get_profile_config(), "Panels", "CookieId", GenId().c_str());
			cookie_id = config_get_string(obs_frontend_get_profile_config(), "Panels", "CookieId");
		}
		if (cookie_id && cookie_id[0] != '\0') {
			std::string sub_path;
			sub_path += "obs_profile_cookies/";
			sub_path += cookie_id;
			panel_cookies = cef->create_cookie_manager(sub_path);
		}
	}

	layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);
	if (cef) {
		cefWidget = cef->create_widget(this, url, panel_cookies);
		layout->addWidget(cefWidget);
	}
}

BrowserDock::~BrowserDock()
{
	layout->removeWidget(cefWidget);
	cefWidget->setParent(nullptr);
	cefWidget->deleteLater();
}

void BrowserDock::Refresh()
{
	if (cefWidget) {
		cefWidget->reloadPage();
	}
}

void BrowserDock::Reset()
{
	if (cefWidget) {
		cefWidget->setURL(url);
	}
}

void DestroyPanelCookieManager()
{
	if (!panel_cookies) {
		return;
	}
	panel_cookies->FlushStore();
	delete panel_cookies;
	panel_cookies = nullptr;
}
