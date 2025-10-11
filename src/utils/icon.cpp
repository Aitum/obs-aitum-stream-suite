#include "icon.hpp"

#include<QMainWindow>
#include <obs-frontend-api.h>

QIcon GetSceneIcon()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return main_window->property("sceneIcon").value<QIcon>();
}

QIcon GetGroupIcon()
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return main_window->property("groupIcon").value<QIcon>();
}

QIcon GetIconFromType(enum obs_icon_type icon_type)
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

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
		//TODO: Add ability for sources to define custom icons
		return main_window->property("defaultIcon").value<QIcon>();
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return main_window->property("audioProcessOutputIcon").value<QIcon>();
	default:
		return main_window->property("defaultIcon").value<QIcon>();
	}
}



QIcon getPlatformIconFromEndpoint(QString endpoint)
{

	if (endpoint.contains(QString::fromUtf8("ingest.global-contribute.live-video.net")) ||
	    endpoint.contains(QString::fromUtf8(".contribute.live-video.net")) ||
	    endpoint.contains(QString::fromUtf8(".twitch.tv"))) { // twitch
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
