#include "canvas-clone-dock.hpp"
#include <QLayout>
#include "canvas-dock.hpp"

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

	auto l = new QBoxLayout(QBoxLayout::TopToBottom, this);
	l->setContentsMargins(0, 0, 0, 0);
	setLayout(l);
	l->addWidget(preview);

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

	auto clone_canvas = obs_get_canvas_by_name(obs_data_get_string(settings, "clone"));
	if (clone_canvas) {
		clone = obs_canvas_get_weak_canvas(clone_canvas);
		obs_video_info ovi;
		if (obs_canvas_get_video_info(clone_canvas, &ovi)) {
			if (ovi.base_width > 0)
				canvas_width = ovi.base_width;
			if (ovi.base_height > 0)
				canvas_height = ovi.base_height;
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
		std::string name = obs_canvas_get_name(canvas);
		if (name != canvas_name) {
			obs_canvas_set_name(canvas, canvas_name.c_str());
		}
	} else {
		canvas = obs_get_canvas_by_name(canvas_name.c_str());
	}
	if (canvas) {
		obs_video_info ovi;
		if (obs_canvas_get_video_info(canvas, &ovi)) {
			if (ovi.base_width != canvas_width || ovi.base_height != canvas_height ||
			    ovi.output_width != canvas_width || ovi.output_height != canvas_height) {
				obs_canvas_remove(canvas);
				obs_canvas_release(canvas);
				canvas = nullptr;
			}
		}
	}
	if (!canvas) {
		obs_video_info ovi;
		obs_get_video_info(&ovi);
		ovi.base_height = canvas_height;
		ovi.base_width = canvas_width;
		ovi.output_height = canvas_height;
		ovi.output_width = canvas_width;
		canvas = obs_frontend_add_canvas(canvas_name.c_str(), &ovi, PROGRAM | EPHEMERAL);
		if (canvas) {
			obs_data_set_string(settings, "uuid", obs_canvas_get_uuid(canvas));
			obs_data_set_int(settings, "width", canvas_width);
			obs_data_set_int(settings, "height", canvas_height);
		}
	}

	obs_data_array_t *arr = obs_data_get_array(settings, "replace_sources");
	size_t count = obs_data_array_count(arr);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *t = obs_data_array_item(arr, i);
		if (!t)
			continue;
		auto src_name = obs_data_get_string(t, "source");
		auto dst_name = obs_data_get_string(t, "replacement");
		if (!src_name || !dst_name || src_name[0] == '\0' || dst_name[0] == '\0') {
			obs_data_release(t);
			continue;
		}
		obs_source_t *src = obs_canvas_get_source_by_name(clone_canvas, src_name);
		if (!src)
			src = obs_get_source_by_name(src_name);
		obs_source_t *dst = obs_canvas_get_source_by_name(clone_canvas, dst_name);
		if (!dst)
			dst = obs_get_source_by_name(dst_name);
		if (!src || !dst) {
			obs_source_release(src);
			obs_source_release(dst);
			obs_data_release(t);
			continue;
		}
		replace_sources[src] = obs_source_get_weak_source(dst);
		obs_source_release(src);
		obs_source_release(dst);
		obs_data_release(t);
	}

	obs_add_tick_callback(Tick, this);
}

CanvasCloneDock::~CanvasCloneDock()
{
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
	if (!window || !window->canvas)
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
	//obs_view_render(window->view);
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
	if (!ccd->clone)
		return;
	obs_canvas_t *c = obs_weak_canvas_get_canvas(ccd->clone);
	if (!c)
		return;
	if (c == ccd->canvas) {
		obs_canvas_release(c);
		return;
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
		obs_scene_enum_items(scene,
				[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
					std::pair<bool *, CanvasCloneDock *> *p =
						(std::pair<bool *, CanvasCloneDock *> *)param;
					CanvasCloneDock *ccd = p->second;
					bool *change_source = p->first;
					if (ccd->replace_sources.find(obs_sceneitem_get_source(item)) != ccd->replace_sources.end()) {
						*change_source = true;
						return true;
					}
					return true;
				},
			&p);
		if (!change_source) {
			duplicate = obs_source_get_ref(source);
		}else if (current && source &&
		    (current == source || strcmp(obs_source_get_name(current), obs_source_get_name(source)) == 0)) {
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

			obs_scene_enum_items(
				scene2,
				[](obs_scene_t *scene2, obs_sceneitem_t *item, void *param) {
					obs_scene_t *scene = (obs_scene_t *)param;
					//if (!obs_scene_find_sceneitem_by_id(scene, obs_sceneitem_get_id(item))) {
					//	obs_sceneitem_remove(item);
					//}
					if (!obs_scene_find_source(scene, obs_source_get_name(obs_sceneitem_get_source(item)))) {
						obs_sceneitem_remove(item);
					}
					return true;
				},
				scene);

			std::pair<obs_scene_t *, CanvasCloneDock *> p = {scene2, this};

			obs_scene_enum_items(
				scene,
				[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
					std::pair<obs_scene_t *, CanvasCloneDock *> *p =
						(std::pair<obs_scene_t *, CanvasCloneDock *> *)param;
					CanvasCloneDock *ccd = p->second;
					obs_scene_t *scene2 = p->first;
					obs_sceneitem_t *item2 =
						obs_scene_find_source(scene2, obs_source_get_name(obs_sceneitem_get_source(item)));
					obs_sceneitem_t *item3 = nullptr;
					obs_source_t *source = obs_sceneitem_get_source(item);
					obs_source_t *source2 = obs_sceneitem_get_source(item2);
					obs_source_t *source3 = ccd->DuplicateSource(source, source2);
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
