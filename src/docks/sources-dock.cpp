#include "../dialogs/name-dialog.hpp"
#include "canvas-dock.hpp"
#include "sources-dock.hpp"
#include <obs-module.h>
#include <QGuiApplication>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>

SourcesDock::SourcesDock(QWidget *parent) : QFrame(parent)
{
	setObjectName("AitumStreamSuiteSourcesDock");

	auto sourcesGroupLayout = new QVBoxLayout(this);
	setLayout(sourcesGroupLayout);

	sourceList = new SourceTree(
		&selectMutex, &hoveredPreviewItems,
		[](void *param) {
			auto s = obs_weak_source_get_source(((SourcesDock *)param)->scene);
			if (!s) {
				return (obs_scene_t *)nullptr;
			}
			auto scene = obs_scene_from_source(s);
			obs_source_release(s);
			return scene;
		},
		this, nullptr, this);

	sourceList->setContentsMargins(0, 0, 0, 0);
	sourceList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	sourceList->setFrameShape(QFrame::NoFrame);
	sourceList->setFrameShadow(QFrame::Plain);
	sourceList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	sourceList->setContextMenuPolicy(Qt::CustomContextMenu);

	sourceList->setDropIndicatorShown(true);
	sourceList->setDragEnabled(true);
	sourceList->setDragDropMode(QAbstractItemView::InternalMove);
	sourceList->setDefaultDropAction(Qt::TargetMoveAction);

	connect(sourceList, &SourceTree::customContextMenuRequested, [this] { ShowSourcesContextMenu(GetCurrentSceneItem()); });

	auto renameAction = new QAction(sourceList);
#ifdef __APPLE__
	renameAction->setShortcut({Qt::Key_Return});
#else
	renameAction->setShortcut({Qt::Key_F2});
#endif
	renameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameAction, &QAction::triggered, [this]() {
		obs_sceneitem_t *sceneItem = GetCurrentSceneItem();
		if (!sceneItem) {
			return;
		}
		obs_source_t *source = obs_source_get_ref(obs_sceneitem_get_source(sceneItem));
		if (!source) {
			return;
		}
		obs_canvas_t *canvas = obs_source_get_canvas(source);
		std::string name = obs_source_get_name(source);
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SourceName")), name)) {
				break;
			}

			s = canvas ? obs_canvas_get_source_by_name(canvas, name.c_str()) : obs_get_source_by_name(name.c_str());
			if (s) {
				continue;
			}
			obs_source_set_name(source, name.c_str());
		} while (s);
		obs_source_release(source);
	});
	sourceList->addAction(renameAction);

	sourcesGroupLayout->addWidget(sourceList);

	auto toolbar = new QToolBar();
	toolbar->setContentsMargins(0, 0, 0, 0);
	toolbar->setObjectName(QStringLiteral("sourcesToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);
	auto a = toolbar->addAction(QIcon(QString::fromUtf8(":/res/images/plus.svg")),
				    QString::fromUtf8(obs_frontend_get_locale_string("AddSource")), [this] {
					    const auto menu = CanvasDock::CreateAddSourcePopupMenu(this);
					    menu->exec(QCursor::pos());
				    });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-plus");

	a = toolbar->addAction(
		QIcon(":/res/images/minus.svg"), QString::fromUtf8(obs_frontend_get_locale_string("RemoveSource")), [this] {
			auto scene_source = obs_weak_source_get_source(scene);
			auto s = obs_scene_from_source(scene_source);
			obs_source_release(scene_source);
			std::vector<OBSSceneItem> items;
			obs_scene_enum_items(s, CanvasDock::selected_items, &items);
			if (!items.size()) {
				return;
			}
			/* ------------------------------------- */
			/* confirm action with user              */

			bool confirmed = false;

			if (items.size() > 1) {
				QString text = QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.TextMultiple"))
						       .arg(QString::number(items.size()));

				QMessageBox remove_items(this);
				remove_items.setText(text);
				QPushButton *Yes = remove_items.addButton(QString::fromUtf8(obs_frontend_get_locale_string("Yes")),
									  QMessageBox::YesRole);
				remove_items.setDefaultButton(Yes);
				remove_items.addButton(QString::fromUtf8(obs_frontend_get_locale_string("No")),
						       QMessageBox::NoRole);
				remove_items.setIcon(QMessageBox::Question);
				remove_items.setWindowTitle(
					QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Title")));
				remove_items.exec();

				confirmed = Yes == remove_items.clickedButton();
			} else {
				OBSSceneItem &item = items[0];
				obs_source_t *source = obs_sceneitem_get_source(item);
				if (source) {
					const char *name = obs_source_get_name(source);

					QString text = QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Text"))
							       .arg(QString::fromUtf8(name));

					QMessageBox remove_source(this);
					remove_source.setText(text);
					QPushButton *Yes = remove_source.addButton(
						QString::fromUtf8(obs_frontend_get_locale_string("Yes")), QMessageBox::YesRole);
					remove_source.setDefaultButton(Yes);
					remove_source.addButton(QString::fromUtf8(obs_frontend_get_locale_string("No")),
								QMessageBox::NoRole);
					remove_source.setIcon(QMessageBox::Question);
					remove_source.setWindowTitle(
						QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Title")));
					remove_source.exec();
					confirmed = Yes == remove_source.clickedButton();
				}
			}
			if (!confirmed) {
				return;
			}

			std::string undo_json = CanvasDock::backup_scene(s);

			/* ----------------------------------------------- */
			/* remove items                                    */

			for (auto &item : items) {
				obs_sceneitem_remove(item);
			}

			std::string redo_json = CanvasDock::backup_scene(s);

			QString action_name;
			if (items.size() > 1) {
				action_name = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Sources.Multi"))
						      .arg(QString::number(items.size()));
			} else {
				QString str = QString::fromUtf8(obs_frontend_get_locale_string("Undo.Delete"));
				action_name = str.arg(obs_source_get_name(obs_sceneitem_get_source(items[0])));
			}

			obs_frontend_add_undo_redo_action(action_name.toUtf8().constData(), CanvasDock::undo_redo_scene,
							  CanvasDock::undo_redo_scene, undo_json.c_str(), redo_json.c_str(), false);
		});
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("removeIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-minus");
	a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
#ifdef __APPLE__
	a->setShortcut({Qt::Key_Backspace});
#else
	a->setShortcut({Qt::Key_Delete});
#endif
	sourceList->addAction(a);
	toolbar->addSeparator();
	a = toolbar->addAction(QIcon(":/res/images/filter.svg"), QString::fromUtf8(obs_frontend_get_locale_string("SourceFilters")),
			       [this] {
				       auto item = GetCurrentSceneItem();
				       auto source = obs_sceneitem_get_source(item);
				       if (source) {
					       obs_frontend_open_source_filters(source);
				       }
			       });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("filtersIcon")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-filter");

	a = toolbar->addAction(QIcon(":/settings/images/settings/general.svg"),
			       QString::fromUtf8(obs_frontend_get_locale_string("SourceProperties")), [this] {
				       auto item = GetCurrentSceneItem();
				       auto source = obs_sceneitem_get_source(item);
				       if (source) {
					       obs_frontend_open_source_properties(source);
				       }
			       });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("propertiesIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-gear");

	toolbar->addSeparator();
	a = toolbar->addAction(
		QIcon(":/res/images/up.svg"), QString::fromUtf8(obs_frontend_get_locale_string("MoveSourceUp")), [this] {
			auto item = GetCurrentSceneItem();
			if (!item) {
				return;
			}
			auto scene = obs_sceneitem_get_scene(item);
			if (!scene) {
				return;
			}
			std::string undo_json = CanvasDock::backup_scene(scene);
			obs_sceneitem_set_order(item, OBS_ORDER_MOVE_UP);
			std::string redo_json = CanvasDock::backup_scene(scene);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.MoveUp"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item))),
						     QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(undoName.toUtf8().constData(), CanvasDock::undo_redo_scene,
							  CanvasDock::undo_redo_scene, undo_json.c_str(), redo_json.c_str(), false);
		});
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("upArrowIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-up");
	a = toolbar->addAction(
		QIcon(":/res/images/down.svg"), QString::fromUtf8(obs_frontend_get_locale_string("MoveSourceDown")), [this] {
			auto item = GetCurrentSceneItem();
			if (!item) {
				return;
			}
			auto scene = obs_sceneitem_get_scene(item);
			if (!scene) {
				return;
			}
			std::string undo_json = CanvasDock::backup_scene(scene);
			obs_sceneitem_set_order(item, OBS_ORDER_MOVE_DOWN);
			std::string redo_json = CanvasDock::backup_scene(scene);
			auto undoName = QString::fromUtf8(obs_frontend_get_locale_string("Undo.MoveDown"))
						.arg(QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(item))),
						     QString::fromUtf8(obs_source_get_name(obs_scene_get_source(scene))));
			obs_frontend_add_undo_redo_action(undoName.toUtf8().constData(), CanvasDock::undo_redo_scene,
							  CanvasDock::undo_redo_scene, undo_json.c_str(), redo_json.c_str(), false);
		});
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("downArrowIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-down");

	sourcesGroupLayout->addWidget(toolbar);
}

SourcesDock::~SourcesDock()
{
	obs_weak_source_release(scene);
}

obs_sceneitem_t *SourcesDock::GetCurrentSceneItem()
{
	return sourceList->Get(GetTopSelectedSourceItem());
}

int SourcesDock::GetTopSelectedSourceItem()
{
	QModelIndexList selectedItems = sourceList->selectionModel()->selectedIndexes();
	return selectedItems.isEmpty() ? -1 : selectedItems[0].row();
}

void SourcesDock::SetScene(obs_source_t *s)
{
	if (obs_weak_source_references_source(scene, s)) {
		return;
	}
	auto old_scene = obs_weak_source_get_source(scene);
	if (old_scene) {
		auto sh = obs_source_get_signal_handler(old_scene);
		if (sh) {
			signal_handler_disconnect(sh, "item_add", SceneItemAdded, this);
			signal_handler_disconnect(sh, "reorder", SceneReordered, this);
			signal_handler_disconnect(sh, "refresh", SceneRefreshed, this);
		}
		obs_source_release(old_scene);
	}
	obs_weak_source_release(scene);
	scene = obs_source_get_weak_source(s);
	auto sh = s ? obs_source_get_signal_handler(s) : nullptr;
	if (sh) {
		signal_handler_connect(sh, "item_add", SceneItemAdded, this);
		signal_handler_connect(sh, "reorder", SceneReordered, this);
		signal_handler_connect(sh, "refresh", SceneRefreshed, this);
	}
	sourceList->GetStm()->SceneChanged();
}

void SourcesDock::SceneItemAdded(void *data, calldata_t *params)
{
	SourcesDock *window = static_cast<SourcesDock *>(data);

	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "AddSceneItem", Qt::QueuedConnection, Q_ARG(OBSSceneItem, OBSSceneItem(item)));
}

void SourcesDock::SceneReordered(void *data, calldata_t *params)
{
	SourcesDock *window = static_cast<SourcesDock *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");

	QMetaObject::invokeMethod(window, "ReorderSources", Qt::QueuedConnection, Q_ARG(OBSScene, OBSScene(scene)));
}

void SourcesDock::SceneRefreshed(void *data, calldata_t *params)
{
	SourcesDock *window = static_cast<SourcesDock *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");

	QMetaObject::invokeMethod(window, "RefreshSources", Qt::QueuedConnection, Q_ARG(OBSScene, OBSScene(scene)));
}

void SourcesDock::AddSceneItem(OBSSceneItem item, bool no_select)
{
	obs_scene_t *add_scene = obs_sceneitem_get_scene(item);

	if (obs_weak_source_references_source(scene, obs_scene_get_source(add_scene))) {
		sourceList->Add(item);
	}

	if (no_select) {
		return;
	}

	obs_scene_enum_items(add_scene, CanvasDock::select_one, (obs_sceneitem_t *)item);
}

void SourcesDock::RefreshSources(OBSScene refresh_scene)
{
	if (!obs_weak_source_references_source(scene, obs_scene_get_source(refresh_scene)) || sourceList->IgnoreReorder()) {
		return;
	}

	sourceList->RefreshItems();
}

void SourcesDock::ReorderSources(OBSScene order_scene)
{
	if (!obs_weak_source_references_source(scene, obs_scene_get_source(order_scene)) || sourceList->IgnoreReorder()) {
		return;
	}

	sourceList->ReorderItems();
}

void SourcesDock::ShowSourcesContextMenu(obs_sceneitem_t *item)
{
	QMenu menu(this);
	menu.addMenu(CanvasDock::CreateAddSourcePopupMenu(this));
	auto s = obs_weak_source_get_source(scene);
	CanvasDock::AddCopyPasteMenuItems(&menu, item, obs_scene_from_source(s));
	obs_source_release(s);
	if (item) {
		CanvasDock::AddSceneItemMenuItems(&menu, item);
	}
	menu.exec(QCursor::pos());
}

void SourcesDock::AddSourceFromAction()
{
	QAction *a = qobject_cast<QAction *>(sender());
	if (!a) {
		return;
	}

	auto t = a->data().toString();
	auto idUtf8 = t.toUtf8();
	const char *id = idUtf8.constData();
	if (id && *id && strlen(id)) {
		const char *v_id = obs_get_latest_input_type_id(id);
		QString placeHolderText = QString::fromUtf8(obs_source_get_display_name(v_id));
		QString text = placeHolderText;
		int i = 2;
		OBSSourceAutoRelease s = nullptr;
		obs_source_t *created_source = nullptr;
		auto canvas_scene = obs_weak_source_get_source(scene);
		if (obs_get_source_output_flags(id) & OBS_SOURCE_REQUIRES_CANVAS) {
			auto c = canvas_scene ? obs_source_get_canvas(canvas_scene) : nullptr;
			if (c) {
				while ((s = obs_canvas_get_source_by_name(c, text.toUtf8().constData()))) {
					text = QString("%1 %2").arg(placeHolderText).arg(i++);
				}
				created_source = obs_scene_get_source(obs_canvas_scene_create(c, text.toUtf8().constData()));
				obs_canvas_release(c);
			}
		} else {
			while ((s = obs_get_source_by_name(text.toUtf8().constData()))) {
				text = QString("%1 %2").arg(placeHolderText).arg(i++);
			}
			created_source = obs_source_create(v_id, text.toUtf8().constData(), nullptr, nullptr);
		}
		obs_scene_add(obs_scene_from_source(canvas_scene), created_source);
		obs_source_release(canvas_scene);
		if (obs_source_configurable(created_source)) {
			obs_frontend_open_source_properties(created_source);
		}
		obs_source_release(created_source);
	}
}

void SourcesDock::AddSourceToScene(OBSSource source)
{
	auto s = obs_weak_source_get_source(scene);
	if (!s) {
		return;
	}
	obs_scene_add(obs_scene_from_source(s), source);
	obs_source_release(s);
}

void SourcesDock::OpenSourceProjector() {
	int monitor = sender()->property("monitor").toInt();
	if (monitor > 9 || monitor > QGuiApplication::screens().size() - 1) {
		return;
	}
	OBSSceneItem item = GetCurrentSceneItem();
	if (!item) {
		return;
	}

	obs_source_t *open_source = obs_sceneitem_get_source(item);
	if (!open_source) {
		return;
	}
	obs_frontend_open_projector("Source", monitor, nullptr, obs_source_get_name(open_source));
}
