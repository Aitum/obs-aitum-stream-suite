#include "icon.hpp"

#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QPainter>
#include <util/platform.h>
#ifndef _WIN32
#include <dlfcn.h>
#endif

QIcon GetSceneIcon()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		return QIcon();
	}
	return main_window->property("sceneIcon").value<QIcon>();
}

QIcon GetGroupIcon()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		return QIcon();
	}
	return main_window->property("groupIcon").value<QIcon>();
}

static bool obs_source_get_dark_icon_func_initialized = false;
static const char *(*obs_source_get_dark_icon_func)(const char *id) = nullptr;
static bool obs_source_get_light_icon_func_initialized = false;
static const char *(*obs_source_get_light_icon_func)(const char *id) = nullptr;

QIcon GetIconFromType(enum obs_icon_type icon_type, const char *id)
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main_window) {
		return QIcon();
	}

	switch (icon_type) {
	case OBS_ICON_TYPE_IMAGE:
		return main_window->property("imageIcon").value<QIcon>();
	case OBS_ICON_TYPE_COLOR:
		return main_window->property("colorIcon").value<QIcon>();
	case OBS_ICON_TYPE_SLIDESHOW:
		return main_window->property("slideshowIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return main_window->property("audioInputIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return main_window->property("audioOutputIcon").value<QIcon>();
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return main_window->property("desktopCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return main_window->property("windowCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return main_window->property("gameCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_CAMERA:
		return main_window->property("cameraIcon").value<QIcon>();
	case OBS_ICON_TYPE_TEXT:
		return main_window->property("textIcon").value<QIcon>();
	case OBS_ICON_TYPE_MEDIA:
		return main_window->property("mediaIcon").value<QIcon>();
	case OBS_ICON_TYPE_BROWSER:
		return main_window->property("browserIcon").value<QIcon>();
	case OBS_ICON_TYPE_CUSTOM:
		if (!id || !*id) {
			return main_window->property("defaultIcon").value<QIcon>();
		}
		if (obs_frontend_is_theme_dark()) {
			if (!obs_source_get_dark_icon_func_initialized) {
#ifdef _WIN32
				void *dl = os_dlopen("obs");
#else
				void *dl = dlopen(nullptr, RTLD_LAZY);
#endif
				obs_source_get_dark_icon_func =
					(const char *(*)(const char *))os_dlsym(dl, "obs_source_get_dark_icon");
				os_dlclose(dl);
				obs_source_get_dark_icon_func_initialized = true;
			}
			if (obs_source_get_dark_icon_func) {
				const char *path = obs_source_get_dark_icon_func(id);
				if (path && *path) {
					QIcon icon(path);
					bfree((void *)path);
					return icon;
				} else if (path) {
					bfree((void *)path);
				}
			}
		} else {
			if (!obs_source_get_light_icon_func_initialized) {
#ifdef _WIN32
				void *dl = os_dlopen("obs");
#else
				void *dl = dlopen(nullptr, RTLD_LAZY);
#endif
				obs_source_get_light_icon_func =
					(const char *(*)(const char *))os_dlsym(dl, "obs_source_get_light_icon");
				os_dlclose(dl);
				obs_source_get_light_icon_func_initialized = true;
			}
			if (obs_source_get_light_icon_func) {
				const char *path = obs_source_get_light_icon_func(id);
				if (path && *path) {
					QIcon icon(path);
					bfree((void *)path);
					return icon;
				} else if (path) {
					bfree((void *)path);
				}
			}
		}
		return main_window->property("defaultIcon").value<QIcon>();
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return main_window->property("audioProcessOutputIcon").value<QIcon>();
	default:
		return main_window->property("defaultIcon").value<QIcon>();
	}
}

extern bool isTwitchServer(QString outputServer);

QIcon getPlatformIconFromEndpoint(QString endpoint)
{

	if (isTwitchServer(endpoint)) { // twitch
		return QIcon(":/aitum/media/twitch.png");
	} else if (endpoint.contains(QString::fromUtf8(".youtube.com"))) { // youtube
		return QIcon(":/aitum/media/youtube.png");
	} else if (endpoint.contains(QString::fromUtf8("fa723fc1b171.global-contribute.live-video.net"))) { // kick
		return QIcon(":/aitum/media/kick.png");
	} else if (endpoint.contains(QString::fromUtf8(".tiktokcdn"))) { // tiktok
		return QIcon(":/aitum/media/tiktok.png");
	} else if (endpoint.contains(QString::fromUtf8(".pscp.tv"))) { // twitter
		return QIcon(":/aitum/media/twitter.png");
	} else if (endpoint.contains(QString::fromUtf8("livepush.trovo.live"))) { // trovo
		return QIcon(":/aitum/media/trovo.png");
	} else if (endpoint.contains(QString::fromUtf8(".facebook.com")) ||
		   endpoint.contains(QString::fromUtf8(".fbcdn.net"))) { // facebook
		return QIcon(":/aitum/media/facebook.png");
	} else { // unknown
		return QIcon(":/aitum/media/unknown.png");
	}
}

QIcon create2StateIcon(QString fileOn, QString fileOff)
{
	QIcon icon = QIcon(fileOff);
	icon.addFile(fileOn, QSize(), QIcon::Normal, QIcon::On);
	return icon;
}

QIcon generateEmojiQIcon(QString ch, QColor color)
{
	QPixmap pixmap(32, 32);
	pixmap.fill(Qt::transparent);

	QPainter painter(&pixmap);
	QFont font = painter.font();
	font.setPixelSize(32);
	font.setBold(true);
	painter.setFont(font);
	painter.setPen(color);
	painter.drawText(pixmap.rect(), Qt::AlignCenter, ch);
	return QIcon(pixmap);
}
