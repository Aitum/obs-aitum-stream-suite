#include "canvas-clone-dock.hpp"
#include "canvas-dock.hpp"
#include <obs-module.h>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QScrollArea>
#include <src/utils/color.hpp>
#include <src/utils/widgets/switching-splitter.hpp>

std::list<CanvasCloneDock *> canvas_clone_docks;
extern std::list<CanvasDock *> canvas_docks;
extern QTabBar *modesTabBar;

CanvasCloneDock::CanvasCloneDock(obs_data_t *settings_, QWidget *parent)
	: QFrame(parent),
	  preview(new OBSQTDisplay(this)),
	  settings(settings_)
{
	obs_enter_graphics();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(1.0f, 1.0f);
	box = gs_render_save();

	obs_leave_graphics();

	canvas_split = new SwitchingSplitter;
	canvas_split->setContentsMargins(0, 0, 0, 0);

	auto c = color_from_int(obs_data_get_int(settings, "color"));
	setStyleSheet(QString("QFrame{border: 2px solid %1;}").arg(c.name(QColor::HexRgb)));

	auto l = new QBoxLayout(QBoxLayout::TopToBottom, this);
	l->setContentsMargins(0, 0, 0, 0);
	setLayout(l);
	//l->addWidget(preview);
	l->addWidget(canvas_split);
	canvas_split->setOrientation(Qt::Vertical);
	canvas_split->addWidget(preview);

	auto replaceGroup = new QGroupBox(QString::fromUtf8(obs_module_text("Replace")));
	replaceGroup->setContentsMargins(0, 0, 0, 0);

	auto scroll = new QScrollArea;
	auto replaceWidget = new QFrame;
	replaceWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	scroll->setWidget(replaceWidget);
	scroll->setWidgetResizable(true);
	scroll->setLineWidth(0);
	scroll->setFrameShape(QFrame::NoFrame);

	replaceGroup->setLayout(new QBoxLayout(QBoxLayout::TopToBottom));
	replaceGroup->layout()->setContentsMargins(0, 0, 0, 0);
	replaceGroup->layout()->addWidget(scroll);

	auto replaceGroupLayout = new QGridLayout();
	replaceGroupLayout->setContentsMargins(0, 0, 0, 0);
	replaceGroupLayout->setSpacing(0);
	replaceWidget->setLayout(replaceGroupLayout);

	replaceGroupLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("CanvasCloneReplace"))), 0, 0);
	replaceGroupLayout->addWidget(new QLabel(QString::fromUtf8(obs_module_text("CanvasCloneReplacement"))), 0, 1);

	for (int i = 0; i < 5; i++) {
		auto sourceCombo = new QComboBox;
		sourceCombo->setEditable(true);
		sourceCombo->insertItem(0, "");

		connect(sourceCombo, &QComboBox::editTextChanged, [this, sourceCombo, i] {
			auto text = sourceCombo->currentText().trimmed();
			auto arr = obs_data_get_array(settings, "replace_sources");
			auto item = obs_data_array_item(arr, i);
			if (!item) {
				item = obs_data_create();
				obs_data_array_push_back(arr, item);
			} else if (strcmp(obs_data_get_string(item, "source"), text.toUtf8().constData()) == 0) {
			} else {
				obs_data_set_string(item, "source", text.toUtf8().constData());
				LoadReplacements();
			}
			obs_data_release(item);
			obs_data_array_release(arr);
		});

		auto replaceCombo = new QComboBox;
		replaceCombo->setEditable(true);
		replaceCombo->insertItem(0, "");

		connect(replaceCombo, &QComboBox::editTextChanged, [this, replaceCombo, i] {
			auto text = replaceCombo->currentText().trimmed();
			auto arr = obs_data_get_array(settings, "replace_sources");
			auto item = obs_data_array_item(arr, i);
			if (!item) {
				item = obs_data_create();
				obs_data_array_push_back(arr, item);
			} else if (strcmp(obs_data_get_string(item, "replacement"), text.toUtf8().constData()) == 0) {
			} else {
				obs_data_set_string(item, "replacement", text.toUtf8().constData());
				LoadReplacements();
			}
			obs_data_release(item);
			obs_data_array_release(arr);
		});
		replaceGroupLayout->addWidget(sourceCombo, i + 1, 0);
		replaceGroupLayout->addWidget(replaceCombo, i + 1, 1);

		replaceCombos.push_back(std::make_pair(sourceCombo, replaceCombo));
	}
	obs_enum_sources(AddSourceToCombos, this);

	canvas_split->addWidget(replaceGroup);

	preview->setObjectName(QStringLiteral("preview"));
	preview->setMinimumSize(QSize(24, 24));
	QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
	sizePolicy1.setHorizontalStretch(0);
	sizePolicy1.setVerticalStretch(0);
	sizePolicy1.setHeightForWidth(preview->sizePolicy().hasHeightForWidth());
	preview->setSizePolicy(sizePolicy1);

	preview->show();
	connect(preview, &OBSQTDisplay::DisplayCreated,
		[this]() { obs_display_add_draw_callback(preview->GetDisplay(), DrawPreview, this); });

	canvas_width = (uint32_t)obs_data_get_int(settings, "width");
	canvas_height = (uint32_t)obs_data_get_int(settings, "height");

	auto clone_name = obs_data_get_string(settings, "clone");
	auto clone_canvas = clone_name[0] == '\0' ? obs_get_main_canvas() : obs_get_canvas_by_name(clone_name);
	if (clone_canvas) {
		clone = obs_canvas_get_weak_canvas(clone_canvas);
		obs_canvas_enum_scenes(clone_canvas, AddSourceToCombos, this);
		obs_video_info ovi;
		if (obs_canvas_get_video_info(clone_canvas, &ovi)) {
			if (ovi.base_width > 0)
				canvas_width = ovi.base_width;
			if (ovi.base_height > 0)
				canvas_height = ovi.base_height;
		} else {
			for (const auto &it : canvas_docks) {
				if (it->GetCanvas() == clone_canvas) {
					canvas_width = it->GetCanvasWidth();
					canvas_height = it->GetCanvasHeight();
				}
			}
			for (const auto &it : canvas_clone_docks) {
				if (it->GetCanvas() == clone_canvas) {
					canvas_width = it->GetCanvasWidth();
					canvas_height = it->GetCanvasHeight();
				}
			}
		}
		obs_canvas_release(clone_canvas);
	}
	if (canvas_width < 1)
		canvas_width = 1080;
	if (canvas_height < 1)
		canvas_height = 1920;

	std::string canvas_name = obs_data_get_string(settings, "name");
	if (canvas_name.empty())
		canvas_name = "Clone";

	canvas = obs_get_canvas_by_uuid(obs_data_get_string(settings, "uuid"));
	if (canvas) {
		if (obs_canvas_removed(canvas)) {
			obs_canvas_release(canvas);
			canvas = nullptr;
		} else if (obs_canvas_get_flags(canvas) != (ACTIVATE | SCENE_REF | EPHEMERAL)) {
			obs_frontend_remove_canvas(canvas);
			obs_canvas_remove(canvas);
			obs_canvas_release(canvas);
			canvas = nullptr;
		} else {
			std::string name = obs_canvas_get_name(canvas);
			if (name != canvas_name) {
				obs_canvas_set_name(canvas, canvas_name.c_str());
			}
		}
	}
	if (!canvas) {
		canvas = obs_get_canvas_by_name(canvas_name.c_str());
		if (canvas && obs_canvas_removed(canvas)) {
			obs_canvas_release(canvas);
			canvas = nullptr;
		} else if (canvas && obs_canvas_get_flags(canvas) != (ACTIVATE | SCENE_REF | EPHEMERAL)) {
			obs_frontend_remove_canvas(canvas);
			obs_canvas_remove(canvas);
			obs_canvas_release(canvas);
			canvas = nullptr;
		}
	}
	if (canvas) {
		obs_video_info ovi;
		if (!obs_canvas_get_video_info(canvas, &ovi) ||
		    (ovi.base_width != canvas_width || ovi.base_height != canvas_height || ovi.output_width != canvas_width ||
		     ovi.output_height != canvas_height)) {
			obs_get_video_info(&ovi);
			ovi.base_height = canvas_height;
			ovi.base_width = canvas_width;
			ovi.output_height = canvas_height;
			ovi.output_width = canvas_width;
			obs_canvas_reset_video(canvas, &ovi);
			blog(LOG_INFO, "[Aitum Stream Suite] Canvas '%s' reset video %ux%u", canvas_name.c_str(), canvas_width,
			     canvas_height);
		}
		if (strcmp(obs_data_get_string(settings, "uuid"), "") == 0) {
			obs_data_set_string(settings, "uuid", obs_canvas_get_uuid(canvas));
		}
	} else {
		obs_video_info ovi;
		obs_get_video_info(&ovi);
		ovi.base_height = canvas_height;
		ovi.base_width = canvas_width;
		ovi.output_height = canvas_height;
		ovi.output_width = canvas_width;
		canvas = obs_frontend_add_canvas(canvas_name.c_str(), &ovi, ACTIVATE | SCENE_REF | EPHEMERAL);
		blog(LOG_INFO, "[Aitum Stream Suite] Add frontend canvas '%s' %ux%u", canvas_name.c_str(), canvas_width,
		     canvas_height);
		if (canvas) {
			obs_data_set_string(settings, "uuid", obs_canvas_get_uuid(canvas));
			obs_data_set_int(settings, "width", canvas_width);
			obs_data_set_int(settings, "height", canvas_height);
		}
	}
	LoadReplacements();

	obs_add_tick_callback(Tick, this);

	auto sh = obs_get_signal_handler();
	signal_handler_connect(sh, "source_create", source_create, this);
	signal_handler_connect(sh, "source_remove", source_remove, this);
	signal_handler_connect(sh, "source_rename", source_rename, this);

	auto index = -1;
	if (modesTabBar)
		index = modesTabBar->currentIndex();
	LoadMode(index);
}

CanvasCloneDock::~CanvasCloneDock()
{
	auto sh = obs_get_signal_handler();
	signal_handler_disconnect(sh, "source_create", source_create, this);
	signal_handler_disconnect(sh, "source_remove", source_remove, this);
	signal_handler_disconnect(sh, "source_rename", source_rename, this);
	canvas_clone_docks.remove(this);
	obs_remove_tick_callback(Tick, this);
	obs_data_release(settings);
	obs_weak_canvas_release(clone);
	obs_canvas_release(canvas);
	obs_enter_graphics();
	gs_vertexbuffer_destroy(box);
	obs_leave_graphics();
	for (auto it = replace_sources.begin(); it != replace_sources.end(); it++)
		obs_weak_source_release(it->second);
	replace_sources.clear();
}

void CanvasCloneDock::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	CanvasCloneDock *window = static_cast<CanvasCloneDock *>(data);
	if (!window || !window->canvas || obs_canvas_removed(window->canvas))
		return;

	uint32_t sourceCX = window->canvas_width;
	if (sourceCX <= 0)
		sourceCX = 1;
	uint32_t sourceCY = window->canvas_height;
	if (sourceCY <= 0)
		sourceCY = 1;

	int x, y;
	float scale;

	CanvasDock::GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);
	//if (window->previewScale != scale)
	//	window->previewScale = scale;
	auto newCX = scale * float(sourceCX);
	auto newCY = scale * float(sourceCY);

	auto extraCx = (window->zoom - 1.0f) * newCX;
	auto extraCy = (window->zoom - 1.0f) * newCY;
	int newCx = newCX * window->zoom;
	int newCy = newCY * window->zoom;
	x -= extraCx * window->scrollX;
	y -= extraCy * window->scrollY;

	gs_viewport_push();
	gs_projection_push();

	gs_ortho(0.0f, newCx, 0.0f, newCy, -100.0f, 100.0f);
	gs_set_viewport(x, y, newCx, newCy);
	window->DrawBackdrop(newCx, newCy);

	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, (int)newCX, (int)newCY);
	obs_canvas_render(window->canvas);

	gs_set_linear_srgb(previous);

	gs_ortho(float(-x), newCX + float(x), float(-y), newCY + float(y), -100.0f, 100.0f);
	gs_reset_viewport();

	gs_projection_pop();
	gs_viewport_pop();
}

void CanvasCloneDock::Tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	CanvasCloneDock *ccd = static_cast<CanvasCloneDock *>(data);
	if (!ccd->clone) {
		auto clone_name = obs_data_get_string(ccd->settings, "clone");
		auto clone_canvas = clone_name[0] == '\0' ? obs_get_main_canvas() : obs_get_canvas_by_name(clone_name);
		if (clone_canvas) {
			ccd->clone = obs_canvas_get_weak_canvas(clone_canvas);
			obs_video_info ovi;
			if (obs_canvas_get_video_info(clone_canvas, &ovi)) {
				if (ovi.base_width > 0)
					ccd->canvas_width = ovi.base_width;
				if (ovi.base_height > 0)
					ccd->canvas_height = ovi.base_height;
			}
			obs_canvas_release(clone_canvas);
		}
	}
	if (!ccd->clone)
		return;
	obs_canvas_t *c = obs_weak_canvas_get_canvas(ccd->clone);
	if (!c)
		return;
	if (c == ccd->canvas) {
		obs_canvas_release(c);
		return;
	}
	obs_video_info ovi;
	if (obs_canvas_get_video_info(c, &ovi)) {
		if (ovi.base_width > 0 && ccd->canvas_width != ovi.base_width)
			ccd->canvas_width = ovi.base_width;
		if (ovi.base_height > 0 && ccd->canvas_height != ovi.base_height)
			ccd->canvas_height = ovi.base_height;
	} else {
		for (const auto &it : canvas_docks) {
			if (it->GetCanvas() == c) {
				ccd->canvas_width = it->GetCanvasWidth();
				ccd->canvas_height = it->GetCanvasHeight();
			}
		}
		for (const auto &it : canvas_clone_docks) {
			if (it->GetCanvas() == c) {
				ccd->canvas_width = it->GetCanvasWidth();
				ccd->canvas_height = it->GetCanvasHeight();
			}
		}
	}
	if (obs_canvas_get_video_info(ccd->canvas, &ovi)) {
		if (ovi.base_width != ccd->canvas_width || ovi.base_height != ccd->canvas_height ||
		    ovi.output_width != ccd->canvas_width || ovi.output_height != ccd->canvas_height) {
			obs_get_video_info(&ovi);
			ovi.base_height = ccd->canvas_height;
			ovi.base_width = ccd->canvas_width;
			ovi.output_height = ccd->canvas_height;
			ovi.output_width = ccd->canvas_width;
			obs_canvas_reset_video(ccd->canvas, &ovi);
			blog(LOG_INFO, "[Aitum Stream Suite] Canvas '%s' reset video %ux%u", obs_canvas_get_name(ccd->canvas),
			     ccd->canvas_width, ccd->canvas_height);
		}
	}
	for (int i = 0; i < MAX_CHANNELS; i++) {
		obs_source_t *s = obs_canvas_get_channel(c, i);
		obs_source_t *s2 = obs_canvas_get_channel(ccd->canvas, i);
		if (!s && !s2)
			continue;
		if (s2 && !s) {
			obs_canvas_set_channel(ccd->canvas, i, nullptr);
			obs_source_release(s2);
			continue;
		}

		obs_source_t *s3 = ccd->DuplicateSource(s, s2);
		if (s3 != s2) {
			obs_canvas_set_channel(ccd->canvas, i, s3);
		}
		obs_source_release(s);
		obs_source_release(s2);
		obs_source_release(s3);
	}

	obs_canvas_release(c);
}

void CanvasCloneDock::DrawBackdrop(float cx, float cy)
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

bool CanvasCloneDock::SceneDetectReplacedSource(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	std::pair<bool *, CanvasCloneDock *> *p = (std::pair<bool *, CanvasCloneDock *> *)param;
	CanvasCloneDock *ccd = p->second;
	bool *change_source = p->first;
	obs_source_t *source = obs_sceneitem_get_source(item);
	if (ccd->replace_sources.find(source) != ccd->replace_sources.end()) {
		*change_source = true;
		return true;
	} else {
		obs_scene_t *scene = obs_scene_from_source(source);
		if (!scene)
			scene = obs_group_from_source(source);
		if (scene) {
			obs_scene_enum_items(scene, SceneDetectReplacedSource, param);
		}
	}
	return true;
}

obs_source_t *CanvasCloneDock::DuplicateSource(obs_source_t *source, obs_source_t *current)
{
	if (!source)
		return nullptr;

	auto replace = replace_sources.find(source);
	if (replace != replace_sources.end()) {
		obs_source_t *s = obs_weak_source_get_source(replace->second);
		if (s) {
			return s;
		}
	}

	obs_source_t *duplicate = nullptr;

	enum obs_source_type source_type = obs_source_get_type(source);
	if (source_type == OBS_SOURCE_TYPE_TRANSITION) {
		if ((current && !source) || (source && !current) ||
		    (source && current && strcmp(obs_source_get_name(current), obs_source_get_name(source)) != 0)) {
			duplicate = obs_source_duplicate(source, obs_source_get_name(source), true);
		} else {
			duplicate = obs_source_get_ref(current);
		}
		obs_source_t *sa = obs_transition_get_source(source, OBS_TRANSITION_SOURCE_A);
		obs_source_t *sa2 = obs_transition_get_source(duplicate, OBS_TRANSITION_SOURCE_A);
		obs_source_t *sa3 = DuplicateSource(sa, sa2);
		obs_source_t *sb = obs_transition_get_source(source, OBS_TRANSITION_SOURCE_B);
		obs_source_t *sb2 = obs_transition_get_source(duplicate, OBS_TRANSITION_SOURCE_B);
		obs_source_t *sb3 = DuplicateSource(sb, sb2);
		if (sa3 != sa2) {
			obs_transition_set(duplicate, sa3);
		}
		if (sb3 && sb3 != sb2) {
			obs_transition_start(duplicate,
					     obs_transition_fixed(source) ? OBS_TRANSITION_MODE_AUTO : OBS_TRANSITION_MODE_MANUAL,
					     obs_frontend_get_transition_duration(), sb3);
		}

		if (!obs_transition_fixed(source))
			obs_transition_set_manual_time(duplicate, obs_transition_get_time(source));
		obs_source_release(sa);
		obs_source_release(sa2);
		obs_source_release(sa3);
		obs_source_release(sb);
		obs_source_release(sb2);
		obs_source_release(sb3);
	} else if (source_type == OBS_SOURCE_TYPE_SCENE) {
		obs_scene_t *scene = obs_scene_from_source(source);
		if (!scene)
			scene = obs_group_from_source(source);

		bool change_source = false;
		std::pair<bool *, CanvasCloneDock *> p = {&change_source, this};
		obs_scene_enum_items(scene, SceneDetectReplacedSource, &p);
		if (!change_source) {
			duplicate = obs_source_get_ref(source);
		} else if (current && source &&
			   ((!change_source && current == source) ||
			    (current != source && strcmp(obs_source_get_name(current), obs_source_get_name(source)) == 0))) {
			duplicate = obs_source_get_ref(current);
		} else {
			//duplicate = obs_source_duplicate(source, obs_source_get_name(source), true);
			duplicate = obs_scene_get_source(
				obs_scene_duplicate(scene, obs_source_get_name(source), OBS_SCENE_DUP_PRIVATE_REFS));
		}
		if (duplicate != source) {
			obs_scene_t *scene2 = obs_scene_from_source(duplicate);
			if (!scene2)
				scene2 = obs_group_from_source(duplicate);
			if (!scene2) {
				return nullptr;
			}
			std::pair<obs_scene_t *, CanvasCloneDock *> p = {scene, this};

			obs_scene_enum_items(
				scene2,
				[](obs_scene_t *scene2, obs_sceneitem_t *item, void *param) {
					UNUSED_PARAMETER(scene2);
					auto p = (std::pair<obs_scene_t *, CanvasCloneDock *> *)param;
					obs_scene_t *scene = p->first;
					CanvasCloneDock *ccd = p->second;
					auto source = obs_sceneitem_get_source(item);
					auto source_name = obs_source_get_name(source);
					if (!obs_scene_find_source(scene, source_name)) {
						//item not found in original scene
						bool found = false;
						for (auto it = ccd->replace_sources.begin();
						     !found && it != ccd->replace_sources.end(); it++) {
							if (obs_weak_source_references_source(it->second, source) &&
							    obs_scene_find_source(scene, obs_source_get_name(it->first)))
								found = true;
						}
						if (!found)
							obs_sceneitem_remove(item);
					}
					return true;
				},
				&p);

			p = {scene2, this};

			obs_scene_enum_items(
				scene,
				[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
					UNUSED_PARAMETER(scene);
					auto p = (std::pair<obs_scene_t *, CanvasCloneDock *> *)param;
					CanvasCloneDock *ccd = p->second;
					obs_scene_t *scene2 = p->first;
					obs_sceneitem_t *item2 =
						obs_scene_find_source(scene2, obs_source_get_name(obs_sceneitem_get_source(item)));
					obs_sceneitem_t *item3 = nullptr;
					obs_source_t *source = obs_sceneitem_get_source(item);
					obs_source_t *source2 = obs_sceneitem_get_source(item2);
					obs_source_t *source3 = ccd->DuplicateSource(source, source2);
					if (!item2) {
						item2 = obs_scene_find_source(scene2, obs_source_get_name(source3));
						source2 = obs_sceneitem_get_source(item2);
					}
					if (source2 && source3 != source2) {
						obs_sceneitem_remove(item2);
						item3 = obs_scene_add(scene2, source3);
					} else if (!item2) {
						item3 = obs_scene_add(scene2, source3);
					} else {
						item3 = item2;
					}
					DuplicateSceneItem(item, item3);
					obs_source_release(source3);
					return true;
				},
				&p);
		}
	} else {
		duplicate = obs_source_get_ref(source);
	}

	//obs_scene_duplicate();
	return duplicate;
}

void CanvasCloneDock::DuplicateSceneItem(obs_sceneitem_t *item, obs_sceneitem_t *item2)
{
	if (!item || !item2)
		return;
	struct obs_transform_info transform;
	struct obs_transform_info transform2;
	obs_sceneitem_get_info2(item, &transform);
	obs_sceneitem_get_info2(item2, &transform2);
	if (memcmp(&transform, &transform2, sizeof(struct obs_transform_info)) != 0) {
		obs_sceneitem_set_info2(item2, &transform);
	}
	struct obs_sceneitem_crop crop;
	struct obs_sceneitem_crop crop2;
	obs_sceneitem_get_crop(item, &crop);
	obs_sceneitem_get_crop(item, &crop2);
	if (memcmp(&crop, &crop2, sizeof(struct obs_sceneitem_crop)) != 0) {
		obs_sceneitem_set_crop(item2, &crop);
	}
	enum obs_blending_method blend_method = obs_sceneitem_get_blending_method(item);
	enum obs_blending_method blend_method2 = obs_sceneitem_get_blending_method(item2);
	if (blend_method != blend_method2)
		obs_sceneitem_set_blending_method(item, blend_method);

	enum obs_blending_type blending_type = obs_sceneitem_get_blending_mode(item);
	enum obs_blending_type blending_type2 = obs_sceneitem_get_blending_mode(item2);
	if (blending_type != blending_type2)
		obs_sceneitem_set_blending_mode(item2, blending_type);

	bool visible = obs_sceneitem_visible(item);
	bool visible2 = obs_sceneitem_visible(item2);
	if (visible != visible2)
		obs_sceneitem_set_visible(item2, visible);

	enum obs_scale_type scale_type = obs_sceneitem_get_scale_filter(item);
	enum obs_scale_type scale_type2 = obs_sceneitem_get_scale_filter(item2);
	if (scale_type != scale_type2)
		obs_sceneitem_set_scale_filter(item2, scale_type);

	obs_source_t *show_transition = obs_sceneitem_get_transition(item, true);
	obs_source_t *show_transition2 = obs_sceneitem_get_transition(item, true);
	if (show_transition != show_transition2) {
		obs_sceneitem_set_transition(item2, true, show_transition);
	}

	obs_source_t *hide_transition = obs_sceneitem_get_transition(item, false);
	obs_source_t *hide_transition2 = obs_sceneitem_get_transition(item, false);
	if (hide_transition != hide_transition2) {
		obs_sceneitem_set_transition(item2, false, hide_transition);
	}

	uint32_t show_transition_duration = obs_sceneitem_get_transition_duration(item, true);
	uint32_t show_transition_duration2 = obs_sceneitem_get_transition_duration(item2, true);
	if (show_transition_duration != show_transition_duration2)
		obs_sceneitem_set_transition_duration(item, true, show_transition_duration);

	uint32_t hide_transition_duration = obs_sceneitem_get_transition_duration(item, false);
	uint32_t hide_transition_duration2 = obs_sceneitem_get_transition_duration(item2, false);
	if (hide_transition_duration != hide_transition_duration2)
		obs_sceneitem_set_transition_duration(item, false, hide_transition_duration);

	int order_position = obs_sceneitem_get_order_position(item);
	int order_position2 = obs_sceneitem_get_order_position(item2);
	if (order_position != order_position2)
		obs_sceneitem_set_order_position(item2, order_position);
}

void CanvasCloneDock::UpdateSettings(obs_data_t *s)
{
	if (s) {
		obs_data_release(settings);
		settings = s;
	}

	auto c = color_from_int(obs_data_get_int(settings, "color"));
	setStyleSheet(QString("QFrame{border: 2px solid %1;}").arg(c.name(QColor::HexRgb)));

	obs_weak_canvas_release(clone);
	clone = nullptr;
	auto clone_name = obs_data_get_string(settings, "clone");
	auto clone_canvas = clone_name[0] == '\0' ? obs_get_main_canvas() : obs_get_canvas_by_name(clone_name);
	if (clone_canvas) {
		clone = obs_canvas_get_weak_canvas(clone_canvas);
		obs_video_info ovi;
		if (obs_canvas_get_video_info(clone_canvas, &ovi)) {
			if (ovi.base_width > 0)
				canvas_width = ovi.base_width;
			if (ovi.base_height > 0)
				canvas_height = ovi.base_height;
		} else {
			for (const auto &it : canvas_docks) {
				if (it->GetCanvas() == clone_canvas) {
					canvas_width = it->GetCanvasWidth();
					canvas_height = it->GetCanvasHeight();
				}
			}
			for (const auto &it : canvas_clone_docks) {
				if (it->GetCanvas() == clone_canvas) {
					canvas_width = it->GetCanvasWidth();
					canvas_height = it->GetCanvasHeight();
				}
			}
		}
		obs_canvas_release(clone_canvas);
	} else {
		canvas_width = (uint32_t)obs_data_get_int(settings, "width");
		canvas_height = (uint32_t)obs_data_get_int(settings, "height");
	}
	if (canvas_width < 1)
		canvas_width = 1080;
	if (canvas_height < 1)
		canvas_height = 1920;

	obs_video_info ovi;
	if (obs_canvas_get_video_info(canvas, &ovi) && (ovi.base_width != canvas_width || ovi.base_height != canvas_height ||
							ovi.output_width != canvas_width || ovi.output_height != canvas_height)) {
		obs_get_video_info(&ovi);
		ovi.base_height = canvas_height;
		ovi.base_width = canvas_width;
		ovi.output_height = canvas_height;
		ovi.output_width = canvas_width;
		obs_canvas_reset_video(canvas, &ovi);
		blog(LOG_INFO, "[Aitum Stream Suite] Canvas '%s' reset video %ux%u", obs_canvas_get_name(canvas), canvas_width,
		     canvas_height);
	}
	LoadReplacements();
}

bool CanvasCloneDock::AddSourceToCombos(void *param, obs_source_t *source)
{
	if (!source)
		return true;
	if (obs_obj_is_private(source))
		return true;
	auto st = obs_source_get_type(source);
	if (st != OBS_SOURCE_TYPE_INPUT && st != OBS_SOURCE_TYPE_SCENE)
		return true;
	auto this_ = (CanvasCloneDock *)param;
	auto canvas = obs_source_get_canvas(source);
	if (canvas) {
		auto cc = obs_weak_canvas_get_canvas(this_->clone);
		obs_canvas_release(canvas);
		obs_canvas_release(cc);
		if (cc != canvas)
			return true;
	}

	auto name = QString::fromUtf8(obs_source_get_name(source));
	auto id = QString::fromUtf8(obs_source_get_uuid(source));

	int index = 0;
	auto combo = this_->replaceCombos[0].first;
	while (index < combo->count() && combo->itemText(index).compare(name, Qt::CaseInsensitive) < 0)
		index++;

	for (auto it = this_->replaceCombos.begin(); it != this_->replaceCombos.end(); ++it) {
		it->first->insertItem(index, name, id);
		it->second->insertItem(index, name, id);
	}
	return true;
}

void CanvasCloneDock::LoadReplacements()
{
	auto clone_name = obs_data_get_string(settings, "clone");
	auto clone_canvas = clone_name[0] == '\0' ? obs_get_main_canvas() : obs_get_canvas_by_name(clone_name);
	for (auto it = replace_sources.begin(); it != replace_sources.end(); it++)
		obs_weak_source_release(it->second);
	replace_sources.clear();
	obs_data_array_t *arr = obs_data_get_array(settings, "replace_sources");
	size_t count = obs_data_array_count(arr);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *t = obs_data_array_item(arr, i);
		if (!t) {
			replaceCombos[i].first->setCurrentIndex(0);
			replaceCombos[i].second->setCurrentIndex(0);
			continue;
		}
		auto src_name = obs_data_get_string(t, "source");
		if (i < replaceCombos.size()) {
			if (src_name && src_name[0] != '\0')
				replaceCombos[i].first->setCurrentText(QString::fromUtf8(src_name));
			else
				replaceCombos[i].first->setCurrentIndex(0);
		}
		auto dst_name = obs_data_get_string(t, "replacement");
		if (i < replaceCombos.size()) {
			if (dst_name && dst_name[0] != '\0')
				replaceCombos[i].second->setCurrentText(QString::fromUtf8(dst_name));
			else
				replaceCombos[i].second->setCurrentIndex(0);
		}
		if (!src_name || !dst_name || src_name[0] == '\0' || dst_name[0] == '\0') {
			obs_data_release(t);
			continue;
		}
		obs_source_t *src = clone_canvas ? obs_canvas_get_source_by_name(clone_canvas, src_name) : nullptr;
		if (!src)
			src = obs_get_source_by_name(src_name);
		obs_source_t *dst = clone_canvas ? obs_canvas_get_source_by_name(clone_canvas, dst_name) : nullptr;
		if (!dst)
			dst = obs_get_source_by_name(dst_name);
		if (src && dst && src != dst) {
			replace_sources[src] = obs_source_get_weak_source(dst);
		}
		obs_source_release(src);
		obs_source_release(dst);
		obs_data_release(t);
	}
	obs_data_array_release(arr);
	obs_canvas_release(clone_canvas);
}

void CanvasCloneDock::source_create(void *param, calldata_t *cd)
{
	auto source = (obs_source_t *)calldata_ptr(cd, "source");
	AddSourceToCombos(param, source);
}

void CanvasCloneDock::source_remove(void *param, calldata_t *cd)
{
	auto source = (obs_source_t *)calldata_ptr(cd, "source");
	auto this_ = (CanvasCloneDock *)param;
	this_->RemoveSource(QString::fromUtf8(obs_source_get_name(source)));
}

void CanvasCloneDock::source_rename(void *param, calldata_t *cd)
{
	auto source = (obs_source_t *)calldata_ptr(cd, "source");
	auto this_ = (CanvasCloneDock *)param;
	this_->RemoveSource(QString::fromUtf8(calldata_string(cd, "prev_name")));
	AddSourceToCombos(param, source);
}

void CanvasCloneDock::RemoveSource(QString source_name)
{
	for (auto it = replaceCombos.begin(); it != replaceCombos.end(); ++it) {
		int index = it->first->findText(source_name, Qt::MatchFixedString);
		if (index >= 0)
			it->first->removeItem(index);
		index = it->second->findText(source_name, Qt::MatchFixedString);
		if (index >= 0)
			it->second->removeItem(index);
	}
}

void CanvasCloneDock::SaveSettings(bool closing)
{
	if (!closing) {
		auto state = canvas_split->saveState();
		auto b64 = state.toBase64();
		auto state_chars = b64.constData();
		auto index = modesTabBar->currentIndex();
		if (index == 0) {
			obs_data_set_string(settings, "canvas_split_live", state_chars);
		} else if (index == 1) {
			obs_data_set_string(settings, "canvas_split_build", state_chars);
		} else if (index == 2) {
			obs_data_set_string(settings, "canvas_split_design", state_chars);
		}
		obs_data_set_string(settings, "canvas_split", state_chars);
	}
}

void CanvasCloneDock::LoadMode(int index)
{
	auto state = "";
	if (index == 0) {
		state = obs_data_get_string(settings, "canvas_split_live");
	} else if (index == 1) {
		state = obs_data_get_string(settings, "canvas_split_build");
	} else if (index == 2) {
		state = obs_data_get_string(settings, "canvas_split_design");
	}
	if (state[0] == '\0')
		state = obs_data_get_string(settings, "canvas_split");
	if (state[0] != '\0')
		canvas_split->restoreState(QByteArray::fromBase64(state));
}

void CanvasCloneDock::reset_live_state()
{
	canvas_split->setSizes({1, 0});
}

void CanvasCloneDock::reset_build_state()
{
	canvas_split->setSizes({1, 1});
}
