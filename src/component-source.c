#include <obs.h>
#include <obs-module.h>

struct component {
	obs_source_t *source;
	obs_weak_source_t *scene_source;
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
	bfree(component);
}

static void component_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct component *component = data;
	if (component->scene_source) {
		obs_source_t *source = obs_weak_source_get_source(component->scene_source);
		if (source) {
			if (strcmp(obs_source_get_name(component->source), obs_source_get_name(source)) != 0) {
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
}

static void component_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct component *component = data;

	obs_source_t *source = obs_weak_source_get_source(component->scene_source);
	if (!source)
		return;
	obs_source_video_render(source);
	obs_source_release(source);
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
	obs_properties_add_int(props, "cx", obs_module_text("Width"), 1, 16384, 1);
	obs_properties_add_int(props, "cy", obs_module_text("Height"), 1, 16384, 1);
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
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_COMPOSITE | OBS_SOURCE_SRGB,
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
