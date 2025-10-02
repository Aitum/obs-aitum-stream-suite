#include "scene-collection-wizard.hpp"
#include "util/platform.h"
#include <obs-module.h>
#include <QLayout>
#include <QListWidget>
#include <QFormLayout>
#include <QComboBox>
#include <obs-frontend-api.h>
#include <util/config-file.h>

SceneCollectionWizard::SceneCollectionWizard(QWidget *parent) : QWizard(parent)
{
	setPage(Page_SceneCollection, new SceneCollectionPage);
	setPage(Page_Cam, new CamPage);
	setPage(Page_Mic, new MicPage);
	setPage(Page_Conclusion, new ConclusionPage);

	setStartId(Page_SceneCollection);

	setWindowTitle(QString::fromUtf8(obs_module_text("SceneCollectionWizard")));

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

#if defined(_WIN32) || defined(__APPLE__)
	setWizardStyle(QWizard::ModernStyle);
#endif
}

SceneCollectionWizard::~SceneCollectionWizard()
{
	if (scene_collection_data)
		obs_data_release(scene_collection_data);
}

void SceneCollectionWizard::LoadSceneCollectionFile(QString path)
{
	if (scene_collection_data)
		obs_data_release(scene_collection_data);

	cams.clear();
	mics.clear();

	scene_collection_data = obs_data_create_from_json_file(path.toUtf8().constData());
	if (!scene_collection_data)
		return;

	auto sources = obs_data_get_array(scene_collection_data, "sources");
	size_t c = obs_data_array_count(sources);
	for (size_t i = 0; i < c; i++) {
		auto item = obs_data_array_item(sources, i);
		if (!item)
			continue;
		CheckSource(item);
		obs_data_release(item);
	}
	obs_data_array_release(sources);
	auto globalAudio = {"DesktopAudioDevice1", "DesktopAudioDevice2", "AuxAudioDevice1",
			    "AuxAudioDevice2",     "AuxAudioDevice3",     "AuxAudioDevice4"};
	for (auto ga : globalAudio) {
		auto item = obs_data_get_obj(scene_collection_data, ga);
		if (!item)
			continue;
		CheckSource(item);
		obs_data_release(item);
	}
}

void SceneCollectionWizard::CheckSource(obs_data_t *item)
{
	auto source_type = obs_data_get_string(item, "id");
	if (strcmp(source_type, "dshow_input") == 0 || strcmp(source_type, "av_capture_input") == 0 ||
	    strcmp(source_type, "v4l2_input") == 0) {
		cams.push_back(item);
	} else if (strcmp(source_type, "wasapi_input_capture") == 0 || strcmp(source_type, "coreaudio_input_capture") == 0 ||
		   strcmp(source_type, "pulse_input_capture") == 0) {
		mics.push_back(item);
	}
}

int SceneCollectionWizard::nextId() const
{
	if (!cams.empty())
		return SceneCollectionWizard::Page_Cam;
	if (!mics.empty())
		return SceneCollectionWizard::Page_Mic;
	if (currentId() == SceneCollectionWizard::Page_Conclusion)
		return -1;
	return SceneCollectionWizard::Page_Conclusion;
}

obs_data_t *SceneCollectionWizard::GetCam()
{
	auto cam = cams.front();
	cams.pop_front();
	return cam;
}

obs_data_t *SceneCollectionWizard::GetMic()
{
	auto mic = mics.front();
	mics.pop_front();
	return mic;
}

static std::string _scene_collections_path;

static std::string SceneCollectionsPath()
{
	if (!_scene_collections_path.empty())
		return _scene_collections_path;
	char *path = obs_module_config_path("");
	size_t l = strlen(path);
	if (l == 0) {
		bfree(path);
		return _scene_collections_path;
	}
	//remove double slashes
	char c = path[l - 1];
	while (l > 0 && (c == '\\' || c == '/')) {
		l--;
		path[l] = '\0';
		c = path[l - 1];
	}
	//remove module name from path
	char *slash = strrchr(path, '/');
	char *backslash = strrchr(path, '\\');
	if (slash && (!backslash || slash > backslash)) {
		*slash = '\0';
	} else if (backslash && (!slash || backslash > slash)) {
		*backslash = '\0';
	}
	//remove double slashes
	l = strlen(path);
	if (l > 0) {
		c = path[l - 1];
		while (l > 0 && (c == '\\' || c == '/')) {
			l--;
			path[l] = '\0';
			c = path[l - 1];
		}
	}
	//remove plugin_config from path
	slash = strrchr(path, '/');
	backslash = strrchr(path, '\\');
	if (slash && (!backslash || slash > backslash)) {
		*slash = '\0';
	} else if (backslash && (!slash || backslash > slash)) {
		*backslash = '\0';
	}
	//remove double slashes
	l = strlen(path);
	if (l > 0) {
		c = path[l - 1];
		while (l > 0 && (c == '\\' || c == '/')) {
			l--;
			path[l] = '\0';
			c = path[l - 1];
		}
	}

	_scene_collections_path = path;
	_scene_collections_path += "/basic/scenes/";
	bfree(path);
	return _scene_collections_path;
}

bool SceneCollectionWizard::SaveSceneCollection()
{
	const auto config = obs_frontend_get_user_config();
	std::string old_name = config_get_string(config, "Basic", "SceneCollection");

	auto name = obs_data_get_string(scene_collection_data, "name");
	if(!obs_frontend_add_scene_collection(name))
		return false;

	std::string path = SceneCollectionsPath() + config_get_string(config, "Basic", "SceneCollectionFile");
	obs_frontend_set_current_scene_collection(old_name.c_str());
	obs_data_save_json_safe(scene_collection_data, path.c_str(), "tmp", "bak");
	obs_frontend_set_current_scene_collection(name);
	return true;
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

	static_cast<SceneCollectionWizard *>(wizard())->LoadSceneCollectionFile(item->data(Qt::UserRole).toString());

	return true;
}

CamPage::CamPage(QWidget *parent) : QWizardPage(parent), preview(new OBSQTDisplay(this))
{
	setTitle(QString::fromUtf8(obs_module_text("Cam")));
	setSubTitle(QString::fromUtf8(obs_module_text("CamDescription")));

	obs_enter_graphics();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(1.0f, 1.0f);
	box = gs_render_save();

	obs_leave_graphics();

	auto fl = new QFormLayout;
	setLayout(fl);
	cc = new QComboBox;
	fl->addRow(QString::fromUtf8(obs_module_text("Cam")), cc);

	fl->addRow(preview);

	preview->setObjectName(QStringLiteral("preview"));
	preview->setMinimumSize(QSize(200, 200));
	connect(preview, &OBSQTDisplay::DisplayCreated,
		[this]() { obs_display_add_draw_callback(preview->GetDisplay(), DrawPreview, this); });

	connect(cc, &QComboBox::currentIndexChanged, [this] {
		auto data = cc->currentData();
		auto settings = obs_source_get_settings(cam);
		if (data.isNull()) {
			obs_data_set_string(settings, setting_name, "");
		} else {
			auto val = data.toString();
			obs_data_set_string(settings, setting_name, val.toUtf8().constData());
		}
		obs_source_update(cam, settings);
		obs_data_release(settings);
	});

	cam = obs_source_create_private(id, "cam", nullptr);

	//obs_get_source_properties(id);
	auto props = obs_source_properties(cam);
	auto prop = obs_properties_get(props, setting_name);
	size_t c = obs_property_list_item_count(prop);
	int list_format = obs_property_list_format(prop);
	if (list_format == OBS_COMBO_FORMAT_INT) {
		for (size_t i = 0; i < c; i++) {
			cc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)),
				    QVariant(obs_property_list_item_int(prop, i)));
		}
	} else if (list_format == OBS_COMBO_FORMAT_FLOAT) {
		for (size_t i = 0; i < c; i++) {
			cc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)),
				    QVariant(obs_property_list_item_float(prop, i)));
		}
	} else if (list_format == OBS_COMBO_FORMAT_STRING) {
		for (size_t i = 0; i < c; i++) {
			auto val = obs_property_list_item_string(prop, i);
			cc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)), QVariant(QString::fromUtf8(val)));
		}
	} else if (list_format == OBS_COMBO_FORMAT_BOOL) {
		for (size_t i = 0; i < c; i++) {
			cc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)),
				    QVariant(obs_property_list_item_bool(prop, i)));
		}
	}
	obs_properties_destroy(props);
}

CamPage::~CamPage()
{
	obs_enter_graphics();
	gs_vertexbuffer_destroy(box);
	obs_leave_graphics();
}

void CamPage::initializePage()
{
	QWizardPage::initializePage();
}

void CamPage::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	CamPage *window = static_cast<CamPage *>(data);
	if (!window || !window->cam)
		return;

	uint32_t sourceCX = obs_source_get_width(window->cam);
	if (sourceCX <= 0)
		sourceCX = 1;
	uint32_t sourceCY = obs_source_get_height(window->cam);
	if (sourceCY <= 0)
		sourceCY = 1;

	int x, y;
	float scale;

	CamPage::GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);
	auto newCX = scale * float(sourceCX);
	auto newCY = scale * float(sourceCY);
	int newCx = newCX;
	int newCy = newCY;

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(0.0f, newCx, 0.0f, newCy, -100.0f, 100.0f);
	gs_set_viewport(x, y, newCx, newCy);
	window->DrawBackdrop(newCx, newCy);

	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, (int)newCX, (int)newCY);
	obs_source_video_render(window->cam);

	gs_set_linear_srgb(previous);

	gs_ortho(float(-x), newCX + float(x), float(-y), newCY + float(y), -100.0f, 100.0f);
	gs_reset_viewport();

	gs_projection_pop();
	gs_viewport_pop();
}

void CamPage::GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY, int &x, int &y, float &scale)
{
	double windowAspect, baseAspect;
	int newCX, newCY;

	windowAspect = double(windowCX) / double(windowCY);
	baseAspect = double(baseCX) / double(baseCY);

	if (windowAspect > baseAspect) {
		scale = float(windowCY) / float(baseCY);
		newCX = int(double(windowCY) * baseAspect);
		newCY = windowCY;
	} else {
		scale = float(windowCX) / float(baseCX);
		newCX = windowCX;
		newCY = int(float(windowCX) / baseAspect);
	}

	x = windowCX / 2 - newCX / 2;
	y = windowCY / 2 - newCY / 2;
}

void CamPage::DrawBackdrop(float cx, float cy)
{
	if (!box)
		return;

	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "DrawBackdrop");

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_eparam_t *color = gs_effect_get_param_by_name(solid, "color");
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	vec4 colorVal;
	vec4_set(&colorVal, 0.0f, 0.0f, 0.0f, 1.0f);
	gs_effect_set_vec4(color, &colorVal);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_scale3f(float(cx), float(cy), 1.0f);

	gs_load_vertexbuffer(box);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_matrix_pop();
	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	gs_load_vertexbuffer(nullptr);

	GS_DEBUG_MARKER_END();
}

bool CamPage::validatePage()
{
	auto data = cc->currentData();
	if (data.isNull())
		return false;

	auto cam_data = static_cast<SceneCollectionWizard *>(wizard())->GetCam();
	obs_data_set_string(cam_data, "id", id);
	auto cam_settings = obs_data_get_obj(cam_data, "settings");
	if (!cam_settings) {
		cam_settings = obs_data_create();
		obs_data_set_obj(cam_data, "settings", cam_settings);
	}
	auto val = data.toString();
	obs_data_set_string(cam_settings, setting_name, val.toUtf8().constData());
	obs_data_release(cam_settings);

	auto settings = obs_source_get_settings(cam);
	obs_data_set_string(settings, setting_name, "");
	obs_source_update(cam, settings);
	obs_data_release(settings);
	return true;
}

MicPage::MicPage(QWidget *parent) : QWizardPage(parent)
{
	setTitle(QString::fromUtf8(obs_module_text("Mic")));
	setSubTitle(QString::fromUtf8(obs_module_text("MicDescription")));

	auto fl = new QFormLayout;
	setLayout(fl);
	mc = new QComboBox;
	fl->addRow(QString::fromUtf8(obs_module_text("Mic")), mc);

	connect(mc, &QComboBox::currentIndexChanged, [this] {
		auto data = mc->currentData();
		auto settings = obs_source_get_settings(mic);
		if (data.isNull()) {
			obs_data_set_string(settings, setting_name, "");
		} else {
			auto val = data.toString();
			obs_data_set_string(settings, setting_name, val.toUtf8().constData());
		}
		obs_source_update(mic, settings);
		obs_data_release(settings);
	});

	mic = obs_source_create_private(id, "mic", nullptr);

	auto props = obs_source_properties(mic);
	auto prop = obs_properties_get(props, setting_name);
	size_t c = obs_property_list_item_count(prop);
	int list_format = obs_property_list_format(prop);
	if (list_format == OBS_COMBO_FORMAT_INT) {
		for (size_t i = 0; i < c; i++) {
			mc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)),
				    QVariant(obs_property_list_item_int(prop, i)));
		}
	} else if (list_format == OBS_COMBO_FORMAT_FLOAT) {
		for (size_t i = 0; i < c; i++) {
			mc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)),
				    QVariant(obs_property_list_item_float(prop, i)));
		}
	} else if (list_format == OBS_COMBO_FORMAT_STRING) {
		for (size_t i = 0; i < c; i++) {
			auto val = obs_property_list_item_string(prop, i);
			mc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)), QVariant(QString::fromUtf8(val)));
		}
	} else if (list_format == OBS_COMBO_FORMAT_BOOL) {
		for (size_t i = 0; i < c; i++) {
			mc->addItem(QString::fromUtf8(obs_property_list_item_name(prop, i)),
				    QVariant(obs_property_list_item_bool(prop, i)));
		}
	}
	obs_properties_destroy(props);
}

bool MicPage::validatePage()
{
	auto data = mc->currentData();
	if (data.isNull())
		return false;

	auto mic_data = static_cast<SceneCollectionWizard *>(wizard())->GetMic();
	obs_data_set_string(mic_data, "id", id);
	auto mic_settings = obs_data_get_obj(mic_data, "settings");
	if (!mic_settings) {
		mic_settings = obs_data_create();
		obs_data_set_obj(mic_data, "settings", mic_settings);
	}
	auto val = data.toString();
	obs_data_set_string(mic_settings, setting_name, val.toUtf8().constData());
	obs_data_release(mic_settings);

	auto settings = obs_source_get_settings(mic);
	obs_data_set_string(settings, setting_name, "");
	obs_source_update(mic, settings);
	obs_data_release(settings);
	return true;
}

ConclusionPage::ConclusionPage(QWidget *parent) : QWizardPage(parent)
{
	setTitle(QString::fromUtf8(obs_module_text("Conclusion")));
	setSubTitle(QString::fromUtf8(obs_module_text("ConclusionDescription")));
	setFinalPage(true);
}

bool ConclusionPage::validatePage() {
	if (!QWizardPage::validatePage())
		return false;
	return static_cast<SceneCollectionWizard *>(wizard())->SaveSceneCollection();
}
