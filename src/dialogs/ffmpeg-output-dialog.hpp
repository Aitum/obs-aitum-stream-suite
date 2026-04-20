#pragma once
#include <QDialog>
#include <obs.h>

class FfmpegOutputDialog : public QDialog {
	Q_OBJECT
private:
	QStringList otherNames;
	QString outputName;
	bool saveFile;
	QString path;
	std::string extension;
	QString filenameFormat;
	//bool overwriteIfExists;
	bool allowSpaces;
	QString url;
	std::string formatName;
	std::string mimeType;
	QString muxCustom;
	int gopSize = 250;
	int vBitrate = 2500;
	bool rescale = false;
	int scaleWidth = 0;
	int scaleHeight = 0;
	bool ignoreCompat = false;
	std::string vEncoder;
	int vEncoderId = 0;
	QString vEncCustom;
	int aBitrate = 128;
	std::string aEncoder;
	int aEncoderId = 0;
	QString aEncCustom;
	unsigned int aMixes = 1;

	void validateOutputs(QPushButton *confirmButton);

public:
	FfmpegOutputDialog(QDialog *parent, QStringList otherNames, obs_data_t* settings = nullptr);

	void SaveSettings(obs_data_t *settings);
};
