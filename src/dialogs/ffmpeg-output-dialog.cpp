#include "ffmpeg-output-dialog.hpp"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>
#include <QCompleter>
#include <QLabel>
#include <QSpinBox>
#include <QScrollArea>

FfmpegOutputDialog::FfmpegOutputDialog(QDialog *parent, QStringList _otherNames, obs_data_t *settings)
	: QDialog(parent),
	  otherNames(_otherNames)
{
	if (settings) {
		outputName = QString::fromUtf8(obs_data_get_string(settings, "name"));
		saveFile = obs_data_get_bool(settings, "save_file");
		path = QString::fromUtf8(obs_data_get_string(settings, "directory"));
		extension = obs_data_get_string(settings, "extension");
		filenameFormat = QString::fromUtf8(obs_data_get_string(settings, "format"));
		allowSpaces = obs_data_get_bool(settings, "allow_spaces");
		url = QString::fromUtf8(obs_data_get_string(settings, "url"));
		formatName = obs_data_get_string(settings, "format_name");
		mimeType = obs_data_get_string(settings, "format_mime_type");
		muxCustom = QString::fromUtf8(obs_data_get_string(settings, "muxer_settings"));
		gopSize = obs_data_get_int(settings, "gop_size");
		rescale = obs_data_get_bool(settings, "rescale");
		scaleWidth = obs_data_get_int(settings, "scale_width");
		scaleHeight = obs_data_get_int(settings, "scale_height");
		vBitrate = obs_data_get_int(settings, "video_bitrate");
		vEncoder = obs_data_get_string(settings, "video_encoder");
		vEncoderId = obs_data_get_int(settings, "video_encoder_id");
		vEncCustom = QString::fromUtf8(obs_data_get_string(settings, "video_settings"));
		aBitrate = obs_data_get_int(settings, "audio_bitrate");
		aEncoder = obs_data_get_string(settings, "audio_encoder");
		aEncoderId = obs_data_get_int(settings, "audio_encoder_id");
		aEncCustom = QString::fromUtf8(obs_data_get_string(settings, "audio_settings"));
		aMixes = obs_data_get_int(settings, "audio_mixes");

		setWindowTitle(obs_module_text("EditFfmpegOutputWindowTitle"));
	} else {
		outputName = QString::fromUtf8(obs_module_text("FfmpegOutput"));
		setWindowTitle(QString::fromUtf8(obs_module_text("NewFfmpegOutputWindowTitle")));
	}

	auto pageLayout = new QVBoxLayout;
	setModal(true);
	setContentsMargins(0, 0, 0, 0);
	setMinimumSize(400, 120);

	auto confirmButton = new QPushButton(QString::fromUtf8(obs_module_text(settings ? "SaveFfmpegOutput" : "CreateFfmpegOutput")));
	confirmButton->setEnabled(false);

	auto formLayout = new QFormLayout;
	formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	formLayout->setSpacing(12);

	auto filenameFormatEdit = new QLineEdit;
	auto nameField = new QLineEdit;
	nameField->setText(outputName);
	connect(nameField, &QLineEdit::textEdited, [this, nameField, confirmButton, filenameFormatEdit] {
		auto oldName = outputName;
		outputName = nameField->text();
		auto spec = QString::fromUtf8(obs_frontend_get_locale_string("FilenameFormatting.completer"))
				    .split(QRegularExpression("\n"))
				    .first();
		if (filenameFormat.isEmpty() || filenameFormat == spec || filenameFormat == spec + " " + oldName) {
			filenameFormat = spec + " " + outputName;
			filenameFormatEdit->setText(filenameFormat);
		}
		validateOutputs(confirmButton);
	});
	formLayout->addRow(QString::fromUtf8(obs_module_text("OutputName")), nameField);

	auto advOutFFType = new QComboBox;
	advOutFFType->addItem(
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.Type.RecordToFile")));
	advOutFFType->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.Type.URL")));

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.Type")),
			   advOutFFType);

	auto advOutFFURL = new QLineEdit;
	advOutFFURL->setText(url);
	connect(advOutFFURL, &QLineEdit::textEdited, [this, advOutFFURL, confirmButton] {
		url = advOutFFURL->text();
		validateOutputs(confirmButton);
	});
	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.SavePathURL")),
			   advOutFFURL);
	formLayout->setRowVisible(advOutFFURL, !saveFile);

	QLayout *recordPathLayout = new QHBoxLayout();
	auto advOutFFRecPath = new QLineEdit();
	advOutFFRecPath->setText(path);
	advOutFFRecPath->setReadOnly(true);

	auto advOutFFPathBrowse = new QPushButton(QString::fromUtf8(obs_frontend_get_locale_string("Browse")));
	advOutFFPathBrowse->setProperty("themeID", "settingsButtons");
	connect(advOutFFPathBrowse, &QPushButton::clicked, [this, advOutFFRecPath, confirmButton] {
		const QString dir = QFileDialog::getExistingDirectory(
			this, QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Simple.SavePath")),
			advOutFFRecPath->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		if (dir.isEmpty())
			return;
		advOutFFRecPath->setText(dir);
		path = dir;
		validateOutputs(confirmButton);
	});

	recordPathLayout->addWidget(advOutFFRecPath);
	recordPathLayout->addWidget(advOutFFPathBrowse);

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.SavePathURL")),
			   recordPathLayout);

	formLayout->setRowVisible(recordPathLayout, saveFile);

	QStringList specList =
		QString::fromUtf8(obs_frontend_get_locale_string("FilenameFormatting.completer")).split(QRegularExpression("\n"));
	if (filenameFormat.isEmpty())
		filenameFormat = specList.first() + " " + outputName;

	filenameFormatEdit->setText(filenameFormat);

	QCompleter *specCompleter = new QCompleter(specList);
	specCompleter->setCaseSensitivity(Qt::CaseSensitive);
	specCompleter->setFilterMode(Qt::MatchContains);
	filenameFormatEdit->setCompleter(specCompleter);
	filenameFormatEdit->setToolTip(QString::fromUtf8(obs_frontend_get_locale_string("FilenameFormatting.TT")));

	connect(filenameFormatEdit, &QLineEdit::textEdited, [this, filenameFormatEdit, confirmButton](const QString &text) {
		QString safeStr = text;

#ifdef __APPLE__
		safeStr.replace(QRegularExpression("[:]"), "");
#elif defined(_WIN32)
	safeStr.replace(QRegularExpression("[<>:\"\\|\\?\\*]"), "");
#else
		// TODO: Add filtering for other platforms
#endif
		if (text != safeStr)
			filenameFormatEdit->setText(safeStr);
		filenameFormat = safeStr;
		validateOutputs(confirmButton);
	});

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.Recording.Filename")),
			   filenameFormatEdit);

	formLayout->setRowVisible(filenameFormatEdit, saveFile);

	auto advOutFFNoSpace = new QCheckBox;
	advOutFFNoSpace->setText(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.NoSpaceFileName")));
	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.SavePathURL")),
			   advOutFFNoSpace);

	connect(advOutFFType, &QComboBox::currentIndexChanged,
		[this, advOutFFType, advOutFFNoSpace, advOutFFURL, formLayout, recordPathLayout, filenameFormatEdit,
		 confirmButton] {
			saveFile = advOutFFType->currentIndex() == 0;
			formLayout->setRowVisible(advOutFFNoSpace, saveFile);
			formLayout->setRowVisible(advOutFFURL, !saveFile);
			formLayout->setRowVisible(recordPathLayout, saveFile);
			formLayout->setRowVisible(filenameFormatEdit, saveFile);
			validateOutputs(confirmButton);
		});
	advOutFFType->setCurrentIndex(saveFile ? 0 : 1);

	auto advOutFFFormat = new QComboBox(this);
	advOutFFFormat->blockSignals(true);

	void *i = 0;
	const AVOutputFormat *output_format;
	while ((output_format = av_muxer_iterate(&i)) != nullptr) {
		if (output_format->priv_class && (output_format->priv_class->category == AV_CLASS_CATEGORY_DEVICE_VIDEO_OUTPUT ||
						  output_format->priv_class->category == AV_CLASS_CATEGORY_DEVICE_AUDIO_OUTPUT ||
						  output_format->priv_class->category == AV_CLASS_CATEGORY_DEVICE_OUTPUT))
			continue;

		bool audio = output_format->audio_codec != AV_CODEC_ID_NONE;
		bool video = output_format->video_codec != AV_CODEC_ID_NONE;

		if (!audio && !video)
			continue;
		QString itemText = QString::fromUtf8(output_format->name);
		if (audio ^ video)
			itemText += QString(" (%1)").arg(audio ? QString::fromUtf8(obs_frontend_get_locale_string(
									 "Basic.Settings.Output.Adv.FFmpeg.FormatAudio"))
							       : QString::fromUtf8(obs_frontend_get_locale_string(
									 "Basic.Settings.Output.Adv.FFmpeg.FormatVideo")));
		advOutFFFormat->addItem(itemText,
					QVariant::fromValue(std::tuple<std::string, std::string, std::string, std::string,
								       AVCodecID, AVCodecID, const struct AVCodecTag *const *>(
						output_format->name, output_format->mime_type ? output_format->mime_type : "",
						output_format->long_name ? output_format->long_name : "",
						output_format->extensions ? output_format->extensions : "",
						output_format->audio_codec, output_format->video_codec, output_format->codec_tag)));
	}

	advOutFFFormat->model()->sort(0);

	advOutFFFormat->insertItem(
		0, QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.FormatDefault")));

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.Format")),
			   advOutFFFormat);

	auto advOutFFFormatDesc = new QLabel;
	advOutFFFormatDesc->setObjectName("advOutFFFormatDesc");

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.FormatDesc")),
			   advOutFFFormatDesc);

	advOutFFFormat->blockSignals(false);

	auto advOutFFAEncoder = new QComboBox;
	auto advOutFFVEncoder = new QComboBox;

	connect(advOutFFFormat, &QComboBox::currentIndexChanged,
		[this, advOutFFFormat, advOutFFFormatDesc, advOutFFAEncoder, advOutFFVEncoder, confirmButton](int index) {
			QVariant v = advOutFFFormat->itemData(index);
			advOutFFAEncoder->blockSignals(true);
			advOutFFVEncoder->blockSignals(true);
			advOutFFAEncoder->clear();
			advOutFFVEncoder->clear();
			if (v.isNull()) {
				advOutFFFormatDesc->clear();
				advOutFFAEncoder->blockSignals(false);
				advOutFFVEncoder->blockSignals(false);
				validateOutputs(confirmButton);
				return;
			}

			auto formatTuple = v.value<std::tuple<std::string, std::string, std::string, std::string, AVCodecID,
							      AVCodecID, const struct AVCodecTag *const *>>();
			formatName = std::get<0>(formatTuple);
			mimeType = std::get<1>(formatTuple);
			extension = std::get<3>(formatTuple);
			auto n = extension.find(',');
			if (n != std::string::npos)
				extension = extension.substr(0, n);

			advOutFFFormatDesc->setText(QString::fromUtf8(std::get<2>(formatTuple).c_str()));

			auto default_audio_codec = std::get<4>(formatTuple);
			auto default_video_codec = std::get<5>(formatTuple);
			auto codec_tag = std::get<6>(formatTuple);

			QString defaultAudioCodecName;
			QString defaultVideoCodecName;
			QString currentAudioCodecName;
			QString currentVideoCodecName;
			const AVCodec *codec;
			void *i = 0;
			while ((codec = av_codec_iterate(&i)) != nullptr) {
				// Not an encoding codec
				if (!av_codec_is_encoder(codec))
					continue;
				// check if codec is compatible with the selected format
				if (!ignoreCompat && !av_codec_get_tag(codec_tag, codec->id))
					continue;

				if (codec->type == AVMEDIA_TYPE_VIDEO) {
					QString codecName;
					if (codec->long_name) {
						codecName = QString("%1 - %2").arg(QString::fromUtf8(codec->name),
										   QString::fromUtf8(codec->long_name));
					} else {
						codecName = QString::fromUtf8(codec->name);
					}
					if (defaultVideoCodecName.isEmpty() && codec->id == default_video_codec) {
						codecName += QString(" (%1)").arg(QString::fromUtf8(obs_frontend_get_locale_string(
							"Basic.Settings.Output.Adv.FFmpeg.AVEncoderDefault")));
						defaultVideoCodecName = codecName;
					}
					if (currentVideoCodecName.isEmpty() && !vEncoder.empty() && vEncoder == codec->name) {
						currentVideoCodecName = codecName;
					}
					advOutFFVEncoder->addItem(codecName, QVariant::fromValue(std::tuple<std::string, int>(
										     codec->name, codec->id)));
				} else if (codec->type == AVMEDIA_TYPE_AUDIO) {
					QString codecName;
					if (codec->long_name) {
						codecName = QString("%1 - %2").arg(QString::fromUtf8(codec->name),
										   QString::fromUtf8(codec->long_name));
					} else {
						codecName = QString::fromUtf8(codec->name);
					}
					if (defaultAudioCodecName.isEmpty() && codec->id == default_audio_codec) {
						codecName += QString(" (%1)").arg(QString::fromUtf8(obs_frontend_get_locale_string(
							"Basic.Settings.Output.Adv.FFmpeg.AVEncoderDefault")));
						defaultAudioCodecName = codecName;
					}
					if (currentAudioCodecName.isEmpty() && !aEncoder.empty() && aEncoder == codec->name) {
						currentAudioCodecName = codecName;
					}
					advOutFFAEncoder->addItem(codecName, QVariant::fromValue(std::tuple<std::string, int>(
										     codec->name, codec->id)));
				}
			}
			advOutFFAEncoder->model()->sort(0);
			advOutFFVEncoder->model()->sort(0);

			advOutFFAEncoder->insertItem(0,
						     QString::fromUtf8(obs_frontend_get_locale_string(
							     "Basic.Settings.Output.Adv.FFmpeg.AVEncoderDisable")),
						     QVariant());
			advOutFFVEncoder->insertItem(0,
						     QString::fromUtf8(obs_frontend_get_locale_string(
							     "Basic.Settings.Output.Adv.FFmpeg.AVEncoderDisable")),
						     QVariant());

			if (!currentAudioCodecName.isEmpty()) {
				advOutFFAEncoder->setCurrentText(currentAudioCodecName);
			} else if (!defaultAudioCodecName.isEmpty()) {
				advOutFFAEncoder->setCurrentText(defaultAudioCodecName);
				aEncoderId = std::get<4>(formatTuple);
			}
			if (!currentVideoCodecName.isEmpty()) {
				advOutFFVEncoder->setCurrentText(currentVideoCodecName);
			} else if (!defaultVideoCodecName.isEmpty()) {
				advOutFFVEncoder->setCurrentText(defaultVideoCodecName);
				vEncoderId = std::get<5>(formatTuple);
			}

			advOutFFAEncoder->blockSignals(false);
			advOutFFVEncoder->blockSignals(false);
			validateOutputs(confirmButton);
		});

	advOutFFFormat->setCurrentIndex(0);
	for (int i = 0; i < advOutFFFormat->count(); i++) {
		QVariant v = advOutFFFormat->itemData(i);
		if (v.isNull())
			continue;

		auto formatTuple = v.value<std::tuple<std::string, std::string, std::string, std::string, AVCodecID, AVCodecID,
						      const struct AVCodecTag *const *>>();
		if (std::get<0>(formatTuple) == formatName && std::get<1>(formatTuple) == mimeType) {
			advOutFFFormat->setCurrentIndex(i);
			advOutFFFormatDesc->setText(QString::fromUtf8(std::get<2>(formatTuple).c_str()));
			break;
		}
	}

	auto advOutFFMCfg = new QLineEdit;
	advOutFFMCfg->setText(muxCustom);
	connect(advOutFFMCfg, &QLineEdit::textEdited, [this, advOutFFMCfg] { muxCustom = advOutFFMCfg->text(); });
	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.MuxerSettings")),
			   advOutFFMCfg);

	auto advOutFFVBitrate = new QSpinBox;
	advOutFFVBitrate->setSingleStep(50);
	advOutFFVBitrate->setSuffix(" Kbps");
	advOutFFVBitrate->setRange(0, 1000000000);
	advOutFFVBitrate->setValue(vBitrate);
	connect(advOutFFVBitrate, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) { vBitrate = value; });
	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.VideoBitrate")),
			   advOutFFVBitrate);

	auto advOutFFVGOPSize = new QSpinBox;
	advOutFFVGOPSize->setRange(0, 1000000000);
	advOutFFVGOPSize->setValue(gopSize);
	connect(advOutFFVGOPSize, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) { gopSize = value; });
	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.GOPSize")),
			   advOutFFVGOPSize);

	auto advOutFFUseRescale = new QCheckBox;
	auto advOutFFRescale = new QComboBox;
	advOutFFUseRescale->setText(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.Rescale")));
	advOutFFUseRescale->setLayoutDirection(Qt::RightToLeft);
	advOutFFUseRescale->setChecked(rescale);
	connect(advOutFFUseRescale, &QCheckBox::toggled, [this, advOutFFUseRescale, advOutFFRescale] {
		rescale = advOutFFUseRescale->isChecked();
		advOutFFRescale->setEnabled(rescale);
	});

	QRegularExpression rx("\\d{1,5}x\\d{1,5}");
	QValidator *validator = new QRegularExpressionValidator(rx, this);
	advOutFFRescale->setEditable(true);
	advOutFFRescale->lineEdit()->setValidator(validator);
	advOutFFRescale->setEnabled(rescale);
	advOutFFRescale->addItem("1280x720");
	advOutFFRescale->addItem("1920x1080");
	advOutFFRescale->addItem("2560x1440");
	advOutFFRescale->addItem("720x1280");
	advOutFFRescale->addItem("1080x1920");
	advOutFFRescale->addItem("1440x2560");
	if (scaleWidth > 0 && scaleHeight > 0)
		advOutFFRescale->setCurrentText(QString("%1x%2").arg(scaleWidth).arg(scaleHeight));

	connect(advOutFFRescale, &QComboBox::currentTextChanged, [this, advOutFFRescale] {
		auto text = advOutFFRescale->currentText();
		auto parts = text.split("x");
		if (parts.size() != 2)
			return;
		scaleWidth = parts[0].toInt();
		scaleHeight = parts[1].toInt();
	});

	formLayout->addRow(advOutFFUseRescale, advOutFFRescale);

	auto advOutFFIgnoreCompat = new QCheckBox;
	advOutFFIgnoreCompat->setText(
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.IgnoreCodecCompat")));
	advOutFFIgnoreCompat->setChecked(ignoreCompat);
	connect(advOutFFIgnoreCompat, &QCheckBox::toggled, [this, advOutFFFormat](bool checked) {
		ignoreCompat = checked;
		advOutFFFormat->currentIndexChanged(advOutFFFormat->currentIndex());
	});

	formLayout->addRow(nullptr, advOutFFIgnoreCompat);

	connect(advOutFFVEncoder, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, advOutFFVEncoder] {
		auto variant = advOutFFVEncoder->currentData();
		if (variant.isNull())
			return;
		auto codec = variant.value<std::tuple<std::string, int>>();
		vEncoder = std::get<0>(codec);
		vEncoderId = std::get<1>(codec);
	});
	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.VEncoder")),
			   advOutFFVEncoder);

	auto advOutFFVCfg = new QLineEdit;
	advOutFFVCfg->setText(vEncCustom);
	connect(advOutFFVCfg, &QLineEdit::textEdited, [this, advOutFFVCfg] { vEncCustom = advOutFFVCfg->text(); });

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.VEncoderSettings")),
			   advOutFFVCfg);

	auto advOutFFABitrate = new QSpinBox;
	advOutFFABitrate->setRange(32, 4096);
	advOutFFABitrate->setSingleStep(16);
	advOutFFABitrate->setValue(aBitrate);
	advOutFFABitrate->setSuffix(" Kbps");
	connect(advOutFFABitrate, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) { aBitrate = value; });

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.AudioBitrate")),
			   advOutFFABitrate);

	auto trackLayout = new QHBoxLayout;
	for (int i = 0; i < 6; i++) {
		auto trackBtn = new QCheckBox(QString::number(i + 1));
		trackBtn->setChecked((aMixes & (1 << i)) != 0);
		connect(trackBtn, &QCheckBox::toggled, [this, i](bool checked) {
			if (checked)
				aMixes |= (1 << i);
			else
				aMixes &= ~(1 << i);
		});
		trackLayout->addWidget(trackBtn);
	}

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.AudioTrack")), trackLayout);


	connect(advOutFFAEncoder, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, advOutFFAEncoder] {
		auto variant = advOutFFAEncoder->currentData();
		if (variant.isNull())
			return;
		auto codec = variant.value<std::tuple<std::string, int>>();
		aEncoder = std::get<0>(codec);
		aEncoderId = std::get<1>(codec);
	});
	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.AEncoder")),
			   advOutFFAEncoder);

	auto advOutFFACfg = new QLineEdit;
	advOutFFACfg->setText(aEncCustom);
	connect(advOutFFACfg, &QLineEdit::textEdited, [this, advOutFFACfg] { aEncCustom = advOutFFACfg->text(); });

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.FFmpeg.AEncoderSettings")),
			   advOutFFACfg);

	auto controlsLayout = new QHBoxLayout;
	controlsLayout->setSpacing(0);
	controlsLayout->setContentsMargins(0, 0, 0, 0);

	connect(confirmButton, &QPushButton::clicked, [this] { accept(); });
	controlsLayout->addWidget(confirmButton, 0, Qt::AlignRight);

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	auto w = new QWidget;
	w->setLayout(formLayout);
	scrollArea->setWidget(w);

	pageLayout->addWidget(scrollArea);
	pageLayout->addLayout(controlsLayout);

	setLayout(pageLayout);
}

void FfmpegOutputDialog::validateOutputs(QPushButton *confirmButton)
{
	if (outputName.isEmpty() || otherNames.contains(outputName)) {
		confirmButton->setEnabled(false);
		return;
	}
	if (formatName.empty() || mimeType.empty()) {
		confirmButton->setEnabled(false);
		return;
	}
	if (saveFile && (extension.empty() || path.isEmpty() || filenameFormat.isEmpty())) {
		confirmButton->setEnabled(false);
		return;
	}
	if (!saveFile && url.isEmpty()) {
		confirmButton->setEnabled(false);
		return;
	}
	confirmButton->setEnabled(true);
}

void FfmpegOutputDialog::SaveSettings(obs_data_t *settings)
{
	// Set the info from the output dialog
	obs_data_set_string(settings, "name", outputName.toUtf8().constData());
	obs_data_set_bool(settings, "save_file", saveFile);
	obs_data_set_string(settings, "directory", path.toUtf8().constData());
	obs_data_set_string(settings, "extension", extension.c_str());
	obs_data_set_string(settings, "format", filenameFormat.toUtf8().constData());
	//obs_data_set_bool(settings, "overwrite_if_exists", overwriteIfExists);
	obs_data_set_bool(settings, "allow_spaces", allowSpaces);

	obs_data_set_string(settings, "url", url.toUtf8().constData());
	obs_data_set_string(settings, "format_name", formatName.c_str());
	obs_data_set_string(settings, "format_mime_type", mimeType.c_str());
	obs_data_set_string(settings, "muxer_settings", muxCustom.toUtf8().constData());
	obs_data_set_int(settings, "gop_size", gopSize);
	obs_data_set_int(settings, "video_bitrate", vBitrate);
	obs_data_set_bool(settings, "rescale", rescale);
	obs_data_set_int(settings, "scale_width", scaleWidth);
	obs_data_set_int(settings, "scale_height", scaleHeight);
	obs_data_set_string(settings, "video_encoder", vEncoder.c_str());
	obs_data_set_int(settings, "video_encoder_id", vEncoderId);
	obs_data_set_string(settings, "video_settings", vEncCustom.toUtf8().constData());
	obs_data_set_int(settings, "audio_bitrate", aBitrate);
	obs_data_set_string(settings, "audio_encoder", aEncoder.c_str());
	obs_data_set_int(settings, "audio_encoder_id", aEncoderId);
	obs_data_set_string(settings, "audio_settings", aEncCustom.toUtf8().constData());
	obs_data_set_int(settings, "audio_mixes", aMixes);
}
