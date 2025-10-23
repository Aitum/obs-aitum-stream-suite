#include "record-output-dialog.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

RecordOutputDialog::RecordOutputDialog(QDialog *parent, QStringList _otherNames) : QDialog(parent), otherNames(_otherNames)
{

	setWindowTitle(obs_module_text("NewRecordOutputWindowTitle"));

	auto pageLayout = new QVBoxLayout;
	setModal(true);
	setContentsMargins(0, 0, 0, 0);
	setMinimumSize(650, 400);

	auto confirmButton = new QPushButton(QString::fromUtf8(obs_module_text("CreateRecordOutput")));
	confirmButton->setEnabled(false);

	auto formLayout = new QFormLayout;
	formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	formLayout->setSpacing(12);

	auto nameField = new QLineEdit;
	connect(nameField, &QLineEdit::textEdited, [this, nameField, confirmButton] {
		outputName = nameField->text();
		validateOutputs(confirmButton);
	});
	nameField->setText(QString::fromUtf8(obs_module_text("RecordOutput")));
	outputName = nameField->text();
	formLayout->addRow(QString::fromUtf8(obs_module_text("OutputName")), nameField);

	QLayout *recordPathLayout = new QHBoxLayout();
	auto recordPathEdit = new QLineEdit();
	recordPathEdit->setReadOnly(true);

	auto recordPathbutton = new QPushButton(QString::fromUtf8(obs_frontend_get_locale_string("Browse")));
	recordPathbutton->setProperty("themeID", "settingsButtons");
	connect(recordPathbutton, &QPushButton::clicked, [this, recordPathEdit, confirmButton] {
		const QString dir = QFileDialog::getExistingDirectory(
			this, QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Simple.SavePath")),
			recordPathEdit->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		if (dir.isEmpty())
			return;
		recordPathEdit->setText(dir);
		recordPath = dir;
		validateOutputs(confirmButton);
	});

	recordPathLayout->addWidget(recordPathEdit);
	recordPathLayout->addWidget(recordPathbutton);

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Simple.SavePath")),
			   recordPathLayout);

	auto filenameFormatEdit = new QLineEdit;
	QStringList specList =
		QString::fromUtf8(obs_frontend_get_locale_string("FilenameFormatting.completer")).split(QRegularExpression("\n"));
	filenameFormatEdit->setText(specList.first());
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
	filenameFormat = specList.first();

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.Recording.Filename")),
			   filenameFormatEdit);

	auto fileFormatCombo = new QComboBox;
	fileFormatCombo->addItem(QString::fromUtf8("mp4"));
	fileFormatCombo->addItem(QString::fromUtf8("hybrid_mp4"));
	fileFormatCombo->addItem(QString::fromUtf8("fragmented_mp4"));
	fileFormatCombo->addItem(QString::fromUtf8("mov"));
	fileFormatCombo->addItem(QString::fromUtf8("hybrid_mov"));
	fileFormatCombo->addItem(QString::fromUtf8("fragmented_mov"));
	fileFormatCombo->addItem(QString::fromUtf8("mkv"));
	fileFormatCombo->addItem(QString::fromUtf8("ts"));
	fileFormatCombo->addItem(QString::fromUtf8("mp3u8"));
	connect(fileFormatCombo, &QComboBox::currentTextChanged, [this, fileFormatCombo, confirmButton] {
		fileFormat = fileFormatCombo->currentText();
		validateOutputs(confirmButton);
	});
	fileFormatCombo->setCurrentText(QString::fromUtf8("hybrid_mp4"));

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Format")), fileFormatCombo);

	auto controlsLayout = new QHBoxLayout;
	controlsLayout->setSpacing(0);
	controlsLayout->setContentsMargins(0, 0, 0, 0);

	connect(confirmButton, &QPushButton::clicked, [this] { accept(); });
	controlsLayout->addWidget(confirmButton, 0, Qt::AlignRight);

	pageLayout->addLayout(formLayout);
	pageLayout->addLayout(controlsLayout);

	setLayout(pageLayout);
}

RecordOutputDialog::RecordOutputDialog(QDialog *parent, QString name, QString path, QString filename, QString format,
				       QStringList _otherNames)
	: QDialog(parent),
	  otherNames(_otherNames)
{
	outputName = name;
	recordPath = path;
	filenameFormat = filename;
	fileFormat = format;

	setWindowTitle(obs_module_text("EditRecordOutputWindowTitle"));

	auto pageLayout = new QVBoxLayout;
	setModal(true);
	setContentsMargins(0, 0, 0, 0);
	setMinimumSize(650, 400);

	auto confirmButton = new QPushButton(QString::fromUtf8(obs_module_text("SaveRecordOutput")));
	confirmButton->setEnabled(false);

	auto formLayout = new QFormLayout;
	formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	formLayout->setSpacing(12);

	auto nameField = new QLineEdit;
	nameField->setText(outputName);
	connect(nameField, &QLineEdit::textEdited, [this, nameField, confirmButton] {
		outputName = nameField->text();
		validateOutputs(confirmButton);
	});
	formLayout->addRow(QString::fromUtf8(obs_module_text("OutputName")), nameField);

	QLayout *recordPathLayout = new QHBoxLayout();
	auto recordPathEdit = new QLineEdit();
	recordPathEdit->setReadOnly(true);
	recordPathEdit->setText(recordPath);

	auto recordPathbutton = new QPushButton(QString::fromUtf8(obs_frontend_get_locale_string("Browse")));
	recordPathbutton->setProperty("themeID", "settingsButtons");
	connect(recordPathbutton, &QPushButton::clicked, [this, recordPathEdit, confirmButton] {
		const QString dir = QFileDialog::getExistingDirectory(
			this, QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Simple.SavePath")),
			recordPathEdit->text(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
		if (dir.isEmpty())
			return;
		recordPathEdit->setText(dir);
		recordPath = dir;
		validateOutputs(confirmButton);
	});

	recordPathLayout->addWidget(recordPathEdit);
	recordPathLayout->addWidget(recordPathbutton);

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Simple.SavePath")),
			   recordPathLayout);

	auto filenameFormatEdit = new QLineEdit;
	filenameFormatEdit->setText(filenameFormat);
	QStringList specList =
		QString::fromUtf8(obs_frontend_get_locale_string("FilenameFormatting.completer")).split(QRegularExpression("\n"));
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

	auto fileFormatCombo = new QComboBox;
	fileFormatCombo->addItem(QString::fromUtf8("mp4"));
	fileFormatCombo->addItem(QString::fromUtf8("hybrid_mp4"));
	fileFormatCombo->addItem(QString::fromUtf8("fragmented_mp4"));
	fileFormatCombo->addItem(QString::fromUtf8("mov"));
	fileFormatCombo->addItem(QString::fromUtf8("hybrid_mov"));
	fileFormatCombo->addItem(QString::fromUtf8("fragmented_mov"));
	fileFormatCombo->addItem(QString::fromUtf8("mkv"));
	fileFormatCombo->addItem(QString::fromUtf8("ts"));
	fileFormatCombo->addItem(QString::fromUtf8("mp3u8"));
	fileFormatCombo->setCurrentText(fileFormat);
	connect(fileFormatCombo, &QComboBox::currentTextChanged, [this, fileFormatCombo, confirmButton] {
		fileFormat = fileFormatCombo->currentText();
		validateOutputs(confirmButton);
	});

	formLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Format")), fileFormatCombo);

	auto controlsLayout = new QHBoxLayout;
	controlsLayout->setSpacing(0);
	controlsLayout->setContentsMargins(0, 0, 0, 0);

	connect(confirmButton, &QPushButton::clicked, [this] { accept(); });
	controlsLayout->addWidget(confirmButton, 0, Qt::AlignRight);

	pageLayout->addLayout(formLayout);
	pageLayout->addLayout(controlsLayout);

	setLayout(pageLayout);
}

void RecordOutputDialog::validateOutputs(QPushButton *confirmButton)
{

	if (outputName.isEmpty() || otherNames.contains(outputName)) {
		confirmButton->setEnabled(false);
	} else if (recordPath.isEmpty() || filenameFormat.isEmpty() || fileFormat.isEmpty()) {
		confirmButton->setEnabled(false);
	} else {
		confirmButton->setEnabled(true);
	}
}
