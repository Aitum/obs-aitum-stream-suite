#pragma once
#include <QIcon>
#include <obs.h>

QIcon GetSceneIcon();
QIcon GetGroupIcon();
QIcon GetIconFromType(enum obs_icon_type icon_type);
QIcon create2StateIcon(QString fileOn, QString fileOff);
QIcon getPlatformIconFromEndpoint(QString endpoint);
