#include "../utils/icon.hpp"
#include "../utils/widgets/locked-checkbox.hpp"
#include "../utils/widgets/visibility-checkbox.hpp"
#include "transform-dock.hpp"
#include <graphics/matrix4.h>
#include <obs-frontend-api.h>
#include <QCoreApplication>
#include <QScrollArea>
#include <QToolBar>
#include <QVBoxLayout>

TransformDock::TransformDock(QWidget *parent) : QFrame(parent)
{

	setObjectName("outerFrame");
	setFrameShape(QFrame::NoFrame);
	setFrameShadow(QFrame::Plain);
	setLineWidth(0);
	setProperty("class", QVariant(QCoreApplication::translate("OBSBasicTransform", "dialog-container", nullptr)));

	auto ml = new QVBoxLayout;
	setLayout(ml);

	auto scrollArea = new QScrollArea;
	scrollArea->setWidgetResizable(true);
	scrollArea->setLineWidth(0);
	scrollArea->setFrameShape(QFrame::NoFrame);
	ml->addWidget(scrollArea);

	auto scrollWidget = new QWidget;
	scrollWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	scrollArea->setWidget(scrollWidget);

	auto verticalLayout_3 = new QVBoxLayout(this);
	verticalLayout_3->setSpacing(0);
	verticalLayout_3->setObjectName("verticalLayout_3");
	verticalLayout_3->setContentsMargins(0, 0, 0, 0);
	scrollWidget->setLayout(verticalLayout_3);

	sourceLabel = new QLabel(this);

	auto titleLayout = new QHBoxLayout;
	titleLayout->setContentsMargins(0, 0, 0, 0);
	titleLayout->addWidget(sourceLabel, 1, Qt::AlignLeft);

	//titleLayout->addWidget(new VisibilityCheckBox(this), 0, Qt::AlignRight);
	//titleLayout->addWidget(new LockedCheckBox(this), 0, Qt::AlignRight);

	verticalLayout_3->addLayout(titleLayout);

	auto toolbar = new QToolBar;

	toolbar->addAction(QIcon::fromTheme(QIcon::ThemeIcon::EditCopy),
			   QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.CopyTransform")),
			   [this] {
				   if (!item)
					   return;
				   obs_sceneitem_get_info2(item, &copiedTransformInfo);
				   obs_sceneitem_get_crop(item, &copiedCropInfo);
				   pasteAction->setEnabled(true);
			   });
	pasteAction = toolbar->addAction(
		QIcon::fromTheme(QIcon::ThemeIcon::EditPaste),
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.PasteTransform")), [this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			obs_sceneitem_defer_update_begin(item);
			obs_sceneitem_set_info2(item, &copiedTransformInfo);
			obs_sceneitem_set_crop(item, &copiedCropInfo);
			obs_sceneitem_defer_update_end(item);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.Paste"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		});
	pasteAction->setEnabled(false);
	toolbar->addAction(
		QIcon::fromTheme(QIcon::ThemeIcon::SystemReboot),
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.ResetTransform")), [this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			struct obs_transform_info transformInfo = {};
			struct obs_sceneitem_crop cropInfo = {};
			vec2_set(&transformInfo.pos, 0.0f, 0.0f);
			vec2_set(&transformInfo.scale, 1.0f, 1.0f);
			transformInfo.rot = 0.0f;
			transformInfo.alignment = OBS_ALIGN_TOP | OBS_ALIGN_LEFT;
			transformInfo.bounds_type = OBS_BOUNDS_NONE;
			transformInfo.bounds_alignment = OBS_ALIGN_CENTER;
			transformInfo.crop_to_bounds = false;
			vec2_set(&transformInfo.bounds, 0.0f, 0.0f);

			obs_sceneitem_defer_update_begin(item);
			obs_sceneitem_set_info2(item, &transformInfo);
			obs_sceneitem_set_crop(item, &cropInfo);
			obs_sceneitem_defer_update_end(item);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.Reset"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		}); //QIcon::ThemeIcon::ViewRestore
	toolbar->addSeparator();
	toolbar->addAction(
		QIcon::fromTheme(QIcon::ThemeIcon::ObjectRotateLeft),
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.Rotate90CCW")), [this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			float rot = 90 + obs_sceneitem_get_rot(item);
			if (rot >= 360.0f)
				rot -= 360.0f;
			else if (rot <= -360.0f)
				rot += 360.0f;
			obs_sceneitem_set_rot(item, rot);

			obs_sceneitem_force_update_transform(item);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.Rotate"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		}); // QIcon::ThemeIcon::ViewRestore
	toolbar->addAction(
		QIcon::fromTheme(QIcon::ThemeIcon::ObjectRotateRight),
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.Rotate90CW")), [this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			float rot = -90 + obs_sceneitem_get_rot(item);
			if (rot >= 360.0f)
				rot -= 360.0f;
			else if (rot <= -360.0f)
				rot += 360.0f;
			obs_sceneitem_set_rot(item, rot);

			obs_sceneitem_force_update_transform(item);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.Rotate"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		}); // QIcon::ThemeIcon::ViewRefresh
	toolbar->addAction(
		QIcon::fromTheme(QIcon::ThemeIcon::ViewRefresh),
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.Rotate180")), [this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			float rot = 180 + obs_sceneitem_get_rot(item);
			if (rot >= 360.0f)
				rot -= 360.0f;
			else if (rot <= -360.0f)
				rot += 360.0f;
			obs_sceneitem_set_rot(item, rot);

			obs_sceneitem_force_update_transform(item);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.Rotate"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		});
	toolbar->addSeparator();
	auto c = this->palette().color(QPalette::Text);
	toolbar->addAction(
		generateEmojiQIcon(QString::fromUtf8("⇄"), c),
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.FlipHorizontal")), [this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			vec2 mul;
			vec2_set(&mul, -1.0f, 1.0f);

			vec2 scale;
			obs_sceneitem_get_scale(item, &scale);
			vec2_mul(&scale, &scale, &mul);

			obs_sceneitem_set_scale(item, &scale);

			obs_sceneitem_force_update_transform(item);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.HFlip"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		});
	toolbar->addAction(
		generateEmojiQIcon(QString::fromUtf8("⇅"), c),
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.FlipVertical")), [this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			vec2 mul;
			vec2_set(&mul, 1.0f, -1.0f);

			vec2 scale;
			obs_sceneitem_get_scale(item, &scale);
			vec2_mul(&scale, &scale, &mul);

			obs_sceneitem_set_scale(item, &scale);

			obs_sceneitem_force_update_transform(item);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.VFlip"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		});

	toolbar->addSeparator();
	toolbar->addAction(QIcon::fromTheme(QIcon::ThemeIcon::ZoomFitBest),
			   QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.FitToScreen")), [this] {
				   if (!item)
					   return;

				   auto scene = obs_sceneitem_get_scene(item);
				   OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

				   obs_video_info ovi;
				   obs_get_video_info(&ovi);

				   obs_transform_info itemInfo;
				   vec2_set(&itemInfo.pos, 0.0f, 0.0f);
				   vec2_set(&itemInfo.scale, 1.0f, 1.0f);
				   itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
				   itemInfo.rot = 0.0f;

				   vec2_set(&itemInfo.bounds, float(ovi.base_width), float(ovi.base_height));
				   itemInfo.bounds_type = OBS_BOUNDS_SCALE_INNER;
				   itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
				   itemInfo.crop_to_bounds = obs_sceneitem_get_bounds_crop(item);

				   obs_sceneitem_set_info2(item, &itemInfo);

				   			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
				   auto undoName =
					   QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.FitToScreen"))
						   .arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
				   obs_frontend_add_undo_redo_action(
					   undoName.toUtf8().constData(),
					   [](const char *data) { obs_scene_load_transform_states(data); },
					   [](const char *data) { obs_scene_load_transform_states(data); },
					   obs_data_get_json(wrapper), obs_data_get_json(rwrapper), false);
			   });
	toolbar->addAction(QIcon::fromTheme(QIcon::ThemeIcon::ViewFullscreen),
			   QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.StretchToScreen")),
		[this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			obs_video_info ovi;
			obs_get_video_info(&ovi);

			obs_transform_info itemInfo;
			vec2_set(&itemInfo.pos, 0.0f, 0.0f);
			vec2_set(&itemInfo.scale, 1.0f, 1.0f);
			itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
			itemInfo.rot = 0.0f;

			vec2_set(&itemInfo.bounds, float(ovi.base_width), float(ovi.base_height));
			itemInfo.bounds_type = OBS_BOUNDS_STRETCH;
			itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
			itemInfo.crop_to_bounds = obs_sceneitem_get_bounds_crop(item);

			obs_sceneitem_set_info2(item, &itemInfo);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.StretchToScreen"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		});
	toolbar->addAction(generateEmojiQIcon(QString::fromUtf8("✥"), c),
			   QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.CenterToScreen")),
			   [this] {
				   if (!item)
					   return;

				   auto scene = obs_sceneitem_get_scene(item);
				   OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

				   matrix4 boxTransform;
				   obs_sceneitem_get_box_transform(item, &boxTransform);

				   vec3 tl, br;

				   vec3_set(&tl, M_INFINITE, M_INFINITE, 0.0f);
				   vec3_set(&br, -M_INFINITE, -M_INFINITE, 0.0f);

				   auto GetMinPos = [&](float x, float y) {
					   vec3 pos;
					   vec3_set(&pos, x, y, 0.0f);
					   vec3_transform(&pos, &pos, &boxTransform);
					   vec3_min(&tl, &tl, &pos);
					   vec3_max(&br, &br, &pos);
				   };

				   GetMinPos(0.0f, 0.0f);
				   GetMinPos(1.0f, 0.0f);
				   GetMinPos(0.0f, 1.0f);
				   GetMinPos(1.0f, 1.0f);

				   vec3 center;
				   center.x = (br.x + tl.x) / 2.0f;
				   center.y = (tl.y + br.y) / 2.0f;
				   center.z = 0.0f;

				   vec3 screenCenter;
				   vec3_set(&screenCenter, float(obs_source_get_width(obs_scene_get_source(scene))),
					    float(obs_source_get_height(obs_scene_get_source(scene))), 0.0f);

				   vec3_mulf(&screenCenter, &screenCenter, 0.5f);

				   vec3 offset;
				   vec3_sub(&offset, &screenCenter, &center);

				   vec2 pos;
				   obs_sceneitem_get_pos(item, &pos);
				   pos.x += offset.x;
				   pos.y += offset.y;
				   obs_sceneitem_set_pos(item, &pos);

				   OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
				   auto undoName =
					   QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.Center"))
						   .arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
				   obs_frontend_add_undo_redo_action(
					   undoName.toUtf8().constData(),
					   [](const char *data) { obs_scene_load_transform_states(data); },
					   [](const char *data) { obs_scene_load_transform_states(data); },
					   obs_data_get_json(wrapper), obs_data_get_json(rwrapper), false);
			   });
	toolbar->addAction(generateEmojiQIcon(QString::fromUtf8("⬍"), c),
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.VerticalCenter")), [this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			matrix4 boxTransform;
			obs_sceneitem_get_box_transform(item, &boxTransform);

			vec3 tl, br;

			vec3_set(&tl, M_INFINITE, M_INFINITE, 0.0f);
			vec3_set(&br, -M_INFINITE, -M_INFINITE, 0.0f);

			auto GetMinPos = [&](float x, float y) {
				vec3 pos;
				vec3_set(&pos, x, y, 0.0f);
				vec3_transform(&pos, &pos, &boxTransform);
				vec3_min(&tl, &tl, &pos);
				vec3_max(&br, &br, &pos);
			};

			GetMinPos(0.0f, 0.0f);
			GetMinPos(1.0f, 0.0f);
			GetMinPos(0.0f, 1.0f);
			GetMinPos(1.0f, 1.0f);

			vec3 center;
			center.x = (br.x + tl.x) / 2.0f;
			center.y = (tl.y + br.y) / 2.0f;
			center.z = 0.0f;

			vec3 screenCenter;
			vec3_set(&screenCenter, float(obs_source_get_width(obs_scene_get_source(scene))),
				 float(obs_source_get_height(obs_scene_get_source(scene))), 0.0f);

			vec3_mulf(&screenCenter, &screenCenter, 0.5f);

			vec3 offset;
			vec3_sub(&offset, &screenCenter, &center);

			vec2 pos;
			obs_sceneitem_get_pos(item, &pos);
			pos.y += offset.y;
			obs_sceneitem_set_pos(item, &pos);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.VCenter"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		});
	toolbar->addAction(generateEmojiQIcon(QString::fromUtf8("⬌"), c),
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.HorizontalCenter")), [this] {
			if (!item)
				return;

			auto scene = obs_sceneitem_get_scene(item);
			OBSDataAutoRelease wrapper = obs_scene_save_transform_states(scene, false);

			matrix4 boxTransform;
			obs_sceneitem_get_box_transform(item, &boxTransform);

			vec3 tl, br;

			vec3_set(&tl, M_INFINITE, M_INFINITE, 0.0f);
			vec3_set(&br, -M_INFINITE, -M_INFINITE, 0.0f);

			auto GetMinPos = [&](float x, float y) {
				vec3 pos;
				vec3_set(&pos, x, y, 0.0f);
				vec3_transform(&pos, &pos, &boxTransform);
				vec3_min(&tl, &tl, &pos);
				vec3_max(&br, &br, &pos);
			};

			GetMinPos(0.0f, 0.0f);
			GetMinPos(1.0f, 0.0f);
			GetMinPos(0.0f, 1.0f);
			GetMinPos(1.0f, 1.0f);

			vec3 center;
			center.x = (br.x + tl.x) / 2.0f;
			center.y = (tl.y + br.y) / 2.0f;
			center.z = 0.0f;

			vec3 screenCenter;
			vec3_set(&screenCenter, float(obs_source_get_width(obs_scene_get_source(scene))),
				 float(obs_source_get_height(obs_scene_get_source(scene))), 0.0f);

			vec3_mulf(&screenCenter, &screenCenter, 0.5f);

			vec3 offset;
			vec3_sub(&offset, &screenCenter, &center);

			vec2 pos;
			obs_sceneitem_get_pos(item, &pos);
			pos.x += offset.x;
			obs_sceneitem_set_pos(item, &pos);

			OBSDataAutoRelease rwrapper = obs_scene_save_transform_states(scene, false);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Transform.VCenter"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(
				undoName.toUtf8().constData(), [](const char *data) { obs_scene_load_transform_states(data); },
				[](const char *data) { obs_scene_load_transform_states(data); }, obs_data_get_json(wrapper),
				obs_data_get_json(rwrapper), false);
		});

	verticalLayout_3->addWidget(toolbar);

	auto transformSettings = new QFrame(this);
	transformSettings->setObjectName("transformSettings");
	transformSettings->setFrameShape(QFrame::NoFrame);
	transformSettings->setFrameShadow(QFrame::Plain);
	transformSettings->setLineWidth(0);
	transformSettings->setProperty(
		"class", QVariant(QCoreApplication::translate("OBSBasicTransform", "dialog-container dialog-frame", nullptr)));

	auto gridLayout_2 = new QGridLayout(transformSettings);
	gridLayout_2->setObjectName("gridLayout_2");
	gridLayout_2->setHorizontalSpacing(8);
	gridLayout_2->setVerticalSpacing(2);
	gridLayout_2->setContentsMargins(0, 0, 0, 0);
	auto sizeWidthLabel = new QLabel(transformSettings);
	sizeWidthLabel->setObjectName("sizeWidthLabel");
	sizeWidthLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	sizeWidthLabel->setText(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.Width")));
	sizeWidthLabel->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Width", nullptr));

	gridLayout_2->addWidget(sizeWidthLabel, 1, 2, 1, 1);

	auto positionXLabel = new QLabel(transformSettings);
	positionXLabel->setObjectName("positionXLabel");
	positionXLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	positionXLabel->setText(QCoreApplication::translate("OBSBasicTransform", "X", nullptr));
	positionXLabel->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Accessible.PositionX", nullptr));

	gridLayout_2->addWidget(positionXLabel, 1, 0, 1, 1);

	sizeX = new QDoubleSpinBox(transformSettings);
	sizeX->setObjectName("sizeX");
	sizeX->setMinimumSize(QSize(120, 0));
	sizeX->setMaximumSize(QSize(120, 16777215));
	sizeX->setDecimals(2);
	sizeX->setMinimum(-90001.000000000000000);
	sizeX->setMaximum(90001.000000000000000);
	sizeX->setSingleStep(1.000000000000000);
	sizeX->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	sizeX->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Width", nullptr));

	gridLayout_2->addWidget(sizeX, 1, 3, 1, 1);

	auto label_2 = new QLabel(transformSettings);
	label_2->setObjectName("label_2");
	label_2->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Rotation", nullptr));
	label_2->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Rotation", nullptr));
	label_2->setProperty("class", QVariant(QCoreApplication::translate("OBSBasicTransform", "subtitle", nullptr)));

	gridLayout_2->addWidget(label_2, 0, 4, 1, 1);

	positionX = new QDoubleSpinBox(transformSettings);
	positionX->setObjectName("positionX");
	positionX->setMinimumSize(QSize(120, 0));
	positionX->setMaximumSize(QSize(120, 16777215));
	positionX->setDecimals(2);
	positionX->setMinimum(-90001.000000000000000);
	positionX->setMaximum(90001.000000000000000);
	positionX->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	positionX->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.PositionX", nullptr));
	positionXLabel->setBuddy(positionX);

	gridLayout_2->addWidget(positionX, 1, 1, 1, 1);

	rotation = new QDoubleSpinBox(transformSettings);
	rotation->setObjectName("rotation");
	QSizePolicy sizePolicy1(QSizePolicy::Policy::Maximum, QSizePolicy::Policy::Fixed);
	sizePolicy1.setHorizontalStretch(0);
	sizePolicy1.setVerticalStretch(0);
	sizePolicy1.setHeightForWidth(rotation->sizePolicy().hasHeightForWidth());
	rotation->setSizePolicy(sizePolicy1);
	rotation->setMinimumSize(QSize(120, 0));
	rotation->setMaximumSize(QSize(120, 16777215));
	rotation->setMinimum(-360.000000000000000);
	rotation->setMaximum(360.000000000000000);
	rotation->setSingleStep(0.100000000000000);
	rotation->setSuffix(QCoreApplication::translate("OBSBasicTransform", "\302\260", nullptr));
	rotation->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Rotation", nullptr));

	gridLayout_2->addWidget(rotation, 1, 4, 1, 1);

	auto label_3 = new QLabel(transformSettings);
	label_3->setObjectName("label_3");
	label_3->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);
	label_3->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Size", nullptr));
	label_3->setProperty("class", QVariant(QCoreApplication::translate("OBSBasicTransform", "subtitle", nullptr)));
	label_3->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Size", nullptr));

	gridLayout_2->addWidget(label_3, 0, 2, 1, 2);

	auto label = new QLabel(transformSettings);
	label->setObjectName("label");
	label->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignVCenter);
	label->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Position", nullptr));
	label->setProperty("class", QVariant(QCoreApplication::translate("OBSBasicTransform", "subtitle", nullptr)));
	label->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Position", nullptr));

	gridLayout_2->addWidget(label, 0, 0, 1, 2);

	auto horizontalSpacer_3 = new QSpacerItem(0, 10, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

	gridLayout_2->addItem(horizontalSpacer_3, 1, 6, 1, 1);

	auto positionYLabel = new QLabel(transformSettings);
	positionYLabel->setObjectName("positionYLabel");
	positionYLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	positionYLabel->setText(QCoreApplication::translate("OBSBasicTransform", "Y", nullptr));
	positionYLabel->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Accessible.PositionY", nullptr));

	gridLayout_2->addWidget(positionYLabel, 2, 0, 1, 1);

	positionY = new QDoubleSpinBox(transformSettings);
	positionY->setObjectName("positionY");
	positionY->setMinimumSize(QSize(120, 0));
	positionY->setMaximumSize(QSize(120, 16777215));
	positionY->setDecimals(2);
	positionY->setMinimum(-90001.000000000000000);
	positionY->setMaximum(90001.000000000000000);
	positionY->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	positionY->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.PositionY", nullptr));
	positionYLabel->setBuddy(positionY);

	gridLayout_2->addWidget(positionY, 2, 1, 1, 1);

	sizeY = new QDoubleSpinBox(transformSettings);
	sizeY->setObjectName("sizeY");
	sizeY->setMinimumSize(QSize(120, 0));
	sizeY->setMaximumSize(QSize(120, 16777215));
	sizeY->setDecimals(2);
	sizeY->setMinimum(-90001.000000000000000);
	sizeY->setMaximum(90001.000000000000000);
	sizeY->setSingleStep(1.000000000000000);
	sizeY->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	sizeY->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Height", nullptr));

	gridLayout_2->addWidget(sizeY, 2, 3, 1, 1);

	auto sizeHeightLabel = new QLabel(transformSettings);
	sizeHeightLabel->setObjectName("sizeHeightLabel");
	sizeHeightLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	sizeHeightLabel->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Height", nullptr));
	sizeHeightLabel->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Height", nullptr));

	gridLayout_2->addWidget(sizeHeightLabel, 2, 2, 1, 1);

	auto alignmentWidget = new QFrame(transformSettings);
	alignmentWidget->setObjectName("alignmentWidget");
	alignmentWidget->setFrameShape(QFrame::NoFrame);
	alignmentWidget->setFrameShadow(QFrame::Plain);
	alignmentWidget->setLineWidth(0);
	auto alignmentLayout = new QVBoxLayout(alignmentWidget);
	alignmentLayout->setSpacing(0);
	alignmentLayout->setObjectName("alignmentLayout");
	alignmentLayout->setContentsMargins(0, 0, 0, 0);

	positionAlignment = new AlignmentSelector(this);
	positionAlignment->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.Alignment")));
	alignmentLayout->addWidget(positionAlignment);
	positionAlignment->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	gridLayout_2->addWidget(alignmentWidget, 1, 5, 1, 1);

	auto label_4 = new QLabel(transformSettings);
	label_4->setObjectName("label_4");
	label_4->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Alignment", nullptr));
	label_4->setProperty("class", QVariant(QCoreApplication::translate("OBSBasicTransform", "subtitle", nullptr)));
	label_4->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Alignment", nullptr));

	gridLayout_2->addWidget(label_4, 0, 5, 1, 1);

	verticalLayout_3->addWidget(transformSettings);

	auto boundsSettings = new QFrame(this);
	boundsSettings->setObjectName("boundsSettings");
	boundsSettings->setFrameShape(QFrame::NoFrame);
	boundsSettings->setFrameShadow(QFrame::Plain);
	boundsSettings->setLineWidth(0);
	boundsSettings->setProperty(
		"class", QVariant(QCoreApplication::translate("OBSBasicTransform", "dialog-container dialog-frame", nullptr)));
	auto gridLayout_3 = new QGridLayout(boundsSettings);
	gridLayout_3->setObjectName("gridLayout_3");
	gridLayout_3->setHorizontalSpacing(8);
	gridLayout_3->setVerticalSpacing(2);
	gridLayout_3->setContentsMargins(0, 0, 0, 0);
	boundsType = new QComboBox(boundsSettings);
	boundsType->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.BoundsType.None")));
	boundsType->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.BoundsType.MaxOnly")));
	boundsType->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.BoundsType.ScaleInner")));
	boundsType->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.BoundsType.ScaleOuter")));
	boundsType->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.BoundsType.ScaleToWidth")));
	boundsType->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.BoundsType.ScaleToHeight")));
	boundsType->addItem(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.BoundsType.Stretch")));
	boundsType->setObjectName("boundsType");
	boundsType->setCurrentText(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.BoundsType.None")));
	boundsType->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.BoundsType", nullptr));

	gridLayout_3->addWidget(boundsType, 1, 0, 1, 3);

	auto horizontalSpacer_4 = new QSpacerItem(0, 10, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

	gridLayout_3->addItem(horizontalSpacer_4, 0, 7, 4, 1);

	auto label_7 = new QLabel(boundsSettings);
	label_7->setObjectName("label_7");
	label_7->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Bounds", nullptr));
	label_7->setProperty("class", QVariant(QCoreApplication::translate("OBSBasicTransform", "subtitle", nullptr)));
	label_7->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Bounds", nullptr));

	gridLayout_3->addWidget(label_7, 0, 0, 1, 3);

	auto alignmentWidget2 = new QFrame(boundsSettings);
	alignmentWidget2->setObjectName("alignmentWidget2");
	alignmentWidget2->setFrameShape(QFrame::NoFrame);
	alignmentWidget2->setFrameShadow(QFrame::Plain);
	alignmentWidget2->setLineWidth(0);
	auto boundsAlignmentLayout = new QVBoxLayout(alignmentWidget2);
	boundsAlignmentLayout->setObjectName("boundsAlignmentLayout");
	boundsAlignmentLayout->setContentsMargins(0, 0, 0, 0);

	boundsAlignment = new AlignmentSelector(this);
	boundsAlignment->setAccessibleName(
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.BoundsAlignment")));
	boundsAlignment->setEnabled(false);
	boundsAlignmentLayout->addWidget(boundsAlignment);
	boundsAlignment->setAlignment(Qt::AlignTop | Qt::AlignLeft);

	gridLayout_3->addWidget(alignmentWidget2, 1, 3, 1, 1);

	auto label_6 = new QLabel(boundsSettings);
	label_6->setObjectName("label_6");
	label_6->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Alignment", nullptr));
	label_6->setProperty("class", QVariant(QCoreApplication::translate("OBSBasicTransform", "subtitle", nullptr)));
	label_6->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Alignment", nullptr));

	gridLayout_3->addWidget(label_6, 0, 3, 1, 1);

	auto boundsWidthLabel = new QLabel(boundsSettings);
	boundsWidthLabel->setObjectName("boundsWidthLabel");
	boundsWidthLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	boundsWidthLabel->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Width", nullptr));
	boundsWidthLabel->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.BoundsWidth", nullptr));

	gridLayout_3->addWidget(boundsWidthLabel, 2, 0, 1, 1);

	auto boundsHeightLabel = new QLabel(boundsSettings);
	boundsHeightLabel->setObjectName("boundsHeightLabel");
	boundsHeightLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	boundsHeightLabel->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Height", nullptr));
	boundsHeightLabel->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.BoundsHeight", nullptr));

	gridLayout_3->addWidget(boundsHeightLabel, 3, 0, 1, 1);

	boundsWidth = new QDoubleSpinBox(boundsSettings);
	boundsWidth->setObjectName("boundsWidth");
	boundsWidth->setEnabled(false);
	boundsWidth->setMinimumSize(QSize(120, 0));
	boundsWidth->setMaximumSize(QSize(120, 16777215));
	boundsWidth->setDecimals(2);
	boundsWidth->setMinimum(1.000000000000000);
	boundsWidth->setMaximum(90001.000000000000000);
	boundsWidth->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	boundsWidth->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.BoundsWidth", nullptr));

	gridLayout_3->addWidget(boundsWidth, 2, 1, 1, 1);

	boundsHeight = new QDoubleSpinBox(boundsSettings);
	boundsHeight->setObjectName("boundsHeight");
	boundsHeight->setEnabled(false);
	boundsHeight->setMinimumSize(QSize(120, 0));
	boundsHeight->setMaximumSize(QSize(120, 16777215));
	boundsHeight->setDecimals(2);
	boundsHeight->setMinimum(1.000000000000000);
	boundsHeight->setMaximum(90001.000000000000000);
	boundsHeight->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	boundsHeight->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.BoundsHeight", nullptr));

	gridLayout_3->addWidget(boundsHeight, 3, 1, 1, 1);

	cropToBounds = new QCheckBox(boundsSettings);
	cropToBounds->setObjectName("cropToBounds");
	cropToBounds->setEnabled(false);
	cropToBounds->setMaximumSize(QSize(100, 16777215));
	cropToBounds->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.CropToBounds", nullptr));
	cropToBounds->setProperty("class", QVariant(QString()));
	cropToBounds->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.BoundsAlignment", nullptr));

	gridLayout_3->addWidget(cropToBounds, 2, 2, 1, 1);

	verticalLayout_3->addWidget(boundsSettings);

	auto cropSettings = new QFrame(this);
	cropSettings->setObjectName("cropSettings");
	cropSettings->setLineWidth(0);
	cropSettings->setProperty(
		"class", QVariant(QCoreApplication::translate("OBSBasicTransform", "dialog-container dialog-frame", nullptr)));
	auto gridLayout = new QGridLayout(cropSettings);
	gridLayout->setObjectName("gridLayout");
	gridLayout->setHorizontalSpacing(8);
	gridLayout->setVerticalSpacing(2);
	gridLayout->setContentsMargins(0, 0, 0, 0);
	auto cropLeftLabel = new QLabel(cropSettings);
	cropLeftLabel->setObjectName("cropLeftLabel");
	cropLeftLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	cropLeftLabel->setText(QCoreApplication::translate("OBSBasicTransform", "Left", nullptr));
	cropLeftLabel->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.CropLeft", nullptr));

	gridLayout->addWidget(cropLeftLabel, 1, 0, 1, 1);

	auto cropRightLabel = new QLabel(cropSettings);
	cropRightLabel->setObjectName("cropRightLabel");
	cropRightLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	cropRightLabel->setText(QCoreApplication::translate("OBSBasicTransform", "Right", nullptr));
	cropRightLabel->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.CropRight")));

	gridLayout->addWidget(cropRightLabel, 1, 2, 1, 1);

	cropBottom = new QSpinBox(cropSettings);
	cropBottom->setObjectName("cropBottom");
	sizePolicy1.setHeightForWidth(cropBottom->sizePolicy().hasHeightForWidth());
	cropBottom->setSizePolicy(sizePolicy1);
	cropBottom->setMinimumSize(QSize(120, 0));
	cropBottom->setMaximumSize(QSize(120, 16777215));
	cropBottom->setMaximum(100000);
	cropBottom->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	cropBottom->setAccessibleName(
		QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.CropBottom", nullptr));

	gridLayout->addWidget(cropBottom, 2, 3, 1, 1);

	auto horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

	gridLayout->addItem(horizontalSpacer_2, 1, 4, 1, 1);

	cropRight = new QSpinBox(cropSettings);
	cropRight->setObjectName("cropRight");
	sizePolicy1.setHeightForWidth(cropRight->sizePolicy().hasHeightForWidth());
	cropRight->setSizePolicy(sizePolicy1);
	cropRight->setMinimumSize(QSize(120, 0));
	cropRight->setMaximumSize(QSize(120, 16777215));
	cropRight->setMaximum(100000);
	cropRight->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	cropRight->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.CropRight", nullptr));

	gridLayout->addWidget(cropRight, 1, 3, 1, 1);

	cropLeft = new QSpinBox(cropSettings);
	cropLeft->setObjectName("cropLeft");
	sizePolicy1.setHeightForWidth(cropLeft->sizePolicy().hasHeightForWidth());
	cropLeft->setSizePolicy(sizePolicy1);
	cropLeft->setMinimumSize(QSize(120, 0));
	cropLeft->setMaximumSize(QSize(120, 16777215));
	cropLeft->setMaximum(100000);
	cropLeft->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	cropLeft->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.CropLeft", nullptr));

	gridLayout->addWidget(cropLeft, 1, 1, 1, 1);

	auto cropTopLabel = new QLabel(cropSettings);
	cropTopLabel->setObjectName("cropTopLabel");
	cropTopLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	cropTopLabel->setText(QCoreApplication::translate("OBSBasicTransform", "Top", nullptr));
	cropTopLabel->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.CropTop")));

	gridLayout->addWidget(cropTopLabel, 2, 0, 1, 1);

	cropTop = new QSpinBox(cropSettings);
	cropTop->setObjectName("cropTop");
	sizePolicy1.setHeightForWidth(cropTop->sizePolicy().hasHeightForWidth());
	cropTop->setSizePolicy(sizePolicy1);
	cropTop->setMinimumSize(QSize(120, 0));
	cropTop->setMaximumSize(QSize(120, 16777215));
	cropTop->setMaximum(100000);
	cropTop->setSuffix(QCoreApplication::translate("OBSBasicTransform", " px", nullptr));
	cropTop->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.CropTop", nullptr));

	gridLayout->addWidget(cropTop, 2, 1, 1, 1);

	auto label_8 = new QLabel(cropSettings);
	label_8->setObjectName("label_8");
	label_8->setText(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Crop", nullptr));
	label_8->setProperty("class", QVariant(QCoreApplication::translate("OBSBasicTransform", "subtitle", nullptr)));
	label_8->setAccessibleName(QCoreApplication::translate("OBSBasicTransform", "Basic.TransformWindow.Crop", nullptr));

	gridLayout->addWidget(label_8, 0, 0, 1, 1);

	auto cropBottomLabel = new QLabel(cropSettings);
	cropBottomLabel->setObjectName("cropBottomLabel");
	cropBottomLabel->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
	cropBottomLabel->setText(QCoreApplication::translate("OBSBasicTransform", "Bottom", nullptr));
	cropBottomLabel->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransformWindow.CropBottom")));

	gridLayout->addWidget(cropBottomLabel, 2, 2, 1, 1);

	verticalLayout_3->addWidget(cropSettings);

	setTabOrder(positionX, positionY);
	setTabOrder(positionY, sizeX);
	setTabOrder(sizeX, sizeY);
	setTabOrder(sizeY, rotation);
	setTabOrder(rotation, positionAlignment);
	setTabOrder(positionAlignment, boundsType);
	setTabOrder(boundsType, boundsAlignment);
	setTabOrder(boundsAlignment, boundsWidth);
	setTabOrder(boundsWidth, boundsHeight);
	setTabOrder(boundsHeight, cropToBounds);
	setTabOrder(cropToBounds, cropLeft);
	setTabOrder(cropLeft, cropRight);
	setTabOrder(cropRight, cropTop);
	setTabOrder(cropTop, cropBottom);

	connect(positionX, &QDoubleSpinBox::valueChanged, [this]() { onControlChanged(); });
	connect(positionY, &QDoubleSpinBox::valueChanged, [this]() { onControlChanged(); });
	connect(rotation, &QDoubleSpinBox::valueChanged, [this]() { onControlChanged(); });
	connect(sizeX, &QDoubleSpinBox::valueChanged, [this]() { onControlChanged(); });
	connect(sizeY, &QDoubleSpinBox::valueChanged, [this]() { onControlChanged(); });
	connect(positionAlignment, &AlignmentSelector::currentIndexChanged, [this](int index) { onAlignChanged(index); });
	connect(boundsType, &QComboBox::currentIndexChanged, [this](int index) { onBoundsType(index); });
	connect(boundsAlignment, &AlignmentSelector::currentIndexChanged, [this]() { onControlChanged(); });
	connect(boundsWidth, &QDoubleSpinBox::valueChanged, [this]() { onControlChanged(); });
	connect(boundsHeight, &QDoubleSpinBox::valueChanged, [this]() { onControlChanged(); });
	connect(cropLeft, &QSpinBox::valueChanged, [this]() { onCropChanged(); });
	connect(cropRight, &QSpinBox::valueChanged, [this]() { onCropChanged(); });
	connect(cropTop, &QSpinBox::valueChanged, [this]() { onCropChanged(); });
	connect(cropBottom, &QSpinBox::valueChanged, [this]() { onCropChanged(); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
	connect(cropToBounds, &QCheckBox::checkStateChanged, [this]() { onControlChanged(); });
#else
	connect(cropToBounds, &QCheckBox::stateChanged, [this]() { onControlChanged(); });
#endif
	setEnabled(false);
}

TransformDock::~TransformDock() {}

void TransformDock::setItem(OBSSceneItem newItem)
{
	item = newItem;
	if (item) {
		auto scene = obs_sceneitem_get_scene(item);
		if (scene) {
			auto ss = obs_scene_get_source(scene);
			auto canvas = obs_source_get_canvas(ss);
			if (canvas) {
				sourceLabel->setText(QString::fromUtf8(obs_canvas_get_name(canvas)) + QString::fromUtf8(" → ") +
						     QString::fromUtf8(obs_source_get_name(ss)) + QString::fromUtf8(" → ") +
						     QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item))));
				obs_canvas_release(canvas);
			} else {
				sourceLabel->setText(QString::fromUtf8(obs_source_get_name(ss)) + QString::fromUtf8(" → ") +
						     QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item))));
			}
		} else {
			sourceLabel->setText(QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item))));
		}
		refreshControls();
	} else {
		sourceLabel->setText("");
	}

	bool enable = !!item && !obs_sceneitem_locked(item);
	setEnabled(enable);
}

void TransformDock::unsetItem(OBSSceneItem unsetItem)
{
	if (item != unsetItem)
		return;
	setItem(nullptr);
}

void TransformDock::refreshControls()
{
	if (!item)
		return;

	obs_transform_info oti;
	obs_sceneitem_crop crop;
	obs_sceneitem_get_info2(item, &oti);
	obs_sceneitem_get_crop(item, &crop);

	obs_source_t *source = obs_sceneitem_get_source(item);
	uint32_t source_cx = obs_source_get_width(source);
	uint32_t source_cy = obs_source_get_height(source);
	float width = float(source_cx);
	float height = float(source_cy);

	int alignIndex = alignToIndex(oti.alignment);
	int boundsAlignIndex = alignToIndex(oti.bounds_alignment);

	ignoreItemChange = true;
	positionX->setValue(oti.pos.x);
	positionY->setValue(oti.pos.y);
	rotation->setValue(oti.rot);
	sizeX->setValue(oti.scale.x * width);
	sizeY->setValue(oti.scale.y * height);
	positionAlignment->setCurrentIndex(alignIndex);

	bool valid_size = source_cx != 0 && source_cy != 0;
	sizeX->setEnabled(valid_size);
	sizeY->setEnabled(valid_size);

	boundsType->setCurrentIndex(int(oti.bounds_type));
	boundsAlignment->setCurrentIndex(boundsAlignIndex);
	boundsWidth->setValue(oti.bounds.x);
	boundsHeight->setValue(oti.bounds.y);
	cropToBounds->setChecked(oti.crop_to_bounds);

	cropLeft->setValue(int(crop.left));
	cropRight->setValue(int(crop.right));
	cropTop->setValue(int(crop.top));
	cropBottom->setValue(int(crop.bottom));
	ignoreItemChange = false;

	std::string name = obs_source_get_name(source);
	//setWindowTitle(QTStr("Basic.TransformWindow.Title").arg(name.c_str()));
}

static const uint32_t indexToAlign[] = {OBS_ALIGN_TOP | OBS_ALIGN_LEFT,
					OBS_ALIGN_TOP,
					OBS_ALIGN_TOP | OBS_ALIGN_RIGHT,
					OBS_ALIGN_LEFT,
					OBS_ALIGN_CENTER,
					OBS_ALIGN_RIGHT,
					OBS_ALIGN_BOTTOM | OBS_ALIGN_LEFT,
					OBS_ALIGN_BOTTOM,
					OBS_ALIGN_BOTTOM | OBS_ALIGN_RIGHT};

int TransformDock::alignToIndex(uint32_t align)
{
	int index = 0;
	for (uint32_t curAlign : indexToAlign) {
		if (curAlign == align)
			return index;

		index++;
	}

	return 0;
}

void TransformDock::sceneItemTransform(OBSSceneItem transformItem)
{
	if (item != transformItem)
		return;
	if (ignoreTransformSignal)
		return;
	refreshControls();
}

void TransformDock::sceneItemLocked(OBSSceneItem lockedItem)
{
	if (item != lockedItem)
		return;
	setEnabled(!obs_sceneitem_locked(item));
}

void TransformDock::onAlignChanged(int index)
{
	uint32_t alignment = indexToAlign[index];

	vec2 flipRatio = getAlignmentConversion(alignment);

	obs_transform_info oti;
	obs_sceneitem_crop crop;
	obs_sceneitem_get_info2(item, &oti);
	obs_sceneitem_get_crop(item, &crop);

	obs_source_t *source = obs_sceneitem_get_source(item);
	uint32_t sourceWidth = obs_source_get_width(source);
	uint32_t sourceHeight = obs_source_get_height(source);

	uint32_t widthForFlip = sourceWidth - crop.left - crop.right;
	uint32_t heightForFlip = sourceHeight - crop.top - crop.bottom;

	if (oti.bounds_type != OBS_BOUNDS_NONE) {
		widthForFlip = oti.bounds.x;
		heightForFlip = oti.bounds.y;
	}

	vec2 currentRatio = getAlignmentConversion(oti.alignment);

	float shiftX = (currentRatio.x - flipRatio.x) * widthForFlip * oti.scale.x;
	float shiftY = (currentRatio.y - flipRatio.y) * heightForFlip * oti.scale.y;

	bool previousIgnoreState = ignoreItemChange;

	ignoreItemChange = true;
	positionX->setValue(oti.pos.x - shiftX);
	positionY->setValue(oti.pos.y - shiftY);
	ignoreItemChange = previousIgnoreState;

	onControlChanged();
}

vec2 TransformDock::getAlignmentConversion(uint32_t alignment)
{
	vec2 ratio = {0.5f, 0.5f};
	if (alignment & OBS_ALIGN_RIGHT) {
		ratio.x = 1.0f;
	}
	if (alignment & OBS_ALIGN_LEFT) {
		ratio.x = 0.0f;
	}
	if (alignment & OBS_ALIGN_BOTTOM) {
		ratio.y = 1.0f;
	}
	if (alignment & OBS_ALIGN_TOP) {
		ratio.y = 0.0f;
	}

	return ratio;
}

void TransformDock::onBoundsType(int index)
{
	if (index == -1)
		return;

	obs_bounds_type type = (obs_bounds_type)index;
	bool enable = (type != OBS_BOUNDS_NONE);

	boundsAlignment->setEnabled(enable && type != OBS_BOUNDS_STRETCH);
	boundsWidth->setEnabled(enable);
	boundsHeight->setEnabled(enable);

	bool isCoverBounds = type == OBS_BOUNDS_SCALE_OUTER || type == OBS_BOUNDS_SCALE_TO_WIDTH ||
			     type == OBS_BOUNDS_SCALE_TO_HEIGHT;
	cropToBounds->setEnabled(isCoverBounds);

	if (!ignoreItemChange) {
		obs_bounds_type lastType = obs_sceneitem_get_bounds_type(item);
		if (lastType == OBS_BOUNDS_NONE) {
			OBSSource source = obs_sceneitem_get_source(item);
			int width = (int)obs_source_get_width(source);
			int height = (int)obs_source_get_height(source);

			vec2 scale;
			obs_sceneitem_get_scale(item, &scale);

			obs_sceneitem_crop crop;
			obs_sceneitem_get_crop(item, &crop);

			sizeX->setValue(width);
			sizeY->setValue(height);

			boundsWidth->setValue((width - crop.left - crop.right) * scale.x);
			boundsHeight->setValue((height - crop.top - crop.bottom) * scale.y);
		} else if (type == OBS_BOUNDS_NONE) {
			OBSSource source = obs_sceneitem_get_source(item);
			int width = (int)obs_source_get_width(source);
			int height = (int)obs_source_get_height(source);

			matrix4 draw;
			obs_sceneitem_get_draw_transform(item, &draw);

			sizeX->setValue(width * draw.x.x);
			sizeY->setValue(height * draw.y.y);

			obs_transform_info oti;
			obs_sceneitem_get_info2(item, &oti);

			// We use the draw transform values here which is always a top left coordinate origin.
			vec2 currentRatio = getAlignmentConversion(OBS_ALIGN_TOP | OBS_ALIGN_LEFT);
			vec2 flipRatio = getAlignmentConversion(oti.alignment);

			float drawX = draw.t.x;
			float drawY = draw.t.y;

			obs_sceneitem_crop crop;
			obs_sceneitem_get_crop(item, &crop);

			uint32_t widthForFlip = width - crop.left - crop.right;
			uint32_t heightForFlip = height - crop.top - crop.bottom;

			float shiftX = (currentRatio.x - flipRatio.x) * (widthForFlip * draw.x.x);
			float shiftY = (currentRatio.y - flipRatio.y) * (heightForFlip * draw.y.y);

			positionX->setValue(oti.pos.x - (oti.pos.x - drawX) - shiftX);
			positionY->setValue(oti.pos.y - (oti.pos.y - drawY) - shiftY);
		}
	}

	onControlChanged();
}

void TransformDock::onControlChanged()
{
	if (ignoreItemChange)
		return;

	obs_source_t *source = obs_sceneitem_get_source(item);
	uint32_t source_cx = obs_source_get_width(source);
	uint32_t source_cy = obs_source_get_height(source);
	double width = double(source_cx);
	double height = double(source_cy);

	obs_transform_info oti;
	obs_sceneitem_get_info2(item, &oti);

	/* do not scale a source if it has 0 width/height */
	if (source_cx != 0 && source_cy != 0) {
		oti.scale.x = float(sizeX->value() / width);
		oti.scale.y = float(sizeY->value() / height);
	}

	oti.pos.x = float(positionX->value());
	oti.pos.y = float(positionY->value());
	oti.rot = float(rotation->value());
	oti.alignment = indexToAlign[positionAlignment->currentIndex()];

	oti.bounds_type = (obs_bounds_type)boundsType->currentIndex();
	oti.bounds_alignment = indexToAlign[boundsAlignment->currentIndex()];
	oti.bounds.x = float(boundsWidth->value());
	oti.bounds.y = float(boundsHeight->value());
	oti.crop_to_bounds = cropToBounds->isChecked();

	ignoreTransformSignal = true;
	obs_sceneitem_set_info2(item, &oti);
	ignoreTransformSignal = false;
}

void TransformDock::onCropChanged()
{
	if (ignoreItemChange)
		return;

	obs_sceneitem_crop crop;
	crop.left = uint32_t(cropLeft->value());
	crop.right = uint32_t(cropRight->value());
	crop.top = uint32_t(cropTop->value());
	crop.bottom = uint32_t(cropBottom->value());

	ignoreTransformSignal = true;
	obs_sceneitem_set_crop(item, &crop);
	ignoreTransformSignal = false;
}
