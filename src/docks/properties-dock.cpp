#include "filters-dock.hpp"
#include "properties-dock.hpp"
#include <obs.h>
#include <obs-module.h>
#include <QCheckBox>
#include <QColorDialog>
#include <QDesktopServices>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>
#include <src/utils/color.hpp>
#include <src/utils/widgets/slider-ignore-scroll.hpp>
#include <src/utils/widgets/double-slider.hpp>

extern FiltersDock *filters_dock;

PropertiesDock::PropertiesDock(QWidget *parent) : QFrame(parent)
{
	auto sh = obs_get_signal_handler();
	signal_handler_connect(sh, "canvas_create", canvas_create, this);
	signal_handler_connect(sh, "canvas_destroy", canvas_destroy, this);
	auto ml = new QVBoxLayout;
	setLayout(ml);

	auto scrollArea = new QScrollArea;
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);
	ml->addWidget(scrollArea);

	auto vl = new QVBoxLayout;
	auto scrollWidget = new QWidget;
	scrollWidget->setLayout(vl);
	scrollArea->setWidget(scrollWidget);
	auto hl = new QHBoxLayout;
	vl->addLayout(hl);

	canvasCombo = new QComboBox;
	auto mc = obs_get_main_canvas();
	current_canvas = obs_canvas_get_weak_canvas(mc);
	canvasCombo->addItem(QString::fromUtf8(obs_canvas_get_name(mc)));
	signal_handler_connect(obs_canvas_get_signal_handler(mc), "channel_change", canvas_channel_change, this);
	obs_canvas_release(mc);
	hl->addWidget(canvasCombo);

	connect(canvasCombo, &QComboBox::currentTextChanged, [this] {
		auto canvas = obs_get_canvas_by_name(canvasCombo->currentText().toUtf8().constData());
		if (!canvas)
			return;
		if (obs_canvas_removed(canvas)) {
			obs_canvas_release(canvas);
			return;
		}
		if (obs_weak_object_references_object((obs_weak_object_t *)current_canvas, (obs_object_t *)canvas)) {
			obs_canvas_release(canvas);
			return;
		}

		if (current_canvas) {
			auto prev_canvas = obs_weak_canvas_get_canvas(current_canvas);
			signal_handler_disconnect(obs_canvas_get_signal_handler(prev_canvas), "channel_change",
						  canvas_channel_change, this);
			obs_canvas_release(prev_canvas);
			obs_weak_canvas_release(current_canvas);
		}
		current_canvas = obs_canvas_get_weak_canvas(canvas);

		signal_handler_connect(obs_canvas_get_signal_handler(canvas), "channel_change", canvas_channel_change, this);
		auto source = obs_canvas_get_channel(canvas, 0);
		if (source) {
			auto source_type = obs_source_get_type(source);
			if (source_type == OBS_SOURCE_TYPE_SCENE) {
				QMetaObject::invokeMethod(this, "SceneChanged", Q_ARG(OBSSource, OBSSource(source)));
			} else if (source_type == OBS_SOURCE_TYPE_TRANSITION) {
				QMetaObject::invokeMethod(this, "TransitionChanged", Q_ARG(OBSSource, OBSSource(source)));
			}
			obs_source_release(source);
		}
		obs_canvas_release(canvas);
	});

	sourceLabel = new QLabel(QString::fromUtf8(obs_module_text("NoSourceSelected")));
	hl->addWidget(sourceLabel);

	layout = new QFormLayout;
	vl->addLayout(layout);
	//QWidget *spacer = new QWidget();
	//spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	//vl->addWidget(spacer);

	obs_frontend_add_event_callback(frontend_event, this);
}

PropertiesDock::~PropertiesDock()
{
	auto sh = obs_get_signal_handler();
	signal_handler_disconnect(sh, "canvas_create", canvas_create, this);
	signal_handler_disconnect(sh, "canvas_destroy", canvas_destroy, this);
	obs_frontend_remove_event_callback(frontend_event, this);
}

void PropertiesDock::canvas_create(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);
	auto canvas = (obs_canvas_t *)calldata_ptr(cd, "canvas");
	auto canvas_name = QString::fromUtf8(obs_canvas_get_name(canvas));
	QMetaObject::invokeMethod(this_, [canvas_name, this_] { this_->canvasCombo->addItem(canvas_name); });
}

void PropertiesDock::canvas_destroy(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);
	auto canvas = (obs_canvas_t *)calldata_ptr(cd, "canvas");
	auto canvas_name = QString::fromUtf8(obs_canvas_get_name(canvas));
	QMetaObject::invokeMethod(this_, [canvas_name, this_] {
		auto idx = this_->canvasCombo->findText(canvas_name);
		if (idx >= 0)
			this_->canvasCombo->removeItem(idx);
	});
}

void PropertiesDock::canvas_channel_change(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);

	//auto canvas = (obs_canvas_t *)calldata_ptr(cd, "canvas");
	auto channel = calldata_int(cd, "channel");
	if (channel != 0)
		return;
	//auto prev_source = (obs_source_t *)calldata_ptr(cd, "prev_source");
	auto source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!source) {
		QMetaObject::invokeMethod(this_, "SceneChanged", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(nullptr)));
		QMetaObject::invokeMethod(this_, "TransitionChanged", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(nullptr)));
		return;
	}
	auto source_type = obs_source_get_type(source);
	if (source_type == OBS_SOURCE_TYPE_SCENE) {
		QMetaObject::invokeMethod(this_, "SceneChanged", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(source)));
	} else if (source_type == OBS_SOURCE_TYPE_TRANSITION) {
		QMetaObject::invokeMethod(this_, "TransitionChanged", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(source)));
	}
}

void PropertiesDock::scene_item_select(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);
	auto item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	QMetaObject::invokeMethod(this_, "SourceChanged", Qt::QueuedConnection,
				  Q_ARG(OBSSource, OBSSource(obs_sceneitem_get_source(item))));
	QMetaObject::invokeMethod(filters_dock, "SourceChanged", Qt::QueuedConnection,
				  Q_ARG(OBSSource, OBSSource(obs_sceneitem_get_source(item))));
}

void PropertiesDock::scene_item_deselect(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);
	auto item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (obs_weak_source_references_source(this_->current_source, obs_sceneitem_get_source(item))) {
		QMetaObject::invokeMethod(this_, "SourceDeselected", Qt::QueuedConnection,
					  Q_ARG(OBSSource, OBSSource(obs_sceneitem_get_source(item))));
		QMetaObject::invokeMethod(filters_dock, "SourceDeselected", Qt::QueuedConnection,
					  Q_ARG(OBSSource, obs_sceneitem_get_source(item)));
	}
}

void PropertiesDock::scene_item_remove(void *param, calldata_t *cd)
{
	UNUSED_PARAMETER(param);
	auto item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (obs_sceneitem_is_group(item)) {
		auto group = obs_sceneitem_get_source(item);
		auto gsh = obs_source_get_signal_handler(group);
		signal_handler_disconnect(gsh, "item_select", scene_item_select, param);
		signal_handler_disconnect(gsh, "item_deselect", scene_item_deselect, param);
	}
}

void PropertiesDock::scene_item_add(void *param, calldata_t *cd)
{
	UNUSED_PARAMETER(param);
	auto item = (obs_sceneitem_t *)calldata_ptr(cd, "item");
	if (obs_sceneitem_is_group(item)) {
		auto group = obs_sceneitem_get_source(item);
		auto gsh = obs_source_get_signal_handler(group);
		signal_handler_connect(gsh, "item_select", scene_item_select, param);
		signal_handler_connect(gsh, "item_deselect", scene_item_deselect, param);
	}
}

void PropertiesDock::SceneChanged(OBSSource scene)
{
	if (current_scene) {
		if (obs_weak_source_references_source(current_scene, scene))
			return;
		auto prev_scene = obs_weak_source_get_source(current_scene);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_scene), "item_select", scene_item_select, this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_scene), "item_deselect", scene_item_deselect, this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_scene), "item_remove", scene_item_remove, this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_scene), "item_add", scene_item_add, this);
		obs_scene_enum_items(
			obs_scene_from_source(scene),
			[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
				UNUSED_PARAMETER(scene);
				if (!obs_sceneitem_is_group(item))
					return true;
				auto group = obs_sceneitem_get_source(item);
				auto gsh = obs_source_get_signal_handler(group);
				signal_handler_disconnect(gsh, "item_select", scene_item_select, param);
				signal_handler_disconnect(gsh, "item_deselect", scene_item_deselect, param);
				return true;
			},
			this);
		obs_source_release(prev_scene);

		obs_weak_source_release(current_scene);
	}
	if (!scene || obs_source_removed(scene)) {
		current_scene = nullptr;
		return;
	}
	current_scene = obs_source_get_weak_source(scene);
	signal_handler_connect(obs_source_get_signal_handler(scene), "item_select", scene_item_select, this);
	signal_handler_connect(obs_source_get_signal_handler(scene), "item_deselect", scene_item_deselect, this);
	signal_handler_connect(obs_source_get_signal_handler(scene), "item_remove", scene_item_remove, this);
	signal_handler_connect(obs_source_get_signal_handler(scene), "item_add", scene_item_add, this);
	QMetaObject::invokeMethod(this, "SourceChanged", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(nullptr)));
	QMetaObject::invokeMethod(filters_dock, "SourceChanged", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(scene)));
	obs_scene_enum_items(
		obs_scene_from_source(scene),
		[](obs_scene_t *scene, obs_sceneitem_t *item, void *param) {
			UNUSED_PARAMETER(scene);
			auto this_ = static_cast<PropertiesDock *>(param);
			if (obs_sceneitem_is_group(item)) {
				auto group = obs_sceneitem_get_source(item);
				auto gsh = obs_source_get_signal_handler(group);
				signal_handler_connect(gsh, "item_select", scene_item_select, this_);
				signal_handler_connect(gsh, "item_deselect", scene_item_deselect, this_);
				return true;
			}
			if (!obs_sceneitem_selected(item))
				return true;
			QMetaObject::invokeMethod(this_, "SourceChanged", Qt::QueuedConnection,
						  Q_ARG(OBSSource, OBSSource(obs_sceneitem_get_source(item))));
			return true;
		},
		this);
}

void PropertiesDock::TransitionChanged(OBSSource transition)
{
	if (current_transition) {
		if (obs_weak_source_references_source(current_transition, transition)) {
			auto active_source = obs_transition_get_active_source(transition);
			if (active_source) {
				if (obs_source_get_type(active_source) == OBS_SOURCE_TYPE_SCENE) {
					QMetaObject::invokeMethod(this, "SceneChanged", Q_ARG(OBSSource, OBSSource(active_source)));
				}
				obs_source_release(active_source);
			}
			return;
		}

		auto prev_transition = obs_weak_source_get_source(current_transition);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_transition), "transition_start", transition_start,
					  this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_transition), "transition_stop", transition_stop, this);
		obs_source_release(prev_transition);

		obs_weak_source_release(current_transition);
	}
	if (!transition || obs_source_removed(transition)) {
		current_transition = nullptr;
		return;
	}
	current_transition = obs_source_get_weak_source(transition);
	auto active_source = obs_transition_get_active_source(transition);
	if (active_source) {
		if (obs_source_get_type(active_source) == OBS_SOURCE_TYPE_SCENE) {
			QMetaObject::invokeMethod(this, "SceneChanged", Q_ARG(OBSSource, OBSSource(active_source)));
		}
		obs_source_release(active_source);
	}
	signal_handler_connect(obs_source_get_signal_handler(transition), "transition_start", transition_start, this);
	signal_handler_connect(obs_source_get_signal_handler(transition), "transition_stop", transition_stop, this);
}

void PropertiesDock::transition_start(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);
	auto transition = (obs_source_t *)calldata_ptr(cd, "source");
	auto active_source = obs_transition_get_active_source(transition);
	if (active_source) {
		if (obs_source_get_type(active_source) == OBS_SOURCE_TYPE_SCENE) {
			QMetaObject::invokeMethod(this_, "SceneChanged", Q_ARG(OBSSource, OBSSource(active_source)));
		}
		obs_source_release(active_source);
	}
}

void PropertiesDock::transition_stop(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);
	auto transition = (obs_source_t *)calldata_ptr(cd, "source");
	auto active_source = obs_transition_get_active_source(transition);
	if (active_source) {
		if (obs_source_get_type(active_source) == OBS_SOURCE_TYPE_SCENE) {
			QMetaObject::invokeMethod(this_, "SceneChanged", Q_ARG(OBSSource, OBSSource(active_source)));
		}
		obs_source_release(active_source);
	}
}

void PropertiesDock::SourceChanged(OBSSource source)
{
	if (current_source) {
		if (obs_weak_source_references_source(current_source, source))
			return;
		auto prev_source = obs_weak_source_get_source(current_source);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_source), "remove", source_remove, this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_source), "destroy", source_remove, this);
		obs_source_release(prev_source);

		obs_weak_source_release(current_source);
	}
	if (!source) {
		sourceLabel->setText(QString::fromUtf8(obs_module_text("NoSourceSelected")));
		current_source = nullptr;
		LoadProperties(nullptr);
		return;
	}
	current_source = obs_source_get_weak_source(source);
	sourceLabel->setText(QString::fromUtf8(obs_source_get_name(source)));

	signal_handler_connect(obs_source_get_signal_handler(source), "remove", source_remove, this);
	signal_handler_connect(obs_source_get_signal_handler(source), "destroy", source_remove, this);

	LoadProperties(source);
}

void PropertiesDock::SourceDeselected(OBSSource source)
{
	if (obs_weak_source_references_source(current_source, source))
		SourceChanged(nullptr);
}

void PropertiesDock::LoadProperties(OBSSource source)
{
	if (current_properties) {
		auto prev_source = obs_weak_source_get_source(current_properties);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_source), "update_properties", update_properties, this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_source), "remove", properties_remove, this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_source), "destroy", properties_remove, this);
		obs_source_release(prev_source);

		obs_weak_source_release(current_properties);
	}
	for (int i = layout->rowCount() - 1; i >= 0; i--) {
		layout->removeRow(i);
	}
	property_widgets.clear();
	if (properties) {
		obs_properties_destroy(properties);
		properties = nullptr;
	}
	if (!source) {
		current_properties = nullptr;
		return;
	}
	current_properties = obs_source_get_weak_source(source);

	properties = obs_source_properties(source);
	if (!properties)
		return;

	signal_handler_connect(obs_source_get_signal_handler(source), "update_properties", update_properties, this);
	signal_handler_connect(obs_source_get_signal_handler(source), "remove", properties_remove, this);
	signal_handler_connect(obs_source_get_signal_handler(source), "destroy", properties_remove, this);

	obs_data_t *settings = obs_source_get_settings(source);
	obs_property_t *property = obs_properties_first(properties);
	while (property) {
		AddProperty(properties, property, settings, layout);
		obs_property_next(&property);
	}
	obs_data_release(settings);
}

void PropertiesDock::source_remove(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);
	auto source = (obs_source_t *)calldata_ptr(cd, "source");
	if (obs_weak_source_references_source(this_->current_source, source)) {
		QMetaObject::invokeMethod(this_, "SourceDeselected", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(source)));
	}
}

void PropertiesDock::properties_remove(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);
	auto source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!obs_weak_source_references_source(this_->current_properties, source))
		return;

	if (this_->current_properties != this_->current_source) {
		auto s = obs_weak_source_get_source(this_->current_source);
		if (s) {
			QMetaObject::invokeMethod(this_, "LoadProperties", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(s)));
			obs_source_release(s);
			return;
		}
	}
	QMetaObject::invokeMethod(this_, "LoadProperties", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(nullptr)));
}

void PropertiesDock::update_properties(void *param, calldata_t *cd)
{
	auto this_ = static_cast<PropertiesDock *>(param);
	auto source = (obs_source_t *)calldata_ptr(cd, "source");
	if (!obs_weak_source_references_source(this_->current_properties, source))
		return;

	QMetaObject::invokeMethod(this_, "LoadProperties", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(source)));
}

void PropertiesDock::AddProperty(obs_properties_t *properties, obs_property_t *property, obs_data_t *settings, QFormLayout *layout)
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
		property_widgets.emplace(property, widget);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
		connect(widget, &QCheckBox::checkStateChanged, [this, properties, property, settings, widget, layout] {
#else
		connect(widget, &QCheckBox::stateChanged, [this, properties, property, settings, widget, layout] {
#endif
			if (obs_data_get_bool(settings, obs_property_name(property)) == widget->isChecked())
				return;
			obs_data_set_bool(settings, obs_property_name(property), widget->isChecked());
			if (obs_property_modified(property, settings)) {
				RefreshProperties(properties, layout);
			}
			auto source = obs_weak_source_get_source(current_properties);
			if (source) {
				obs_source_update(source, settings);
				obs_source_release(source);
			}
		});
	} else if (type == OBS_PROPERTY_INT) {
		obs_number_type int_type = obs_property_int_type(property);
		QWidget *widget = nullptr;

		auto spin = new QSpinBox();
		spin->setEnabled(obs_property_enabled(property));
		spin->setMinimum(obs_property_int_min(property));
		spin->setMaximum(obs_property_int_max(property));
		spin->setSingleStep(obs_property_int_step(property));
		spin->setValue((int)obs_data_get_int(settings, obs_property_name(property)));
		spin->setToolTip(QString::fromUtf8(obs_property_long_description(property)));
		spin->setSuffix(QString::fromUtf8(obs_property_int_suffix(property)));
		if (int_type == OBS_NUMBER_SLIDER) {
			widget = new QWidget;
			auto l = new QHBoxLayout();
			widget->setLayout(l);
			QSlider *slider = new SliderIgnoreScroll();
			slider->setMinimum(obs_property_int_min(property));
			slider->setMaximum(obs_property_int_max(property));
			slider->setPageStep(obs_property_int_step(property));
			slider->setValue((int)obs_data_get_int(settings, obs_property_name(property)));
			slider->setOrientation(Qt::Horizontal);
			slider->setEnabled(obs_property_enabled(property));
			l->addWidget(slider);
			l->addWidget(spin);
			connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
			connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
		} else {
			widget = spin;
		}

		auto label = new QLabel(QString::fromUtf8(obs_property_description(property)));
		layout->addRow(label, widget);
		if (!obs_property_visible(property)) {
			widget->setVisible(false);
			label->setVisible(false);
		}
		property_widgets.emplace(property, widget);
		connect(spin, &QSpinBox::valueChanged, [this, properties, property, settings, spin, layout] {
			if (obs_data_get_int(settings, obs_property_name(property)) == spin->value())
				return;
			obs_data_set_int(settings, obs_property_name(property), spin->value());
			if (obs_property_modified(property, settings)) {
				RefreshProperties(properties, layout);
			}
			auto source = obs_weak_source_get_source(current_properties);
			if (source) {
				obs_source_update(source, settings);
				obs_source_release(source);
			}
		});
	} else if (type == OBS_PROPERTY_FLOAT) {
		obs_number_type float_type = obs_property_float_type(property);
		QWidget *widget = nullptr;

		auto spin = new QDoubleSpinBox();
		spin->setEnabled(obs_property_enabled(property));
		spin->setMinimum(obs_property_float_min(property));
		spin->setMaximum(obs_property_float_max(property));
		spin->setSingleStep(obs_property_float_step(property));
		spin->setValue(obs_data_get_double(settings, obs_property_name(property)));
		spin->setToolTip(QString::fromUtf8(obs_property_long_description(property)));
		spin->setSuffix(QString::fromUtf8(obs_property_float_suffix(property)));
		auto label = new QLabel(QString::fromUtf8(obs_property_description(property)));
		if (float_type == OBS_NUMBER_SLIDER) {
			widget = new QWidget;
			auto l = new QHBoxLayout();
			widget->setLayout(l);
			DoubleSlider *slider = new DoubleSlider();
			slider->setDoubleConstraints(obs_property_float_min(property), obs_property_float_max(property),
						     obs_property_float_step(property),
						     obs_data_get_double(settings, obs_property_name(property)));
			slider->setOrientation(Qt::Horizontal);
			slider->setEnabled(obs_property_enabled(property));
			l->addWidget(slider);
			l->addWidget(spin);
			connect(slider, &DoubleSlider::valueChanged, spin, &QDoubleSpinBox::setValue);
			connect(spin, &QDoubleSpinBox::valueChanged, slider, &DoubleSlider::setValue);
		} else {
			widget = spin;
		}

		layout->addRow(label, widget);
		if (!obs_property_visible(property)) {
			widget->setVisible(false);
			label->setVisible(false);
		}
		property_widgets.emplace(property, widget);
		connect(spin, &QDoubleSpinBox::valueChanged, [this, properties, property, settings, spin, layout] {
			if (obs_data_get_double(settings, obs_property_name(property)) == spin->value())
				return;
			obs_data_set_double(settings, obs_property_name(property), spin->value());
			if (obs_property_modified(property, settings)) {
				RefreshProperties(properties, layout);
			}
			auto source = obs_weak_source_get_source(current_properties);
			if (source) {
				obs_source_update(source, settings);
				obs_source_release(source);
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
			property_widgets.emplace(property, widget);
			connect(widget, &QPlainTextEdit::textChanged, [this, properties, property, settings, widget, layout] {
				auto t = widget->toPlainText().toUtf8();
				if (strcmp(obs_data_get_string(settings, obs_property_name(property)), t.constData()) == 0)
					return;
				obs_data_set_string(settings, obs_property_name(property), t.constData());
				if (obs_property_modified(property, settings)) {
					RefreshProperties(properties, layout);
				}
				auto source = obs_weak_source_get_source(current_properties);
				if (source) {
					obs_source_update(source, settings);
					obs_source_release(source);
				}
			});
		} else if (text_type == OBS_TEXT_INFO) {
			obs_text_info_type info_type = obs_property_text_info_type(property);
			auto desc = QString::fromUtf8(obs_property_description(property));
			const char *long_desc = obs_property_long_description(property);
			QLabel *info_label =
				new QLabel(QString::fromUtf8(obs_data_get_string(settings, obs_property_name(property))));
			QLabel *label = nullptr;
			if (info_label->text().isEmpty() && long_desc == nullptr) {
				info_label->setText(desc);
			} else {
				label = new QLabel(desc);
			}

			if (long_desc != nullptr && !info_label->text().isEmpty()) {
				QString file = !obs_frontend_is_theme_dark() ? ":/res/images/help.svg"
									     : ":/res/images/help_light.svg";
				QString lStr = "<html>%1 <img src='%2' style=' \
				vertical-align: bottom; ' /></html>";

				info_label->setText(lStr.arg(info_label->text(), file));
				info_label->setToolTip(QString::fromUtf8(long_desc));
			} else if (long_desc != nullptr) {
				info_label->setText(QString::fromUtf8(long_desc));
			}

			info_label->setOpenExternalLinks(true);
			info_label->setWordWrap(obs_property_text_info_word_wrap(property));

			if (info_type == OBS_TEXT_INFO_WARNING)
				info_label->setProperty("class", "text-warning");
			else if (info_type == OBS_TEXT_INFO_ERROR)
				info_label->setProperty("class", "text-danger");

			if (label)
				label->setObjectName(info_label->objectName());

			layout->addRow(label, info_label);

			if (!obs_property_visible(property)) {
				info_label->setVisible(false);
				if (label)
					label->setVisible(false);
			}
			property_widgets.emplace(property, info_label);
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
			property_widgets.emplace(property, widget);

			connect(widget, &QLineEdit::textChanged, [this, properties, property, settings, widget, layout] {
				auto t = widget->text().toUtf8();
				if (strcmp(obs_data_get_string(settings, obs_property_name(property)), t.constData()) == 0)
					return;

				obs_data_set_string(settings, obs_property_name(property), t.constData());
				if (obs_property_modified(property, settings)) {
					RefreshProperties(properties, layout);
				}
				auto source = obs_weak_source_get_source(current_properties);
				if (source) {
					obs_source_update(source, settings);
					obs_source_release(source);
				}
			});
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
		property_widgets.emplace(property, widget);
		switch (format) {
		case OBS_COMBO_FORMAT_INT:
			connect(widget, &QComboBox::currentIndexChanged, [this, properties, property, settings, widget, layout] {
				auto i = widget->currentData().toInt();
				if (obs_data_get_int(settings, obs_property_name(property)) == i)
					return;
				obs_data_set_int(settings, obs_property_name(property), i);
				if (obs_property_modified(property, settings)) {
					RefreshProperties(properties, layout);
				}
				auto source = obs_weak_source_get_source(current_properties);
				if (source) {
					obs_source_update(source, settings);
					obs_source_release(source);
				}
			});
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			connect(widget, &QComboBox::currentIndexChanged, [this, properties, property, settings, widget, layout] {
				auto d = widget->currentData().toDouble();
				if (obs_data_get_double(settings, obs_property_name(property)) == d)
					return;
				obs_data_set_double(settings, obs_property_name(property), d);
				if (obs_property_modified(property, settings)) {
					RefreshProperties(properties, layout);
				}
				auto source = obs_weak_source_get_source(current_properties);
				if (source) {
					obs_source_update(source, settings);
					obs_source_release(source);
				}
			});
			break;
		case OBS_COMBO_FORMAT_STRING:
			if (list_type == OBS_COMBO_TYPE_EDITABLE) {
				connect(widget, &QComboBox::currentTextChanged,
					[this, properties, property, settings, widget, layout] {
						auto t = widget->currentText().toUtf8();
						if (strcmp(obs_data_get_string(settings, obs_property_name(property)),
							   t.constData()) == 0)
							return;
						obs_data_set_string(settings, obs_property_name(property), t.constData());
						if (obs_property_modified(property, settings)) {
							RefreshProperties(properties, layout);
						}
						auto source = obs_weak_source_get_source(current_properties);
						if (source) {
							obs_source_update(source, settings);
							obs_source_release(source);
						}
					});
			} else {
				connect(widget, &QComboBox::currentIndexChanged,
					[this, properties, property, settings, widget, layout] {
						auto t = widget->currentData().toString().toUtf8();
						if (strcmp(obs_data_get_string(settings, obs_property_name(property)),
							   t.constData()) == 0)
							return;
						obs_data_set_string(settings, obs_property_name(property), t.constData());
						if (obs_property_modified(property, settings)) {
							RefreshProperties(properties, layout);
						}
						auto source = obs_weak_source_get_source(current_properties);
						if (source) {
							obs_source_update(source, settings);
							obs_source_release(source);
						}
					});
			}
			break;
		default:;
		}
	} else if (type == OBS_PROPERTY_GROUP) {
		enum obs_group_type type = obs_property_group_type(property);

		// Create GroupBox
		QGroupBox *groupBox = new QGroupBox(QString::fromUtf8(obs_property_description(property)));
		groupBox->setCheckable(type == OBS_GROUP_CHECKABLE);
		groupBox->setChecked(groupBox->isCheckable() ? obs_data_get_bool(settings, obs_property_name(property)) : true);
		groupBox->setAccessibleName("group");
		groupBox->setEnabled(obs_property_enabled(property));

		auto subLayout = new QFormLayout;

		groupBox->setLayout(subLayout);

		layout->addRow(groupBox);
		if (!obs_property_visible(property)) {
			groupBox->setVisible(false);
		}
		property_widgets.emplace(property, groupBox);

		obs_properties_t *content = obs_property_group_content(property);
		obs_property_t *el = obs_properties_first(content);
		while (el != nullptr) {
			AddProperty(properties, el, settings, subLayout);
			obs_property_next(&el);
		}
		connect(groupBox, &QGroupBox::toggled, [this, properties, property, settings, groupBox, layout] {
			if (obs_data_get_bool(settings, obs_property_name(property)) == groupBox->isChecked())
				return;
			obs_data_set_bool(settings, obs_property_name(property), groupBox->isChecked());
			if (obs_property_modified(property, settings)) {
				RefreshProperties(properties, layout);
			}
			auto source = obs_weak_source_get_source(current_properties);
			if (source) {
				obs_source_update(source, settings);
				obs_source_release(source);
			}
		});
	} else if (type == OBS_PROPERTY_COLOR || type == OBS_PROPERTY_COLOR_ALPHA) {
		QPushButton *button = new QPushButton;
		button->setText(QString::fromUtf8(obs_frontend_get_locale_string("Basic.PropertiesWindow.SelectColor")));
		button->setEnabled(obs_property_enabled(property));
		QColor color = color_from_int(obs_data_get_int(settings, obs_property_name(property)));
		QColor::NameFormat format = (type == OBS_PROPERTY_COLOR_ALPHA) ? QColor::HexArgb : QColor::HexRgb;
		if (type == OBS_PROPERTY_COLOR)
			color.setAlpha(255);

		QPalette palette = QPalette(color);
		QLabel *colorLabel = new QLabel;
		colorLabel->setEnabled(obs_property_enabled(property));
		colorLabel->setFrameStyle(QFrame::Sunken | QFrame::Panel);
		colorLabel->setText(color.name(format));
		colorLabel->setPalette(palette);
		colorLabel->setStyleSheet(QString("background-color :%1; color: %2;")
						  .arg(palette.color(QPalette::Window).name(format))
						  .arg(palette.color(QPalette::WindowText).name(format)));
		colorLabel->setAutoFillBackground(true);
		colorLabel->setAlignment(Qt::AlignCenter);
		colorLabel->setToolTip(QString::fromUtf8(obs_property_description(property)));

		QHBoxLayout *subLayout = new QHBoxLayout;
		subLayout->setContentsMargins(0, 0, 0, 0);
		subLayout->addWidget(colorLabel);
		subLayout->addWidget(button);

		auto label = new QLabel(QString::fromUtf8(obs_property_description(property)));
		auto widget = new QWidget;

		widget->setLayout(subLayout);
		layout->addRow(label, widget);
		if (!obs_property_visible(property)) {
			widget->setVisible(false);
			label->setVisible(false);
		}
		property_widgets.emplace(property, widget);
		bool supportAlpha = (type == OBS_PROPERTY_COLOR_ALPHA);
		connect(button, &QPushButton::clicked,
			[this, properties, property, settings, button, layout, supportAlpha, colorLabel, format] {
				QColor initial = color_from_int(obs_data_get_int(settings, obs_property_name(property)));
				QColorDialog::ColorDialogOptions options;

				if (supportAlpha) {
					options |= QColorDialog::ShowAlphaChannel;
				}

#ifdef __linux__
				// TODO: Revisit hang on Ubuntu with native dialog
				options |= QColorDialog::DontUseNativeDialog;
#endif

				QColor color = QColorDialog::getColor(
					initial, this, QString::fromUtf8(obs_property_description(property)), options);
				if (!color.isValid())
					return;
				if (!supportAlpha)
					color.setAlpha(255);
				if (color == initial)
					return;

				colorLabel->setText(color.name(format));
				QPalette palette = QPalette(color);
				colorLabel->setPalette(palette);
				colorLabel->setStyleSheet(QString("background-color :%1; color: %2;")
								  .arg(palette.color(QPalette::Window).name(format))
								  .arg(palette.color(QPalette::WindowText).name(format)));

				obs_data_set_int(settings, obs_property_name(property), color_to_int(color));
				if (obs_property_modified(property, settings)) {
					RefreshProperties(properties, layout);
				}
				auto source = obs_weak_source_get_source(current_properties);
				if (source) {
					obs_source_update(source, settings);
					obs_source_release(source);
				}
			});
	} else if (type == OBS_PROPERTY_BUTTON) {
		QPushButton *button = new QPushButton(QString::fromUtf8(obs_property_description(property)));
		button->setEnabled(obs_property_enabled(property));
		layout->addRow(button);
		if (!obs_property_visible(property)) {
			button->setVisible(false);
		}
		property_widgets.emplace(property, button);
		connect(button, &QPushButton::clicked, [this, properties, property, settings, button, layout] {
			obs_button_type type = obs_property_button_type(property);
			auto savedUrl = QString::fromUtf8(obs_property_button_url(property));
			if (type == OBS_BUTTON_URL && !savedUrl.isEmpty()) {
				QUrl url(savedUrl, QUrl::StrictMode);
				if (url.isValid() && (url.scheme().compare("http") == 0 || url.scheme().compare("https") == 0)) {
					QString msg(QString::fromUtf8(
						obs_frontend_get_locale_string("Basic.PropertiesView.UrlButton.Text")));
					msg += "\n\n";
					msg += QString::fromUtf8(
						       obs_frontend_get_locale_string("Basic.PropertiesView.UrlButton.Text.Url"))
						       .arg(savedUrl);

					QMessageBox::StandardButton button =
						QMessageBox::question(this,
								      QString::fromUtf8(obs_frontend_get_locale_string(
									      "Basic.PropertiesView.UrlButton.OpenUrl")),
								      msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

					if (button == QMessageBox::Yes)
						QDesktopServices::openUrl(url);
				}
			} else {
				auto source = obs_weak_source_get_source(current_properties);
				if (obs_property_button_clicked(property, obs_obj_get_data(source))) {
					RefreshProperties(properties, layout);
				}
			}
		});
	} else if (type == OBS_PROPERTY_PATH) {
		auto l = new QHBoxLayout();
		QLineEdit *edit = new QLineEdit();
		QPushButton *button = new QPushButton(QString::fromUtf8(obs_frontend_get_locale_string("Browse")));
		edit->setText(QString::fromUtf8(obs_data_get_string(settings, obs_property_name(property))));
		edit->setReadOnly(true);
		edit->setToolTip(QString::fromUtf8(obs_property_long_description(property)));
		l->addWidget(edit);
		l->addWidget(button);
		auto label = new QLabel(QString::fromUtf8(obs_property_description(property)));
		auto widget = new QWidget;
		widget->setLayout(l);
		layout->addRow(label, widget);
		if (!obs_property_visible(property)) {
			widget->setVisible(false);
			label->setVisible(false);
		}
		property_widgets.emplace(property, widget);
		connect(button, &QPushButton::clicked, [this, property, settings, properties, layout, edit] {
			QString startDir = edit->text();
			if (startDir.isEmpty())
				startDir = QString::fromUtf8(obs_property_path_default_path(property));

			obs_path_type path_type = obs_property_path_type(property);
			QString path;
			if (path_type == OBS_PATH_DIRECTORY) {
				path = QFileDialog::getExistingDirectory(
					this, QString::fromUtf8(obs_property_description(property)), startDir,
					QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
			} else if (path_type == OBS_PATH_FILE) {
				path = QFileDialog::getOpenFileName(this, QString::fromUtf8(obs_property_description(property)),
								    startDir,
								    QString::fromUtf8(obs_property_path_filter(property)));
			} else if (path_type == OBS_PATH_FILE_SAVE) {
				path = QFileDialog::getSaveFileName(this, QString::fromUtf8(obs_property_description(property)),
								    startDir,
								    QString::fromUtf8(obs_property_path_filter(property)));
			}
			if (path.isEmpty())
				return;
			edit->setText(path);
			obs_data_set_string(settings, obs_property_name(property), path.toUtf8().constData());
			if (obs_property_modified(property, settings)) {
				RefreshProperties(properties, layout);
			}
			auto source = obs_weak_source_get_source(current_properties);
			if (source) {
				obs_source_update(source, settings);
				obs_source_release(source);
			}
		});
	} else {
		// OBS_PROPERTY_FONT
		// OBS_PROPERTY_EDITABLE_LIST
		// OBS_PROPERTY_FRAME_RATE
	}
	obs_property_modified(property, settings);
}

void PropertiesDock::RefreshProperties(obs_properties_t *properties, QFormLayout *layout)
{
	obs_property_t *property = obs_properties_first(properties);
	while (property) {
		auto it = property_widgets.find(property);
		if (it == property_widgets.end()) {
			obs_property_next(&property);
			continue;
		}
		auto widget = it->second;
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

void PropertiesDock::frontend_event(enum obs_frontend_event event, void *param)
{
	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED || event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		auto this_ = static_cast<PropertiesDock *>(param);
		if (this_->current_canvas) {
			auto canvas = obs_weak_canvas_get_canvas(this_->current_canvas);
			if (canvas) {
				auto source = obs_canvas_get_channel(canvas, 0);
				if (source) {
					auto source_type = obs_source_get_type(source);
					if (source_type == OBS_SOURCE_TYPE_SCENE) {
						QMetaObject::invokeMethod(this_, "SceneChanged", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(source)));
					} else if (source_type == OBS_SOURCE_TYPE_TRANSITION) {
						QMetaObject::invokeMethod(this_, "TransitionChanged", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(source)));
					}
					obs_source_release(source);
				}
				obs_canvas_release(canvas);
			}
		}
	} else if (event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN || event == OBS_FRONTEND_EVENT_EXIT) {
		auto this_ = static_cast<PropertiesDock *>(param);
		if (this_->current_canvas) {
			obs_weak_canvas_release(this_->current_canvas);
			this_->current_canvas = nullptr;
		}
	}
}
