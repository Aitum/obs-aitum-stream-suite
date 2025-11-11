#include "../version.h"
#include "config-dialog.hpp"
#include "obs-module.h"
#include "stream-output-dialog.hpp"
#include "record-output-dialog.hpp"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCompleter>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QUrl>
#include <src/docks/canvas-dock.hpp>
#include <src/utils/color.hpp>
#include <sstream>
#include <util/config-file.h>
#include <util/dstr.h>
#include <util/platform.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif
#include "name-dialog.hpp"

template<typename T> std::string to_string_with_precision(const T a_value, const int n = 6)
{
	std::ostringstream out;
	out.precision(n);
	out << std::fixed << a_value;
	return std::move(out).str();
}

void RemoveWidget(QWidget *widget);

void RemoveLayoutItem(QLayoutItem *item)
{
	if (!item)
		return;
	RemoveWidget(item->widget());
	if (item->layout()) {
		while (QLayoutItem *item2 = item->layout()->takeAt(0))
			RemoveLayoutItem(item2);
	}
	delete item;
}

void RemoveWidget(QWidget *widget)
{
	if (!widget)
		return;
	if (widget->layout()) {
		auto l = widget->layout();
		QLayoutItem *item;
		while (l->count() > 0 && (item = l->takeAt(0))) {
			RemoveLayoutItem(item);
		}
		delete l;
	}
	delete widget;
}

OBSBasicSettings::OBSBasicSettings(QMainWindow *parent) : QDialog(parent)
{
	setMinimumWidth(983);
	setMinimumHeight(480);
	setWindowTitle(QString::fromUtf8(obs_module_text("AitumStreamSuiteSettings")));
	setSizeGripEnabled(true);

	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	listWidget = new QListWidget(this);

	listWidget->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
	listWidget->setMaximumWidth(180);
	QListWidgetItem *listwidgetitem = new QListWidgetItem(listWidget);
	listwidgetitem->setIcon(QIcon(QString::fromUtf8(":/settings/images/settings/general.svg")));
	//listwidgetitem->setProperty("themeID", QVariant(QString::fromUtf8("configIconSmall")));
	//listwidgetitem->setProperty("class", "icon-gear");
	//cogsIcon
	listwidgetitem->setText(QString::fromUtf8(obs_module_text("General")));

	listwidgetitem = new QListWidgetItem(listWidget);
	listwidgetitem->setIcon(QIcon(QString::fromUtf8(":/settings/images/settings/output.svg")));
	listwidgetitem->setText(QString::fromUtf8(obs_module_text("Canvas")));

	listwidgetitem = new QListWidgetItem(listWidget);
	listwidgetitem->setIcon(QIcon(QString::fromUtf8(":/settings/images/settings/stream.svg")));
	listwidgetitem->setText(QString::fromUtf8(obs_module_text("Output")));

	listwidgetitem = new QListWidgetItem(listWidget);
	listwidgetitem->setIcon(main_window->property("defaultIcon").value<QIcon>());
	listwidgetitem->setText(QString::fromUtf8(obs_module_text("SetupTroubleshooter")));
	listwidgetitem->setHidden(true);

	listwidgetitem = new QListWidgetItem(listWidget);
	listwidgetitem->setIcon(main_window->property("defaultIcon").value<QIcon>());
	listwidgetitem->setText(QString::fromUtf8(obs_module_text("Help")));

	listwidgetitem = new QListWidgetItem(listWidget);
	listwidgetitem->setIcon(QIcon(QString::fromUtf8(":/aitum/media/aitum.png")));
	listwidgetitem->setText(QString::fromUtf8(obs_module_text("SupportButton")));

	listWidget->setCurrentRow(0);
	listWidget->setSpacing(1);

	auto settingsPages = new QStackedWidget;
	settingsPages->setContentsMargins(0, 0, 0, 0);
	settingsPages->setFrameShape(QFrame::NoFrame);
	settingsPages->setLineWidth(0);

	QWidget *generalPage = new QWidget;
	auto generalPageLayout = new QVBoxLayout;
	generalPage->setLayout(generalPageLayout);

	auto infoBox = new QGroupBox(QString::fromUtf8(obs_module_text("WelcomeTitle")));
	infoBox->setStyleSheet("padding-top: 12px");
	auto infoLayout = new QVBoxLayout;
	infoBox->setLayout(infoLayout);

	auto infoLabel = new QLabel(QString::fromUtf8(obs_module_text("WelcomeText")));
	infoLabel->setWordWrap(true);
	infoLayout->addWidget(infoLabel, 1);

	auto buttonGroupBox = new QWidget();
	auto buttonLayout = new QHBoxLayout;
	buttonLayout->setSpacing(8);
	buttonLayout->setAlignment(Qt::AlignCenter);

	generalCanvasButton = new QToolButton();
	generalCanvasButton->setText(QString::fromUtf8(obs_module_text("SettingsCanvasButton")));
	generalCanvasButton->setIcon(QIcon(QString::fromUtf8(":/settings/images/settings/output.svg")));
	//generalCanvasButton->setIconSize(QSize(100, 100));

	generalOutputsButton = new QToolButton();
	generalOutputsButton->setText(QString::fromUtf8(obs_module_text("SettingsOutputsButton")));
	generalOutputsButton->setIcon(QIcon(QString::fromUtf8(":/settings/images/settings/stream.svg")));
	//generalOutputsButton->setIconSize(QSize(100, 100));

	generalHelpButton = new QToolButton();
	generalHelpButton->setText(QString::fromUtf8(obs_module_text("SettingsHelpButton")));
	generalHelpButton->setIcon(main_window->property("defaultIcon").value<QIcon>());
	//generalHelpButton->setIconSize(QSize(100, 100));

	generalSupportAitumButton = new QToolButton();
	generalSupportAitumButton->setText(QString::fromUtf8(obs_module_text("SupportButton")));
	generalSupportAitumButton->setIcon(QIcon(QString::fromUtf8(":/aitum/media/aitum.png")));
	//generalSupportAitumButton->setIconSize(QSize(100, 100));

	buttonLayout->addWidget(generalCanvasButton, 0);
	buttonLayout->addWidget(generalOutputsButton, 0);
	buttonLayout->addWidget(generalHelpButton, 0);
	buttonLayout->addWidget(generalSupportAitumButton, 0);

	buttonGroupBox->setLayout(buttonLayout);

	generalPageLayout->addWidget(infoBox, 0);
	generalPageLayout->addWidget(buttonGroupBox, 1);

	QScrollArea *scrollArea = new QScrollArea;
	scrollArea->setWidget(generalPage);
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);
	settingsPages->addWidget(scrollArea);

	auto canvasPage = new QGroupBox;
	canvasPage->setProperty("customTitle", QVariant(true));
	canvasPage->setStyleSheet(QString("QGroupBox[customTitle=\"true\"]{ padding-top: 4px;}"));
	canvasPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	scrollArea = new QScrollArea;
	scrollArea->setWidget(canvasPage);
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);
	settingsPages->addWidget(scrollArea);

	auto outputsPage = new QGroupBox;
	outputsPage->setProperty("customTitle", QVariant(true));
	outputsPage->setStyleSheet(QString("QGroupBox[customTitle=\"true\"]{ padding-top: 4px;}"));
	outputsPage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

	scrollArea = new QScrollArea;
	scrollArea->setWidget(outputsPage);
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);
	settingsPages->addWidget(scrollArea);

	troubleshooterText = new QTextEdit;
	troubleshooterText->setReadOnly(true);

	scrollArea = new QScrollArea;
	scrollArea->setWidget(troubleshooterText);
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);
	settingsPages->addWidget(scrollArea);

	// Help page
	auto helpPage = new QWidget;
	auto helpPageLayout = new QVBoxLayout;
	helpPage->setLayout(helpPageLayout);
	scrollArea = new QScrollArea;
	scrollArea->setWidget(helpPage);
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);

	auto helpInfoBox = new QGroupBox(QString::fromUtf8(obs_module_text("HelpTitle")));
	helpInfoBox->setStyleSheet("padding-top: 12px");
	auto helpLayout = new QVBoxLayout;
	helpInfoBox->setLayout(helpLayout);

	auto helpLabel = new QLabel(QString::fromUtf8(obs_module_text("HelpText")));
	helpLabel->setStyleSheet("font-size: 14px");
	helpLabel->setWordWrap(true);
	helpLabel->setTextFormat(Qt::RichText);
	helpLabel->setOpenExternalLinks(true);
	helpLayout->addWidget(helpLabel, 1);
	helpPageLayout->addWidget(helpInfoBox, 1, Qt::AlignTop);

	settingsPages->addWidget(scrollArea);

	//canvasPage

	canvasLayout = new QFormLayout;
	canvasLayout->setContentsMargins(9, 2, 9, 9);
	canvasLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	canvasLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

	auto canvas_title_layout = new QHBoxLayout;
	auto canvas_title = new QLabel(QString::fromUtf8(obs_module_text("Canvas")));
	canvas_title->setStyleSheet(QString::fromUtf8("font-weight: bold;"));
	canvas_title_layout->addWidget(canvas_title, 0, Qt::AlignLeft);
	//auto guide_link = new QLabel(QString::fromUtf8("<a href=\"https://l.aitum.tv/vh-streaming-settings\">") + QString::fromUtf8(obs_module_text("ViewGuide")) + QString::fromUtf8("</a>"));
	//guide_link->setOpenExternalLinks(true);

	auto addButton = new QPushButton(QIcon(":/res/images/plus.svg"), QString::fromUtf8(obs_module_text("AddCanvas")));
	addButton->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	addButton->setProperty("class", "icon-plus");

	connect(addButton, &QPushButton::clicked, [this] { AddCanvas(); });

	//streaming_title_layout->addWidget(guide_link, 0, Qt::AlignRight);
	canvas_title_layout->addWidget(addButton, 0, Qt::AlignRight);

	canvasLayout->addRow(canvas_title_layout);

	auto mainCanvasGroup = new QGroupBox;
	mainCanvasGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	mainCanvasGroup->setStyleSheet(QString("QGroupBox{background-color: %1; padding-top: 4px;}")
					       .arg(palette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb)));

	auto mainCanvasGroupLayout = new QFormLayout;
	mainCanvasGroupLayout->setContentsMargins(9, 2, 9, 9);
	mainCanvasGroupLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	mainCanvasGroupLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

	auto mainTitle = new QLabel(QString::fromUtf8(obs_module_text("SettingsMainCanvasTitle")));
	mainTitle->setStyleSheet("font-weight: bold;");
	mainCanvasGroupLayout->addRow(mainTitle);

	auto mainDescription = new QLabel(QString::fromUtf8(obs_module_text("SettingsMainCanvasDescription")));
	//	mainTitle->setStyleSheet(QString::fromUtf8("font-weight: bold;"));
	mainCanvasGroupLayout->addRow(mainDescription);

	mainCanvasGroup->setLayout(mainCanvasGroupLayout);

	canvasLayout->addRow(mainCanvasGroup);

	canvasPage->setLayout(canvasLayout);

	outputsLayout = new QFormLayout;
	outputsLayout->setContentsMargins(9, 2, 9, 9);
	outputsLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	outputsLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

	auto output_title_layout = new QHBoxLayout;
	auto output_title = new QLabel(QString::fromUtf8(obs_module_text("Outputs")));
	output_title->setStyleSheet(QString::fromUtf8("font-weight: bold;"));
	output_title_layout->addWidget(output_title, 0, Qt::AlignLeft);
	//auto guide_link = new QLabel(QString::fromUtf8("<a href=\"https://l.aitum.tv/vh-streaming-settings\">") + QString::fromUtf8(obs_module_text("ViewGuide")) + QString::fromUtf8("</a>"));
	//guide_link->setOpenExternalLinks(true);
	//	addButton = new QPushButton(QIcon(":/res/images/plus.svg"), QString::fromUtf8(obs_module_text("AddOutput")));
	//	addButton->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	// 	addButton->setProperty("class", "icon-plus");
	//	connect(addButton, &QPushButton::clicked, [this] {
	//		if (!vertical_outputs)
	//			return;
	//		auto s = obs_data_create();
	//		obs_data_set_string(s, "name", obs_module_text("Unnamed"));
	//		obs_data_array_push_back(vertical_outputs, s);
	//		AddServer(outputsLayout, s);
	//		obs_data_release(s);
	//	});

	auto outputAddButton = new QPushButton(QIcon(":/res/images/plus.svg"), QString::fromUtf8(obs_module_text("AddOutput")));
	outputAddButton->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	outputAddButton->setProperty("class", "icon-plus");
	connect(outputAddButton, &QPushButton::clicked, [this] {
		QMenu addMenu;

		auto a =
			addMenu.addAction(QIcon(":/aitum/media/stream.svg"), QString::fromUtf8(obs_module_text("AddStreamOutput")));
		connect(a, &QAction::triggered, [this] {
			QStringList otherNames;
			obs_data_array_enum(
				extra_outputs,
				[](obs_data_t *data2, void *param) {
					((QStringList *)param)->append(QString::fromUtf8(obs_data_get_string(data2, "name")));
				},
				&otherNames);
			otherNames.removeDuplicates();
			auto outputDialog = new StreamOutputDialog(this, otherNames);

			outputDialog->setWindowModality(Qt::WindowModal);
			outputDialog->setModal(true);

			if (outputDialog->exec() == QDialog::Accepted) {
				// create a new output
				if (!extra_outputs)
					return;
				auto s = obs_data_create();
				obs_data_set_bool(s, "enabled", true);
				obs_data_set_bool(s, "expanded", true);
				obs_data_set_string(s, "type", "stream");
				obs_data_set_string(s, "name", outputDialog->outputName.toUtf8().constData());
				obs_data_set_string(s, "stream_server", outputDialog->outputServer.toUtf8().constData());
				obs_data_set_string(s, "stream_key", outputDialog->outputKey.toUtf8().constData());
				obs_data_array_push_back(extra_outputs, s);
				AddOutput(outputsLayout, s, extra_outputs, true);
				obs_data_release(s);
			}

			delete outputDialog;
		});

		a = addMenu.addAction(QIcon(":/aitum/media/record.svg"), QString::fromUtf8(obs_module_text("AddRecordOutput")));
		connect(a, &QAction::triggered, [this] { AddRecord(false); });
		//a->setEnabled(false);
		a = addMenu.addAction(QIcon(":/aitum/media/backtrack_off.svg"),
				      QString::fromUtf8(obs_module_text("AddBacktrackOutput")));
		connect(a, &QAction::triggered, [this] { AddRecord(true); });
		a = addMenu.addAction(QIcon(":/aitum/media/virtual_cam_off.svg"),
				      QString::fromUtf8(obs_module_text("AddVirtualCameraOutput")));
		connect(a, &QAction::triggered, [this] {
			QStringList otherNames;
			obs_data_array_enum(
				extra_outputs,
				[](obs_data_t *data2, void *param) {
					((QStringList *)param)->append(QString::fromUtf8(obs_data_get_string(data2, "name")));
				},
				&otherNames);
			otherNames.removeDuplicates();

			std::string name = obs_module_text("VirtualCam");
			while (true) {
				if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("OutputName")), name))
					break;

				if (otherNames.contains(QString::fromUtf8(name)))
					continue;

				auto s = obs_data_create();
				obs_data_set_bool(s, "enabled", true);
				obs_data_set_bool(s, "expanded", true);
				obs_data_set_string(s, "type", "virtual_cam");
				obs_data_set_string(s, "name", name.c_str());
				obs_data_array_push_back(extra_outputs, s);
				AddOutput(outputsLayout, s, extra_outputs, true);
				obs_data_release(s);
				break;
			}
		});
		addMenu.exec(QCursor::pos());
	});

	output_title_layout->addWidget(outputAddButton, 0, Qt::AlignRight);

	outputsLayout->addRow(output_title_layout);

	auto outputGroup = new QGroupBox;
	outputGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	outputGroup->setStyleSheet(QString("QGroupBox{background-color: %1; padding-top: 4px;}")
					   .arg(palette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb)));

	auto outputLayout = new QFormLayout;
	outputLayout->setContentsMargins(9, 2, 9, 9);
	outputLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	outputLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

	auto mainTitle2 = new QLabel(QString::fromUtf8(obs_module_text("SettingsMainOutputTitle")));
	mainTitle2->setStyleSheet("font-weight: bold;");
	outputLayout->addRow(mainTitle2);

	auto mainDescription2 = new QLabel(QString::fromUtf8(obs_module_text("SettingsMainOutputDescription")));
	//	mainTitle->setStyleSheet(QString::fromUtf8("font-weight: bold;"));
	outputLayout->addRow(mainDescription2);

	outputGroup->setLayout(outputLayout);

	outputsLayout->addRow(outputGroup);

	outputsPage->setLayout(outputsLayout);

	// Support page
	QWidget *supportPage = new QWidget;
	auto supportPageLayout = new QVBoxLayout;
	supportPage->setLayout(supportPageLayout);

	auto supportInfoBox = new QGroupBox(QString::fromUtf8(obs_module_text("SupportTitle")));
	supportInfoBox->setStyleSheet("padding-top: 12px");
	auto supportLayout = new QVBoxLayout;
	supportInfoBox->setLayout(supportLayout);

	auto supportLabel = new QLabel(QString::fromUtf8(obs_module_text("SupportText")));
	supportLabel->setStyleSheet("font-size: 14px");
	supportLabel->setWordWrap(true);
	supportLabel->setTextFormat(Qt::RichText);
	supportLabel->setOpenExternalLinks(true);
	supportLayout->addWidget(supportLabel, 1);
	supportPageLayout->addWidget(supportInfoBox, 1, Qt::AlignTop);

	settingsPages->addWidget(supportPage);

	///
	const auto version =
		new QLabel(QString::fromUtf8(obs_module_text("Version")) + " " + QString::fromUtf8(PROJECT_VERSION) + " " +
			   QString::fromUtf8(obs_module_text("MadeBy")) + " <a href=\"https://aitum.tv\">Aitum</a>");
	version->setOpenExternalLinks(true);
	version->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

	newVersion = new QLabel;
	newVersion->setProperty("themeID", "warning");
	newVersion->setProperty("class", "text-warning");
	newVersion->setVisible(false);
	newVersion->setOpenExternalLinks(true);
	newVersion->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

	auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QHBoxLayout *bottomLayout = new QHBoxLayout;
	bottomLayout->addWidget(version, 1, Qt::AlignLeft);
	bottomLayout->addWidget(newVersion, 1, Qt::AlignLeft);
	bottomLayout->addWidget(buttonBox, 0, Qt::AlignRight);

	QHBoxLayout *contentLayout = new QHBoxLayout;
	contentLayout->addWidget(listWidget);

	contentLayout->addWidget(settingsPages, 1);

	listWidget->connect(listWidget, &QListWidget::currentRowChanged, settingsPages, &QStackedWidget::setCurrentIndex);
	listWidget->setCurrentRow(0);

	QVBoxLayout *vlayout = new QVBoxLayout;
	vlayout->setContentsMargins(11, 11, 11, 11);
	vlayout->addLayout(contentLayout);
	vlayout->addLayout(bottomLayout);
	setLayout(vlayout);

	// Button connects for general page, clean this up in the future when we abstract pages
	connect(generalCanvasButton, &QPushButton::clicked, [this] { listWidget->setCurrentRow(1); });

	connect(generalOutputsButton, &QPushButton::clicked, [this] { listWidget->setCurrentRow(2); });

	connect(generalHelpButton, &QPushButton::clicked, [this] { listWidget->setCurrentRow(listWidget->count() - 2); });

	connect(generalSupportAitumButton, &QPushButton::clicked, [this] { listWidget->setCurrentRow(listWidget->count() - 1); });
}

OBSBasicSettings::~OBSBasicSettings()
{
	if (extra_outputs)
		obs_data_array_release(extra_outputs);
	for (auto it = video_encoder_properties.begin(); it != video_encoder_properties.end(); it++)
		obs_properties_destroy(it->second);
	for (auto it = audio_encoder_properties.begin(); it != audio_encoder_properties.end(); it++)
		obs_properties_destroy(it->second);
}

QIcon OBSBasicSettings::GetGeneralIcon() const
{
	return listWidget->item(0)->icon();
}

QIcon OBSBasicSettings::GetAppearanceIcon() const
{
	return QIcon();
}

QIcon OBSBasicSettings::GetStreamIcon() const
{
	return listWidget->item(2)->icon();
}

QIcon OBSBasicSettings::GetOutputIcon() const
{
	return listWidget->item(1)->icon();
}

QIcon OBSBasicSettings::GetAudioIcon() const
{
	return QIcon();
}

QIcon OBSBasicSettings::GetVideoIcon() const
{
	return QIcon();
}

QIcon OBSBasicSettings::GetHotkeysIcon() const
{
	return QIcon();
}

QIcon OBSBasicSettings::GetAccessibilityIcon() const
{
	return QIcon();
}

QIcon OBSBasicSettings::GetAdvancedIcon() const
{
	return QIcon();
}

QIcon OBSBasicSettings::GetHelpIcon() const
{
	return listWidget->item(4)->icon();
}

QIcon OBSBasicSettings::GetCanvasIcon() const
{
	return listWidget->item(1)->icon();
}

QIcon OBSBasicSettings::GetAitumIcon() const
{
	return listWidget->item(5)->icon();
}

void OBSBasicSettings::SetGeneralIcon(const QIcon &icon)
{
	listWidget->item(0)->setIcon(icon);
}

void OBSBasicSettings::SetAppearanceIcon(const QIcon &icon)
{
	UNUSED_PARAMETER(icon);
}

void OBSBasicSettings::SetStreamIcon(const QIcon &icon)
{
	//listWidget->item(1)->setIcon(icon);
	listWidget->item(2)->setIcon(icon);
	generalOutputsButton->setIcon(icon);
}

void OBSBasicSettings::SetOutputIcon(const QIcon &icon)
{
	UNUSED_PARAMETER(icon);
	listWidget->item(1)->setIcon(icon);
	generalCanvasButton->setIcon(icon);
}

void OBSBasicSettings::SetAudioIcon(const QIcon &icon)
{
	UNUSED_PARAMETER(icon);
}

void OBSBasicSettings::SetVideoIcon(const QIcon &icon)
{
	UNUSED_PARAMETER(icon);
}

void OBSBasicSettings::SetHotkeysIcon(const QIcon &icon)
{
	UNUSED_PARAMETER(icon);
}

void OBSBasicSettings::SetAccessibilityIcon(const QIcon &icon)
{
	UNUSED_PARAMETER(icon);
}

void OBSBasicSettings::SetAdvancedIcon(const QIcon &icon)
{
	UNUSED_PARAMETER(icon);
}

void OBSBasicSettings::SetHelpIcon(const QIcon &icon)
{
	listWidget->item(4)->setIcon(icon);
	generalHelpButton->setIcon(icon);
}

void OBSBasicSettings::SetCanvasIcon(const QIcon &icon)
{
	listWidget->item(1)->setIcon(icon);
	generalCanvasButton->setIcon(icon);
}

void OBSBasicSettings::SetAitumIcon(const QIcon &icon)
{
	listWidget->item(5)->setIcon(icon);
	generalSupportAitumButton->setIcon(icon);
}

void OBSBasicSettings::AddCanvas(QFormLayout *canvasesLayout, obs_data_t *settings, obs_data_array_t *canvas)
{
	auto canvasGroup = new QGroupBox;
	canvasGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	canvasGroup->setProperty("altColor", QVariant(true));
	canvasGroup->setProperty("customTitle", QVariant(true));
	canvasGroup->setStyleSheet(
		QString("QGroupBox[altColor=\"true\"]{background-color: %1;} QGroupBox[customTitle=\"true\"]{padding-top: 4px;}")
			.arg(palette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb)));

	auto canvasLayout = new QFormLayout;
	canvasLayout->setContentsMargins(9, 2, 9, 2);

	canvasLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	canvasLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

	// Title
	auto canvas_title_layout = new QHBoxLayout;

	//auto platformIconLabel = new QLabel;
	//auto platformIcon = ConfigUtils::getPlatformIconFromEndpoint(QString::fromUtf8(obs_data_get_string(settings, "stream_server")));
	//platformIconLabel->setPixmap(platformIcon.pixmap(36, 36));
	//canvas_title_layout->addWidget(platformIconLabel, 0);

	//auto canvas_title = new QLabel(QString::fromUtf8(obs_data_get_string(settings, "name")));
	auto canvas_title = new QToolButton;
	canvas_title->setText(QString::fromUtf8(obs_data_get_string(settings, "name")));
	canvas_title->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	canvas_title->setArrowType(Qt::ArrowType::RightArrow);
	canvas_title->setCheckable(true);
	canvas_title->setChecked(false);
	canvas_title->setStyleSheet(QString("QToolButton{background-color: %1;font-weight: bold;border: none;}")
					    .arg(palette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb)));
	canvas_title_layout->addWidget(canvas_title, 1, Qt::AlignLeft);

	auto canvas_color = new QPushButton(QString::fromUtf8("Color"));
	auto c = color_from_int(obs_data_get_int(settings, "color"));
	canvas_color->setStyleSheet(QString("border: 2px solid %1;").arg(c.name(QColor::HexRgb)));
	canvas_title_layout->addWidget(canvas_color, 0, Qt::AlignCenter);
	connect(canvas_color, &QPushButton::clicked, [this, canvas_color, settings] {
		auto currentColor = color_from_int(obs_data_get_int(settings, "color"));
		QColor newColor = QColorDialog::getColor(currentColor, this, QString::fromUtf8(obs_module_text("SelectColor")));
		if (newColor.isValid()) {
			obs_data_set_int(settings, "color", color_to_int(newColor));
			canvas_color->setStyleSheet(QString("border: 2px solid %1;").arg(newColor.name(QColor::HexRgb)));
		}
	});

	const bool expanded = obs_data_get_bool(settings, "expanded");
	auto advancedGroup = new QFrame;
	advancedGroup->setContentsMargins(0, 4, 0, 0);

	if (expanded) {
		canvas_title->setArrowType(Qt::ArrowType::DownArrow);
	} else {
		advancedGroup->setVisible(false);
	}

	connect(canvas_title, &QToolButton::toggled, [advancedGroup, canvas_title, settings](bool checked) {
		advancedGroup->setVisible(checked);
		obs_data_set_bool(settings, "expanded", checked);
		canvas_title->setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
	});

	auto advancedGroupLayout = new QFormLayout;
	advancedGroup->setLayout(advancedGroupLayout);

	auto nameEdit = new QLineEdit(QString::fromUtf8(obs_data_get_string(settings, "name")));
	advancedGroupLayout->addRow(QString::fromUtf8(obs_module_text("CanvasName")), nameEdit);
	connect(nameEdit, &QLineEdit::textChanged, [this, nameEdit, settings, canvas, canvas_title] {
		auto newName = nameEdit->text().trimmed();
		if (newName.isEmpty()) {
			newName = QString::fromUtf8(obs_module_text("Unnamed"));
			nameEdit->setText(newName);
		}
		obs_data_set_string(settings, "name", newName.toUtf8().constData());
		canvas_title->setText(newName);
	});

	auto resolution = new QComboBox;
	resolution->setEditable(true);
	resolution->addItem("720x1280");
	resolution->addItem("1080x1920");
	resolution->addItem("1080x1350");
	resolution->addItem("1280x720");
	resolution->addItem("1920x1080");
	resolution->addItem("2560x1440");
	resolution->addItem("3840x2160");
	auto canvas_width = obs_data_get_int(settings, "width");
	if (canvas_width < 1)
		canvas_width = 1080;
	auto canvas_height = obs_data_get_int(settings, "height");
	if (canvas_height < 1)
		canvas_height = 1920;
	resolution->setCurrentText(QString::number(canvas_width) + "x" + QString::number(canvas_height));

	connect(resolution, &QComboBox::editTextChanged, [this, resolution, settings] {
		auto text = resolution->currentText().trimmed();
		auto parts = text.split('x');
		if (parts.size() != 2)
			return;
		bool ok1 = false;
		bool ok2 = false;
		auto width = parts[0].toInt(&ok1);
		auto height = parts[1].toInt(&ok2);
		if (!ok1 || !ok2 || width < 1 || height < 1)
			return;
		obs_data_set_int(settings, "width", width);
		obs_data_set_int(settings, "height", height);
	});

	auto cloneCombo = new QComboBox;
	obs_enum_canvases(
		[](void *param, obs_canvas_t *canvas) {
			auto combo = (QComboBox *)param;
			combo->addItem(QString::fromUtf8(obs_canvas_get_name(canvas)));
			return true;
		},
		cloneCombo);
	cloneCombo->setCurrentText(QString::fromUtf8(obs_data_get_string(settings, "clone")));
	connect(cloneCombo, &QComboBox::currentTextChanged, [this, cloneCombo, settings] {
		auto text = cloneCombo->currentText().trimmed();
		obs_data_set_string(settings, "clone", text.toUtf8().constData());
	});

	auto replaceLayout = new QGridLayout;
	replaceLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("CanvasCloneReplace"))), 0, 0);
	replaceLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("CanvasCloneReplacement"))), 0, 1);
	auto replace_sources = obs_data_get_array(settings, "replace_sources");
	if (!replace_sources) {
		replace_sources = obs_data_array_create();
		obs_data_set_array(settings, "replace_sources", replace_sources);
	}

	for (int i = 0; i < 5; i++) {
		obs_data_t *item = obs_data_array_item(replace_sources, i);
		if (!item) {
			item = obs_data_create();
			obs_data_array_push_back(replace_sources, item);
		}
		auto sourceCombo = new QComboBox;
		sourceCombo->setEditable(true);
		obs_enum_all_sources(
			[](void *param, obs_source_t *source) {
				auto combo = (QComboBox *)param;
				if (!obs_obj_is_private(source)) {
					auto name = QString::fromUtf8(obs_source_get_name(source));
					int index = 0;
					while (index < combo->count() &&
					       combo->itemText(index).compare(name, Qt::CaseInsensitive) < 0)
						index++;
					combo->insertItem(index, name);
				}
				return true;
			},
			sourceCombo);
		sourceCombo->insertItem(0, "");
		sourceCombo->setCurrentText(QString::fromUtf8(obs_data_get_string(item, "source")));
		connect(sourceCombo, &QComboBox::editTextChanged, [this, sourceCombo, item] {
			auto text = sourceCombo->currentText().trimmed();
			obs_data_set_string(item, "source", text.toUtf8().constData());
		});

		auto replaceCombo = new QComboBox;
		replaceCombo->setEditable(true);
		obs_enum_all_sources(
			[](void *param, obs_source_t *source) {
				auto combo = (QComboBox *)param;
				if (!obs_obj_is_private(source)) {
					auto name = QString::fromUtf8(obs_source_get_name(source));
					int index = 0;
					while (index < combo->count() &&
					       combo->itemText(index).compare(name, Qt::CaseInsensitive) < 0)
						index++;
					combo->insertItem(index, name);
				}
				return true;
			},
			replaceCombo);
		replaceCombo->insertItem(0, "");
		replaceCombo->setCurrentText(QString::fromUtf8(obs_data_get_string(item, "replacement")));
		connect(replaceCombo, &QComboBox::editTextChanged, [this, replaceCombo, item] {
			auto text = replaceCombo->currentText().trimmed();
			obs_data_set_string(item, "replacement", text.toUtf8().constData());
		});
		replaceLayout->addWidget(sourceCombo, i + 1, 0);
		replaceLayout->addWidget(replaceCombo, i + 1, 1);

		obs_data_release(item);
	}
	obs_data_array_release(replace_sources);

	bool clone = strcmp(obs_data_get_string(settings, "type"), "clone") == 0;

	auto canvas_type = new QComboBox;
	canvas_type->addItem(QString::fromUtf8(obs_module_text("ExtraCanvas")), "extra");
	canvas_type->addItem(QString::fromUtf8(obs_module_text("CanvasClone")), "clone");
	if (clone)
		canvas_type->setCurrentIndex(1);
	else
		canvas_type->setCurrentIndex(0);
	advancedGroupLayout->addRow(QString::fromUtf8(obs_module_text("CanvasType")), canvas_type);
	connect(canvas_type, &QComboBox::currentIndexChanged,
		[this, canvas_type, settings, advancedGroupLayout, resolution, cloneCombo, replaceLayout] {
			auto text = canvas_type->currentData().toString();
			obs_data_set_string(settings, "type", text.toUtf8().constData());
			bool clone = text == "clone";
			advancedGroupLayout->setRowVisible(resolution, !clone);
			advancedGroupLayout->setRowVisible(cloneCombo, clone);
			advancedGroupLayout->setRowVisible(replaceLayout, clone);
		});

	advancedGroupLayout->addRow(QString::fromUtf8(obs_module_text("CanvasResolution")), resolution);
	advancedGroupLayout->setRowVisible(resolution, !clone);
	advancedGroupLayout->addRow(QString::fromUtf8(obs_module_text("CanvasClone")), cloneCombo);
	advancedGroupLayout->setRowVisible(cloneCombo, clone);
	advancedGroupLayout->addRow(replaceLayout);
	advancedGroupLayout->setRowVisible(replaceLayout, clone);

	// Remove button
	auto removeButton =
		new QPushButton(QIcon(":/res/images/minus.svg"), QString::fromUtf8(obs_frontend_get_locale_string("Remove")));
	removeButton->setProperty("themeID", QVariant(QString::fromUtf8("removeIconSmall")));
	removeButton->setProperty("class", "icon-minus");
	connect(removeButton, &QPushButton::clicked, [this, canvasLayout, canvasGroup, settings, canvas] {
		outputsLayout->removeWidget(canvasGroup);
		RemoveWidget(canvasGroup);
		obs_data_set_bool(settings, "delete", true);
	});
	canvas_title_layout->addWidget(removeButton, 0, Qt::AlignRight);

	canvasLayout->addRow(canvas_title_layout);

	canvasLayout->addRow(advancedGroup);

	canvasGroup->setLayout(canvasLayout);

	canvasesLayout->addRow(canvasGroup);
}

void OBSBasicSettings::LoadSettings(obs_data_t *settings)
{
	while (canvasLayout->rowCount() > 2) {
		auto i = canvasLayout->takeRow(2).fieldItem;
		RemoveLayoutItem(i);
		canvasLayout->removeRow(2);
	}
	hotkeys.clear();
	main_settings = settings;
	auto canvas = obs_data_get_array(settings, "canvas");

	obs_data_array_enum(
		canvas,
		[](obs_data_t *data2, void *param) {
			if (obs_data_get_bool(data2, "delete"))
				return;
			auto d = (OBSBasicSettings *)param;
			auto canvas2 = obs_data_get_array(d->main_settings, "canvas");
			//auto c = obs_get_canvas_by_name(obs_data_get_string(data2, "name"));
			d->AddCanvas(d->canvasLayout, data2, canvas2);
			obs_data_array_release(canvas2);
		},
		this);
	obs_data_array_release(canvas);

	while (outputsLayout->rowCount() > 2) {
		auto i = outputsLayout->takeRow(2).fieldItem;
		RemoveLayoutItem(i);
		outputsLayout->removeRow(2);
	}
	obs_data_array_release(extra_outputs);
	extra_outputs = obs_data_get_array(settings, "outputs");
	if (!extra_outputs) {
		extra_outputs = obs_data_array_create();
		obs_data_set_array(settings, "outputs", extra_outputs);
	}
	obs_data_array_enum(
		extra_outputs,
		[](obs_data_t *data2, void *param) {
			auto d = (OBSBasicSettings *)param;
			auto outputs2 = obs_data_get_array(d->main_settings, "outputs");
			d->AddOutput(d->outputsLayout, data2, outputs2);
			obs_data_array_release(outputs2);
		},
		this);
}

void OBSBasicSettings::AddProperty(obs_properties_t *properties, obs_property_t *property, obs_data_t *settings,
				   QFormLayout *layout)
{
	obs_property_type type = obs_property_get_type(property);
	if (type == OBS_PROPERTY_BOOL) {
		auto widget = new QCheckBox(QString::fromUtf8(obs_property_description(property)));
		widget->setChecked(obs_data_get_bool(settings, obs_property_name(property)));
		layout->addWidget(widget);
		if (!obs_property_visible(property)) {
			widget->setVisible(false);
			int row = 0;
			layout->getWidgetPosition(widget, &row, nullptr);
			auto item = layout->itemAt(row, QFormLayout::LabelRole);
			if (item) {
				auto w = item->widget();
				if (w)
					w->setVisible(false);
			}
		}
		encoder_property_widgets.emplace(property, widget);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		connect(widget, &QCheckBox::checkStateChanged, [this, properties, property, settings, widget, layout] {
#else
		connect(widget, &QCheckBox::stateChanged, [this, properties, property, settings, widget, layout] {
#endif
			obs_data_set_bool(settings, obs_property_name(property), widget->isChecked());
			if (obs_property_modified(property, settings)) {
				RefreshProperties(properties, layout);
			}
		});
	} else if (type == OBS_PROPERTY_INT) {
		auto widget = new QSpinBox();
		widget->setEnabled(obs_property_enabled(property));
		widget->setMinimum(obs_property_int_min(property));
		widget->setMaximum(obs_property_int_max(property));
		widget->setSingleStep(obs_property_int_step(property));
		widget->setValue((int)obs_data_get_int(settings, obs_property_name(property)));
		widget->setToolTip(QString::fromUtf8(obs_property_long_description(property)));
		widget->setSuffix(QString::fromUtf8(obs_property_int_suffix(property)));
		auto label = new QLabel(QString::fromUtf8(obs_property_description(property)));
		layout->addRow(label, widget);
		if (!obs_property_visible(property)) {
			widget->setVisible(false);
			label->setVisible(false);
		}
		encoder_property_widgets.emplace(property, widget);
		connect(widget, &QSpinBox::valueChanged, [this, properties, property, settings, widget, layout] {
			obs_data_set_int(settings, obs_property_name(property), widget->value());
			if (obs_property_modified(property, settings)) {
				RefreshProperties(properties, layout);
			}
		});
	} else if (type == OBS_PROPERTY_FLOAT) {
		auto widget = new QDoubleSpinBox();
		widget->setEnabled(obs_property_enabled(property));
		widget->setMinimum(obs_property_float_min(property));
		widget->setMaximum(obs_property_float_max(property));
		widget->setSingleStep(obs_property_float_step(property));
		widget->setValue(obs_data_get_double(settings, obs_property_name(property)));
		widget->setToolTip(QString::fromUtf8(obs_property_long_description(property)));
		widget->setSuffix(QString::fromUtf8(obs_property_float_suffix(property)));
		auto label = new QLabel(QString::fromUtf8(obs_property_description(property)));
		layout->addRow(label, widget);
		if (!obs_property_visible(property)) {
			widget->setVisible(false);
			label->setVisible(false);
		}
		encoder_property_widgets.emplace(property, widget);
		connect(widget, &QDoubleSpinBox::valueChanged, [this, properties, property, settings, widget, layout] {
			obs_data_set_double(settings, obs_property_name(property), widget->value());
			if (obs_property_modified(property, settings)) {
				RefreshProperties(properties, layout);
			}
		});
	} else if (type == OBS_PROPERTY_TEXT) {
		obs_text_type text_type = obs_property_text_type(property);
		if (text_type == OBS_TEXT_MULTILINE) {
			auto widget = new QPlainTextEdit;
			widget->document()->setDefaultStyleSheet("font { white-space: pre; }");
			widget->setTabStopDistance(40);
			widget->setPlainText(QString::fromUtf8(obs_data_get_string(settings, obs_property_name(property))));
			auto label = new QLabel(QString::fromUtf8(obs_property_description(property)));
			layout->addRow(label, widget);
			if (!obs_property_visible(property)) {
				widget->setVisible(false);
				label->setVisible(false);
			}
			encoder_property_widgets.emplace(property, widget);
			connect(widget, &QPlainTextEdit::textChanged, [this, properties, property, settings, widget, layout] {
				obs_data_set_string(settings, obs_property_name(property), widget->toPlainText().toUtf8());
				if (obs_property_modified(property, settings)) {
					RefreshProperties(properties, layout);
				}
			});
		} else {
			auto widget = new QLineEdit();
			widget->setText(QString::fromUtf8(obs_data_get_string(settings, obs_property_name(property))));
			if (text_type == OBS_TEXT_PASSWORD)
				widget->setEchoMode(QLineEdit::Password);
			auto label = new QLabel(QString::fromUtf8(obs_property_description(property)));
			layout->addRow(label, widget);
			if (!obs_property_visible(property)) {
				widget->setVisible(false);
				label->setVisible(false);
			}
			encoder_property_widgets.emplace(property, widget);
			if (text_type != OBS_TEXT_INFO) {
				connect(widget, &QLineEdit::textChanged, [this, properties, property, settings, widget, layout] {
					obs_data_set_string(settings, obs_property_name(property), widget->text().toUtf8());
					if (obs_property_modified(property, settings)) {
						RefreshProperties(properties, layout);
					}
				});
			}
		}
	} else if (type == OBS_PROPERTY_LIST) {
		auto widget = new QComboBox();
		widget->setMaxVisibleItems(40);
		widget->setToolTip(QString::fromUtf8(obs_property_long_description(property)));
		auto list_type = obs_property_list_type(property);
		obs_combo_format format = obs_property_list_format(property);

		size_t count = obs_property_list_item_count(property);
		for (size_t i = 0; i < count; i++) {
			QVariant var;
			if (format == OBS_COMBO_FORMAT_INT) {
				long long val = obs_property_list_item_int(property, i);
				var = QVariant::fromValue<long long>(val);

			} else if (format == OBS_COMBO_FORMAT_FLOAT) {
				double val = obs_property_list_item_float(property, i);
				var = QVariant::fromValue<double>(val);

			} else if (format == OBS_COMBO_FORMAT_STRING) {
				var = QByteArray(obs_property_list_item_string(property, i));
			}
			widget->addItem(QString::fromUtf8(obs_property_list_item_name(property, i)), var);
		}

		if (list_type == OBS_COMBO_TYPE_EDITABLE)
			widget->setEditable(true);

		auto name = obs_property_name(property);
		QVariant value;
		switch (format) {
		case OBS_COMBO_FORMAT_INT:
			value = QVariant::fromValue(obs_data_get_int(settings, name));
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			value = QVariant::fromValue(obs_data_get_double(settings, name));
			break;
		case OBS_COMBO_FORMAT_STRING:
			value = QByteArray(obs_data_get_string(settings, name));
			break;
		default:;
		}

		if (format == OBS_COMBO_FORMAT_STRING && list_type == OBS_COMBO_TYPE_EDITABLE) {
			widget->lineEdit()->setText(value.toString());
		} else {
			auto idx = widget->findData(value);
			if (idx != -1)
				widget->setCurrentIndex(idx);
		}

		auto label = new QLabel(QString::fromUtf8(obs_property_description(property)));
		layout->addRow(label, widget);
		if (!obs_property_visible(property)) {
			widget->setVisible(false);
			label->setVisible(false);
		}
		encoder_property_widgets.emplace(property, widget);
		switch (format) {
		case OBS_COMBO_FORMAT_INT:
			connect(widget, &QComboBox::currentIndexChanged, [this, properties, property, settings, widget, layout] {
				obs_data_set_int(settings, obs_property_name(property), widget->currentData().toInt());
				if (obs_property_modified(property, settings)) {
					RefreshProperties(properties, layout);
				}
			});
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			connect(widget, &QComboBox::currentIndexChanged, [this, properties, property, settings, widget, layout] {
				obs_data_set_double(settings, obs_property_name(property), widget->currentData().toDouble());
				if (obs_property_modified(property, settings)) {
					RefreshProperties(properties, layout);
				}
			});
			break;
		case OBS_COMBO_FORMAT_STRING:
			if (list_type == OBS_COMBO_TYPE_EDITABLE) {
				connect(widget, &QComboBox::currentTextChanged,
					[this, properties, property, settings, widget, layout] {
						obs_data_set_string(settings, obs_property_name(property),
								    widget->currentText().toUtf8().constData());
						if (obs_property_modified(property, settings)) {
							RefreshProperties(properties, layout);
						}
					});
			} else {
				connect(widget, &QComboBox::currentIndexChanged,
					[this, properties, property, settings, widget, layout] {
						obs_data_set_string(settings, obs_property_name(property),
								    widget->currentData().toString().toUtf8().constData());
						if (obs_property_modified(property, settings)) {
							RefreshProperties(properties, layout);
						}
					});
			}
			break;
		default:;
		}
	} else {
		// OBS_PROPERTY_PATH
		// OBS_PROPERTY_COLOR
		// OBS_PROPERTY_BUTTON
		// OBS_PROPERTY_FONT
		// OBS_PROPERTY_EDITABLE_LIST
		// OBS_PROPERTY_FRAME_RATE
		// OBS_PROPERTY_GROUP
		// OBS_PROPERTY_COLOR_ALPHA
	}
	obs_property_modified(property, settings);
}

void OBSBasicSettings::RefreshProperties(obs_properties_t *properties, QFormLayout *layout)
{

	obs_property_t *property = obs_properties_first(properties);
	while (property) {
		auto widget = encoder_property_widgets.at(property);
		auto visible = obs_property_visible(property);
		if (widget->isVisible() != visible) {
			widget->setVisible(visible);
			int row = 0;
			layout->getWidgetPosition(widget, &row, nullptr);
			auto item = layout->itemAt(row, QFormLayout::LabelRole);
			if (item) {
				widget = item->widget();
				if (widget)
					widget->setVisible(visible);
			}
		}
		obs_property_next(&property);
	}
}
static bool obs_encoder_parent_video_loaded = false;
static video_t *(*obs_encoder_parent_video_wrapper)(const obs_encoder_t *encoder) = nullptr;

void OBSBasicSettings::LoadOutputStats(std::vector<video_t *> *oldVideos)
{
	if (!obs_encoder_parent_video_loaded) {
#ifdef _WIN32
		void *dl = os_dlopen("obs");
#else
		void *dl = dlopen(nullptr, RTLD_LAZY);
#endif
		if (dl) {
			auto sym = os_dlsym(dl, "obs_encoder_parent_video");
			if (sym)
				obs_encoder_parent_video_wrapper = (video_t * (*)(const obs_encoder_t *encoder)) sym;
			os_dlclose(dl);
		}
		obs_encoder_parent_video_loaded = true;
	}
	std::vector<std::tuple<video_t *, obs_encoder_t *, obs_output_t *>> refs;
	obs_enum_outputs(
		[](void *param, obs_output_t *output) {
			auto refs2 = (std::vector<std::tuple<video_t *, obs_encoder_t *, obs_output_t *>> *)param;
			auto ec = 0;
			for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
				auto venc = obs_output_get_video_encoder2(output, i);
				if (!venc)
					continue;
				ec++;
				video_t *video = obs_encoder_parent_video_wrapper ? obs_encoder_parent_video_wrapper(venc)
										  : obs_encoder_video(venc);
				if (!video)
					video = obs_output_video(output);
				refs2->push_back(std::tuple<video_t *, obs_encoder_t *, obs_output_t *>(video, venc, output));
			}
			if (!ec) {
				refs2->push_back(std::tuple<video_t *, obs_encoder_t *, obs_output_t *>(obs_output_video(output),
													nullptr, output));
			}
			return true;
		},
		&refs);
	std::sort(refs.begin(), refs.end(),
		  [](std::tuple<video_t *, obs_encoder_t *, obs_output_t *> const &a,
		     std::tuple<video_t *, obs_encoder_t *, obs_output_t *> const &b) {
			  video_t *va;
			  video_t *vb;
			  obs_encoder_t *ea;
			  obs_encoder_t *eb;
			  obs_output_t *oa;
			  obs_output_t *ob;
			  std::tie(va, ea, oa) = a;
			  std::tie(vb, eb, ob) = b;
			  if (va == vb) {
				  if (ea == eb) {
					  return oa < ob;
				  }
				  return va < vb;
			  }
			  return va < vb;
		  });
	std::string stats;
	video_t *last_video = nullptr;
	auto video_count = 0;
	video_t *vertical_video = nullptr;
	auto ph = obs_get_proc_handler();
	struct calldata cd;
	calldata_init(&cd);
	if (proc_handler_call(ph, "aitum_vertical_get_video", &cd))
		vertical_video = (video_t *)calldata_ptr(&cd, "video");
	calldata_free(&cd);
	for (auto it = refs.begin(); it != refs.end(); it++) {
		video_t *video;
		obs_encoder_t *encoder;
		obs_output_t *output;
		std::tie(video, encoder, output) = *it;

		if (!video) {
			stats += "No Canvas ";
		} else if (vertical_video && vertical_video == video) {
			stats += "Vertical Canvas ";
		} else if (video == obs_get_video()) {
			stats += "Main Canvas ";
		} else if (std::find(oldVideos->begin(), oldVideos->end(), video) != oldVideos->end()) {
			stats += "Old Main Canvas ";
		} else {
			if (last_video != video) {
				video_count++;
				last_video = video;
			}
			stats += "Custom Canvas ";
			stats += std::to_string(video_count);
			stats += " ";
		}
		if (video && std::find(oldVideos->begin(), oldVideos->end(), video) != oldVideos->end()) {
			stats += obs_output_active(output) ? "Active " : "Inactive ";
		} else if (encoder) {
			stats += "(";
			stats += std::to_string(obs_encoder_get_width(encoder));
			stats += "x";
			stats += std::to_string(obs_encoder_get_height(encoder));
			stats += ") ";
			stats += obs_encoder_get_name(encoder);
			stats += "(";
			stats += obs_encoder_get_id(encoder);
			stats += ") ";
			stats += to_string_with_precision(
				video_output_get_frame_rate(video) / obs_encoder_get_frame_rate_divisor(encoder), 2);
			stats += "fps ";
			stats += obs_encoder_active(encoder) ? "Active " : "Inactive ";
			if (video) {
				stats += "skipped frames ";
				stats += std::to_string(video_output_get_skipped_frames(video));
				stats += "/";
				stats += std::to_string(video_output_get_total_frames(video));
				stats += " ";
			}
		} else if (video) {
			stats += "(";
			stats += std::to_string(video_output_get_width(video));
			stats += "x";
			stats += std::to_string(video_output_get_height(video));
			stats += ") ";
			stats += to_string_with_precision(video_output_get_frame_rate(video), 2);
			stats += "fps ";
			stats += video_output_active(video) ? "Active " : "Inactive ";
			stats += "skipped frames ";
			stats += std::to_string(video_output_get_skipped_frames(video));
			stats += "/";
			stats += std::to_string(video_output_get_total_frames(video));
			stats += " ";
		} else {
			stats += "(";
			stats += std::to_string(obs_output_get_width(output));
			stats += "x";
			stats += std::to_string(obs_output_get_height(output));
			stats += ") ";
			stats += obs_output_active(output) ? "Active " : "Inactive ";
		}
		stats += obs_output_get_name(output);
		stats += "(";
		stats += obs_output_get_id(output);
		stats += ") ";
		stats += std::to_string(obs_output_get_connect_time_ms(output));
		stats += "ms ";
		//obs_output_get_total_bytes(output);
		stats += "dropped frames ";
		stats += std::to_string(obs_output_get_frames_dropped(output));
		stats += "/";
		stats += std::to_string(obs_output_get_total_frames(output));
		obs_service_t *service = obs_output_get_service(output);
		if (service) {
			stats += " ";
			stats += obs_service_get_name(service);
			stats += "(";
			stats += obs_service_get_id(service);
			stats += ")";
			auto url = obs_service_get_connect_info(service, OBS_SERVICE_CONNECT_INFO_SERVER_URL);
			if (url) {
				stats += " ";
				stats += url;
			}
		}
		stats += "\n";
	}
	troubleshooterText->setText(QString::fromUtf8(stats));
}

void OBSBasicSettings::SetNewerVersion(QString newer_version_available)
{
	if (newer_version_available.isEmpty())
		return;
	newVersion->setText(QString::fromUtf8(obs_module_text("NewVersion")).arg(newer_version_available));
	newVersion->setVisible(true);
}

void OBSBasicSettings::AddCanvas()
{
	QString newName = QString::fromUtf8(obs_module_text("AitumStreamSuiteCanvas"));
	QStringList otherNames;
	auto canvases = obs_data_get_array(main_settings, "canvas");
	obs_data_array_enum(
		canvases,
		[](obs_data_t *data2, void *param) {
			((QStringList *)param)->append(QString::fromUtf8(obs_data_get_string(data2, "name")));
		},
		&otherNames);
	otherNames.removeDuplicates();
	int i = 2;
	while (otherNames.contains(newName))
		newName = QString::fromUtf8(obs_module_text("AitumStreamSuiteCanvas")) + " " + QString::number(i++);

	if (!canvases) {
		canvases = obs_data_array_create();
		obs_data_set_array(main_settings, "canvas", canvases);
	}

	auto s = obs_data_create();
	// Set the info from the output dialog
	obs_data_set_string(s, "name", newName.toUtf8().constData());
	obs_data_set_int(s, "color", 0x1F1A17);
	obs_data_set_bool(s, "expanded", true);
	obs_data_array_push_back(canvases, s);
	AddCanvas(canvasLayout, s, canvases);
	obs_data_release(s);

	obs_data_array_release(canvases);
	listWidget->setCurrentRow(1);
}

void OBSBasicSettings::ShowTab(int i)
{
	listWidget->setCurrentRow(i);
}

QIcon getPlatformIconFromEndpoint(QString endpoint);

void OBSBasicSettings::AddOutput(QFormLayout *outputsLayout, obs_data_t *settings, obs_data_array_t *outputs, bool isNew)
{
	auto outputGroup = new QGroupBox;
	outputGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
	outputGroup->setProperty("altColor", QVariant(true));
	outputGroup->setProperty("customTitle", QVariant(true));
	outputGroup->setStyleSheet(
		QString("QGroupBox[altColor=\"true\"]{background-color: %1;} QGroupBox[customTitle=\"true\"]{padding-top: 4px;}")
			.arg(palette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb)));

	auto outputLayout = new QFormLayout;
	outputLayout->setContentsMargins(9, 2, 9, 2);

	outputLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	outputLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);

	// Title
	auto output_title_layout = new QHBoxLayout;

	auto iconLabel = new QLabel;
	auto output_type = obs_data_get_string(settings, "type");
	if (strcmp(output_type, "record") == 0) {
		iconLabel->setPixmap(QIcon(":/aitum/media/record.svg").pixmap(36, 36));
	} else if (strcmp(output_type, "backtrack") == 0) {
		iconLabel->setPixmap(QIcon(":/aitum/media/backtrack_off.svg").pixmap(36, 36));
	} else if (strcmp(output_type, "virtual_cam") == 0) {
		iconLabel->setPixmap(QIcon(":/aitum/media/virtual_cam_off.svg").pixmap(36, 36));
	} else {
		auto platformIcon = getPlatformIconFromEndpoint(QString::fromUtf8(obs_data_get_string(settings, "stream_server")));
		iconLabel->setPixmap(platformIcon.pixmap(36, 36));
	}
	output_title_layout->addWidget(iconLabel, 0);

	const bool expanded = obs_data_get_bool(settings, "expanded");

	auto streaming_title = new QToolButton;
	streaming_title->setText(QString::fromUtf8(obs_data_get_string(settings, "name")));
	streaming_title->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	streaming_title->setArrowType(expanded ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
	streaming_title->setCheckable(true);
	streaming_title->setChecked(expanded);
	streaming_title->setStyleSheet(QString::fromUtf8("font-weight: bold;"));
	output_title_layout->addWidget(streaming_title, 1, Qt::AlignLeft);

	auto canvasCombo = new QComboBox;
	obs_enum_canvases(
		[](void *param, obs_canvas_t *canvas) {
			auto c = (QComboBox *)param;
			c->addItem(QString::fromUtf8(obs_canvas_get_name(canvas)));
			return true;
		},
		canvasCombo);
	canvasCombo->setCurrentText(QString::fromUtf8(obs_data_get_string(settings, "canvas")));

	// Remove button
	auto removeButton =
		new QPushButton(QIcon(":/res/images/minus.svg"), QString::fromUtf8(obs_frontend_get_locale_string("Remove")));
	removeButton->setProperty("themeID", QVariant(QString::fromUtf8("removeIconSmall")));
	removeButton->setProperty("class", "icon-minus");
	connect(removeButton, &QPushButton::clicked, [this, outputsLayout, outputGroup, settings, outputs] {
		outputsLayout->removeWidget(outputGroup);
		RemoveWidget(outputGroup);
		auto count = obs_data_array_count(outputs);
		for (size_t i = 0; i < count; i++) {
			auto item = obs_data_array_item(outputs, i);
			if (item == settings) {
				obs_data_array_erase(outputs, i);
				obs_data_release(item);
				break;
			}
			obs_data_release(item);
		}
	});

	// Edit button
	auto editButton = new QPushButton(QString::fromUtf8(obs_module_text("EditServerSettings")));
	editButton->setProperty("themeID", "configIconSmall");
	editButton->setProperty("class", "icon-gear");

	connect(editButton, &QPushButton::clicked, [this, settings, outputs] {
		QStringList otherNames;
		obs_data_array_enum(
			outputs,
			[](obs_data_t *data2, void *param) {
				((QStringList *)param)->append(QString::fromUtf8(obs_data_get_string(data2, "name")));
			},
			&otherNames);
		otherNames.removeDuplicates();
		otherNames.removeOne(QString::fromUtf8(obs_data_get_string(settings, "name")));

		auto output_type = obs_data_get_string(settings, "type");
		if (strcmp(output_type, "record") == 0 || strcmp(output_type, "backtrack") == 0) {
			auto outputDialog = new RecordOutputDialog(this, QString::fromUtf8(obs_data_get_string(settings, "name")),
								   QString::fromUtf8(obs_data_get_string(settings, "path")),
								   QString::fromUtf8(obs_data_get_string(settings, "filename")),
								   QString::fromUtf8(obs_data_get_string(settings, "format")),
								   obs_data_get_int(settings, "max_size_mb"),
								   obs_data_get_int(settings, "max_time_sec"), otherNames,
								   strcmp(output_type, "backtrack") == 0);

			outputDialog->setWindowModality(Qt::WindowModal);
			outputDialog->setModal(true);

			if (outputDialog->exec() == QDialog::Accepted) { // edit an output
				if (!settings)
					return;

				obs_data_set_bool(settings, "enabled", true);
				// Set the info from the output dialog
				obs_data_set_string(settings, "name", outputDialog->outputName.toUtf8().constData());
				obs_data_set_string(settings, "path", outputDialog->recordPath.toUtf8().constData());
				obs_data_set_string(settings, "filename", outputDialog->filenameFormat.toUtf8().constData());
				obs_data_set_string(settings, "format", outputDialog->fileFormat.toUtf8().constData());
				obs_data_set_int(settings, "max_size_mb", outputDialog->maxSize);
				obs_data_set_int(settings, "max_time_sec", outputDialog->maxTime);

				// Reload
				LoadSettings(main_settings);
			}

			delete outputDialog;

		} else if (strcmp(output_type, "virtual_cam") == 0) {
			QStringList otherNames;
			obs_data_array_enum(
				extra_outputs,
				[](obs_data_t *data2, void *param) {
					((QStringList *)param)->append(QString::fromUtf8(obs_data_get_string(data2, "name")));
				},
				&otherNames);
			otherNames.removeDuplicates();
			otherNames.removeOne(QString::fromUtf8(obs_data_get_string(settings, "name")));

			std::string name = obs_module_text("VirtualCam");
			while (true) {
				if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("OutputName")), name))
					break;

				if (otherNames.contains(QString::fromUtf8(name)))
					continue;

				obs_data_set_string(settings, "name", name.c_str());
				LoadSettings(main_settings);
				break;
			}
		} else {
			auto outputDialog = new StreamOutputDialog(this, obs_data_get_string(settings, "name"),
								   obs_data_get_string(settings, "stream_server"),
								   obs_data_get_string(settings, "stream_key"), otherNames);

			outputDialog->setWindowModality(Qt::WindowModal);
			outputDialog->setModal(true);

			if (outputDialog->exec() == QDialog::Accepted) { // edit an output
				if (!settings)
					return;

				obs_data_set_bool(settings, "enabled", true);
				// Set the info from the output dialog
				obs_data_set_string(settings, "name", outputDialog->outputName.toUtf8().constData());
				obs_data_set_string(settings, "stream_server", outputDialog->outputServer.toUtf8().constData());
				obs_data_set_string(settings, "stream_key", outputDialog->outputKey.toUtf8().constData());

				// Reload
				LoadSettings(main_settings);
			}

			delete outputDialog;
		}
	});

	// Buttons to layout
	output_title_layout->addWidget(editButton, 0, Qt::AlignRight);
	output_title_layout->addWidget(removeButton, 0, Qt::AlignRight);

	outputLayout->addRow(output_title_layout);
	outputLayout->addRow(QString::fromUtf8(obs_module_text("Canvas")), canvasCombo);

	OBSHotkeyWidget *startHotkeyWidget = nullptr;
	OBSHotkeyWidget *stopHotkeyWidget = nullptr;
	OBSHotkeyWidget *extraHotkeyWidget = nullptr;
	if (!isNew) {
		std::string startName = "AitumStreamSuiteStartOutput";
		startName += obs_data_get_string(settings, "name");
		auto hotkey = GetHotkeyByName(startName.c_str());
		if (hotkey) {
			auto id = obs_hotkey_get_id(hotkey);
			std::vector<obs_key_combination_t> combos = GetCombosForHotkey(id);
			auto hn = obs_hotkey_get_name(hotkey);
			startHotkeyWidget = new OBSHotkeyWidget(this, id, hn, combos);
			auto label = new OBSHotkeyLabel;
			label->setText(QString::fromUtf8(obs_module_text("StartHotkey")));
			startHotkeyWidget->label = label;
			outputLayout->addRow(label, startHotkeyWidget);
			hotkeys.push_back(startHotkeyWidget);
		}

		std::string stopName = "AitumStreamSuiteStopOutput";
		stopName += obs_data_get_string(settings, "name");
		hotkey = GetHotkeyByName(stopName.c_str());
		if (hotkey) {
			auto id = obs_hotkey_get_id(hotkey);
			std::vector<obs_key_combination_t> combos = GetCombosForHotkey(id);
			auto hn = obs_hotkey_get_name(hotkey);
			stopHotkeyWidget = new OBSHotkeyWidget(this, id, hn, combos);
			auto label = new OBSHotkeyLabel;
			label->setText(QString::fromUtf8(obs_module_text("StopHotkey")));
			stopHotkeyWidget->label = label;
			outputLayout->addRow(label, stopHotkeyWidget);
			hotkeys.push_back(stopHotkeyWidget);
			if (startHotkeyWidget) {
				stopHotkeyWidget->label->pairPartner = startHotkeyWidget->label;
				startHotkeyWidget->label->pairPartner = stopHotkeyWidget->label;
			}
		}

		std::string ebName = "AitumStreamSuiteSaveBacktrack";
		ebName += obs_data_get_string(settings, "name");
		hotkey = GetHotkeyByName(ebName.c_str());
		if (hotkey) {
			auto id = obs_hotkey_get_id(hotkey);
			std::vector<obs_key_combination_t> combos = GetCombosForHotkey(id);
			auto hn = obs_hotkey_get_name(hotkey);
			extraHotkeyWidget = new OBSHotkeyWidget(this, id, hn, combos);
			auto label = new OBSHotkeyLabel;
			label->setText(QString::fromUtf8(obs_module_text("SaveBacktrack")));
			extraHotkeyWidget->label = label;
			outputLayout->addRow(label, extraHotkeyWidget);
			hotkeys.push_back(extraHotkeyWidget);
		}
	}

	if (!expanded) {
		outputLayout->setRowVisible(canvasCombo, false);
		if (startHotkeyWidget)
			outputLayout->setRowVisible(startHotkeyWidget, false);
		if (stopHotkeyWidget)
			outputLayout->setRowVisible(stopHotkeyWidget, false);
		if (extraHotkeyWidget)
			outputLayout->setRowVisible(extraHotkeyWidget, false);

		streaming_title->setArrowType(Qt::ArrowType::RightArrow);
	}

	outputGroup->setLayout(outputLayout);

	outputsLayout->addRow(outputGroup);

	if (strcmp(output_type, "virtual_cam") == 0) {
		connect(streaming_title, &QToolButton::toggled,
			[streaming_title, settings, canvasCombo, outputLayout, startHotkeyWidget, stopHotkeyWidget,
			 extraHotkeyWidget](bool checked) {
				outputLayout->setRowVisible(canvasCombo, checked);
				if (startHotkeyWidget)
					outputLayout->setRowVisible(startHotkeyWidget, checked);
				if (stopHotkeyWidget)
					outputLayout->setRowVisible(stopHotkeyWidget, checked);
				if (extraHotkeyWidget)
					outputLayout->setRowVisible(extraHotkeyWidget, checked);
				obs_data_set_bool(settings, "expanded", checked);
				streaming_title->setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
			});
		connect(canvasCombo, &QComboBox::currentTextChanged, [canvasCombo, settings, outputs] {
			obs_data_set_string(settings, "canvas", canvasCombo->currentText().toUtf8().constData());
		});
		return;
	}

	auto advancedGroup = new QGroupBox(QString::fromUtf8(obs_module_text("AdvancedGroupHeader")));
	advancedGroup->setContentsMargins(0, 4, 0, 0);

	advancedGroup->setProperty("customTitle", QVariant(true));
	advancedGroup->setStyleSheet(
		"QGroupBox[customTitle=\"true\"]::title { subcontrol-origin: margin; subcontrol-position: top right; padding: 12px 18px 0 0; }"
		"QGroupBox[customTitle=\"true\"] { padding-top: 4px; padding-bottom: 0;}");

	auto advancedGroupLayout = new QVBoxLayout;
	advancedGroup->setLayout(advancedGroupLayout);

	// Tab widget
	// 1 = bg for active tab + pane, 2 = inactive tabs, 3 = tab text colour, 4 = border colour for pane
	auto tabStyles =
		QString("QTabWidget::pane {  background: %1; border-bottom-left-radius: 4px; border-bottom-right-radius: 4px; border-top-right-radius: 4px; margin-top: -1px; padding-top: 8px; border: 1px solid %4; } QTabWidget::tab-bar { margin-bottom: 0; padding-bottom: 0; border-color: %4; } QTabBar::tab { color: %3; padding: 10px; margin-bottom: 0; border: 1px solid %4; } QTabBar::tab:selected { background: %1; font-weight: bold; border-bottom: none; } QTabBar::tab:!selected { background: %2; }")
			.arg(palette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb),
			     palette().color(QPalette::ColorRole::Light).name(QColor::HexRgb),
			     palette().color(QPalette::ColorRole::Text).name(QColor::HexRgb),
			     palette().color(QPalette::ColorRole::Light).name(QColor::HexRgb));

	auto advancedTabWidget = new QTabWidget;
	advancedTabWidget->setContentsMargins(0, 0, 0, 0);
	advancedTabWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	advancedTabWidget->setStyleSheet(tabStyles);
	//	advancedTabWidget->setStyleSheet("QTabWidget::tab-bar { border: 1px solid gray; }"
	//									 "QTabBar::tab { background: gray; color: white; padding: 10px; }"
	//									 "QTabBar::tab:selected { background: lightgray; }"
	//									 "QTabWidget::pane { border: none; background: pink; }");

	//	auto pageStyle = QString("QWidget[page=\"true\"] { border: 1px solid %1; padding-top: 0; margin-top: 0; }")
	//					.arg(QPalette().color(QPalette::ColorRole::Mid).name(QColor::HexRgb));
	//
	auto videoPage = new QWidget;
	videoPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//	videoPage->setStyleSheet(pageStyle);
	//	videoPage->setProperty("page", true);
	auto videoPageLayout = new QFormLayout;
	videoPage->setLayout(videoPageLayout);

	auto audioPage = new QWidget;
	audioPage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//	audioPage->setStyleSheet(pageStyle);
	//	audioPage->setProperty("page", true);
	auto audioPageLayout = new QFormLayout;
	audioPage->setLayout(audioPageLayout);

	auto outputVideoEncoder = new QComboBox;
	outputVideoEncoder->addItem(QString::fromUtf8(obs_module_text("None")), QVariant(QString::fromUtf8("")));
	videoPageLayout->addRow(QString::fromUtf8(obs_module_text("OutputVideoEncoder")), outputVideoEncoder);

	// VIDEO ENCODER
	auto videoEncoder = new QComboBox;
	videoEncoder->addItem(QString::fromUtf8(obs_module_text("DefaultEncoder")), QVariant(QString::fromUtf8("")));
	videoEncoder->setCurrentIndex(0);
	videoPageLayout->addRow(QString::fromUtf8(obs_module_text("VideoEncoder")), videoEncoder);

	bool allEmpty = false;
	QComboBox *videoEncoderIndex = nullptr;

	videoEncoderIndex = new QComboBox;
	for (int i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; i++) {
		QString settingName = QString::fromUtf8("video_encoder_description") + QString::number(i);
		auto description = obs_data_get_string(main_settings, settingName.toUtf8().constData());
		if (!description || description[0] == '\0') {
			if (i != 0 && !allEmpty) {
				break;
			}
			allEmpty = true;
		}
		videoEncoderIndex->addItem(QString::number(i + 1) + " " + description);
	}
	videoEncoderIndex->setCurrentIndex((int)obs_data_get_int(settings, "video_encoder_index"));

	connect(videoEncoderIndex, &QComboBox::currentIndexChanged, [videoEncoderIndex, settings] {
		if (videoEncoderIndex->currentIndex() >= 0)
			obs_data_set_int(settings, "video_encoder_index", videoEncoderIndex->currentIndex());
	});
	videoPageLayout->addRow(QString::fromUtf8(obs_module_text("VideoEncoderIndex")), videoEncoderIndex);

	if (videoEncoderIndex->currentIndex() < 0 &&
	    !config_get_bool(obs_frontend_get_profile_config(), "Stream1", "EnableMultitrackVideo"))
		videoPageLayout->setRowVisible(videoEncoderIndex, false);

	auto canvasComboChanged = [canvasCombo, outputVideoEncoder, settings, outputs] {
		obs_data_set_string(settings, "canvas", canvasCombo->currentText().toUtf8().constData());
		auto canvas_name = obs_data_get_string(settings, "canvas");
		auto canvas = canvas_name[0] == '\0' ? obs_get_main_canvas() : obs_get_canvas_by_name(canvas_name);
		auto main_canvas = obs_get_main_canvas();
		outputVideoEncoder->clear();
		outputVideoEncoder->addItem(QString::fromUtf8(obs_module_text("None")), QVariant(QString::fromUtf8("")));
		if (!canvas || main_canvas == canvas) {
			outputVideoEncoder->addItem(QString::fromUtf8(obs_module_text("MainEncoder")),
						    QVariant(QString::fromUtf8("MainEncoder")));
		}
		auto count = obs_data_array_count(outputs);
		for (size_t i = 0; i < count; i++) {
			auto output = obs_data_array_item(outputs, i);
			auto name = obs_data_get_string(output, "name");
			if (name && name[0] != '\0' && strcmp(obs_data_get_string(settings, "name"), name) != 0) {
				auto cn = obs_data_get_string(output, "canvas");
				if (cn[0] == '\0' || strcmp(cn, canvas_name) == 0) {
					outputVideoEncoder->addItem(QString::fromUtf8(name), QVariant(QString::fromUtf8(name)));
				}
			}
			obs_data_release(output);
		}
		obs_canvas_release(main_canvas);
		obs_canvas_release(canvas);
		auto ovei = outputVideoEncoder->findData(QString::fromUtf8(obs_data_get_string(settings, "output_video_encoder")));
		if (ovei >= 0)
			outputVideoEncoder->setCurrentIndex(ovei);
	};

	connect(canvasCombo, &QComboBox::currentTextChanged, canvasComboChanged);
	canvasComboChanged();

	auto videoEncoderGroup = new QWidget();
	videoEncoderGroup->setProperty("altColor", QVariant(true));
	auto videoEncoderGroupLayout = new QFormLayout();
	videoEncoderGroup->setLayout(videoEncoderGroupLayout);
	videoPageLayout->addRow(videoEncoderGroup);

	struct obs_video_info ovi;
	obs_get_video_info(&ovi);
	double fps = ovi.fps_den > 0 ? (double)ovi.fps_num / (double)ovi.fps_den : 0.0;
	auto fpsDivisor = new QComboBox;
	auto frd = obs_data_get_int(settings, "frame_rate_divisor");
	for (int i = 1; i <= 10; i++) {
		if (i == 1) {
			fpsDivisor->addItem(QString::number(fps, 'g', 3) + " " + QString::fromUtf8(obs_module_text("Original")), i);
			fpsDivisor->setCurrentIndex(0);
		} else {
			fpsDivisor->addItem(QString::number(fps / i, 'g', 3), i);
			if (frd == i) {
				fpsDivisor->setCurrentIndex(fpsDivisor->count() - 1);
			}
		}
	}
	connect(fpsDivisor, &QComboBox::currentIndexChanged,
		[fpsDivisor, settings] { obs_data_set_int(settings, "frame_rate_divisor", fpsDivisor->currentData().toInt()); });

	videoEncoderGroupLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Video.FPS")), fpsDivisor);
	//obs_encoder_get_frame_rate_divisor

	auto scale = new QGroupBox(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Output.Adv.Rescale")));
	scale->setCheckable(true);
	scale->setChecked(obs_data_get_bool(settings, "scale"));

	connect(scale, &QGroupBox::toggled, [scale, settings] { obs_data_set_bool(settings, "scale", scale->isChecked()); });

	auto scaleLayout = new QFormLayout();
	scale->setLayout(scaleLayout);

	auto downscale = new QComboBox;

	auto downscale_type = obs_data_get_int(settings, "scale_type");
	if (downscale_type == OBS_SCALE_DISABLE) {
		downscale_type = OBS_SCALE_BILINEAR;
		obs_data_set_int(settings, "scale_type", downscale_type);
	}
	downscale->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Video.DownscaleFilter.Bilinear")),
			   OBS_SCALE_BILINEAR);
	if (downscale_type == OBS_SCALE_BILINEAR)
		downscale->setCurrentIndex(0);
	downscale->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Video.DownscaleFilter.Area")),
			   OBS_SCALE_AREA);
	if (downscale_type == OBS_SCALE_AREA)
		downscale->setCurrentIndex(1);
	downscale->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Video.DownscaleFilter.Bicubic")),
			   OBS_SCALE_BICUBIC);
	if (downscale_type == OBS_SCALE_BICUBIC)
		downscale->setCurrentIndex(2);
	downscale->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Video.DownscaleFilter.Lanczos")),
			   OBS_SCALE_LANCZOS);
	if (downscale_type == OBS_SCALE_LANCZOS)
		downscale->setCurrentIndex(3);

	connect(downscale, &QComboBox::currentIndexChanged,
		[downscale, settings] { obs_data_set_int(settings, "scale_type", downscale->currentData().toInt()); });

	scaleLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Video.DownscaleFilter")), downscale);

	auto resolution = new QComboBox;
	resolution->setEditable(true);
	resolution->addItem("1280x720");
	resolution->addItem("1920x1080");
	resolution->addItem("2560x1440");
	resolution->addItem("720x1280");
	resolution->addItem("1080x1920");
	resolution->addItem("1440x2560");

	resolution->setCurrentText(QString::number(obs_data_get_int(settings, "width")) + "x" +
				   QString::number(obs_data_get_int(settings, "height")));
	if (resolution->currentText() == "0x0")
		resolution->setCurrentText(QString::number(ovi.output_width) + "x" + QString::number(ovi.output_height));

	connect(resolution, &QComboBox::currentTextChanged, [settings, resolution] {
		const auto res = resolution->currentText();
		uint32_t width, height;
		if (sscanf(res.toUtf8().constData(), "%dx%d", &width, &height) == 2 && width > 0 && height > 0) {
			obs_data_set_int(settings, "width", width);
			obs_data_set_int(settings, "height", height);
		}
	});

	scaleLayout->addRow(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Settings.Video.ScaledResolution")), resolution);

	videoEncoderGroupLayout->addRow(scale);

	auto ouputVideoEncoderChanged = [settings, outputVideoEncoder, videoEncoder, videoEncoderIndex, videoEncoderGroup,
					 videoPageLayout] {
		obs_data_set_string(settings, "output_video_encoder",
				    outputVideoEncoder->currentData().toString().toUtf8().constData());
		auto ove = obs_data_get_string(settings, "output_video_encoder");
		if (!ove || ove[0] == '\0') {
			videoPageLayout->setRowVisible(videoEncoder, true);
			QMetaObject::invokeMethod(videoEncoder, "currentIndexChanged", Q_ARG(int, videoEncoder->currentIndex()));
		} else if (strcmp(ove, "MainEncoder") == 0) {
			videoEncoder->setCurrentIndex(0);
			QMetaObject::invokeMethod(videoEncoder, "currentIndexChanged", Q_ARG(int, 0));
		} else {
			videoPageLayout->setRowVisible(videoEncoder, false);
			videoPageLayout->setRowVisible(videoEncoderIndex, false);
			videoEncoderGroup->setVisible(false);
		}
	};
	connect(outputVideoEncoder, &QComboBox::currentTextChanged, ouputVideoEncoderChanged);

	ouputVideoEncoderChanged();

	connect(videoEncoder, &QComboBox::currentIndexChanged,
		[this, outputGroup, advancedGroupLayout, videoPageLayout, videoEncoder, videoEncoderIndex, videoEncoderGroup,
		 videoEncoderGroupLayout, settings, videoPage] {
			auto encoder_string = videoEncoder->currentData().toString().toUtf8();
			auto encoder = encoder_string.constData();
			const bool encoder_changed = strcmp(obs_data_get_string(settings, "video_encoder"), encoder) != 0;
			if (encoder_changed)
				obs_data_set_string(settings, "video_encoder", encoder);
			if (!encoder || encoder[0] == '\0') {
				if (!videoEncoderIndex) {
				} else if (config_get_bool(obs_frontend_get_profile_config(), "Stream1", "EnableMultitrackVideo")) {
					videoPageLayout->setRowVisible(videoEncoderIndex, true);
				} else {
					videoPageLayout->setRowVisible(videoEncoderIndex, false);
					if (videoEncoderIndex->currentIndex() != 0)
						videoEncoderIndex->setCurrentIndex(0);
				}
				videoEncoderGroup->setVisible(false);
			} else {
				if (videoEncoderIndex)
					videoPageLayout->setRowVisible(videoEncoderIndex, false);
				if (!videoEncoderGroup->isVisibleTo(videoPage))
					videoEncoderGroup->setVisible(true);
				auto t = video_encoder_properties.find(outputGroup);
				if (t != video_encoder_properties.end()) {
					obs_properties_destroy(t->second);
					video_encoder_properties.erase(t);
				}
				for (int i = videoEncoderGroupLayout->rowCount() - 1; i >= 2; i--) {
					videoEncoderGroupLayout->removeRow(i);
				}
				auto ves = encoder_changed ? nullptr : obs_data_get_obj(settings, "video_encoder_settings");
				if (!ves) {
					auto defaults = obs_encoder_defaults(encoder);
					if (defaults) {
						ves = obs_data_get_defaults(defaults);
						obs_data_release(defaults);
					} else {
						ves = obs_data_create();
					}
					obs_data_set_obj(settings, "video_encoder_settings", ves);
				}
				auto stream_encoder_properties = obs_get_encoder_properties(encoder);
				video_encoder_properties[outputGroup] = stream_encoder_properties;

				obs_property_t *property = obs_properties_first(stream_encoder_properties);
				while (property) {
					AddProperty(stream_encoder_properties, property, ves, videoEncoderGroupLayout);
					obs_property_next(&property);
				}
				obs_data_release(ves);
			}
		});

	const char *current_type = obs_data_get_string(settings, "video_encoder");
	const char *type;
	size_t idx = 0;
	while (obs_enum_encoder_types(idx++, &type)) {
		if (obs_get_encoder_type(type) != OBS_ENCODER_VIDEO)
			continue;
		uint32_t caps = obs_get_encoder_caps(type);
		if ((caps & (OBS_ENCODER_CAP_DEPRECATED | OBS_ENCODER_CAP_INTERNAL)) != 0)
			continue;
		if (output_type[0] == '\0' || strcmp(output_type, "stream") == 0) {
			const char *codec = obs_get_encoder_codec(type);
			if (astrcmpi(codec, "h264") != 0 && astrcmpi(codec, "hevc") != 0 && astrcmpi(codec, "av1") != 0)
				continue;
		}
		videoEncoder->addItem(QString::fromUtf8(obs_encoder_get_display_name(type)), QVariant(QString::fromUtf8(type)));
		if (strcmp(type, current_type) == 0)
			videoEncoder->setCurrentIndex(videoEncoder->count() - 1);
	}
	if (videoEncoder->currentIndex() <= 0)
		videoEncoderGroup->setVisible(false);

	auto audioEncoder = new QComboBox;
	audioPageLayout->addRow(QString::fromUtf8(obs_module_text("AudioEncoder")), audioEncoder);
	if (output_type[0] == '\0' || strcmp(output_type, "stream") == 0) {
		auto audioTrack = new QComboBox;
		for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
			auto trackConfigName = QString::fromUtf8("Track") + QString::number(i + 1) + QString::fromUtf8("Name");
			auto trackName = QString::fromUtf8(config_get_string(obs_frontend_get_profile_config(), "AdvOut",
									     trackConfigName.toUtf8().constData()));
			if (trackName.isEmpty()) {
				auto trackTranslationName =
					QString::fromUtf8("Basic.Settings.Output.Adv.Audio.Track") + QString::number(i + 1);
				trackName = QString::fromUtf8(
					obs_frontend_get_locale_string(trackTranslationName.toUtf8().constData()));
			}
			if (trackName.isEmpty())
				trackName = QString::number(i + 1);
			audioTrack->addItem(trackName);
		}
		audioTrack->setCurrentIndex((int)obs_data_get_int(settings, "audio_track"));
		connect(audioTrack, &QComboBox::currentIndexChanged, [audioTrack, settings] {
			if (audioTrack->currentIndex() >= 0)
				obs_data_set_int(settings, "audio_track", audioTrack->currentIndex());
		});
		audioPageLayout->addRow(QString::fromUtf8(obs_module_text("AudioTrack")), audioTrack);
	} else {
		auto at = obs_data_get_int(settings, "audio_tracks");
		std::vector<QCheckBox *> audioTracks;
		auto al = new QHBoxLayout;
		for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
			auto trackConfigName = QString::fromUtf8("Track") + QString::number(i + 1) + QString::fromUtf8("Name");
			auto trackName = QString::fromUtf8(config_get_string(obs_frontend_get_profile_config(), "AdvOut",
									     trackConfigName.toUtf8().constData()));
			if (trackName.isEmpty()) {
				auto trackTranslationName =
					QString::fromUtf8("Basic.Settings.Output.Adv.Audio.Track") + QString::number(i + 1);
				trackName = QString::fromUtf8(
					obs_frontend_get_locale_string(trackTranslationName.toUtf8().constData()));
			}
			if (trackName.isEmpty())
				trackName = QString::number(i + 1);
			auto check = new QCheckBox(trackName);
			check->setChecked((at & (1ll << i)) != 0);
			audioTracks.push_back(check);
			al->addWidget(check);
		}
		for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
			connect(audioTracks[i], &QCheckBox::checkStateChanged, [this, settings, audioTracks] {
#else
			connect(audioTracks[i], &QCheckBox::stateChanged, [this, settings, audioTracks] {
#endif
				long long tracks = 0;
				for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
					if (audioTracks[i]->isChecked())
						tracks += (1ll << i);
				}
				if (!tracks)
					tracks = 1;
				obs_data_set_int(settings, "audio_tracks", tracks);
			});
		}
		audioPageLayout->addRow(QString::fromUtf8(obs_module_text("AudioTracks")), al);
	}

	auto audioEncoderGroup = new QWidget();
	audioEncoderGroup->setProperty("altColor", QVariant(true));
	auto audioEncoderGroupLayout = new QFormLayout();
	audioEncoderGroup->setLayout(audioEncoderGroupLayout);
	audioPageLayout->addRow(audioEncoderGroup);

	int audio_encoder_index = 0;
	current_type = obs_data_get_string(settings, "audio_encoder");
	idx = 0;
	while (obs_enum_encoder_types(idx++, &type)) {
		if (obs_get_encoder_type(type) != OBS_ENCODER_AUDIO)
			continue;
		uint32_t caps = obs_get_encoder_caps(type);
		if ((caps & (OBS_ENCODER_CAP_DEPRECATED | OBS_ENCODER_CAP_INTERNAL)) != 0)
			continue;
		if (output_type[0] == '\0' || strcmp(output_type, "stream") == 0) {
			const char *codec = obs_get_encoder_codec(type);
			if (astrcmpi(codec, "aac") != 0 && astrcmpi(codec, "opus") != 0)
				continue;
		}
		audioEncoder->addItem(QString::fromUtf8(obs_encoder_get_display_name(type)), QVariant(QString::fromUtf8(type)));
		if (strcmp(type, current_type) == 0)
			audio_encoder_index = audioEncoder->count() - 1;
	}

	connect(audioEncoder, &QComboBox::currentIndexChanged,
		[this, outputGroup, advancedGroupLayout, audioPageLayout, audioEncoder, audioEncoderGroup, audioEncoderGroupLayout,
		 settings, audioPage] {
			auto encoder_string = audioEncoder->currentData().toString().toUtf8();
			auto encoder = encoder_string.constData();
			const bool encoder_changed = !encoder_string.isEmpty() &&
						     strcmp(obs_data_get_string(settings, "audio_encoder"), encoder) != 0;
			if (encoder_changed)
				obs_data_set_string(settings, "audio_encoder", encoder);

			auto t = audio_encoder_properties.find(outputGroup);
			if (t != audio_encoder_properties.end()) {
				obs_properties_destroy(t->second);
				audio_encoder_properties.erase(t);
			}
			for (int i = audioEncoderGroupLayout->rowCount() - 1; i >= 0; i--) {
				audioEncoderGroupLayout->removeRow(i);
			}
			auto aes = encoder_changed ? nullptr : obs_data_get_obj(settings, "audio_encoder_settings");
			if (!aes) {
				auto defaults = obs_encoder_defaults(encoder);
				if (defaults) {
					aes = obs_data_get_defaults(defaults);
					obs_data_release(defaults);
				} else {
					aes = obs_data_create();
				}
				obs_data_set_obj(settings, "audio_encoder_settings", aes);
			}
			auto stream_encoder_properties = obs_get_encoder_properties(encoder);
			audio_encoder_properties[outputGroup] = stream_encoder_properties;

			obs_property_t *property = obs_properties_first(stream_encoder_properties);
			while (property) {
				AddProperty(stream_encoder_properties, property, aes, audioEncoderGroupLayout);
				obs_property_next(&property);
			}
			obs_data_release(aes);
		});

	if (audio_encoder_index == audioEncoder->currentIndex())
		audioEncoder->setCurrentIndex(-1);
	audioEncoder->setCurrentIndex(audio_encoder_index);

	const bool advanced = obs_data_get_bool(settings, "advanced");

	auto advancedButton = new QCheckBox(QString::fromUtf8(obs_module_text("CustomEncoderSettings")));
	advancedButton->setCheckable(true);
	advancedButton->setChecked(advanced);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	connect(advancedButton, &QCheckBox::checkStateChanged, [advancedButton, advancedGroup, settings] {
#else
	connect(advancedButton, &QCheckBox::stateChanged, [advancedButton, advancedGroup, settings] {
#endif
		const bool is_advanced = advancedButton->isChecked();
		const bool expanded = obs_data_get_bool(settings, "expanded");
		advancedGroup->setVisible(is_advanced && expanded);
		obs_data_set_bool(settings, "advanced", is_advanced);
	});

	connect(streaming_title, &QToolButton::toggled,
		[advancedGroup, advancedButton, streaming_title, settings, canvasCombo, outputLayout, startHotkeyWidget,
		 stopHotkeyWidget, extraHotkeyWidget](bool checked) {
			const bool advanced = obs_data_get_bool(settings, "advanced");
			advancedButton->setVisible(checked);
			outputLayout->setRowVisible(canvasCombo, checked);
			if (startHotkeyWidget)
				outputLayout->setRowVisible(startHotkeyWidget, checked);
			if (stopHotkeyWidget)
				outputLayout->setRowVisible(stopHotkeyWidget, checked);
			if (extraHotkeyWidget)
				outputLayout->setRowVisible(extraHotkeyWidget, checked);
			advancedGroup->setVisible(checked && advanced);
			obs_data_set_bool(settings, "expanded", checked);
			streaming_title->setArrowType(checked ? Qt::ArrowType::DownArrow : Qt::ArrowType::RightArrow);
		});

	if (expanded) {
		streaming_title->setArrowType(Qt::ArrowType::DownArrow);
		if (!advanced)
			advancedGroup->setVisible(false);
	} else {
		advancedButton->setVisible(false);
		advancedGroup->setVisible(false);
	}

	// Hook up
	advancedTabWidget->addTab(videoPage, QString::fromUtf8(obs_module_text("VideoEncoderSettings")));
	advancedTabWidget->addTab(audioPage, QString::fromUtf8(obs_module_text("AudioEncoderSettings")));
	advancedGroupLayout->addWidget(advancedTabWidget, 1);

	outputLayout->addWidget(advancedButton);

	outputLayout->addRow(advancedGroup);

	if (isNew) {
		canvasCombo->show();
		canvasCombo->setFocus();
		//canvasCombo->setFocus(Qt::OtherFocusReason);
	}
}

void OBSBasicSettings::AddRecord(bool backtrack)
{
	QStringList otherNames;
	obs_data_array_enum(
		extra_outputs,
		[](obs_data_t *data2, void *param) {
			((QStringList *)param)->append(QString::fromUtf8(obs_data_get_string(data2, "name")));
		},
		&otherNames);
	otherNames.removeDuplicates();
	auto outputDialog = new RecordOutputDialog(this, otherNames, backtrack);

	outputDialog->setWindowModality(Qt::WindowModal);
	outputDialog->setModal(true);

	if (outputDialog->exec() == QDialog::Accepted) {
		// create a new output
		if (!extra_outputs)
			return;
		auto s = obs_data_create();
		obs_data_set_bool(s, "enabled", true);
		obs_data_set_bool(s, "expanded", true);
		obs_data_set_string(s, "type", backtrack ? "backtrack" : "record");
		obs_data_set_string(s, "name", outputDialog->outputName.toUtf8().constData());
		obs_data_set_string(s, "path", outputDialog->recordPath.toUtf8().constData());
		obs_data_set_string(s, "filename", outputDialog->filenameFormat.toUtf8().constData());
		obs_data_set_string(s, "format", outputDialog->fileFormat.toUtf8().constData());
		obs_data_set_int(s, "max_size_mb", outputDialog->maxSize);
		obs_data_set_int(s, "max_time_sec", outputDialog->maxTime);
		obs_data_array_push_back(extra_outputs, s);
		AddOutput(outputsLayout, s, extra_outputs, true);
		obs_data_release(s);
	}

	delete outputDialog;
}

obs_hotkey_t *OBSBasicSettings::GetHotkeyByName(const char *name)
{
	struct find_hotkey {
		obs_hotkey_t *hotkey;
		const char *name;
	};
	find_hotkey t = {};
	t.name = name;
	obs_enum_hotkeys(
		[](void *param, obs_hotkey_id id, obs_hotkey_t *key) {
			UNUSED_PARAMETER(id);
			const auto hp = (struct find_hotkey *)param;
			const auto hn = obs_hotkey_get_name(key);
			if (strcmp(hp->name, hn) == 0)
				hp->hotkey = key;
			return true;
		},
		&t);
	return t.hotkey;
}

std::vector<obs_key_combination_t> OBSBasicSettings::GetCombosForHotkey(obs_hotkey_id hotkey)
{
	struct find_combos {
		obs_hotkey_id hotkey;
		std::vector<obs_key_combination_t> combos;
	};
	find_combos t = {hotkey, {}};
	obs_enum_hotkey_bindings(
		[](void *param, size_t idx, obs_hotkey_binding_t *binding) {
			UNUSED_PARAMETER(idx);
			auto fc = (struct find_combos *)param;
			if (fc->hotkey == obs_hotkey_binding_get_hotkey_id(binding)) {
				fc->combos.push_back(obs_hotkey_binding_get_key_combination(binding));
			}
			return true;
		},
		&t);
	return t.combos;
}

void OBSBasicSettings::SaveHotkeys()
{
	for (auto &hw : hotkeys) {
		hw->Save();
	}
}
