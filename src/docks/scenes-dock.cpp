#include "../dialogs/name-dialog.hpp"
#include "canvas-dock.hpp"
#include "scenes-dock.hpp"
#include "sources-dock.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QApplication>
#include <QFocusEvent>
#include <QMenu>
#include <QMessageBox>
#include <QSpinBox>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <src/utils/obs-websocket-api.h>

extern obs_websocket_vendor vendor;
extern SourcesDock *sources_dock;

ScenesDock::ScenesDock(QWidget *parent) : QFrame(parent)
{
	setMinimumWidth(100);
	setMinimumHeight(50);

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	sceneList = new QListWidget();
	sceneList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	sceneList->setFrameShape(QFrame::NoFrame);
	sceneList->setFrameShadow(QFrame::Plain);
	sceneList->setSelectionMode(QAbstractItemView::SingleSelection);
	sceneList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(sceneList, &QListWidget::customContextMenuRequested,
		[this](const QPoint &pos) { ShowScenesContextMenu(sceneList->itemAt(pos)); });

	connect((QApplication *)QApplication::instance(), &QApplication::focusChanged, this, &ScenesDock::handleFocusChange);

	connect(sceneList, &QListWidget::currentItemChanged, [this]() {
		const auto item = sceneList->currentItem();
		if (!item) {
			return;
		}
		if (!item->isSelected()) {
			item->setSelected(true);
		}
		auto c = obs_weak_canvas_get_canvas(canvas);
		if (!c) {
			return;
		}
		auto scene = obs_get_source_by_uuid(item->data(Qt::UserRole).toString().toUtf8().constData());
		if (!scene) {
			scene = obs_canvas_get_source_by_name(c, item->text().toUtf8().constData());
		}
		auto current = obs_canvas_get_channel(c, 0);
		obs_source_t *parent = nullptr;
		while (obs_source_get_type(current) == OBS_SOURCE_TYPE_TRANSITION) {
			obs_source_release(parent);
			parent = current;
			current = obs_transition_get_active_source(parent);
		}
		if (parent && obs_source_get_type(parent) == OBS_SOURCE_TYPE_TRANSITION) {
			signal_handler_t *sh = obs_source_get_signal_handler(parent);
			signal_handler_disconnect(sh, "transition_start", transition_action, this);
			signal_handler_disconnect(sh, "transition_video_stop", transition_action, this);
			signal_handler_disconnect(sh, "transition_stop", transition_action, this);
			signal_handler_connect(sh, "transition_start", transition_action, this);
			signal_handler_connect(sh, "transition_video_stop", transition_action, this);
			signal_handler_connect(sh, "transition_stop", transition_action, this);
		}
		if (scene && scene != current) {
			auto mc = obs_get_main_canvas();
			if (c == mc) {
				obs_frontend_set_current_scene(scene);
			} else if (parent) {
				obs_transition_start(parent, OBS_TRANSITION_MODE_AUTO, 0, scene);
			} else {
				obs_canvas_set_channel(c, 0, scene);
			}
			obs_canvas_release(mc);
		}
		obs_source_release(current);
		obs_source_release(parent);

		obs_source_release(scene);
		obs_canvas_release(c);
	});
	connect(sceneList, &QListWidget::itemSelectionChanged, [this] {
		const auto item = sceneList->currentItem();
		if (!item) {
			return;
		}
		if (!item->isSelected()) {
			item->setSelected(true);
		}
	});

	mainLayout->addWidget(sceneList, 1);

	auto toolbar = new QToolBar();
	toolbar->setObjectName(QStringLiteral("scenesToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);
	auto a = toolbar->addAction(QIcon(QString::fromUtf8(":/res/images/plus.svg")),
				    QString::fromUtf8(obs_frontend_get_locale_string("Add")), [this] { AddScene(); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-plus");

	a = toolbar->addAction(QIcon(":/res/images/minus.svg"), QString::fromUtf8(obs_frontend_get_locale_string("RemoveScene")),
			       [this] { RemoveCurrentScene(); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("removeIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-minus");
	a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
#ifdef __APPLE__
	a->setShortcut({Qt::Key_Backspace});
#else
	a->setShortcut({Qt::Key_Delete});
#endif
	sceneList->addAction(a);

	toolbar->addSeparator();
	a = toolbar->addAction(QIcon(":/res/images/filter.svg"), QString::fromUtf8(obs_frontend_get_locale_string("SceneFilters")),
			       [this] {
				       auto item = sceneList->currentItem();
				       if (!item) {
					       return;
				       }
				       auto s = obs_get_source_by_uuid(item->data(Qt::UserRole).toString().toUtf8().constData());
				       if (!s) {
					       auto c = obs_weak_canvas_get_canvas(canvas);
					       if (!c) {
						       return;
					       }
					       s = obs_canvas_get_source_by_name(c, item->text().toUtf8().constData());
					       obs_canvas_release(c);
				       }
				       if (!s) {
					       return;
				       }
				       obs_frontend_open_source_filters(s);
				       obs_source_release(s);
			       });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("filtersIcon")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-filter");
	toolbar->addSeparator();
	a = toolbar->addAction(QIcon(":/res/images/up.svg"), QString::fromUtf8(obs_frontend_get_locale_string("MoveSceneUp")),
			       [this] { ChangeSceneIndex(true, -1, 0); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("upArrowIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-up");
	a = toolbar->addAction(QIcon(":/res/images/down.svg"), QString::fromUtf8(obs_frontend_get_locale_string("MoveSceneDown")),
			       [this] { ChangeSceneIndex(true, 1, sceneList->count() - 1); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("downArrowIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-down");
	mainLayout->addWidget(toolbar, 0);

	setObjectName(QStringLiteral("contextContainer"));
	setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	setLayout(mainLayout);
}

ScenesDock::~ScenesDock()
{
	obs_weak_canvas_release(canvas);
}

void ScenesDock::ChangeSceneIndex(bool relative, int offset, int invalidIdx)
{
	int idx = sceneList->currentRow();
	if (idx < 0) {
		return;
	}

	auto canvasItem = sceneList->item(idx);
	if (!canvasItem) {
		return;
	}

	if (idx == invalidIdx) {
		return;
	}

	sceneList->blockSignals(true);
	auto item = sceneList->takeItem(idx);
	if (relative) {
		sceneList->insertItem(idx + offset, item);
		sceneList->setCurrentRow(idx + offset);
	} else if (offset == 0) {
		sceneList->insertItem(offset, item);
	} else {
		sceneList->insertItem(sceneList->count(), item);
	}
	item->setSelected(true);
	sceneList->blockSignals(false);
}

void ScenesDock::handleFocusChange(QWidget *old, QWidget *now)
{
	UNUSED_PARAMETER(old);
	auto parent = now;
	while (parent != nullptr) {
		if (parent->objectName() == "AitumStreamSuiteMainCanvas") {
			auto canvas = obs_get_main_canvas();
			if (!canvas) {
				return;
			}
			SwitchToCanvas(canvas);
			obs_canvas_release(canvas);
			return;
		}
		auto canvas_dock = dynamic_cast<CanvasDock *>(parent);
		if (canvas_dock) {
			SwitchToCanvas(canvas_dock->GetCanvas());
			return;
		}

		parent = parent->parentWidget();
	}
}

void ScenesDock::SwitchToCanvas(obs_canvas_t *c)
{
	auto orig = obs_weak_canvas_get_canvas(canvas);
	if (orig == c) {
		obs_canvas_release(orig);
		return;
	}
	auto sh = orig ? obs_canvas_get_signal_handler(orig) : nullptr;
	if (sh) {
		signal_handler_disconnect(sh, "source_add", scene_add, this);
		signal_handler_disconnect(sh, "source_remove", scene_remove, this);
		signal_handler_disconnect(sh, "channel_change", channel_change, this);
	}
	obs_canvas_release(orig);
	canvas = obs_canvas_get_weak_canvas(c);
	sceneList->clear();
	if (!canvas) {
		return;
	}
	sh = obs_canvas_get_signal_handler(c);
	if (sh) {
		signal_handler_connect(sh, "source_add", scene_add, this);
		signal_handler_connect(sh, "source_remove", scene_remove, this);
		signal_handler_connect(sh, "channel_change", channel_change, this);
	}
	auto mc = obs_get_main_canvas();
	obs_canvas_release(mc);
	if (mc == c) {
		struct obs_frontend_source_list scenes = {0};
		obs_frontend_get_scenes(&scenes);
		for (size_t i = 0; i < scenes.sources.num; i++) {
			auto scene = scenes.sources.array[i];
			if (!scene) {
				continue;
			}
			auto sh = obs_source_get_signal_handler(scene);
			signal_handler_disconnect(sh, "rename", scene_rename, this);
			signal_handler_connect(sh, "rename", scene_rename, this);
			auto sn = QString::fromUtf8(obs_source_get_name(scene));
			auto sli = new QListWidgetItem(sn, sceneList);
			sli->setData(Qt::UserRole, QString::fromUtf8(obs_source_get_uuid(scene)));
			sceneList->addItem(sli);
		}
		obs_frontend_source_list_free(&scenes);
	} else {
		obs_canvas_enum_scenes(
			c,
			[](void *param, obs_source_t *scene) {
				auto sh = obs_source_get_signal_handler(scene);
				signal_handler_disconnect(sh, "rename", scene_rename, param);
				signal_handler_connect(sh, "rename", scene_rename, param);
				auto self = (ScenesDock *)param;
				auto sn = QString::fromUtf8(obs_source_get_name(scene));
				auto sli = new QListWidgetItem(sn, self->sceneList);
				sli->setData(Qt::UserRole, QString::fromUtf8(obs_source_get_uuid(scene)));
				self->sceneList->addItem(sli);
				return true;
			},
			this);
	}
	auto current_scene = obs_canvas_get_channel(c, 0);
	obs_source_t *parent = nullptr;
	while (current_scene && obs_source_get_type(current_scene) == OBS_SOURCE_TYPE_TRANSITION) {
		obs_source_release(parent);
		parent = current_scene;
		current_scene = obs_transition_get_active_source(parent);
	}
	if (parent && obs_source_get_type(parent) == OBS_SOURCE_TYPE_TRANSITION) {
		signal_handler_t *sh = obs_source_get_signal_handler(parent);
		signal_handler_disconnect(sh, "transition_start", transition_action, this);
		signal_handler_disconnect(sh, "transition_video_stop", transition_action, this);
		signal_handler_disconnect(sh, "transition_stop", transition_action, this);
		signal_handler_connect(sh, "transition_start", transition_action, this);
		signal_handler_connect(sh, "transition_video_stop", transition_action, this);
		signal_handler_connect(sh, "transition_stop", transition_action, this);
	}
	obs_source_release(parent);
	if (!current_scene) {
		return;
	}
	auto items = sceneList->findItems(QString::fromUtf8(obs_source_get_name(current_scene)), Qt::MatchExactly);
	obs_source_release(current_scene);
	if (items.isEmpty()) {
		return;
	}
	sceneList->setCurrentItem(items.first());
}

void ScenesDock::ShowScenesContextMenu(QListWidgetItem *widget_item)
{
	auto menu = QMenu(this);
	auto a = menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Main.GridMode")),
				[this](bool checked) { SetGridMode(checked); });
	a->setCheckable(true);
	a->setChecked(IsGridMode());
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Add")), [this] { AddScene(); });
	if (!widget_item) {
		menu.exec(QCursor::pos());
		return;
	}
	menu.addSeparator();
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Duplicate")), [this] {
		auto item = sceneList->currentItem();
		if (!item) {
			return;
		}
		AddScene(item->text());
	});
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Remove")), [this] { RemoveCurrentScene(); });
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Rename")), [this] {
		const auto item = sceneList->currentItem();
		if (!item) {
			return;
		}
		auto c = obs_weak_canvas_get_canvas(canvas);
		if (!c) {
			return;
		}

		std::string name = item->text().toUtf8().constData();
		std::string uuid = item->data(Qt::UserRole).toString().toUtf8().constData();
		obs_source_t *source = obs_get_source_by_uuid(uuid.c_str());
		if (!source) {
			source = obs_canvas_get_source_by_name(c, name.c_str());
		}
		if (!source) {
			obs_canvas_release(c);
			return;
		}
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SceneName")), name)) {
				break;
			}
			s = obs_canvas_get_source_by_name(c, name.c_str());
			if (!s) {
				s = obs_get_source_by_name(name.c_str());
			}
			if (s) {
				continue;
			}
			obs_source_set_name(source, name.c_str());
		} while (s);
		obs_source_release(source);
	});
	auto orderMenu = menu.addMenu(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order")));
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveUp")),
			     [this] { ChangeSceneIndex(true, -1, 0); });
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveDown")),
			     [this] { ChangeSceneIndex(true, 1, sceneList->count() - 1); });
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToTop")),
			     [this] { ChangeSceneIndex(false, 0, 0); });
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToBottom")),
			     [this] { ChangeSceneIndex(false, 1, sceneList->count() - 1); });

	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Screenshot.Scene")), [this] {
		auto item = sceneList->currentItem();
		if (!item) {
			return;
		}
		auto s = obs_get_source_by_uuid(item->data(Qt::UserRole).toString().toUtf8().constData());
		if (!s) {
			auto c = obs_weak_canvas_get_canvas(canvas);
			if (!c) {
				return;
			}
			s = obs_canvas_get_source_by_name(c, item->text().toUtf8().constData());
			obs_canvas_release(c);
		}
		if (s) {
			obs_frontend_take_source_screenshot(s);
			obs_source_release(s);
		}
	});
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Filters")), [this] {
		auto item = sceneList->currentItem();
		if (!item) {
			return;
		}
		auto s = obs_get_source_by_uuid(item->data(Qt::UserRole).toString().toUtf8().constData());
		if (!s) {
			auto c = obs_weak_canvas_get_canvas(canvas);
			if (!c) {
				return;
			}
			s = obs_canvas_get_source_by_name(c, item->text().toUtf8().constData());
			obs_canvas_release(c);
		}
		if (s) {
			obs_frontend_open_source_filters(s);
			obs_source_release(s);
		}
	});

	auto tom = menu.addMenu(QString::fromUtf8(obs_frontend_get_locale_string("TransitionOverride")));
	std::string scene_name = widget_item->text().toUtf8().constData();
	std::string scene_uuid = widget_item->data(Qt::UserRole).toString().toUtf8().constData();
	OBSSourceAutoRelease scene_source = obs_get_source_by_uuid(scene_uuid.c_str());
	if (!scene_source) {
		auto c = obs_weak_canvas_get_canvas(canvas);
		if (c) {
			scene_source = obs_canvas_get_source_by_name(c, scene_name.c_str());
			obs_canvas_release(c);
		}
	}
	OBSDataAutoRelease private_settings = obs_source_get_private_settings(scene_source);
	obs_data_set_default_int(private_settings, "transition_duration", 300);
	const char *curTransition = obs_data_get_string(private_settings, "transition");
	int curDuration = (int)obs_data_get_int(private_settings, "transition_duration");

	QSpinBox *duration = new QSpinBox(tom);
	duration->setMinimum(50);
	duration->setSuffix(" ms");
	duration->setMaximum(20000);
	duration->setSingleStep(50);
	duration->setValue(curDuration);

	connect(duration, (void (QSpinBox::*)(int))&QSpinBox::valueChanged, [this, scene_name, scene_uuid](int dur) {
		auto source = obs_get_source_by_uuid(scene_uuid.c_str());
		if (!source) {
			auto c = obs_weak_canvas_get_canvas(canvas);
			if (!c) {
				return;
			}
			source = obs_canvas_get_source_by_name(c, scene_name.c_str());
			obs_canvas_release(c);
		}
		OBSDataAutoRelease ps = obs_source_get_private_settings(source);
		obs_source_release(source);
		obs_data_set_int(ps, "transition_duration", dur);
	});

	auto action = tom->addAction(QString::fromUtf8(obs_frontend_get_locale_string("None")));
	action->setCheckable(true);
	action->setChecked(!curTransition || !strlen(curTransition));
	connect(action, &QAction::triggered, [this, scene_name, scene_uuid] {
		auto source = obs_get_source_by_uuid(scene_uuid.c_str());
		if (!source) {
			auto c = obs_weak_canvas_get_canvas(canvas);
			if (!c) {
				return;
			}
			source = obs_canvas_get_source_by_name(c, scene_name.c_str());
			obs_canvas_release(c);
		}
		OBSDataAutoRelease ps = obs_source_get_private_settings(source);
		obs_source_release(source);
		obs_data_set_string(ps, "transition", "");
	});

	/* for (auto t : transitions) {
		const char *name = obs_source_get_name(t);
		bool match = (name && curTransition && strcmp(name, curTransition) == 0);

		if (!name || !*name) {
			name = obs_frontend_get_locale_string("None");
		}

		auto a2 = tom->addAction(QString::fromUtf8(name));
		a2->setCheckable(true);
		a2->setChecked(match);
		connect(a, &QAction::triggered, [this, scene_name, a2] {
			OBSSourceAutoRelease source = obs_canvas_get_source_by_name(canvas, scene_name.c_str());
			OBSDataAutoRelease ps = obs_source_get_private_settings(source);
			obs_data_set_string(ps, "transition", a2->text().toUtf8().constData());
		});
	}*/

	QWidgetAction *durationAction = new QWidgetAction(tom);
	durationAction->setDefaultWidget(duration);

	tom->addSeparator();
	tom->addAction(durationAction);

	auto linkedScenesMenu = menu.addMenu(QString::fromUtf8(obs_module_text("LinkedScenes")));
	connect(linkedScenesMenu, &QMenu::aboutToShow, [linkedScenesMenu, this] {
		linkedScenesMenu->clear();
		struct obs_frontend_source_list scenes = {};
		obs_frontend_get_scenes(&scenes);
		for (size_t i = 0; i < scenes.sources.num; i++) {
			obs_source_t *src = scenes.sources.array[i];
			obs_data_t *settings = obs_source_get_settings(src);

			auto name = QString::fromUtf8(obs_source_get_name(src));
			auto *checkBox = new QCheckBox(name, linkedScenesMenu);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
			connect(checkBox, &QCheckBox::checkStateChanged, [this, src, checkBox] {
#else
				connect(checkBox, &QCheckBox::stateChanged, [this, src, checkBox] {
#endif
				//SetLinkedScene(src, checkBox->isChecked() ? sceneList->currentItem()->text() : "");
			});
			auto *checkableAction = new QWidgetAction(linkedScenesMenu);
			checkableAction->setDefaultWidget(checkBox);
			linkedScenesMenu->addAction(checkableAction);

			auto cc = obs_weak_canvas_get_canvas(canvas);
			auto c = obs_data_get_array(settings, "canvas");
			if (c) {
				const auto count = obs_data_array_count(c);

				for (size_t j = 0; j < count; j++) {
					auto item = obs_data_array_item(c, j);
					if (!item)
						continue;
					if (strcmp(obs_data_get_string(item, "name"), obs_canvas_get_name(cc)) == 0) {
						auto sn = QString::fromUtf8(obs_data_get_string(item, "scene"));
						if (sn == sceneList->currentItem()->text()) {
							checkBox->setChecked(true);
						}
					}
					obs_data_release(item);
				}

				obs_data_array_release(c);
			}
			obs_canvas_release(cc);

			obs_data_release(settings);
		}
		obs_frontend_source_list_free(&scenes);
	});

	/* menu.addAction(QString::fromUtf8(obs_module_text("OnMainCanvas")), [this] {
		auto item = sceneList->currentItem();
		if (!item) {
			return;
		}
		auto s = obs_canvas_get_source_by_name(canvas, item->text().toUtf8().constData());
		if (!s) {
			return;
		}

		if (obs_frontend_preview_program_mode_active()) {
			obs_frontend_set_current_preview_scene(s);
		} else {
			obs_frontend_set_current_scene(s);
		}
		obs_source_release(s);
	});*/

	a = menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("ShowInMultiview")),
			   [this, scene_name, scene_uuid](bool checked) {
				   auto source = obs_get_source_by_uuid(scene_uuid.c_str());
				   if (!source) {
					   auto c = obs_weak_canvas_get_canvas(canvas);
					   if (!c) {
						   return;
					   }
					   source = obs_canvas_get_source_by_name(c, scene_name.c_str());
					   obs_canvas_release(c);
				   }
				   OBSDataAutoRelease ps = obs_source_get_private_settings(source);
				   obs_source_release(source);
				   obs_data_set_bool(ps, "show_in_multiview", checked);
			   });
	a->setCheckable(true);
	obs_data_set_default_bool(private_settings, "show_in_multiview", true);
	a->setChecked(obs_data_get_bool(private_settings, "show_in_multiview"));
	menu.exec(QCursor::pos());
}

void ScenesDock::SetGridMode(bool checked)
{
	if (checked) {
		sceneList->setResizeMode(QListView::Adjust);
		sceneList->setViewMode(QListView::IconMode);
		sceneList->setUniformItemSizes(true);
		sceneList->setStyleSheet("*{padding: 0; margin: 0;}");
	} else {
		sceneList->setViewMode(QListView::ListMode);
		sceneList->setResizeMode(QListView::Fixed);
		sceneList->setStyleSheet("");
	}
}

bool ScenesDock::IsGridMode()
{
	return sceneList->viewMode() == QListView::IconMode;
}

void ScenesDock::AddScene(QString duplicate, bool ask_name)
{
	auto c = obs_weak_canvas_get_canvas(canvas);
	std::string name = duplicate.isEmpty() ? obs_module_text("Scene") : duplicate.toUtf8().constData();
	obs_source_t *s = obs_canvas_get_source_by_name(c, name.c_str());
	int i = 0;
	while (s) {
		obs_source_release(s);
		i++;
		name = obs_module_text("Scene");
		name += " ";
		name += std::to_string(i);
		s = obs_canvas_get_source_by_name(c, name.c_str());
	}
	do {
		obs_source_release(s);
		if (ask_name && !NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SceneName")), name)) {
			break;
		}
		s = obs_canvas_get_source_by_name(c, name.c_str());
		if (s) {
			continue;
		}

		obs_source_t *new_scene = nullptr;
		if (!duplicate.isEmpty()) {
			auto origSceneSource = obs_canvas_get_source_by_name(c, duplicate.toUtf8().constData());
			if (origSceneSource) {
				auto origScene = obs_scene_from_source(origSceneSource);
				if (origScene) {
					new_scene = obs_scene_get_source(
						obs_scene_duplicate(origScene, name.c_str(), OBS_SCENE_DUP_REFS));
				}
				obs_source_release(origSceneSource);
				if (new_scene) {
					obs_source_save(new_scene);
					obs_source_load(new_scene);
				}
			}
		}
		if (!new_scene) {
			obs_scene_t *ns = obs_canvas_scene_create(c, name.c_str());
			new_scene = obs_scene_get_source(ns);
			obs_source_load(new_scene);
			auto sh = obs_source_get_signal_handler(new_scene);
			signal_handler_connect(sh, "rename", scene_rename, this);
		}
		if (vendor) {
			const auto d = obs_data_create();
			obs_data_set_string(d, "canvas", obs_canvas_get_name(c));
			obs_data_set_string(d, "name", obs_source_get_name(new_scene));
			obs_data_set_string(d, "uuid", obs_source_get_uuid(new_scene));
			obs_websocket_vendor_emit_event(vendor, "scene_added", d);
			obs_data_release(d);
		}
		//auto sn = QString::fromUtf8(obs_source_get_name(new_scene));
		//SwitchScene(sn);
		obs_source_release(new_scene);
	} while (ask_name && s);
}

void ScenesDock::RemoveCurrentScene()
{
	auto item = sceneList->currentItem();
	if (!item) {
		return;
	}
	auto s = obs_get_source_by_uuid(item->data(Qt::UserRole).toString().toUtf8().constData());
	if (!s) {
		auto c = obs_weak_canvas_get_canvas(canvas);
		if (!c) {
			return;
		}
		s = obs_canvas_get_source_by_name(c, item->text().toUtf8().constData());
		obs_canvas_release(c);
	}
	if (!s) {
		return;
	}
	if (!obs_source_is_scene(s)) {
		obs_source_release(s);
		return;
	}

	QMessageBox mb(QMessageBox::Question, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Title")),
		       QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Text"))
			       .arg(QString::fromUtf8(obs_source_get_name(s))),
		       QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));
	mb.setDefaultButton(QMessageBox::NoButton);
	if (mb.exec() == QMessageBox::Yes) {
		auto c = obs_source_get_canvas(s);
		std::string canvas_name = c ? obs_canvas_get_name(c) : "";
		obs_canvas_release(c);
		obs_source_remove(s);
		if (vendor) {
			const auto d = obs_data_create();
			obs_data_set_string(d, "canvas", canvas_name.c_str());
			obs_data_set_string(d, "name", obs_source_get_name(s));
			obs_data_set_string(d, "uuid", obs_source_get_uuid(s));
			obs_websocket_vendor_emit_event(vendor, "scene_removed", d);
			obs_data_release(d);
		}
	}

	obs_source_release(s);
}

void ScenesDock::scene_add(void *data, calldata_t *cd)
{
	auto scene = (obs_source_t *)calldata_ptr(cd, "source");
	auto self = (ScenesDock *)data;
	auto sn = QString::fromUtf8(obs_source_get_name(scene));
	auto sli = new QListWidgetItem(sn, self->sceneList);
	sli->setData(Qt::UserRole, QString::fromUtf8(obs_source_get_uuid(scene)));
	self->sceneList->addItem(sli);
}

void ScenesDock::scene_remove(void *data, calldata_t *cd)
{
	auto scene = (obs_source_t *)calldata_ptr(cd, "source");
	auto self = (ScenesDock *)data;
	auto items = self->sceneList->findItems(QString::fromUtf8(obs_source_get_name(scene)), Qt::MatchExactly);
	if (items.isEmpty()) {
		return;
	}
	self->sceneList->takeItem(self->sceneList->row(items.first()));
}

void ScenesDock::scene_rename(void *data, calldata_t *cd)
{
	auto new_name = calldata_string(cd, "new_name");
	auto prev_name = calldata_string(cd, "prev_name");
	auto scene = (obs_source_t *)calldata_ptr(cd, "source");
	auto self = (ScenesDock *)data;
	auto items = self->sceneList->findItems(QString::fromUtf8(prev_name), Qt::MatchExactly);
	if (items.isEmpty()) {
		return;
	}
	auto item = items.first();
	if (item->data(Qt::UserRole).toString() != QString::fromUtf8(obs_source_get_uuid(scene))) {
		return;
	}
	item->setText(QString::fromUtf8(new_name));
}

void ScenesDock::channel_change(void *data, calldata_t *cd)
{
	auto self = (ScenesDock *)data;
	if (calldata_int(cd, "channel") != 0) {
		return;
	}
	QMetaObject::invokeMethod(self, [self] { self->UpdateCurrentScene(); }, Qt::QueuedConnection);	
}

void ScenesDock::UpdateCurrentScene()
{
	auto c = obs_weak_canvas_get_canvas(canvas);
	if (!c) {
		return;
	}
	auto current_scene = obs_canvas_get_channel(c, 0);
	obs_canvas_release(c);

	obs_source_t *parent = nullptr;
	while (current_scene && obs_source_get_type(current_scene) == OBS_SOURCE_TYPE_TRANSITION) {
		obs_source_release(parent);
		parent = current_scene;
		current_scene = obs_transition_get_active_source(parent);
	}
	if (parent && obs_source_get_type(parent) == OBS_SOURCE_TYPE_TRANSITION) {
		signal_handler_t *sh = obs_source_get_signal_handler(parent);
		signal_handler_disconnect(sh, "transition_start", transition_action, this);
		signal_handler_disconnect(sh, "transition_video_stop", transition_action, this);
		signal_handler_disconnect(sh, "transition_stop", transition_action, this);
		signal_handler_connect(sh, "transition_start", transition_action, this);
		signal_handler_connect(sh, "transition_video_stop", transition_action, this);
		signal_handler_connect(sh, "transition_stop", transition_action, this);
	}
	obs_source_release(parent);
	if (!current_scene) {
		return;
	}
	sources_dock->SetScene(current_scene);
	auto items = sceneList->findItems(QString::fromUtf8(obs_source_get_name(current_scene)), Qt::MatchExactly);
	obs_source_release(current_scene);
	if (items.isEmpty()) {
		return;
	}
	if (sceneList->currentItem() == items.first()) {
		return;
	}
	sceneList->setCurrentItem(items.first());
}

void ScenesDock::transition_action(void *data, calldata_t *cd) {
	UNUSED_PARAMETER(cd);
	auto self = (ScenesDock *)data;
	QMetaObject::invokeMethod(self, [self] { self->UpdateCurrentScene(); }, Qt::QueuedConnection);
}
