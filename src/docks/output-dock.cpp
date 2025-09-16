#include "output-dock.hpp"

#include <obs-module.h>
#include <QLabel>
#include <QIcon>
#include <QPushButton>

QIcon create2StateIcon(QString fileOn, QString fileOff);

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

OutputDock::OutputDock(QWidget *parent) : QFrame(parent)
{
	mainLayout = new QGridLayout(this);
	//mainLayout->setContentsMargins(0, 0, 0, 0);
	setLayout(mainLayout);

	mainLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("Canvas"))), 0, 0);
	mainLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("Output"))), 0, 1, 1, 3);

	mainLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("MainCanvas"))), 1, 0);

	auto mainPlatformIconLabel = new QLabel;
	auto platformIcon = getPlatformIconFromEndpoint(QString::fromUtf8(""));
	mainPlatformIconLabel->setPixmap(platformIcon.pixmap(outputPlatformIconSize, outputPlatformIconSize));
	mainLayout->addWidget(mainPlatformIconLabel, 1, 1);

	mainLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("StreamMain"))), 1, 2);

	auto mainStreamButton = new QPushButton;
	mainStreamButton->setCheckable(true);
	mainStreamButton->setIcon(create2StateIcon(":/aitum/media/streaming.svg", ":/aitum/media/stream.svg"));
	mainStreamButton->setStyleSheet("QAbstractButton:checked{background: rgb(0,210,153);}");
	mainLayout->addWidget(mainStreamButton, 1, 3);

	mainLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("MainCanvas"))), 2, 0);
	mainLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("RecordMain"))), 2, 1, 1, 2);
	auto mainRecordButton = new QPushButton;
	mainRecordButton->setCheckable(true);
	mainRecordButton->setIcon(create2StateIcon(":/aitum/media/recording.svg", ":/aitum/media/record.svg"));
	mainRecordButton->setStyleSheet("QAbstractButton:checked{background: rgb(255,0,0);}");
	mainLayout->addWidget(mainRecordButton, 2, 3);

	mainLayout->setColumnStretch(0, 1);
	mainLayout->setColumnStretch(1, 0);
	mainLayout->setColumnStretch(2, 2);
	mainLayout->setColumnStretch(3, 0);
}

OutputDock::~OutputDock()
{
	/*
	videoCheckTimer.stop();
	for (auto it = outputs.begin(); it != outputs.end(); it++) {
		auto old = std::get<obs_output_t *>(*it);
		signal_handler_t *signal = obs_output_get_signal_handler(old);
		signal_handler_disconnect(signal, "start", stream_output_start, this);
		signal_handler_disconnect(signal, "stop", stream_output_stop, this);
		auto service = obs_output_get_service(old);
		if (obs_output_active(old)) {
			obs_output_force_stop(old);
		}
		if (!exiting)
			obs_output_release(old);
		obs_service_release(service);
	}
	outputs.clear();
	
	obs_frontend_remove_event_callback(frontend_event, this);*/
}
