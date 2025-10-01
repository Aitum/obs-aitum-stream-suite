#include "scene-collection-wizard.hpp"
#include "util/platform.h"
#include <obs-module.h>
#include <QLayout>
#include <QListWidget>
#include <QFormLayout>
#include <QComboBox>

SceneCollectionWizard::SceneCollectionWizard(QWidget *parent) : QWizard(parent)
{
	setPage(Page_SceneCollection, new SceneCollectionPage);
	setPage(Page_Cam, new CamPage);
	setPage(Page_Mic, new MicPage);
	setPage(Page_Conclusion, new ConclusionPage);

	setStartId(Page_SceneCollection);

	setWindowTitle(QString::fromUtf8("SceneCollectionWizard"));

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

#if defined(_WIN32) || defined(__APPLE__)
	setWizardStyle(QWizard::ModernStyle);
#endif
}

int SceneCollectionWizard::nextId() const
{
	return QWizard::nextId();
	//return SceneCollectionWizard::Page_Cam;
}

SceneCollectionPage::SceneCollectionPage(QWidget *parent) : QWizardPage(parent)
{
	setTitle(QString::fromUtf8(obs_module_text("SceneCollection")));
	setSubTitle(QString::fromUtf8(obs_module_text("SceneCollectionDescription")));

	scl = new QListWidget;

	auto fl = new QFormLayout;
	setLayout(fl);

	fl->addRow(QString::fromUtf8(obs_module_text("SceneCollection")), scl);

	std::string path = obs_get_module_data_path(obs_current_module());
	if (path.back() != '/')
		path += '/';
	path += "scenecollections/*.json";

	os_glob_t *glob;
	if (os_glob(path.c_str(), 0, &glob) != 0) {
		return;
	}
	for (size_t i = 0; i < glob->gl_pathc; i++) {
		if (glob->gl_pathv[i].directory)
			continue;
		const char *filePath = glob->gl_pathv[i].path;
		const char *fn = filePath;
		const char *slash = strrchr(filePath, '/');
		const char *backslash = strrchr(filePath, '\\');
		if (slash && (!backslash || backslash < slash)) {
			fn = slash + 1;
		} else if (backslash && (!slash || slash < backslash)) {
			fn = backslash + 1;
		}
		auto item = new QListWidgetItem(QString::fromUtf8(fn, strlen(fn) - 5), scl);
		item->setData(Qt::UserRole, QString::fromUtf8(filePath));
		scl->addItem(item);
	}
	os_globfree(glob);
}

bool SceneCollectionPage::validatePage()
{
	if (!QWizardPage::validatePage())
		return false;

	auto item = scl->currentItem();
	if (!item)
		return false;

	return true;
}

CamPage::CamPage(QWidget *parent) : QWizardPage(parent)
{
	setTitle(QString::fromUtf8(obs_module_text("Cam")));
	setSubTitle(QString::fromUtf8(obs_module_text("CamDescription")));

	auto fl = new QFormLayout;
	setLayout(fl);
	cc = new QComboBox;
	fl->addRow(QString::fromUtf8("Cam"), cc);

#ifdef WIN32
	const char *id = "dshow_input";
	const char *setting_name = "video_device_id";
#elif __APPLE__
	const char *id = "av_capture_input";
	const char *setting_name = "device";
#else
	const char *id = "v4l2_input";
	const char *setting_name = "input";
#endif
	cam = obs_source_create_private(id, "cam", nullptr);

	//obs_get_source_properties(id);
	auto props = obs_source_properties(cam);
	auto prop = obs_properties_get(props, setting_name);
	size_t c = obs_property_list_item_count(prop);
	int list_format = obs_property_list_format(prop);
	if (list_format == OBS_COMBO_FORMAT_INT) {
		for (size_t i = 0; i < c; i++) {
			cc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)), QVariant(obs_property_list_item_int(prop, i)));
		}
	} else if (list_format == OBS_COMBO_FORMAT_FLOAT) {
		for (size_t i = 0; i < c; i++) {
			cc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)),
				    QVariant(obs_property_list_item_float(prop, i)));
		}
	} else if (list_format == OBS_COMBO_FORMAT_STRING) {
		for (size_t i = 0; i < c; i++) {
			cc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)),
				    QVariant(QString::fromUtf8(obs_property_list_item_string(prop, i))));
		}
	} else if (list_format == OBS_COMBO_FORMAT_BOOL) {
		for (size_t i = 0; i < c; i++) {
			cc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)),
				    QVariant(obs_property_list_item_bool(prop, i)));
		}
	}
	obs_properties_destroy(props);
}

void CamPage::initializePage()
{
	QWizardPage::initializePage();
}

MicPage::MicPage(QWidget *parent) : QWizardPage(parent)
{
	setTitle(QString::fromUtf8(obs_module_text("Mic")));
	setSubTitle(QString::fromUtf8(obs_module_text("MicDescription")));
}

ConclusionPage::ConclusionPage(QWidget *parent) : QWizardPage(parent)
{
	setTitle(QString::fromUtf8(obs_module_text("Conclusion")));
	setSubTitle(QString::fromUtf8(obs_module_text("ConclusionDescription")));
}
