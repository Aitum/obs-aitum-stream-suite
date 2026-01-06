#include <obs.h>
#include <obs-module.h>

struct component {
	obs_source_t *source;
	obs_weak_source_t *scene_source;
	gs_texrender_t *scene_render;
};

static const char *component_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Component");
}

static void *component_create(obs_data_t *settings, obs_source_t *source)
{
	struct component *component = bzalloc(sizeof(struct component));
	component->source = source;
	obs_source_update(source, settings);
	return component;
}

static void component_destroy(void *data)
{
	struct component *component = data;
	obs_source_t *source = obs_weak_source_get_source(component->scene_source);
	if (source) {
		obs_source_remove_active_child(component->source, source);
		obs_source_release(source);
	}
	obs_weak_source_release(component->scene_source);
	if (component->scene_render) {
		obs_enter_graphics();
		gs_texrender_destroy(component->scene_render);
		obs_leave_graphics();
	}
	bfree(component);
}

static void component_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct component *component = data;
	if (component->scene_source) {
		obs_source_t *source = obs_weak_source_get_source(component->scene_source);
		if (source) {
			if (obs_source_removed(source)) {
				obs_weak_source_release(component->scene_source);
				component->scene_source = NULL;
			} else if (strcmp(obs_source_get_name(component->source), obs_source_get_name(source)) != 0) {
				obs_canvas_t *canvas = obs_source_get_canvas(source);
				if (!canvas)
					canvas = obs_get_canvas_by_name("Components");
				obs_source_t *other_source =
					canvas ? obs_canvas_get_source_by_name(canvas, obs_source_get_name(component->source))
					       : NULL;
				obs_canvas_release(canvas);
				if (other_source) {
					obs_source_remove_active_child(component->source, source);
					obs_weak_source_release(component->scene_source);
					component->scene_source = obs_source_get_weak_source(other_source);
					obs_source_add_active_child(component->source, other_source);
					obs_source_release(other_source);
				} else {
					obs_source_set_name(source, obs_source_get_name(component->source));
				}
			}
			obs_source_release(source);
		} else {
			obs_weak_source_release(component->scene_source);
			component->scene_source = NULL;
		}
	}

	if (!component->scene_source) {
		obs_canvas_t *canvas = obs_get_canvas_by_name("Components");
		if (canvas) {
			obs_source_t *source = obs_canvas_get_source_by_name(canvas, obs_source_get_name(component->source));
			if (source && obs_source_removed(source)) {
				obs_source_release(source);
				source = NULL;
			}
			if (source) {
				if (obs_source_is_scene(source)) {
					component->scene_source = obs_source_get_weak_source(source);
					obs_source_add_active_child(component->source, source);
				}
				obs_source_release(source);
			} else {
				obs_scene_t *scene = obs_canvas_scene_create(canvas, obs_source_get_name(component->source));
				source = obs_scene_get_source(scene);
				if (source) {
					obs_data_t *settings = obs_source_get_settings(component->source);
					obs_data_t *ss = obs_source_get_settings(source);
					obs_data_set_bool(ss, "custom_size", true);
					obs_data_set_int(ss, "cx", obs_data_get_int(settings, "cx"));
					obs_data_set_int(ss, "cy", obs_data_get_int(settings, "cy"));
					obs_source_load(source);
					obs_data_release(ss);
					obs_data_release(settings);
					component->scene_source = obs_source_get_weak_source(source);
					obs_source_add_active_child(component->source, source);
				}
				obs_scene_release(scene);
			}
			obs_canvas_release(canvas);
		}
	}

	gs_texrender_reset(component->scene_render);
}

static void component_video_render(void *data, gs_effect_t *effect)
{
	struct component *component = data;

	obs_source_t *source = obs_weak_source_get_source(component->scene_source);
	if (!source)
		return;

	const enum gs_color_space current_space = gs_get_color_space();
	const enum gs_color_space source_space = obs_source_get_color_space(source, 1, &current_space);
	const enum gs_color_format format = gs_get_format_from_space(source_space);

	if (component->scene_render && (gs_texrender_get_format(component->scene_render) != format)) {
		gs_texrender_destroy(component->scene_render);
		component->scene_render = NULL;
	}

	if (!component->scene_render) {
		component->scene_render = gs_texrender_create(format, GS_ZS_NONE);
	}
	if (!component->scene_render) {
		obs_source_release(source);
		return;
	}
	uint32_t width = obs_source_get_width(source);
	uint32_t height = obs_source_get_height(source);

	if (!width || !height) {
		obs_source_release(source);
		return;
	}
	gs_blend_state_push();
	gs_reset_blend_state();

	if (gs_texrender_begin_with_color_space(component->scene_render, width, height, source_space)) {
		struct vec4 clear_color;

		vec4_zero(&clear_color);
		gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
		obs_source_video_render(source);
		gs_texrender_end(component->scene_render);
	}
	obs_source_release(source);

	gs_texture_t *tex = gs_texrender_get_texture(component->scene_render);
	if (!tex) {
		gs_blend_state_pop();
		return;
	}
	uint32_t cx = gs_texture_get_width(tex);
	uint32_t cy = gs_texture_get_height(tex);

	effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);

	gs_blend_state_push();
	gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);

	const bool linear_srgb = gs_set_linear_srgb(true);
	const bool previous = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(true);

	gs_effect_set_texture_srgb(gs_effect_get_param_by_name(effect, "image"), tex);

	gs_technique_t *tech = gs_effect_get_technique(effect, "DrawAlphaDivide");
	size_t passes = gs_technique_begin(tech);
	for (size_t i = 0; i < passes; i++) {
		gs_technique_begin_pass(tech, i);
		gs_draw_sprite(tex, 0, cx, cy);
		gs_technique_end_pass(tech);
	}
	gs_technique_end(tech);
	gs_enable_framebuffer_srgb(previous);
	gs_set_linear_srgb(linear_srgb);
	gs_blend_state_pop();
	gs_blend_state_pop();
}

static inline void mix_audio(float *p_out, float *p_in, size_t pos, size_t count)
{
	register float *out = p_out + pos;
	register float *in = p_in;
	register float *end = in + count;

	while (in < end)
		*(out++) += *(in++);
}

static bool component_audio_render(void *data, uint64_t *ts_out, struct obs_source_audio_mix *audio_output, uint32_t mixers,
				   size_t channels, size_t sample_rate)
{
	UNUSED_PARAMETER(sample_rate);
	struct component *component = data;

	obs_source_t *source = obs_weak_source_get_source(component->scene_source);
	if (!source)
		return false;

	if (obs_source_audio_pending(source))
		return false;

	uint64_t timestamp = obs_source_get_audio_timestamp(source);

	struct obs_source_audio_mix child_audio;
	obs_source_get_audio_mix(source, &child_audio);
	obs_source_release(source);

	for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
		if ((mixers & (1 << mix)) == 0)
			continue;

		for (size_t ch = 0; ch < channels; ch++) {
			float *out = audio_output->output[mix].data[ch];
			float *in = child_audio.output[mix].data[ch];
			mix_audio(out, in, 0, AUDIO_OUTPUT_FRAMES);
		}
	}
	*ts_out = timestamp;
	return true;
}

static uint32_t component_getwidth(void *data)
{
	struct component *component = data;
	obs_source_t *source = obs_weak_source_get_source(component->scene_source);
	if (!source)
		return 0;
	uint32_t width = obs_source_get_width(source);
	obs_source_release(source);
	return width;
}

static uint32_t component_getheight(void *data)
{
	struct component *component = data;
	obs_source_t *source = obs_weak_source_get_source(component->scene_source);
	if (!source)
		return 0;
	uint32_t height = obs_source_get_height(source);
	obs_source_release(source);
	return height;
}

static void component_update(void *data, obs_data_t *settings)
{
	struct component *component = data;
	obs_source_t *source = obs_weak_source_get_source(component->scene_source);
	if (!source)
		return;
	bool changed = false;
	obs_source_save(source);
	obs_data_t *ss = obs_source_get_settings(source);
	obs_data_set_bool(ss, "custom_size", true);
	if (obs_data_get_int(ss, "cx") != obs_data_get_int(settings, "cx")) {
		obs_data_set_int(ss, "cx", obs_data_get_int(settings, "cx"));
		changed = true;
	}
	if (obs_data_get_int(ss, "cy") != obs_data_get_int(settings, "cy")) {
		obs_data_set_int(ss, "cy", obs_data_get_int(settings, "cy"));
		changed = true;
	}
	if (changed)
		obs_source_load(source);
	obs_data_release(ss);
	obs_source_release(source);
}

void show_component_editor(const char *name);

static bool component_edit_button_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	struct component *component = data;
	show_component_editor(obs_source_get_name(component->source));
	return false;
}

static obs_properties_t *component_get_properties(void *data, void *type_data)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(type_data);
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p = obs_properties_add_int(props, "cx", obs_module_text("Width"), 1, 16384, 1);
	obs_property_int_set_suffix(p, "px");
	p = obs_properties_add_int(props, "cy", obs_module_text("Height"), 1, 16384, 1);
	obs_property_int_set_suffix(p, "px");
	obs_properties_add_button2(props, "edit_button", obs_module_text("Edit"), component_edit_button_clicked, data);
	return props;
}

static void component_enum_sources(void *data, obs_source_enum_proc_t enum_callback, void *param)
{
	struct component *component = data;
	obs_source_t *source = obs_weak_source_get_source(component->scene_source);
	if (!source)
		return;
	enum_callback(component->source, source, param);
	obs_source_release(source);
}

enum gs_color_space component_video_get_color_space(void *data, size_t count, const enum gs_color_space *preferred_spaces)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred_spaces);

	enum gs_color_space space = GS_CS_SRGB;
	struct obs_video_info ovi;
	if (obs_get_video_info(&ovi)) {
		if (ovi.colorspace == VIDEO_CS_2100_PQ || ovi.colorspace == VIDEO_CS_2100_HLG)
			space = GS_CS_709_EXTENDED;
	}
	return space;
}

const struct obs_source_info component_info = {
	.id = "component_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE | OBS_SOURCE_SRGB |
			OBS_SOURCE_CAP_DISABLED,
	.get_name = component_getname,
	.create = component_create,
	.destroy = component_destroy,
	.video_tick = component_video_tick,
	.video_render = component_video_render,
	.audio_render = component_audio_render,
	.get_width = component_getwidth,
	.get_height = component_getheight,
	.update = component_update,
	.get_properties2 = component_get_properties,
	.enum_active_sources = component_enum_sources,
	.enum_all_sources = component_enum_sources,
	.video_get_color_space = component_video_get_color_space,
};
