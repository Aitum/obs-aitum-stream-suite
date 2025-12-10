#include "../dialogs/name-dialog.hpp"
#include "../utils/color.hpp"
#include "../utils/icon.hpp"
#include "../utils/widgets/source-tree.hpp"
#include "../utils/widgets/switching-splitter.hpp"
#include "canvas-dock.hpp"
#include <QComboBox>
#include <QDockWidget>
#include <QGroupBox>
#include <QGuiApplication>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSpinBox>
#include <QSplitter>
#include <QToolBar>
#include <QWidgetAction>

#include <graphics/matrix4.h>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/dstr.h>
#include <util/platform.h>
#include <src/utils/obs-websocket-api.h>

#define HANDLE_RADIUS 4.0f
#define HELPER_ROT_BREAKPONT 45.0f
#define SPACER_LABEL_MARGIN 6.0f

std::list<CanvasDock *> canvas_docks;

extern QTabBar *modesTabBar;

CanvasDock::CanvasDock(obs_data_t *settings_, QWidget *parent)
	: QFrame(parent),
	  preview(new OBSQTDisplay(this)),
	  settings(settings_),
	  eventFilter(BuildEventFilter())
{
	obs_enter_graphics();

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(1.0f, 1.0f);
	box = gs_render_save();

	obs_leave_graphics();

	auto c = color_from_int(obs_data_get_int(settings, "color"));
	setStyleSheet(QString::fromUtf8("#contextContainer { border: 2px solid %1}").arg(c.name(QColor::HexRgb)));

	setObjectName(QStringLiteral("contextContainer"));
	setContentsMargins(0, 0, 0, 0);

	canvas_split = new SwitchingSplitter;
	canvas_split->setContentsMargins(0, 0, 0, 0);
	auto l = new QBoxLayout(QBoxLayout::TopToBottom, this);
	l->setContentsMargins(0, 0, 0, 0);
	setLayout(l);
	l->addWidget(canvas_split);
	canvas_split->setOrientation(Qt::Vertical);

	canvas_width = (uint32_t)obs_data_get_int(settings, "width");
	if (canvas_width < 1)
		canvas_width = 1080;
	canvas_height = (uint32_t)obs_data_get_int(settings, "height");
	if (canvas_height < 1)
		canvas_height = 1920;

	canvas_name = obs_data_get_string(settings, "name");
	if (canvas_name.empty())
		canvas_name = "Vertical";

	canvas = obs_get_canvas_by_uuid(obs_data_get_string(settings, "uuid"));
	if (canvas) {
		if (obs_canvas_removed(canvas)) {
			obs_canvas_release(canvas);
			canvas = nullptr;
		} else if (obs_canvas_get_flags(canvas) != PROGRAM) {
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
		} else if (canvas && obs_canvas_get_flags(canvas) != PROGRAM) {
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
			blog(LOG_INFO, "[Aitum Stream Suite] Canvas '%s' reset video %ux%u", obs_canvas_get_name(canvas),
			     canvas_width, canvas_height);
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
		canvas = obs_frontend_add_canvas(canvas_name.c_str(), &ovi, PROGRAM);
		blog(LOG_INFO, "[Aitum Stream Suite] Add frontend canvas '%s' %ux%u", canvas_name.c_str(), canvas_width,
		     canvas_height);
		if (canvas) {
			obs_data_set_string(settings, "uuid", obs_canvas_get_uuid(canvas));
			obs_data_set_int(settings, "width", canvas_width);
			obs_data_set_int(settings, "height", canvas_height);
		}
	}
	auto canvas_preview = new QWidget;
	auto cwl = new QVBoxLayout;
	canvas_preview->setLayout(cwl);
	cwl->addWidget(preview);

	//setLayout(mainLayout);

	preview->setObjectName(QStringLiteral("preview"));
	preview->setMinimumSize(QSize(24, 24));
	QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
	sizePolicy1.setHorizontalStretch(0);
	sizePolicy1.setVerticalStretch(0);
	sizePolicy1.setHeightForWidth(preview->sizePolicy().hasHeightForWidth());
	preview->setSizePolicy(sizePolicy1);

	preview->setMouseTracking(true);
	preview->setFocusPolicy(Qt::StrongFocus);
	preview->installEventFilter(eventFilter.get());

	preview->show();
	connect(preview, &OBSQTDisplay::DisplayCreated,
		[this]() { obs_display_add_draw_callback(preview->GetDisplay(), DrawPreview, this); });

	previewDisabledWidget = new QFrame;
	auto lv = new QVBoxLayout;

	auto enablePreviewButton =
		new QPushButton(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Main.PreviewConextMenu.Enable")));
	connect(enablePreviewButton, &QPushButton::clicked, [this] {
		preview_disabled = false;
		obs_display_set_enabled(preview->GetDisplay(), true);
		preview->setVisible(true);
		previewDisabledWidget->setVisible(false);
	});
	lv->addWidget(enablePreviewButton);

	previewDisabledWidget->setLayout(lv);

	previewDisabledWidget->setVisible(preview_disabled);
	cwl->addWidget(previewDisabledWidget);
	canvas_split->addWidget(canvas_preview);

	//obs_display_set_enabled(preview->GetDisplay(), !preview_disabled);

	panel_split = new SwitchingSplitter;
	panel_split->setContentsMargins(0, 0, 0, 0);
	auto scenesGroup = new QGroupBox(QString::fromUtf8(obs_module_text("Scenes")));
	scenesGroup->setContentsMargins(0, 0, 0, 0);
	auto scenesGroupLayout = new QVBoxLayout();
	scenesGroupLayout->setContentsMargins(0, 0, 0, 0);
	scenesGroupLayout->setSpacing(0);
	scenesGroup->setLayout(scenesGroupLayout);
	sceneList = new QListWidget();
	//sceneList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	sceneList->setFrameShape(QFrame::NoFrame);
	sceneList->setFrameShadow(QFrame::Plain);
	sceneList->setSelectionMode(QAbstractItemView::SingleSelection);
	sceneList->setViewMode(QListView::ListMode);
	//sceneList->setResizeMode(QListView::Fixed);
	sceneList->setSelectionMode(QAbstractItemView::SingleSelection);
	sceneList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(sceneList, &QListWidget::customContextMenuRequested,
		[this](const QPoint &pos) { ShowScenesContextMenu(sceneList->itemAt(pos)); });

	connect(sceneList, &QListWidget::currentItemChanged, [this]() {
		const auto item = sceneList->currentItem();
		if (!item)
			return;
		SwitchScene(item->text());
		if (!item->isSelected())
			item->setSelected(true);
	});
	connect(sceneList, &QListWidget::itemSelectionChanged, [this] {
		const auto item = sceneList->currentItem();
		if (!item)
			return;
		if (!item->isSelected())
			item->setSelected(true);
	});

	QAction *renameAction = new QAction(sceneList);
#ifdef __APPLE__
	renameAction->setShortcut({Qt::Key_Return});
#else
	renameAction->setShortcut({Qt::Key_F2});
#endif
	renameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameAction, &QAction::triggered, [this]() {
		const auto item = sceneList->currentItem();
		if (!item)
			return;
		obs_source_t *source = obs_canvas_get_source_by_name(canvas, item->text().toUtf8().constData());
		if (!source)
			return;
		std::string name = obs_source_get_name(source);
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SceneName")), name)) {
				break;
			}
			s = obs_canvas_get_source_by_name(canvas, name.c_str());
			if (s)
				continue;
			obs_source_set_name(source, name.c_str());
		} while (s);
		obs_source_release(source);
	});
	sceneList->addAction(renameAction);

	scenesGroupLayout->addWidget(sceneList);

	auto toolbar = new QToolBar();
	toolbar->setObjectName(QStringLiteral("scenesToolbar"));
	toolbar->setContentsMargins(0, 0, 0, 0);
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);
	auto a = toolbar->addAction(QIcon(QString::fromUtf8(":/res/images/plus.svg")),
				    QString::fromUtf8(obs_frontend_get_locale_string("Add")), [this] { AddScene(); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-plus");

	a = toolbar->addAction(QIcon(":/res/images/minus.svg"), QString::fromUtf8(obs_frontend_get_locale_string("RemoveScene")),
			       [this] {
				       auto item = sceneList->currentItem();
				       if (!item)
					       return;
				       RemoveScene(item->text());
			       });
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
				       if (!item)
					       return;
				       auto s = obs_canvas_get_source_by_name(canvas, item->text().toUtf8().constData());
				       if (!s)
					       return;
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
	scenesGroupLayout->addWidget(toolbar);

	panel_split->addWidget(scenesGroup);

	auto sourcesGroup = new QGroupBox(QString::fromUtf8(obs_module_text("Sources")));
	sourcesGroup->setContentsMargins(0, 0, 0, 0);
	auto sourcesGroupLayout = new QVBoxLayout();
	sourcesGroupLayout->setContentsMargins(0, 0, 0, 0);
	sourcesGroup->setLayout(sourcesGroupLayout);
	sourceList = new SourceTree(
		&selectMutex, &hoveredPreviewItems, [](void *param) { return ((CanvasDock *)param)->scene; }, this, this, this);
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

	renameAction = new QAction(sourceList);
#ifdef __APPLE__
	renameAction->setShortcut({Qt::Key_Return});
#else
	renameAction->setShortcut({Qt::Key_F2});
#endif
	renameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameAction, &QAction::triggered, [this]() {
		obs_sceneitem_t *sceneItem = GetCurrentSceneItem();
		if (!sceneItem)
			return;
		obs_source_t *source = obs_source_get_ref(obs_sceneitem_get_source(sceneItem));
		if (!source)
			return;
		obs_canvas_t *canvas = obs_source_get_canvas(source);
		std::string name = obs_source_get_name(source);
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SourceName")), name)) {
				break;
			}

			s = canvas ? obs_canvas_get_source_by_name(canvas, name.c_str()) : obs_get_source_by_name(name.c_str());
			if (s)
				continue;
			obs_source_set_name(source, name.c_str());
		} while (s);
		obs_source_release(source);
	});
	sourceList->addAction(renameAction);

	sourcesGroupLayout->addWidget(sourceList);

	toolbar = new QToolBar();
	toolbar->setContentsMargins(0, 0, 0, 0);
	toolbar->setObjectName(QStringLiteral("scenesToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);
	a = toolbar->addAction(QIcon(QString::fromUtf8(":/res/images/plus.svg")),
			       QString::fromUtf8(obs_frontend_get_locale_string("AddSource")), [this] {
				       const auto menu = CreateAddSourcePopupMenu();
				       menu->exec(QCursor::pos());
			       });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-plus");

	a = toolbar->addAction(
		QIcon(":/res/images/minus.svg"), QString::fromUtf8(obs_frontend_get_locale_string("RemoveSource")), [this] {
			std::vector<OBSSceneItem> items;
			obs_scene_enum_items(scene, selected_items, &items);
			if (!items.size())
				return;
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
			if (!confirmed)
				return;

			/* ----------------------------------------------- */
			/* remove items                                    */

			for (auto &item : items)
				obs_sceneitem_remove(item);
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
				       if (source)
					       obs_frontend_open_source_filters(source);
			       });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("filtersIcon")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-filter");

	a = toolbar->addAction(QIcon(":/settings/images/settings/general.svg"),
			       QString::fromUtf8(obs_frontend_get_locale_string("SourceProperties")), [this] {
				       auto item = GetCurrentSceneItem();
				       auto source = obs_sceneitem_get_source(item);
				       if (source)
					       obs_frontend_open_source_properties(source);
			       });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("propertiesIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-gear");

	toolbar->addSeparator();
	a = toolbar->addAction(QIcon(":/res/images/up.svg"), QString::fromUtf8(obs_frontend_get_locale_string("MoveSourceUp")),
			       [this] {
				       auto item = GetCurrentSceneItem();
				       obs_sceneitem_set_order(item, OBS_ORDER_MOVE_UP);
			       });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("upArrowIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-up");
	a = toolbar->addAction(QIcon(":/res/images/down.svg"), QString::fromUtf8(obs_frontend_get_locale_string("MoveSourceDown")),
			       [this] {
				       auto item = GetCurrentSceneItem();
				       obs_sceneitem_set_order(item, OBS_ORDER_MOVE_DOWN);
			       });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("downArrowIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-down");

	sourcesGroupLayout->addWidget(toolbar);

	panel_split->addWidget(sourcesGroup);

	auto transitionsGroup = new QGroupBox(QString::fromUtf8(obs_frontend_get_locale_string("Basic.SceneTransitions")));
	transitionsGroup->setContentsMargins(0, 0, 0, 0);
	auto transitionsGroupLayout = new QVBoxLayout();
	transitionsGroupLayout->setContentsMargins(0, 0, 0, 0);
	transitionsGroupLayout->setSpacing(0);
	transitionsGroup->setLayout(transitionsGroupLayout);

	transition = new QComboBox();
	transitionsGroupLayout->addWidget(transition);
	auto hl = new QHBoxLayout();
	hl->addStretch();
	auto addButton = new QPushButton();
	addButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	addButton->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.AddTransition")));
	addButton->setToolTip(QString::fromUtf8(obs_frontend_get_locale_string("Basic.AddTransition")));
	addButton->setIcon(QIcon(":/res/images/add.png"));
	addButton->setProperty("themeID", "addIconSmall");
	addButton->setProperty("class", "icon-plus");
	addButton->setProperty("toolButton", true);
	addButton->setFlat(false);

	connect(addButton, &QPushButton::clicked, [this] {
		auto menu = QMenu(this);
		auto subMenu = menu.addMenu(QString::fromUtf8(obs_module_text("CopyFromMain")));
		struct obs_frontend_source_list frontend_transitions = {};
		obs_frontend_get_transitions(&frontend_transitions);
		for (size_t i = 0; i < frontend_transitions.sources.num; i++) {
			auto tr = frontend_transitions.sources.array[i];
			const char *name = obs_source_get_name(tr);
			auto action = subMenu->addAction(QString::fromUtf8(name));
			if (!obs_is_source_configurable(obs_source_get_unversioned_id(tr))) {
				action->setEnabled(false);
			}
			for (auto t : transitions) {
				if (strcmp(name, obs_source_get_name(t)) == 0) {
					action->setEnabled(false);
					break;
				}
			}
			connect(action, &QAction::triggered, [this, tr] {
				OBSDataAutoRelease d = obs_save_source(tr);
				OBSSourceAutoRelease t = obs_load_private_source(d);
				if (t) {
					transitions.emplace_back(t);
					auto n = QString::fromUtf8(obs_source_get_name(t));
					transition->addItem(n);
					transition->setCurrentText(n);
				}
			});
		}
		obs_frontend_source_list_free(&frontend_transitions);
		menu.addSeparator();
		size_t idx = 0;
		const char *id;
		while (obs_enum_transition_types(idx++, &id)) {
			if (!obs_is_source_configurable(id))
				continue;
			const char *display_name = obs_source_get_display_name(id);

			auto action = menu.addAction(QString::fromUtf8(display_name));
			connect(action, &QAction::triggered, [this, id] {
				OBSSourceAutoRelease t = obs_source_create_private(id, obs_source_get_display_name(id), nullptr);
				if (t) {
					std::string name = obs_source_get_name(t);
					while (true) {
						if (!NameDialog::AskForName(
							    this, QString::fromUtf8(obs_module_text("TransitionName")), name)) {
							obs_source_release(t);
							return;
						}
						if (name.empty())
							continue;
						bool found = false;
						for (auto tr : transitions) {
							if (strcmp(obs_source_get_name(tr), name.c_str()) == 0) {
								found = true;
								break;
							}
						}
						if (found)
							continue;

						obs_source_set_name(t, name.c_str());
						break;
					}
					transitions.emplace_back(t);
					auto n = QString::fromUtf8(obs_source_get_name(t));
					transition->addItem(n);
					transition->setCurrentText(n);
					obs_frontend_open_source_properties(t);
				}
			});
		}
		menu.exec(QCursor::pos());
	});

	hl->addWidget(addButton);

	auto removeButton = new QPushButton();
	removeButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	removeButton->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.RemoveTransition")));
	removeButton->setToolTip(QString::fromUtf8(obs_frontend_get_locale_string("Basic.RemoveTransition")));
	removeButton->setIcon(QIcon(":/res/images/list_remove.png"));
	removeButton->setProperty("themeID", "removeIconSmall");
	removeButton->setProperty("class", "icon-minus");
	removeButton->setProperty("toolButton", true);
	removeButton->setFlat(false);

	connect(removeButton, &QPushButton::clicked, [this] {
		QMessageBox mb(
			QMessageBox::Question, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Title")),
			QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Text")).arg(transition->currentText()),
			QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));
		mb.setDefaultButton(QMessageBox::NoButton);
		if (mb.exec() != QMessageBox::Yes)
			return;

		auto n = transition->currentText().toUtf8();
		for (auto it = transitions.begin(); it != transitions.end(); ++it) {
			if (strcmp(n.constData(), obs_source_get_name(it->Get())) == 0) {
				if (!obs_is_source_configurable(obs_source_get_unversioned_id(it->Get())))
					return;
				transitions.erase(it);
				break;
			}
		}
		transition->removeItem(transition->currentIndex());
		if (transition->currentIndex() < 0)
			transition->setCurrentIndex(0);
	});

	hl->addWidget(removeButton);

	auto propsButton = new QPushButton();
	propsButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	propsButton->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransitionProperties")));
	propsButton->setToolTip(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransitionProperties")));
	propsButton->setIcon(QIcon(":/settings/images/settings/general.svg"));
	propsButton->setProperty("themeID", "menuIconSmall");
	propsButton->setProperty("class", "icon-dots-vert");
	propsButton->setProperty("toolButton", true);
	propsButton->setFlat(false);

	connect(propsButton, &QPushButton::clicked, [this] {
		auto menu = QMenu(this);
		auto action = menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Rename")));
		connect(action, &QAction::triggered, [this] {
			auto tn = transition->currentText().toUtf8();
			obs_source_t *t = GetTransition(tn.constData());
			if (!t)
				return;
			std::string name = obs_source_get_name(t);
			while (true) {
				if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("TransitionName")), name)) {
					return;
				}
				if (name.empty())
					continue;
				bool found = false;
				for (auto tr : transitions) {
					if (strcmp(obs_source_get_name(tr), name.c_str()) == 0) {
						found = true;
						break;
					}
				}
				if (found)
					continue;

				transition->setItemText(transition->currentIndex(), QString::fromUtf8(name.c_str()));
				obs_source_set_name(t, name.c_str());
				break;
			}
		});
		action = menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Properties")));
		connect(action, &QAction::triggered, [this] {
			auto tn = transition->currentText().toUtf8();
			auto t = GetTransition(tn.constData());
			if (!t)
				return;
			obs_frontend_open_source_properties(t);
		});
		menu.exec(QCursor::pos());
	});

	hl->addWidget(propsButton);

	transitionsGroupLayout->addLayout(hl);
	transitionsGroupLayout->addStretch();

	panel_split->addWidget(transitionsGroup);
	panel_split->setCollapsible(0, false);

	canvas_split->addWidget(panel_split);

	auto index = -1;
	if (modesTabBar)
		index = modesTabBar->currentIndex();
	LoadMode(index);

	LoadScenes();
	LogScenes();

	LoadTransitions();

	connect(transition, &QComboBox::currentTextChanged, [this, removeButton, propsButton] {
		auto tn = transition->currentText().toUtf8();
		auto t = GetTransition(tn.constData());
		if (!t)
			return;
		SwapTransition(t);
		bool config = obs_is_source_configurable(obs_source_get_unversioned_id(t));
		removeButton->setEnabled(config);
		propsButton->setEnabled(config);
	});

	connect(canvas_split, &SwitchingSplitter::splitterMoved, this, &CanvasDock::SaveSettings);
	connect(panel_split, &SwitchingSplitter::splitterMoved, this, &CanvasDock::SaveSettings);

	obs_frontend_add_save_callback(save_load, this);
}

void CanvasDock::GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY, int &x, int &y, float &scale)
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

void CanvasDock::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	CanvasDock *window = static_cast<CanvasDock *>(data);
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

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);
	if (window->previewScale != scale)
		window->previewScale = scale;
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

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	if (window->scene && !window->locked) {
		gs_matrix_push();
		gs_matrix_scale3f(scale, scale, 1.0f);
		obs_scene_enum_items(window->scene, DrawSelectedItem, data);
		gs_matrix_pop();
	}

	if (window->selectionBox) {
		if (!window->rectFill) {
			gs_render_start(true);

			gs_vertex2f(0.0f, 0.0f);
			gs_vertex2f(1.0f, 0.0f);
			gs_vertex2f(0.0f, 1.0f);
			gs_vertex2f(1.0f, 1.0f);

			window->rectFill = gs_render_save();
		}

		window->DrawSelectionBox(window->startPos.x * scale, window->startPos.y * scale, window->mousePos.x * scale,
					 window->mousePos.y * scale, window->rectFill);
	}
	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);

	if (window->drawSpacingHelpers)
		window->DrawSpacingHelpers(window->scene, (float)x, (float)y, newCX, newCY, scale, float(sourceCX),
					   float(sourceCY));

	gs_projection_pop();
	gs_viewport_pop();
}

CanvasDock::~CanvasDock()
{
	for (auto projector : projectors) {
		delete projector;
	}
	obs_frontend_remove_save_callback(save_load, this);
	canvas_docks.remove(this);
	obs_display_remove_draw_callback(preview->GetDisplay(), DrawPreview, this);
	SaveSettings(true);
	obs_data_release(settings);
	obs_enter_graphics();
	gs_vertexbuffer_destroy(box);
	obs_leave_graphics();
	obs_canvas_release(canvas);

	obs_source_t *oldTransition = obs_weak_source_get_source(source);
	if (oldTransition && obs_source_get_type(oldTransition) == OBS_SOURCE_TYPE_TRANSITION) {
		obs_weak_source_release(source);
		source = nullptr;
		signal_handler_t *handler = obs_source_get_signal_handler(oldTransition);
		signal_handler_disconnect(handler, "transition_stop", transition_override_stop, this);
		obs_source_dec_showing(oldTransition);
		obs_source_dec_active(oldTransition);
	}
	obs_source_release(oldTransition);

	transitions.clear();
}

void CanvasDock::SaveSettings(bool closing)
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
		state = panel_split->saveState();
		b64 = state.toBase64();
		state_chars = b64.constData();
		if (index == 0) {
			obs_data_set_string(settings, "panel_split_live", state_chars);
		} else if (index == 1) {
			obs_data_set_string(settings, "panel_split_build", state_chars);
		} else if (index == 2) {
			obs_data_set_string(settings, "panel_split_design", state_chars);
		}
		obs_data_set_string(settings, "panel_split", state_chars);
	}

	obs_data_array_t *transition_array = obs_data_array_create();
	for (auto transition : transitions) {
		const char *id = obs_source_get_unversioned_id(transition);
		if (!obs_is_source_configurable(id))
			continue;
		obs_data_t *transition_data = obs_save_source(transition);
		if (!transition_data)
			continue;
		obs_data_array_push_back(transition_array, transition_data);
		obs_data_release(transition_data);
	}
	obs_data_set_array(settings, "transitions", transition_array);
	obs_data_array_release(transition_array);

	obs_data_set_string(settings, "transition", transition->currentText().toUtf8().constData());
}

void CanvasDock::DrawBackdrop(float cx, float cy)
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

void CanvasDock::DrawSpacingHelpers(obs_scene_t *s, float x, float y, float cx, float cy, float scale, float sourceX, float sourceY)
{
	UNUSED_PARAMETER(x);
	UNUSED_PARAMETER(y);
	if (locked)
		return;

	OBSSceneItem item = GetSelectedItem();
	if (!item)
		return;

	if (obs_sceneitem_locked(item))
		return;

	vec2 itemSize = GetItemSize(item);
	if (itemSize.x == 0.0f || itemSize.y == 0.0f)
		return;

	obs_sceneitem_t *parentGroup = obs_sceneitem_get_group(s, item);

	if (parentGroup && obs_sceneitem_locked(parentGroup))
		return;

	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3 size;
	vec3_set(&size, sourceX, sourceY, 1.0f);

	// Init box transform side locations
	vec3 left, right, top, bottom;

	vec3_set(&left, 0.0f, 0.5f, 1.0f);
	vec3_set(&right, 1.0f, 0.5f, 1.0f);
	vec3_set(&top, 0.5f, 0.0f, 1.0f);
	vec3_set(&bottom, 0.5f, 1.0f, 1.0f);

	// Decide which side to use with box transform, based on rotation
	// Seems hacky, probably a better way to do it
	float rot = obs_sceneitem_get_rot(item);

	if (parentGroup) {

		//Correct the scene item rotation angle
		rot += obs_sceneitem_get_rot(parentGroup);

		vec2 group_scale;
		obs_sceneitem_get_scale(parentGroup, &group_scale);

		vec2 group_pos;
		obs_sceneitem_get_pos(parentGroup, &group_pos);

		// Correct the scene item box transform
		// Based on scale, rotation angle, position of parent's group
		matrix4_scale3f(&boxTransform, &boxTransform, group_scale.x, group_scale.y, 1.0f);
		matrix4_rotate_aa4f(&boxTransform, &boxTransform, 0.0f, 0.0f, 1.0f, RAD(obs_sceneitem_get_rot(parentGroup)));
		matrix4_translate3f(&boxTransform, &boxTransform, group_pos.x, group_pos.y, 0.0f);
	}

	if (rot >= HELPER_ROT_BREAKPONT) {
		for (float i = HELPER_ROT_BREAKPONT; i <= 360.0f; i += 90.0f) {
			if (rot < i)
				break;

			vec3 l = left;
			vec3 r = right;
			vec3 t = top;
			vec3 b = bottom;

			vec3_copy(&top, &l);
			vec3_copy(&right, &t);
			vec3_copy(&bottom, &r);
			vec3_copy(&left, &b);
		}
	} else if (rot <= -HELPER_ROT_BREAKPONT) {
		for (float i = -HELPER_ROT_BREAKPONT; i >= -360.0f; i -= 90.0f) {
			if (rot > i)
				break;

			vec3 l = left;
			vec3 r = right;
			vec3 t = top;
			vec3 b = bottom;

			vec3_copy(&top, &r);
			vec3_copy(&right, &b);
			vec3_copy(&bottom, &l);
			vec3_copy(&left, &t);
		}
	}
	vec2 item_scale;
	obs_sceneitem_get_scale(item, &item_scale);

	// Switch top/bottom or right/left if scale is negative
	if (item_scale.x < 0.0f) {
		vec3 l = left;
		vec3 r = right;

		vec3_copy(&left, &r);
		vec3_copy(&right, &l);
	}

	if (item_scale.y < 0.0f) {
		vec3 t = top;
		vec3 b = bottom;

		vec3_copy(&top, &b);
		vec3_copy(&bottom, &t);
	}

	// Get sides of box transform
	left = GetTransformedPos(left.x, left.y, boxTransform);
	right = GetTransformedPos(right.x, right.y, boxTransform);
	top = GetTransformedPos(top.x, top.y, boxTransform);
	bottom = GetTransformedPos(bottom.x, bottom.y, boxTransform);

	bottom.y = size.y - bottom.y;
	right.x = size.x - right.x;

	// Init viewport
	vec3 viewport;
	vec3_set(&viewport, cx, cy, 1.0f);

	vec3_div(&left, &left, &viewport);
	vec3_div(&right, &right, &viewport);
	vec3_div(&top, &top, &viewport);
	vec3_div(&bottom, &bottom, &viewport);

	vec3_mulf(&left, &left, scale);
	vec3_mulf(&right, &right, scale);
	vec3_mulf(&top, &top, scale);
	vec3_mulf(&bottom, &bottom, scale);

	// Draw spacer lines and labels
	vec3 start, end;

	float pixelRatio = 1.0f; //main->GetDevicePixelRatio();
	if (!spacerLabel[3]) {
		QMetaObject::invokeMethod(this, [this, pixelRatio]() {
			for (int i = 0; i < 4; i++) {
				if (!spacerLabel[i])
					spacerLabel[i] = CreateLabel(pixelRatio, i);
			}
		});
		return;
	}

	vec3_set(&start, top.x, 0.0f, 1.0f);
	vec3_set(&end, top.x, top.y, 1.0f);
	RenderSpacingHelper(0, start, end, viewport, pixelRatio);

	vec3_set(&start, bottom.x, 1.0f - bottom.y, 1.0f);
	vec3_set(&end, bottom.x, 1.0f, 1.0f);
	RenderSpacingHelper(1, start, end, viewport, pixelRatio);

	vec3_set(&start, 0.0f, left.y, 1.0f);
	vec3_set(&end, left.x, left.y, 1.0f);
	RenderSpacingHelper(2, start, end, viewport, pixelRatio);

	vec3_set(&start, 1.0f - right.x, right.y, 1.0f);
	vec3_set(&end, 1.0f, right.y, 1.0f);
	RenderSpacingHelper(3, start, end, viewport, pixelRatio);
}

struct SceneFindData {
	const vec2 &pos;
	OBSSceneItem item;
	bool selectBelow;

	obs_sceneitem_t *group = nullptr;

	SceneFindData(const SceneFindData &) = delete;
	SceneFindData(SceneFindData &&) = delete;
	SceneFindData &operator=(const SceneFindData &) = delete;
	SceneFindData &operator=(SceneFindData &&) = delete;

	inline SceneFindData(const vec2 &pos_, bool selectBelow_) : pos(pos_), selectBelow(selectBelow_) {}
};

struct SceneFindBoxData {
	const vec2 &startPos;
	const vec2 &pos;
	std::vector<obs_sceneitem_t *> sceneItems;

	SceneFindBoxData(const SceneFindData &) = delete;
	SceneFindBoxData(SceneFindData &&) = delete;
	SceneFindBoxData &operator=(const SceneFindData &) = delete;
	SceneFindBoxData &operator=(SceneFindData &&) = delete;

	inline SceneFindBoxData(const vec2 &startPos_, const vec2 &pos_) : startPos(startPos_), pos(pos_) {}
};

obs_scene_item *CanvasDock::GetSelectedItem(obs_scene_t *s)
{
	vec2 pos;
	SceneFindBoxData sfbd(pos, pos);

	if (!s)
		s = this->scene;
	obs_scene_enum_items(s, FindSelected, &sfbd);

	if (sfbd.sceneItems.size() != 1)
		return nullptr;

	return sfbd.sceneItems.at(0);
}

bool CanvasDock::FindSelected(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	SceneFindBoxData *data = reinterpret_cast<SceneFindBoxData *>(param);

	if (obs_sceneitem_selected(item))
		data->sceneItems.push_back(item);

	UNUSED_PARAMETER(scene);
	return true;
}

vec2 CanvasDock::GetItemSize(obs_sceneitem_t *item)
{
	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(item);
	vec2 size;

	if (boundsType != OBS_BOUNDS_NONE) {
		obs_sceneitem_get_bounds(item, &size);
	} else {
		obs_source_t *source = obs_sceneitem_get_source(item);
		obs_sceneitem_crop crop;
		vec2 scale;

		obs_sceneitem_get_scale(item, &scale);
		obs_sceneitem_get_crop(item, &crop);
		size.x = float(obs_source_get_width(source) - crop.left - crop.right) * scale.x;
		size.y = float(obs_source_get_height(source) - crop.top - crop.bottom) * scale.y;
	}

	return size;
}

vec3 CanvasDock::GetTransformedPos(float x, float y, const matrix4 &mat)
{
	vec3 result;
	vec3_set(&result, x, y, 0.0f);
	vec3_transform(&result, &result, &mat);
	return result;
}

obs_source_t *CanvasDock::CreateLabel(float pixelRatio, int i)
{
	OBSDataAutoRelease settings = obs_data_create();
	OBSDataAutoRelease font = obs_data_create();

#if defined(_WIN32)
	obs_data_set_string(font, "face", "Arial");
#elif defined(__APPLE__)
	obs_data_set_string(font, "face", "Helvetica");
#else
	obs_data_set_string(font, "face", "Monospace");
#endif
	obs_data_set_int(font, "flags", 1); // Bold text
	obs_data_set_int(font, "size", (int)(16.0f * pixelRatio));

	obs_data_set_obj(settings, "font", font);
	obs_data_set_bool(settings, "outline", true);

#ifdef _WIN32
	obs_data_set_int(settings, "outline_color", 0x000000);
	obs_data_set_int(settings, "outline_size", 3);
	const char *text_source_id = "text_gdiplus";
#else
	const char *text_source_id = "text_ft2_source";
#endif

	struct dstr name;
	dstr_init(&name);
	dstr_printf(&name, "Aitum Stream Suite Preview spacing label %d", i);
	OBSSource txtSource = obs_source_create_private(text_source_id, name.array, settings);
	dstr_free(&name);
	return txtSource;
}

void CanvasDock::RenderSpacingHelper(int sourceIndex, vec3 &start, vec3 &end, vec3 &viewport, float pixelRatio)
{
	bool horizontal = (sourceIndex == 2 || sourceIndex == 3);

	// If outside of preview, don't render
	if (!((horizontal && (end.x >= start.x)) || (!horizontal && (end.y >= start.y))))
		return;

	float length = vec3_dist(&start, &end);

	float px;

	if (horizontal) {
		px = length * (float)canvas_width;
	} else {
		px = length * (float)canvas_height;
	}

	if (px <= 0.0f)
		return;

	obs_source_t *s = spacerLabel[sourceIndex];
	vec3 labelSize, labelPos;
	vec3_set(&labelSize, (float)obs_source_get_width(s), (float)obs_source_get_height(s), 1.0f);

	vec3_div(&labelSize, &labelSize, &viewport);

	vec3 labelMargin;
	vec3_set(&labelMargin, SPACER_LABEL_MARGIN * pixelRatio, SPACER_LABEL_MARGIN * pixelRatio, 1.0f);
	vec3_div(&labelMargin, &labelMargin, &viewport);

	vec3_set(&labelPos, end.x, end.y, end.z);
	if (horizontal) {
		labelPos.x -= (end.x - start.x) / 2;
		labelPos.x -= labelSize.x / 2;
		labelPos.y -= labelMargin.y + (labelSize.y / 2) + (HANDLE_RADIUS / viewport.y);
	} else {
		labelPos.y -= (end.y - start.y) / 2;
		labelPos.y -= labelSize.y / 2;
		labelPos.x += labelMargin.x;
	}

	DrawSpacingLine(start, end, viewport, pixelRatio);
	SetLabelText(sourceIndex, (int)px);
	DrawLabel(s, labelPos, viewport);
}

void CanvasDock::DrawSpacingLine(vec3 &start, vec3 &end, vec3 &viewport, float pixelRatio)
{
	matrix4 transform;
	matrix4_identity(&transform);
	transform.x.x = viewport.x;
	transform.y.y = viewport.y;

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	QColor selColor = GetSelectionColor();
	vec4 color;
	vec4_set(&color, selColor.redF(), selColor.greenF(), selColor.blueF(), 1.0f);

	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &color);

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_matrix_push();
	gs_matrix_mul(&transform);

	vec2 scale;
	vec2_set(&scale, viewport.x, viewport.y);

	DrawLine(start.x, start.y, end.x, end.y, pixelRatio * (HANDLE_RADIUS / 2), scale);

	gs_matrix_pop();

	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

config_t *CanvasDock::GetUserConfig(void)
{
	return obs_frontend_get_user_config();
}

QColor CanvasDock::GetSelectionColor() const
{
	auto config = GetUserConfig();
	if (config && config_get_bool(config, "Accessibility", "OverrideColors")) {
		return color_from_int(config_get_int(config, "Accessibility", "SelectRed"));
	}
	return QColor::fromRgb(255, 0, 0);
}

QColor CanvasDock::GetCropColor() const
{
	auto config = GetUserConfig();
	if (config && config_get_bool(config, "Accessibility", "OverrideColors")) {
		return color_from_int(config_get_int(config, "Accessibility", "SelectGreen"));
	}
	return QColor::fromRgb(0, 255, 0);
}

QColor CanvasDock::GetHoverColor() const
{
	auto config = GetUserConfig();
	if (config && config_get_bool(config, "Accessibility", "OverrideColors")) {
		return color_from_int(config_get_int(config, "Accessibility", "SelectBlue"));
	}
	return QColor::fromRgb(0, 127, 255);
}

void CanvasDock::SetLabelText(int sourceIndex, int px)
{

	if (px == spacerPx[sourceIndex])
		return;

	std::string text = std::to_string(px) + " px";

	obs_source_t *s = spacerLabel[sourceIndex];

	OBSDataAutoRelease settings = obs_source_get_settings(s);
	obs_data_set_string(settings, "text", text.c_str());
	obs_source_update(s, settings);

	spacerPx[sourceIndex] = px;
}

void CanvasDock::DrawLabel(OBSSource source, vec3 &pos, vec3 &viewport)
{
	if (!source)
		return;

	vec3_mul(&pos, &pos, &viewport);

	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_translate(&pos);
	obs_source_video_render(source);
	gs_matrix_pop();
}

void CanvasDock::DrawLine(float x1, float y1, float x2, float y2, float thickness, vec2 scale)
{
	float ySide = (y1 == y2) ? (y1 < 0.5f ? 1.0f : -1.0f) : 0.0f;
	float xSide = (x1 == x2) ? (x1 < 0.5f ? 1.0f : -1.0f) : 0.0f;

	gs_render_start(true);

	gs_vertex2f(x1, y1);
	gs_vertex2f(x1 + (xSide * (thickness / scale.x)), y1 + (ySide * (thickness / scale.y)));
	gs_vertex2f(x2 + (xSide * (thickness / scale.x)), y2 + (ySide * (thickness / scale.y)));
	gs_vertex2f(x2, y2);
	gs_vertex2f(x1, y1);

	gs_vertbuffer_t *line = gs_render_save();

	gs_load_vertexbuffer(line);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(line);
}

void CanvasDock::DrawRotationHandle(gs_vertbuffer_t *circle, float rot, float pixelRatio)
{
	struct vec3 pos;
	vec3_set(&pos, 0.5f, 0.0f, 0.0f);

	struct matrix4 matrix;
	gs_matrix_get(&matrix);
	vec3_transform(&pos, &pos, &matrix);

	gs_render_start(true);

	gs_vertex2f(0.5f - 0.34f / HANDLE_RADIUS, 0.5f);
	gs_vertex2f(0.5f - 0.34f / HANDLE_RADIUS, -2.0f);
	gs_vertex2f(0.5f + 0.34f / HANDLE_RADIUS, -2.0f);
	gs_vertex2f(0.5f + 0.34f / HANDLE_RADIUS, 0.5f);
	gs_vertex2f(0.5f - 0.34f / HANDLE_RADIUS, 0.5f);

	gs_vertbuffer_t *line = gs_render_save();

	gs_load_vertexbuffer(line);

	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_translate(&pos);

	gs_matrix_rotaa4f(0.0f, 0.0f, 1.0f, RAD(rot));
	gs_matrix_translate3f(-HANDLE_RADIUS * 1.5f * pixelRatio, -HANDLE_RADIUS * 1.5f * pixelRatio, 0.0f);
	gs_matrix_scale3f(HANDLE_RADIUS * 3 * pixelRatio, HANDLE_RADIUS * 3 * pixelRatio, 1.0f);

	gs_draw(GS_TRISTRIP, 0, 0);

	gs_matrix_translate3f(0.0f, -HANDLE_RADIUS * 2 / 3, 0.0f);

	gs_load_vertexbuffer(circle);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_matrix_pop();
	gs_vertexbuffer_destroy(line);
}

void CanvasDock::DrawStripedLine(float x1, float y1, float x2, float y2, float thickness, vec2 scale)
{
	float ySide = (y1 == y2) ? (y1 < 0.5f ? 1.0f : -1.0f) : 0.0f;
	float xSide = (x1 == x2) ? (x1 < 0.5f ? 1.0f : -1.0f) : 0.0f;

	float dist = sqrtf(powf((x1 - x2) * scale.x, 2.0f) + powf((y1 - y2) * scale.y, 2.0f));
	if (dist > 1000000.0f) {
		// too many stripes to draw, draw it as a line as fallback
		DrawLine(x1, y1, x2, y2, thickness, scale);
		return;
	}

	float offX = (x2 - x1) / dist;
	float offY = (y2 - y1) / dist;

	for (int i = 0, l = (int)ceil(dist / 15.0); i < l; i++) {
		gs_render_start(true);

		float xx1 = x1 + (float)i * 15.0f * offX;
		float yy1 = y1 + (float)i * 15.0f * offY;

		float dx;
		float dy;

		if (x1 < x2) {
			dx = std::min(xx1 + 7.5f * offX, x2);
		} else {
			dx = std::max(xx1 + 7.5f * offX, x2);
		}

		if (y1 < y2) {
			dy = std::min(yy1 + 7.5f * offY, y2);
		} else {
			dy = std::max(yy1 + 7.5f * offY, y2);
		}

		gs_vertex2f(xx1, yy1);
		gs_vertex2f(xx1 + (xSide * (thickness / scale.x)), yy1 + (ySide * (thickness / scale.y)));
		gs_vertex2f(dx, dy);
		gs_vertex2f(dx + (xSide * (thickness / scale.x)), dy + (ySide * (thickness / scale.y)));

		gs_vertbuffer_t *line = gs_render_save();

		gs_load_vertexbuffer(line);
		gs_draw(GS_TRISTRIP, 0, 0);
		gs_vertexbuffer_destroy(line);
	}
}

void CanvasDock::DrawRect(float thickness, vec2 scale)
{
	if (scale.x <= 0.0f || scale.y <= 0.0f || thickness <= 0.0f) {
		return;
	}
	gs_render_start(true);

	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f + (thickness / scale.x), 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(0.0f + (thickness / scale.x), 1.0f);
	gs_vertex2f(0.0f, 1.0f - (thickness / scale.y));
	gs_vertex2f(1.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f - (thickness / scale.y));
	gs_vertex2f(1.0f - (thickness / scale.x), 1.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(1.0f - (thickness / scale.x), 0.0f);
	gs_vertex2f(1.0f, 0.0f + (thickness / scale.y));
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(0.0f, 0.0f + (thickness / scale.y));

	gs_vertbuffer_t *rect = gs_render_save();

	gs_load_vertexbuffer(rect);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(rect);
}

bool CanvasDock::DrawSelectionBox(float x1, float y1, float x2, float y2, gs_vertbuffer_t *rect_fill)
{
	float pixelRatio = GetDevicePixelRatio();

	x1 = std::round(x1);
	x2 = std::round(x2);
	y1 = std::round(y1);
	y2 = std::round(y2);

	gs_effect_t *eff = gs_get_effect();
	gs_eparam_t *colParam = gs_effect_get_param_by_name(eff, "color");

	vec4 fillColor;
	vec4_set(&fillColor, 0.7f, 0.7f, 0.7f, 0.5f);

	vec4 borderColor;
	vec4_set(&borderColor, 1.0f, 1.0f, 1.0f, 1.0f);

	vec2 scale;
	vec2_set(&scale, std::abs(x2 - x1), std::abs(y2 - y1));

	gs_matrix_push();
	gs_matrix_identity();

	gs_matrix_translate3f(x1, y1, 0.0f);
	gs_matrix_scale3f(x2 - x1, y2 - y1, 1.0f);

	gs_effect_set_vec4(colParam, &fillColor);
	gs_load_vertexbuffer(rect_fill);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_effect_set_vec4(colParam, &borderColor);
	DrawRect(HANDLE_RADIUS * pixelRatio / 2, scale);

	gs_matrix_pop();

	return true;
}

float CanvasDock::GetDevicePixelRatio()
{
	return 1.0f;
}

bool CanvasDock::DrawSelectedItem(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_locked(item))
		return true;

	if (!SceneItemHasVideo(item))
		return true;

	CanvasDock *window = static_cast<CanvasDock *>(param);

	if (obs_sceneitem_is_group(item)) {
		matrix4 mat;
		obs_sceneitem_get_draw_transform(item, &mat);

		window->groupRot = obs_sceneitem_get_rot(item);

		gs_matrix_push();
		gs_matrix_mul(&mat);
		obs_sceneitem_group_enum_items(item, DrawSelectedItem, param);
		gs_matrix_pop();

		window->groupRot = 0.0f;
	}

	float pixelRatio = window->GetDevicePixelRatio();

	bool hovered = false;
	{
		std::lock_guard<std::mutex> lock(window->selectMutex);
		for (size_t i = 0; i < window->hoveredPreviewItems.size(); i++) {
			if (window->hoveredPreviewItems[i] == item) {
				hovered = true;
				break;
			}
		}
	}

	bool selected = obs_sceneitem_selected(item);

	if (!selected && !hovered)
		return true;

	matrix4 boxTransform;
	matrix4 invBoxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);
	matrix4_inv(&invBoxTransform, &boxTransform);

	vec3 bounds[] = {
		{{{0.f, 0.f, 0.f}}},
		{{{1.f, 0.f, 0.f}}},
		{{{0.f, 1.f, 0.f}}},
		{{{1.f, 1.f, 0.f}}},
	};

	//main->GetCameraIcon();

	QColor selColor = window->GetSelectionColor();
	QColor cropColor = window->GetCropColor();
	QColor hoverColor = window->GetHoverColor();

	vec4 red;
	vec4 green;
	vec4 blue;

	vec4_set(&red, selColor.redF(), selColor.greenF(), selColor.blueF(), 1.0f);
	vec4_set(&green, cropColor.redF(), cropColor.greenF(), cropColor.blueF(), 1.0f);
	vec4_set(&blue, hoverColor.redF(), hoverColor.greenF(), hoverColor.blueF(), 1.0f);

	bool visible = std::all_of(std::begin(bounds), std::end(bounds), [&](const vec3 &b) {
		vec3 pos;
		vec3_transform(&pos, &b, &boxTransform);
		vec3_transform(&pos, &pos, &invBoxTransform);
		return CloseFloat(pos.x, b.x) && CloseFloat(pos.y, b.y);
	});

	if (!visible)
		return true;

	GS_DEBUG_MARKER_BEGIN(GS_DEBUG_COLOR_DEFAULT, "DrawSelectedItem");

	matrix4 curTransform;
	vec2 boxScale;
	gs_matrix_get(&curTransform);
	obs_sceneitem_get_box_scale(item, &boxScale);
	boxScale.x *= curTransform.x.x;
	boxScale.y *= curTransform.y.y;

	gs_matrix_push();
	gs_matrix_mul(&boxTransform);

	obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(item, &crop);

	gs_effect_t *eff = gs_get_effect();
	gs_eparam_t *colParam = gs_effect_get_param_by_name(eff, "color");

	gs_effect_set_vec4(colParam, &red);

	if (obs_sceneitem_get_bounds_type(item) == OBS_BOUNDS_NONE && crop_enabled(&crop)) {
#define DRAW_SIDE(side, x1, y1, x2, y2)                                                   \
	if (hovered && !selected) {                                                       \
		gs_effect_set_vec4(colParam, &blue);                                      \
		DrawLine(x1, y1, x2, y2, HANDLE_RADIUS *pixelRatio / 2, boxScale);        \
	} else if (crop.side > 0) {                                                       \
		gs_effect_set_vec4(colParam, &green);                                     \
		DrawStripedLine(x1, y1, x2, y2, HANDLE_RADIUS *pixelRatio / 2, boxScale); \
	} else {                                                                          \
		DrawLine(x1, y1, x2, y2, HANDLE_RADIUS *pixelRatio / 2, boxScale);        \
	}                                                                                 \
	gs_effect_set_vec4(colParam, &red);

		DRAW_SIDE(left, 0.0f, 0.0f, 0.0f, 1.0f);
		DRAW_SIDE(top, 0.0f, 0.0f, 1.0f, 0.0f);
		DRAW_SIDE(right, 1.0f, 0.0f, 1.0f, 1.0f);
		DRAW_SIDE(bottom, 0.0f, 1.0f, 1.0f, 1.0f);
#undef DRAW_SIDE
	} else {
		if (!selected) {
			gs_effect_set_vec4(colParam, &blue);
			DrawRect(HANDLE_RADIUS * pixelRatio / 2, boxScale);
		} else {
			DrawRect(HANDLE_RADIUS * pixelRatio / 2, boxScale);
		}
	}

	gs_load_vertexbuffer(window->box);
	gs_effect_set_vec4(colParam, &red);

	if (selected) {
		DrawSquareAtPos(0.0f, 0.0f, pixelRatio);
		DrawSquareAtPos(0.0f, 1.0f, pixelRatio);
		DrawSquareAtPos(1.0f, 0.0f, pixelRatio);
		DrawSquareAtPos(1.0f, 1.0f, pixelRatio);
		DrawSquareAtPos(0.5f, 0.0f, pixelRatio);
		DrawSquareAtPos(0.0f, 0.5f, pixelRatio);
		DrawSquareAtPos(0.5f, 1.0f, pixelRatio);
		DrawSquareAtPos(1.0f, 0.5f, pixelRatio);

		if (!window->circleFill) {
			gs_render_start(true);

			float angle = 180;
			for (int i = 0, l = 40; i < l; i++) {
				gs_vertex2f(sin(RAD(angle)) / 2 + 0.5f, cos(RAD(angle)) / 2 + 0.5f);
				angle += 360.0f / (float)l;
				gs_vertex2f(sin(RAD(angle)) / 2 + 0.5f, cos(RAD(angle)) / 2 + 0.5f);
				gs_vertex2f(0.5f, 1.0f);
			}

			window->circleFill = gs_render_save();
		}

		DrawRotationHandle(window->circleFill, obs_sceneitem_get_rot(item) + window->groupRot, pixelRatio);
	}

	gs_matrix_pop();

	GS_DEBUG_MARKER_END();

	UNUSED_PARAMETER(scene);
	return true;
}

bool CanvasDock::SceneItemHasVideo(obs_sceneitem_t *item)
{
	const obs_source_t *source = obs_sceneitem_get_source(item);
	const uint32_t flags = obs_source_get_output_flags(source);
	return (flags & OBS_SOURCE_VIDEO) != 0;
}

bool CanvasDock::CloseFloat(float a, float b, float epsilon)
{
	return std::abs(a - b) <= epsilon;
}

inline bool CanvasDock::crop_enabled(const obs_sceneitem_crop *crop)
{
	return crop->left > 0 || crop->top > 0 || crop->right > 0 || crop->bottom > 0;
}

void CanvasDock::DrawSquareAtPos(float x, float y, float pixelRatio)
{
	struct vec3 pos;
	vec3_set(&pos, x, y, 0.0f);

	struct matrix4 matrix;
	gs_matrix_get(&matrix);
	vec3_transform(&pos, &pos, &matrix);

	gs_matrix_push();
	gs_matrix_identity();
	gs_matrix_translate(&pos);

	gs_matrix_translate3f(-HANDLE_RADIUS * pixelRatio, -HANDLE_RADIUS * pixelRatio, 0.0f);
	gs_matrix_scale3f(HANDLE_RADIUS * pixelRatio * 2, HANDLE_RADIUS * pixelRatio * 2, 1.0f);
	gs_draw(GS_TRISTRIP, 0, 0);

	gs_matrix_pop();
}

obs_sceneitem_t *CanvasDock::GetCurrentSceneItem()
{
	return sourceList->Get(GetTopSelectedSourceItem());
}

int CanvasDock::GetTopSelectedSourceItem()
{
	QModelIndexList selectedItems = sourceList->selectionModel()->selectedIndexes();
	return selectedItems.count() ? selectedItems[0].row() : -1;
}

void CanvasDock::ChangeSceneIndex(bool relative, int offset, int invalidIdx)
{
	int idx = sceneList->currentRow();
	if (idx < 0)
		return;

	auto canvasItem = sceneList->item(idx);
	if (!canvasItem)
		return;

	if (idx == invalidIdx)
		return;

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

QListWidget *CanvasDock::GetGlobalScenesList()
{
	auto p = parentWidget();
	if (!p)
		return nullptr;
	p = p->parentWidget();
	if (!p)
		return nullptr;
	auto sd = p->findChild<QDockWidget *>(QStringLiteral("scenesDock"));
	if (!sd)
		return nullptr;
	return sd->findChild<QListWidget *>(QStringLiteral("scenes"));
}

void CanvasDock::AddScene(QString duplicate, bool ask_name)
{
	std::string name = duplicate.isEmpty() ? obs_module_text("Scene") : duplicate.toUtf8().constData();
	obs_source_t *s = obs_canvas_get_source_by_name(canvas, name.c_str());
	int i = 0;
	while (s) {
		obs_source_release(s);
		i++;
		name = obs_module_text("Scene");
		name += " ";
		name += std::to_string(i);
		s = obs_canvas_get_source_by_name(canvas, name.c_str());
	}
	do {
		obs_source_release(s);
		if (ask_name && !NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SceneName")), name)) {
			break;
		}
		s = obs_canvas_get_source_by_name(canvas, name.c_str());
		if (s)
			continue;

		obs_source_t *new_scene = nullptr;
		if (!duplicate.isEmpty()) {
			auto origSceneSource = obs_canvas_get_source_by_name(canvas, duplicate.toUtf8().constData());
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
			obs_scene_t *ns = obs_canvas_scene_create(canvas, name.c_str());
			new_scene = obs_scene_get_source(ns);
			obs_source_load(new_scene);

			std::string ssn = obs_canvas_get_name(canvas);
			ssn += " ";
			ssn += obs_frontend_get_locale_string("Basic.Hotkeys.SelectScene");
			obs_hotkey_register_source(
				new_scene, "OBSBasic.SelectScene", ssn.c_str(),
				[](void *data, obs_hotkey_id, obs_hotkey_t *key, bool pressed) {
					if (!pressed)
						return;
					auto p = (CanvasDock *)data;
					auto potential_source = (obs_weak_source_t *)obs_hotkey_get_registerer(key);
					OBSSourceAutoRelease source = obs_weak_source_get_source(potential_source);
					if (source) {
						auto sn = QString::fromUtf8(obs_source_get_name(source));
						QMetaObject::invokeMethod(p, "SwitchScene", Q_ARG(QString, sn), Q_ARG(bool, true));
					}
				},
				this);
			auto sh = obs_source_get_signal_handler(new_scene);
			signal_handler_connect(sh, "rename", source_rename, this);
			signal_handler_connect(sh, "remove", source_remove, this);
		}
		auto sn = QString::fromUtf8(obs_source_get_name(new_scene));
		//if (scenesCombo)
		//	scenesCombo->addItem(sn);
		auto sli = new QListWidgetItem(sn, sceneList);
		sli->setIcon(QIcon(":/aitum/media/unlinked.svg"));
		sceneList->addItem(sli);

		SwitchScene(sn);
		obs_source_release(new_scene);
	} while (ask_name && s);
}

void CanvasDock::RemoveScene(const QString &sceneName)
{
	auto s = obs_canvas_get_source_by_name(canvas, sceneName.toUtf8().constData());
	if (!s)
		return;
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
		obs_source_remove(s);
	}

	obs_source_release(s);
}

extern obs_websocket_vendor vendor;

void CanvasDock::SwitchScene(const QString &scene_name, bool transition)
{
	auto s = scene_name.isEmpty() ? nullptr : obs_canvas_get_source_by_name(canvas, scene_name.toUtf8().constData());
	if (s == obs_scene_get_source(scene) || (!obs_source_is_scene(s) && !scene_name.isEmpty())) {
		obs_source_release(s);
		return;
	}
	auto oldSource = obs_scene_get_source(scene);
	auto sh = oldSource ? obs_source_get_signal_handler(oldSource) : nullptr;
	if (sh) {
		signal_handler_disconnect(sh, "item_add", SceneItemAdded, this);
		signal_handler_disconnect(sh, "reorder", SceneReordered, this);
		signal_handler_disconnect(sh, "refresh", SceneRefreshed, this);
	}
	if (!source || obs_weak_source_references_source(source, oldSource)) {
		obs_weak_source_release(source);
		source = obs_source_get_weak_source(s);
		//if (view)
		//	obs_view_set_source(view, 0, s);
		if (canvas)
			obs_canvas_set_channel(canvas, 0, s);
	} else {
		oldSource = obs_weak_source_get_source(source);
		if (oldSource) {
			auto ost = obs_source_get_type(oldSource);
			if (ost == OBS_SOURCE_TYPE_TRANSITION) {
				auto private_settings = s ? obs_source_get_private_settings(s) : nullptr;
				obs_source_t *override_transition =
					GetTransition(obs_data_get_string(private_settings, "transition"));
				if (SwapTransition(override_transition)) {
					obs_source_release(oldSource);
					oldSource = obs_weak_source_get_source(source);
					signal_handler_t *handler = obs_source_get_signal_handler(oldSource);
					signal_handler_connect(handler, "transition_stop", transition_override_stop, this);
				}
				int duration = 0;
				if (override_transition)
					duration = (int)obs_data_get_int(private_settings, "transition_duration");
				if (duration <= 0)
					duration = obs_frontend_get_transition_duration();
				obs_data_release(private_settings);

				auto sourceA = obs_transition_get_source(oldSource, OBS_TRANSITION_SOURCE_A);
				if (sourceA != obs_scene_get_source(scene))
					obs_transition_set(oldSource, obs_scene_get_source(scene));
				obs_source_release(sourceA);
				if (transition) {
					obs_transition_start(oldSource, OBS_TRANSITION_MODE_AUTO, duration, s);
				} else {
					obs_transition_set(oldSource, s);
				}
			} else {
				obs_weak_source_release(source);
				source = obs_source_get_weak_source(s);
				//if (view)
				//	obs_view_set_source(view, 0, s);
				if (canvas)
					obs_canvas_set_channel(canvas, 0, s);
			}
			obs_source_release(oldSource);
		} else {
			obs_weak_source_release(source);
			source = obs_source_get_weak_source(s);
			//if (view)
			//	obs_view_set_source(view, 0, s);
			if (canvas)
				obs_canvas_set_channel(canvas, 0, s);
		}
	}
	scene = obs_scene_from_source(s);
	if (scene) {
		sh = obs_source_get_signal_handler(s);
		if (sh) {
			signal_handler_connect(sh, "item_add", SceneItemAdded, this);
			signal_handler_connect(sh, "reorder", SceneReordered, this);
			signal_handler_connect(sh, "refresh", SceneRefreshed, this);
		}
	}
	auto oldName = currentSceneName;
	if (!scene_name.isEmpty())
		currentSceneName = scene_name;
	//if (scenesCombo && scenesCombo->currentText() != scene_name) {
	//	scenesCombo->setCurrentText(scene_name);
	//}
	if (!scene_name.isEmpty()) {
		QListWidgetItem *item = sceneList->currentItem();
		if (!item || item->text() != scene_name) {
			for (int i = 0; i < sceneList->count(); i++) {
				item = sceneList->item(i);
				if (item->text() == scene_name) {
					sceneList->setCurrentRow(i);
					item->setSelected(true);
					break;
				}
			}
		}
	}
	sourceList->GetStm()->SceneChanged();

	obs_source_release(s);
	if (vendor && oldName != currentSceneName) {
		const auto d = obs_data_create();
		obs_data_set_int(d, "width", canvas_width);
		obs_data_set_int(d, "height", canvas_height);
		obs_data_set_string(d, "canvas", obs_canvas_get_name(canvas));
		obs_data_set_string(d, "old_scene", oldName.toUtf8().constData());
		obs_data_set_string(d, "new_scene", currentSceneName.toUtf8().constData());
		obs_websocket_vendor_emit_event(vendor, "switch_scene", d);
		obs_data_release(d);
	}
}

void CanvasDock::SceneItemAdded(void *data, calldata_t *params)
{
	CanvasDock *window = static_cast<CanvasDock *>(data);

	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "AddSceneItem", Qt::QueuedConnection, Q_ARG(OBSSceneItem, OBSSceneItem(item)));
}

void CanvasDock::SceneReordered(void *data, calldata_t *params)
{
	CanvasDock *window = static_cast<CanvasDock *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");

	QMetaObject::invokeMethod(window, "ReorderSources", Qt::QueuedConnection, Q_ARG(OBSScene, OBSScene(scene)));
}

void CanvasDock::SceneRefreshed(void *data, calldata_t *params)
{
	CanvasDock *window = static_cast<CanvasDock *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");

	QMetaObject::invokeMethod(window, "RefreshSources", Qt::QueuedConnection, Q_ARG(OBSScene, OBSScene(scene)));
}

obs_source_t *CanvasDock::GetTransition(const char *transition_name)
{
	if (!transition_name || !strlen(transition_name))
		return nullptr;
	for (auto transition : transitions) {
		if (strcmp(transition_name, obs_source_get_name(transition)) == 0) {
			return transition;
		}
	}
	return nullptr;
}

bool CanvasDock::SwapTransition(obs_source_t *newTransition)
{
	if (!newTransition || obs_weak_source_references_source(source, newTransition))
		return false;

	obs_transition_set_size(newTransition, canvas_width, canvas_height);

	obs_source_t *oldTransition = obs_weak_source_get_source(source);
	if (!oldTransition || obs_source_get_type(oldTransition) != OBS_SOURCE_TYPE_TRANSITION) {
		if (oldTransition) {
			obs_transition_set(newTransition, oldTransition);
			obs_source_release(oldTransition);
		}
		obs_weak_source_release(source);
		source = obs_source_get_weak_source(newTransition);
		//if (view)
		//	obs_view_set_source(view, 0, newTransition);
		if (canvas)
			obs_canvas_set_channel(canvas, 0, newTransition);
		obs_source_inc_showing(newTransition);
		obs_source_inc_active(newTransition);
		return true;
	}
	signal_handler_t *handler = obs_source_get_signal_handler(oldTransition);
	signal_handler_disconnect(handler, "transition_stop", transition_override_stop, this);
	obs_source_inc_showing(newTransition);
	obs_source_inc_active(newTransition);
	obs_transition_swap_begin(newTransition, oldTransition);
	obs_weak_source_release(source);
	source = obs_source_get_weak_source(newTransition);
	//if (view)
	//	obs_view_set_source(view, 0, newTransition);
	if (canvas)
		obs_canvas_set_channel(canvas, 0, newTransition);
	obs_transition_swap_end(newTransition, oldTransition);
	obs_source_dec_showing(oldTransition);
	obs_source_dec_active(oldTransition);
	obs_source_release(oldTransition);
	return true;
}

void CanvasDock::transition_override_stop(void *data, calldata_t *)
{
	auto dock = (CanvasDock *)data;
	QMetaObject::invokeMethod(dock, "SwitchBackToSelectedTransition", Qt::QueuedConnection);
}

QMenu *CanvasDock::CreateAddSourcePopupMenu()
{
	const char *unversioned_type;
	const char *type;
	bool foundValues = false;
	bool foundDeprecated = false;
	size_t idx = 0;

	QMenu *popup = new QMenu(QString::fromUtf8(obs_frontend_get_locale_string("Add")), this);
	QMenu *deprecated = new QMenu(QString::fromUtf8(obs_frontend_get_locale_string("Deprecated")), popup);

	while (obs_enum_input_types2(idx++, &type, &unversioned_type)) {
		const char *name = obs_source_get_display_name(type);
		if (!name)
			continue;
		uint32_t caps = obs_get_source_output_flags(type);

		if ((caps & OBS_SOURCE_CAP_DISABLED) != 0)
			continue;

		if ((caps & OBS_SOURCE_DEPRECATED) == 0) {
			AddSourceTypeToMenu(popup, unversioned_type, name);
		} else {
			AddSourceTypeToMenu(deprecated, unversioned_type, name);
			foundDeprecated = true;
		}
		foundValues = true;
	}

	AddSourceTypeToMenu(popup, "scene", obs_frontend_get_locale_string("Basic.Scene"));
	AddSourceTypeToMenu(popup, "group", obs_frontend_get_locale_string("Group"));

	if (!foundDeprecated) {
		delete deprecated;
		deprecated = nullptr;
	}

	if (!foundValues) {
		delete popup;
		popup = nullptr;

	} else if (foundDeprecated) {
		popup->addSeparator();
		popup->addMenu(deprecated);
	}

	return popup;
}

struct descendant_info {
	bool exists;
	obs_weak_source_t *target;
	obs_source_t *target2;
};

static void check_descendant(obs_source_t *parent, obs_source_t *child, void *param)
{
	auto *info = (struct descendant_info *)param;
	if (parent == info->target2 || child == info->target2 || obs_weak_source_references_source(info->target, child) ||
	    obs_weak_source_references_source(info->target, parent))
		info->exists = true;
}

bool CanvasDock::add_sources_of_type_to_menu(void *param, obs_source_t *source)
{
	QMenu *menu = static_cast<QMenu *>(param);
	auto parent = qobject_cast<QMenu *>(menu->parent());
	auto a = parent && !parent->menuAction()->data().isNull() ? parent->menuAction() : menu->menuAction();
	while (parent && qobject_cast<QMenu *>(parent->parent()))
		parent = qobject_cast<QMenu *>(parent->parent());
	CanvasDock *cd = static_cast<CanvasDock *>(parent ? parent->parent() : menu->parent());
	auto t = a->data().toString();
	auto idUtf8 = t.toUtf8();
	const char *id = idUtf8.constData();
	if (strcmp(obs_source_get_unversioned_id(source), id) == 0) {
		auto name = QString::fromUtf8(obs_source_get_name(source));
		QList<QAction *> actions = menu->actions();
		QAction *after = nullptr;
		for (QAction *menuAction : actions) {
			if (menuAction->text().compare(name, Qt::CaseInsensitive) >= 0) {
				after = menuAction;
				break;
			}
		}
		auto na = new QAction(name, menu);
		connect(na, &QAction::triggered, cd, [cd, source] { cd->AddSourceToScene(source); }, Qt::QueuedConnection);
		menu->insertAction(after, na);
		struct descendant_info info = {false, cd->source, obs_scene_get_source(cd->scene)};
		obs_source_enum_full_tree(source, check_descendant, &info);
		na->setEnabled(!info.exists);
	}
	return true;
}

void CanvasDock::LoadSourceTypeMenu(QMenu *menu, const char *type)
{
	menu->clear();
	if (obs_get_source_output_flags(type) & OBS_SOURCE_REQUIRES_CANVAS) {
		obs_enum_canvases(
			[](void *param, obs_canvas_t *canvas) {
				QMenu *m = (QMenu *)param;
				auto canvas_name = QString::fromUtf8(obs_canvas_get_name(canvas));
				auto cm = new QMenu(canvas_name, m);
				obs_canvas_enum_scenes(canvas, add_sources_of_type_to_menu, cm);
				if (cm->actions().count() == 0) {
					delete cm;
				} else {
					m->addMenu(cm);
				}
				return true;
			},
			menu);
	} else if (strcmp(type, "scene") == 0) {
		obs_enum_scenes(add_sources_of_type_to_menu, menu);
	} else {
		obs_enum_sources(add_sources_of_type_to_menu, menu);

		auto popupItem = new QAction(QString::fromUtf8(obs_frontend_get_locale_string("New")), menu);
		popupItem->setData(QString::fromUtf8(type));
		connect(popupItem, SIGNAL(triggered(bool)), this, SLOT(AddSourceFromAction()));

		QList<QAction *> actions = menu->actions();
		QAction *first = actions.size() ? actions.first() : nullptr;
		menu->insertAction(first, popupItem);
		menu->insertSeparator(first);
	}
}

void CanvasDock::AddSourceToScene(obs_source_t *s)
{
	obs_scene_add(scene, s);
}

void CanvasDock::AddSourceTypeToMenu(QMenu *popup, const char *source_type, const char *name)
{
	QString qname = QString::fromUtf8(name);
	QAction *popupItem = new QAction(qname, popup);
	if (strcmp(source_type, "scene") == 0) {
		popupItem->setIcon(GetSceneIcon());
	} else if (strcmp(source_type, "group") == 0) {
		popupItem->setIcon(GetGroupIcon());
	} else {
		popupItem->setIcon(GetIconFromType(obs_source_get_icon_type(source_type)));
	}
	popupItem->setData(QString::fromUtf8(source_type));
	QMenu *menu = new QMenu(popup);
	popupItem->setMenu(menu);
	QObject::connect(menu, &QMenu::aboutToShow, [this, menu, source_type] { LoadSourceTypeMenu(menu, source_type); });
	QList<QAction *> actions = popup->actions();
	QAction *after = nullptr;
	for (QAction *menuAction : actions) {
		if (menuAction->text().compare(name, Qt::CaseInsensitive) >= 0) {
			after = menuAction;
			break;
		}
	}
	popup->insertAction(after, popupItem);
}

bool CanvasDock::selected_items(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	std::vector<OBSSceneItem> &items = *reinterpret_cast<std::vector<OBSSceneItem> *>(param);

	if (obs_sceneitem_selected(item)) {
		items.emplace_back(item);
	} else if (obs_sceneitem_is_group(item)) {
		obs_sceneitem_group_enum_items(item, selected_items, &items);
	}
	return true;
}

void CanvasDock::ShowScenesContextMenu(QListWidgetItem *widget_item)
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
		if (!item)
			return;
		AddScene(item->text());
	});
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Remove")), [this] {
		auto item = sceneList->currentItem();
		if (!item)
			return;
		RemoveScene(item->text());
	});
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Rename")), [this] {
		const auto item = sceneList->currentItem();
		if (!item)
			return;
		std::string name = item->text().toUtf8().constData();
		obs_source_t *source = obs_canvas_get_source_by_name(canvas, name.c_str());
		if (!source)
			source = obs_get_source_by_name(name.c_str());
		if (!source)
			return;
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SceneName")), name)) {
				break;
			}
			s = obs_canvas_get_source_by_name(canvas, name.c_str());
			if (!s)
				s = obs_get_source_by_name(name.c_str());
			if (s)
				continue;
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
		if (!item)
			return;
		auto s = obs_canvas_get_source_by_name(canvas, item->text().toUtf8().constData());
		if (s) {
			obs_frontend_take_source_screenshot(s);
			obs_source_release(s);
		}
	});
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Filters")), [this] {
		auto item = sceneList->currentItem();
		if (!item)
			return;
		auto s = obs_canvas_get_source_by_name(canvas, item->text().toUtf8().constData());
		if (s) {
			obs_frontend_open_source_filters(s);
			obs_source_release(s);
		}
	});

	auto tom = menu.addMenu(QString::fromUtf8(obs_frontend_get_locale_string("TransitionOverride")));
	std::string scene_name = widget_item->text().toUtf8().constData();
	OBSSourceAutoRelease scene_source = obs_canvas_get_source_by_name(canvas, scene_name.c_str());
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

	connect(duration, (void (QSpinBox::*)(int))&QSpinBox::valueChanged, [this, scene_name](int dur) {
		OBSSourceAutoRelease source = obs_canvas_get_source_by_name(canvas, scene_name.c_str());
		OBSDataAutoRelease ps = obs_source_get_private_settings(source);

		obs_data_set_int(ps, "transition_duration", dur);
	});

	auto action = tom->addAction(QString::fromUtf8(obs_frontend_get_locale_string("None")));
	action->setCheckable(true);
	action->setChecked(!curTransition || !strlen(curTransition));
	connect(action, &QAction::triggered, [this, scene_name] {
		OBSSourceAutoRelease source = obs_canvas_get_source_by_name(canvas, scene_name.c_str());
		OBSDataAutoRelease ps = obs_source_get_private_settings(source);
		obs_data_set_string(ps, "transition", "");
	});

	for (auto t : transitions) {
		const char *name = obs_source_get_name(t);
		bool match = (name && curTransition && strcmp(name, curTransition) == 0);

		if (!name || !*name)
			name = obs_frontend_get_locale_string("None");

		auto a2 = tom->addAction(QString::fromUtf8(name));
		a2->setCheckable(true);
		a2->setChecked(match);
		connect(a, &QAction::triggered, [this, scene_name, a2] {
			OBSSourceAutoRelease source = obs_canvas_get_source_by_name(canvas, scene_name.c_str());
			OBSDataAutoRelease ps = obs_source_get_private_settings(source);
			obs_data_set_string(ps, "transition", a2->text().toUtf8().constData());
		});
	}

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
				SetLinkedScene(src, checkBox->isChecked() ? sceneList->currentItem()->text() : "");
			});
			auto *checkableAction = new QWidgetAction(linkedScenesMenu);
			checkableAction->setDefaultWidget(checkBox);
			linkedScenesMenu->addAction(checkableAction);

			auto c = obs_data_get_array(settings, "canvas");
			if (c) {
				const auto count = obs_data_array_count(c);

				for (size_t j = 0; j < count; j++) {
					auto item = obs_data_array_item(c, j);
					if (!item)
						continue;
					if (strcmp(obs_data_get_string(item, "name"), obs_canvas_get_name(canvas)) == 0) {
						auto sn = QString::fromUtf8(obs_data_get_string(item, "scene"));
						if (sn == sceneList->currentItem()->text()) {
							checkBox->setChecked(true);
						}
					} else if (strcmp(obs_data_get_string(item, "name"), "") == 0 &&
						   obs_data_get_int(item, "width") == canvas_width &&
						   obs_data_get_int(item, "height") == canvas_height) {
						obs_data_set_string(item, "name", obs_canvas_get_name(canvas));
						auto sn = QString::fromUtf8(obs_data_get_string(item, "scene"));
						if (sn == sceneList->currentItem()->text()) {
							checkBox->setChecked(true);
						}
					}
					obs_data_release(item);
				}

				obs_data_array_release(c);
			}

			obs_data_release(settings);
		}
		obs_frontend_source_list_free(&scenes);
	});

	menu.addAction(QString::fromUtf8(obs_module_text("OnMainCanvas")), [this] {
		auto item = sceneList->currentItem();
		if (!item)
			return;
		auto s = obs_canvas_get_source_by_name(canvas, item->text().toUtf8().constData());
		if (!s)
			return;

		if (obs_frontend_preview_program_mode_active())
			obs_frontend_set_current_preview_scene(s);
		else
			obs_frontend_set_current_scene(s);
		obs_source_release(s);
	});

	a = menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("ShowInMultiview")), [this, scene_name](bool checked) {
		OBSSourceAutoRelease source = obs_canvas_get_source_by_name(canvas, scene_name.c_str());
		OBSDataAutoRelease ps = obs_source_get_private_settings(source);
		obs_data_set_bool(ps, "show_in_multiview", checked);
	});
	a->setCheckable(true);
	obs_data_set_default_bool(private_settings, "show_in_multiview", true);
	a->setChecked(obs_data_get_bool(private_settings, "show_in_multiview"));
	menu.exec(QCursor::pos());
}

void CanvasDock::SetGridMode(bool checked)
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

bool CanvasDock::IsGridMode()
{
	return sceneList->viewMode() == QListView::IconMode;
}

void CanvasDock::SetLinkedScene(obs_source_t *scene_, const QString &linkedScene)
{
	auto ss = obs_source_get_settings(scene_);
	auto c = obs_data_get_array(ss, "canvas");

	auto count = obs_data_array_count(c);
	obs_data_t *found = nullptr;
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(c, i);
		if (!item)
			continue;
		if (strcmp(obs_data_get_string(item, "name"), obs_canvas_get_name(canvas)) == 0) {
			found = item;
			if (linkedScene.isEmpty()) {
				obs_data_array_erase(c, i);
			}
			break;
		} else if (strcmp(obs_data_get_string(item, "name"), "") == 0 && obs_data_get_int(item, "width") == canvas_width &&
			   obs_data_get_int(item, "height") == canvas_height) {
			obs_data_set_string(item, "name", obs_canvas_get_name(canvas));
			found = item;
			if (linkedScene.isEmpty()) {
				obs_data_array_erase(c, i);
			}
			break;
		}
		obs_data_release(item);
	}
	if (!linkedScene.isEmpty()) {
		if (!found) {
			if (!c) {
				c = obs_data_array_create();
				obs_data_set_array(ss, "canvas", c);
			}
			found = obs_data_create();
			obs_data_set_string(found, "name", obs_canvas_get_name(canvas));
			obs_data_set_int(found, "width", canvas_width);
			obs_data_set_int(found, "height", canvas_height);
			obs_data_array_push_back(c, found);
		}
		obs_data_set_string(found, "scene", linkedScene.toUtf8().constData());
	}
	obs_data_release(ss);
	obs_data_release(found);
	obs_data_array_release(c);

	for (int row = 0; row < sceneList->count(); row++) {
		auto item = sceneList->item(row);
		item->setIcon(QIcon(":/aitum/media/unlinked.svg"));
	}
	UpdateLinkedScenes();
}

void CanvasDock::ShowSourcesContextMenu(obs_sceneitem_t *item)
{
	auto menu = QMenu(this);
	menu.addMenu(CreateAddSourcePopupMenu());
	if (item) {
		AddSceneItemMenuItems(&menu, item);
	}
	menu.exec(QCursor::pos());
}

void CanvasDock::AddSceneItemMenuItems(QMenu *popup, OBSSceneItem sceneItem)
{

	popup->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Rename")), [this, sceneItem] {
		obs_source_t *item_source = obs_source_get_ref(obs_sceneitem_get_source(sceneItem));
		if (!item_source)
			return;
		obs_canvas_t *canvas = obs_source_get_canvas(item_source);
		std::string name = obs_source_get_name(item_source);
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SourceName")), name)) {
				break;
			}
			s = canvas ? obs_canvas_get_source_by_name(canvas, name.c_str()) : obs_get_source_by_name(name.c_str());
			if (s)
				continue;
			obs_source_set_name(item_source, name.c_str());
		} while (s);
		obs_source_release(item_source);
	});
	popup->addAction(
		//removeButton->icon(),
		QString::fromUtf8(obs_frontend_get_locale_string("Remove")), this, [sceneItem] {
			QMessageBox mb(QMessageBox::Question,
				       QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Title")),
				       QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Text"))
					       .arg(QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(sceneItem)))),
				       QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));
			mb.setDefaultButton(QMessageBox::NoButton);
			if (mb.exec() == QMessageBox::Yes) {
				obs_sceneitem_remove(sceneItem);
			}
		});

	popup->addSeparator();
	auto orderMenu = popup->addMenu(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order")));
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveUp")), this,
			     [sceneItem] { obs_sceneitem_set_order(sceneItem, OBS_ORDER_MOVE_UP); });
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveDown")), this,
			     [sceneItem] { obs_sceneitem_set_order(sceneItem, OBS_ORDER_MOVE_DOWN); });
	orderMenu->addSeparator();
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToTop")), this,
			     [sceneItem] { obs_sceneitem_set_order(sceneItem, OBS_ORDER_MOVE_TOP); });
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToBottom")), this,
			     [sceneItem] { obs_sceneitem_set_order(sceneItem, OBS_ORDER_MOVE_BOTTOM); });

	auto transformMenu = popup->addMenu(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform")));
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.EditTransform")),
				 [this, sceneItem] {
					 const auto mainDialog = static_cast<QMainWindow *>(obs_frontend_get_main_window());
					 auto transformDialog = mainDialog->findChild<QDialog *>("OBSBasicTransform");
					 if (!transformDialog) {
						 // make sure there is an item selected on the main canvas before starting the transform dialog
						 const auto currentScene = obs_frontend_preview_program_mode_active()
										   ? obs_frontend_get_current_preview_scene()
										   : obs_frontend_get_current_scene();
						 auto selected = GetSelectedItem(obs_scene_from_source(currentScene));
						 if (!selected) {
							 obs_scene_enum_items(
								 obs_scene_from_source(currentScene),
								 [](obs_scene_t *, obs_sceneitem_t *item, void *) {
									 obs_sceneitem_select(item, true);
									 return false;
								 },
								 nullptr);
						 }
						 obs_source_release(currentScene);
						 QMetaObject::invokeMethod(mainDialog, "on_actionEditTransform_triggered");
						 transformDialog = mainDialog->findChild<QDialog *>("OBSBasicTransform");
					 }
					 if (!transformDialog)
						 return;
					 QMetaObject::invokeMethod(transformDialog, "SetItemQt",
								   Q_ARG(OBSSceneItem, OBSSceneItem(sceneItem)));
				 });
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.ResetTransform")),
				 this, [sceneItem] {
					 obs_sceneitem_set_alignment(sceneItem, OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
					 obs_sceneitem_set_bounds_type(sceneItem, OBS_BOUNDS_NONE);
					 vec2 scale;
					 scale.x = 1.0f;
					 scale.y = 1.0f;
					 obs_sceneitem_set_scale(sceneItem, &scale);
					 vec2 pos;
					 pos.x = 0.0f;
					 pos.y = 0.0f;
					 obs_sceneitem_set_pos(sceneItem, &pos);
					 obs_sceneitem_crop crop = {0, 0, 0, 0};
					 obs_sceneitem_set_crop(sceneItem, &crop);
					 obs_sceneitem_set_rot(sceneItem, 0.0f);
				 });
	transformMenu->addSeparator();
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.Rotate90CW")),
				 this, [this] {
					 float rotation = 90.0f;
					 obs_scene_enum_items(scene, RotateSelectedSources, &rotation);
				 });
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.Rotate90CCW")),
				 this, [this] {
					 float rotation = -90.0f;
					 obs_scene_enum_items(scene, RotateSelectedSources, &rotation);
				 });
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.Rotate180")), this,
				 [this] {
					 float rotation = 180.0f;
					 obs_scene_enum_items(scene, RotateSelectedSources, &rotation);
				 });
	transformMenu->addSeparator();
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.FlipHorizontal")),
				 this, [this] {
					 vec2 scale;
					 vec2_set(&scale, -1.0f, 1.0f);
					 obs_scene_enum_items(scene, MultiplySelectedItemScale, &scale);
				 });
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.FlipVertical")),
				 this, [this] {
					 vec2 scale;
					 vec2_set(&scale, 1.0f, -1.0f);
					 obs_scene_enum_items(scene, MultiplySelectedItemScale, &scale);
				 });
	transformMenu->addSeparator();
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.FitToScreen")),
				 this, [this] {
					 obs_bounds_type boundsType = OBS_BOUNDS_SCALE_INNER;
					 obs_scene_enum_items(scene, CenterAlignSelectedItems, &boundsType);
				 });
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.StretchToScreen")),
				 this, [this] {
					 obs_bounds_type boundsType = OBS_BOUNDS_STRETCH;
					 obs_scene_enum_items(scene, CenterAlignSelectedItems, &boundsType);
				 });
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.CenterToScreen")),
				 this, [this] { CenterSelectedItems(CenterType::Scene); });
	transformMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.VerticalCenter")),
				 this, [this] { CenterSelectedItems(CenterType::Vertical); });
	transformMenu->addAction(
		QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Transform.HorizontalCenter")), this,
		[this] { CenterSelectedItems(CenterType::Horizontal); });

	popup->addSeparator();

	obs_scale_type scaleFilter = obs_sceneitem_get_scale_filter(sceneItem);
	auto scaleMenu = popup->addMenu(QString::fromUtf8(obs_frontend_get_locale_string("ScaleFiltering")));
	auto a = scaleMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Disable")), this,
				      [sceneItem] { obs_sceneitem_set_scale_filter(sceneItem, OBS_SCALE_DISABLE); });
	a->setCheckable(true);
	a->setChecked(scaleFilter == OBS_SCALE_DISABLE);
	a = scaleMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("ScaleFiltering.Point")), this,
				 [sceneItem] { obs_sceneitem_set_scale_filter(sceneItem, OBS_SCALE_POINT); });
	a->setCheckable(true);
	a->setChecked(scaleFilter == OBS_SCALE_POINT);
	a = scaleMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("ScaleFiltering.Bilinear")), this,
				 [sceneItem] { obs_sceneitem_set_scale_filter(sceneItem, OBS_SCALE_BILINEAR); });
	a->setCheckable(true);
	a->setChecked(scaleFilter == OBS_SCALE_BILINEAR);
	a = scaleMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("ScaleFiltering.Bicubic")), this,
				 [sceneItem] { obs_sceneitem_set_scale_filter(sceneItem, OBS_SCALE_BICUBIC); });
	a->setCheckable(true);
	a->setChecked(scaleFilter == OBS_SCALE_BICUBIC);
	a = scaleMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("ScaleFiltering.Lanczos")), this,
				 [sceneItem] { obs_sceneitem_set_scale_filter(sceneItem, OBS_SCALE_LANCZOS); });
	a->setCheckable(true);
	a->setChecked(scaleFilter == OBS_SCALE_LANCZOS);
	a = scaleMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("ScaleFiltering.Area")), this,
				 [sceneItem] { obs_sceneitem_set_scale_filter(sceneItem, OBS_SCALE_AREA); });
	a->setCheckable(true);
	a->setChecked(scaleFilter == OBS_SCALE_AREA);

	auto blendingMode = obs_sceneitem_get_blending_mode(sceneItem);
	auto blendingMenu = popup->addMenu(QString::fromUtf8(obs_frontend_get_locale_string("BlendingMode")));
	a = blendingMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("BlendingMode.Normal")), this,
				    [sceneItem] { obs_sceneitem_set_blending_mode(sceneItem, OBS_BLEND_NORMAL); });
	a->setCheckable(true);
	a->setChecked(blendingMode == OBS_BLEND_NORMAL);

	a = blendingMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("BlendingMode.Additive")), this,
				    [sceneItem] { obs_sceneitem_set_blending_mode(sceneItem, OBS_BLEND_ADDITIVE); });
	a->setCheckable(true);
	a->setChecked(blendingMode == OBS_BLEND_ADDITIVE);

	a = blendingMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("BlendingMode.Subtract")), this,
				    [sceneItem] { obs_sceneitem_set_blending_mode(sceneItem, OBS_BLEND_SUBTRACT); });
	a->setCheckable(true);
	a->setChecked(blendingMode == OBS_BLEND_SUBTRACT);

	a = blendingMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("BlendingMode.Screen")), this,
				    [sceneItem] { obs_sceneitem_set_blending_mode(sceneItem, OBS_BLEND_SCREEN); });
	a->setCheckable(true);
	a->setChecked(blendingMode == OBS_BLEND_SCREEN);

	a = blendingMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("BlendingMode.Multiply")), this,
				    [sceneItem] { obs_sceneitem_set_blending_mode(sceneItem, OBS_BLEND_MULTIPLY); });
	a->setCheckable(true);
	a->setChecked(blendingMode == OBS_BLEND_MULTIPLY);

	a = blendingMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("BlendingMode.Lighten")), this,
				    [sceneItem] { obs_sceneitem_set_blending_mode(sceneItem, OBS_BLEND_LIGHTEN); });
	a->setCheckable(true);
	a->setChecked(blendingMode == OBS_BLEND_LIGHTEN);

	a = blendingMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("BlendingMode.Darken")), this,
				    [sceneItem] { obs_sceneitem_set_blending_mode(sceneItem, OBS_BLEND_DARKEN); });
	a->setCheckable(true);
	a->setChecked(blendingMode == OBS_BLEND_DARKEN);

	popup->addSeparator();
	popup->addMenu(CreateVisibilityTransitionMenu(true, sceneItem));
	popup->addMenu(CreateVisibilityTransitionMenu(false, sceneItem));

	popup->addSeparator();

	auto projectorMenu = popup->addMenu(QString::fromUtf8(obs_frontend_get_locale_string("Projector.Open.Source")));
	AddProjectorMenuMonitors(projectorMenu, this, SLOT(OpenSourceProjector()));
	a = popup->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Projector.Window")));
	connect(a, &QAction::triggered, this, &CanvasDock::OpenSourceProjector);
	a->setProperty("monitor", -1);

	obs_source_t *s = obs_sceneitem_get_source(sceneItem);
	popup->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Screenshot.Source")), this,
			 [s] { obs_frontend_take_source_screenshot(s); });
	popup->addSeparator();
	popup->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Filters")), this,
			 [s] { obs_frontend_open_source_filters(s); });
	a = popup->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Properties")), this,
			     [s] { obs_frontend_open_source_properties(s); });
	a->setEnabled(obs_source_configurable(s));
}

bool CanvasDock::RotateSelectedSources(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, RotateSelectedSources, param);
	if (!obs_sceneitem_selected(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	float rot = *reinterpret_cast<float *>(param);

	vec3 tl = GetItemTL(item);

	rot += obs_sceneitem_get_rot(item);
	if (rot >= 360.0f)
		rot -= 360.0f;
	else if (rot <= -360.0f)
		rot += 360.0f;
	obs_sceneitem_set_rot(item, rot);

	obs_sceneitem_force_update_transform(item);

	SetItemTL(item, tl);

	UNUSED_PARAMETER(scene);
	return true;
};

vec3 CanvasDock::GetItemTL(obs_sceneitem_t *item)
{
	vec3 tl, br;
	GetItemBox(item, tl, br);
	return tl;
}

void CanvasDock::SetItemTL(obs_sceneitem_t *item, const vec3 &tl)
{
	vec3 newTL;
	vec2 pos;

	obs_sceneitem_get_pos(item, &pos);
	newTL = GetItemTL(item);
	pos.x += tl.x - newTL.x;
	pos.y += tl.y - newTL.y;
	obs_sceneitem_set_pos(item, &pos);
}

void CanvasDock::GetItemBox(obs_sceneitem_t *item, vec3 &tl, vec3 &br)
{
	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

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
}

bool CanvasDock::MultiplySelectedItemScale(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	vec2 &mul = *reinterpret_cast<vec2 *>(param);

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, MultiplySelectedItemScale, param);
	if (!obs_sceneitem_selected(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	vec3 tl = GetItemTL(item);

	vec2 scale;
	obs_sceneitem_get_scale(item, &scale);
	vec2_mul(&scale, &scale, &mul);
	obs_sceneitem_set_scale(item, &scale);

	obs_sceneitem_force_update_transform(item);

	SetItemTL(item, tl);

	UNUSED_PARAMETER(scene);
	return true;
}

bool CanvasDock::CenterAlignSelectedItems(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	obs_bounds_type boundsType = *reinterpret_cast<obs_bounds_type *>(param);

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, CenterAlignSelectedItems, param);
	if (!obs_sceneitem_selected(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	obs_source_t *scene_source = obs_scene_get_source(scene);

	obs_transform_info itemInfo;
	vec2_set(&itemInfo.pos, 0.0f, 0.0f);
	vec2_set(&itemInfo.scale, 1.0f, 1.0f);
	itemInfo.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	itemInfo.rot = 0.0f;

	vec2_set(&itemInfo.bounds, float(obs_source_get_base_width(scene_source)), float(obs_source_get_base_height(scene_source)));
	itemInfo.bounds_type = boundsType;
	itemInfo.bounds_alignment = OBS_ALIGN_CENTER;
	itemInfo.crop_to_bounds = obs_sceneitem_get_bounds_crop(item);
	obs_sceneitem_set_info2(item, &itemInfo);

	UNUSED_PARAMETER(scene);
	return true;
}

QMenu *CanvasDock::CreateVisibilityTransitionMenu(bool visible, obs_sceneitem_t *si)
{
	QMenu *menu = new QMenu(QString::fromUtf8(obs_frontend_get_locale_string(visible ? "ShowTransition" : "HideTransition")));

	obs_source_t *curTransition = obs_sceneitem_get_transition(si, visible);
	const char *curId = curTransition ? obs_source_get_id(curTransition) : nullptr;
	int curDuration = (int)obs_sceneitem_get_transition_duration(si, visible);

	if (curDuration <= 0)
		curDuration = obs_frontend_get_transition_duration();

	QSpinBox *duration = new QSpinBox(menu);
	duration->setMinimum(50);
	duration->setSuffix(" ms");
	duration->setMaximum(20000);
	duration->setSingleStep(50);
	duration->setValue(curDuration);

	auto setTransition = [](QAction *a, bool vis, obs_sceneitem_t *si2) {
		std::string id = a->property("transition_id").toString().toUtf8().constData();
		if (id.empty()) {
			obs_sceneitem_set_transition(si2, vis, nullptr);
		} else {
			obs_source_t *tr = obs_sceneitem_get_transition(si2, vis);

			if (!tr || strcmp(id.c_str(), obs_source_get_id(tr)) != 0) {
				QString name = QString::fromUtf8(obs_source_get_name(obs_sceneitem_get_source(si2)));
				name += " ";
				name += QString::fromUtf8(
					obs_frontend_get_locale_string(vis ? "ShowTransition" : "HideTransition"));
				tr = obs_source_create_private(id.c_str(), name.toUtf8().constData(), nullptr);
				obs_sceneitem_set_transition(si2, vis, tr);
				obs_source_release(tr);

				int dur = (int)obs_sceneitem_get_transition_duration(si2, vis);
				if (dur <= 0) {
					dur = obs_frontend_get_transition_duration();
					obs_sceneitem_set_transition_duration(si2, vis, dur);
				}
			}
			if (obs_source_configurable(tr))
				obs_frontend_open_source_properties(tr);
		}
	};
	auto setDuration = [visible, si](int dur) {
		obs_sceneitem_set_transition_duration(si, visible, dur);
	};
	connect(duration, (void (QSpinBox::*)(int))&QSpinBox::valueChanged, setDuration);

	QAction *a = menu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("None")));
	a->setProperty("transition_id", QString::fromUtf8(""));
	a->setCheckable(true);
	a->setChecked(!curId);
	connect(a, &QAction::triggered, std::bind(setTransition, a, visible, si));
	size_t idx = 0;
	const char *id;
	while (obs_enum_transition_types(idx++, &id)) {
		const char *name = obs_source_get_display_name(id);
		const bool match = id && curId && strcmp(id, curId) == 0;
		a = menu->addAction(QString::fromUtf8(name));
		a->setProperty("transition_id", QString::fromUtf8(id));
		a->setCheckable(true);
		a->setChecked(match);
		connect(a, &QAction::triggered, std::bind(setTransition, a, visible, si));
	}

	QWidgetAction *durationAction = new QWidgetAction(menu);
	durationAction->setDefaultWidget(duration);

	menu->addSeparator();
	menu->addAction(durationAction);
	if (curId && obs_is_source_configurable(curId)) {
		menu->addSeparator();
		menu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Properties")), this,
				[curTransition] { obs_frontend_open_source_properties(curTransition); });
	}

	return menu;
}

void CanvasDock::CenterSelectedItems(CenterType centerType)
{
	std::vector<obs_sceneitem_t *> items;
	obs_scene_enum_items(scene, GetSelectedItemsWithSize, &items);
	if (!items.size())
		return;

	// Get center x, y coordinates of items
	vec3 center;

	float top = M_INFINITE;
	float left = M_INFINITE;
	float right = 0.0f;
	float bottom = 0.0f;

	for (auto &item : items) {
		vec3 tl, br;

		GetItemBox(item, tl, br);

		left = (std::min)(tl.x, left);
		top = (std::min)(tl.y, top);
		right = (std::max)(br.x, right);
		bottom = (std::max)(br.y, bottom);
	}

	center.x = (right + left) / 2.0f;
	center.y = (top + bottom) / 2.0f;
	center.z = 0.0f;

	// Get coordinates of screen center
	vec3 screenCenter;
	vec3_set(&screenCenter, float(canvas_width), float(canvas_height), 0.0f);

	vec3_mulf(&screenCenter, &screenCenter, 0.5f);

	// Calculate difference between screen center and item center
	vec3 offset;
	vec3_sub(&offset, &screenCenter, &center);

	// Shift items by offset
	for (auto &item : items) {
		vec3 tl, br;

		GetItemBox(item, tl, br);

		vec3_add(&tl, &tl, &offset);

		vec3 itemTL = GetItemTL(item);

		if (centerType == CenterType::Vertical)
			tl.x = itemTL.x;
		else if (centerType == CenterType::Horizontal)
			tl.y = itemTL.y;

		SetItemTL(item, tl);
	}
}

bool CanvasDock::GetSelectedItemsWithSize(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	auto items = static_cast<std::vector<obs_sceneitem_t *> *>(param);

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, GetSelectedItemsWithSize, param);
	if (!obs_sceneitem_selected(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	vec2 scale;
	obs_sceneitem_get_scale(item, &scale);

	obs_source_t *source = obs_sceneitem_get_source(item);
	const float width = float(obs_source_get_width(source)) * scale.x;
	const float height = float(obs_source_get_height(source)) * scale.y;

	if (width == 0.0f || height == 0.0f)
		return true;

	items->push_back(item);

	UNUSED_PARAMETER(scene);
	return true;
}

void CanvasDock::AddProjectorMenuMonitors(QMenu *parent, QObject *target, const char *slot)
{

	QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QScreen *screen = screens[i];
		QRect screenGeometry = screen->geometry();
		qreal ratio = screen->devicePixelRatio();
		QString name = "";
#if defined(_WIN32) && QT_VERSION < QT_VERSION_CHECK(6, 4, 0)
		QTextStream fullname(&name);
		fullname << GetMonitorName(screen->name());
		fullname << " (";
		fullname << (i + 1);
		fullname << ")";
#elif defined(__APPLE__) || defined(_WIN32)
		name = screen->name();
#else
		name = screen->model().simplified();

		if (name.length() > 1 && name.endsWith("-"))
			name.chop(1);
#endif
		name = name.simplified();

		if (name.length() == 0) {
			name = QString("%1 %2")
				       .arg(QString::fromUtf8(obs_frontend_get_locale_string("Display")))
				       .arg(QString::number(i + 1));
		}
		QString str = QString("%1: %2x%3 @ %4,%5")
				      .arg(name, QString::number(screenGeometry.width() * ratio),
					   QString::number(screenGeometry.height() * ratio), QString::number(screenGeometry.x()),
					   QString::number(screenGeometry.y()));

		QAction *a = parent->addAction(str, target, slot);
		a->setProperty("monitor", i);
	}
}

void CanvasDock::LoadScenes()
{
	/* for (uint32_t i = MAX_CHANNELS - 1; i > 0; i--) {
		auto s = obs_get_output_source(i);
		if (s == nullptr) {
			obs_set_output_source(i, transitionAudioWrapper);
			break;
		}
		obs_source_release(s);
	}*/

	sceneList->clear();

	obs_canvas_enum_scenes(
		canvas,
		[](void *param, obs_source_t *src) {
			auto t = (CanvasDock *)param;

			std::string ssn = obs_canvas_get_name(t->canvas);
			ssn += " ";
			ssn += obs_frontend_get_locale_string("Basic.Hotkeys.SelectScene");
			obs_hotkey_register_source(
				src, "OBSBasic.SelectScene", ssn.c_str(),
				[](void *data, obs_hotkey_id, obs_hotkey_t *key, bool pressed) {
					if (!pressed)
						return;
					auto p = (CanvasDock *)data;
					auto potential_source = (obs_weak_source_t *)obs_hotkey_get_registerer(key);
					OBSSourceAutoRelease source = obs_weak_source_get_source(potential_source);
					if (source) {
						auto sn = QString::fromUtf8(obs_source_get_name(source));
						QMetaObject::invokeMethod(p, "SwitchScene", Q_ARG(QString, sn), Q_ARG(bool, true));
					}
				},
				t);
			auto sh = obs_source_get_signal_handler(src);
			signal_handler_connect(sh, "rename", source_rename, t);
			signal_handler_connect(sh, "remove", source_remove, t);
			QString name = QString::fromUtf8(obs_source_get_name(src));
			auto sli = new QListWidgetItem(name, t->sceneList);
			sli->setIcon(QIcon(":/aitum/media/unlinked.svg"));
			obs_data_t *settings = obs_source_get_settings(src);
			const int order = (int)obs_data_get_int(settings, "order");
			t->sceneList->insertItem(order, sli);
			if ((t->currentSceneName.isEmpty() && obs_data_get_bool(settings, "canvas_active")) ||
			    name == t->currentSceneName) {
				for (int j = 0; j < t->sceneList->count(); j++) {
					auto item = t->sceneList->item(j);
					if (item->text() != name)
						continue;
					t->sceneList->setCurrentItem(item);
				}
			}
			obs_data_release(settings);
			return true;
		},
		this);
	QListWidgetItem *selectedItem = nullptr;
	sceneList->blockSignals(true);
	for (int idx = 0; idx < sceneList->count(); idx++) {
		auto item = sceneList->takeItem(idx);
		auto scene = obs_canvas_get_source_by_name(canvas, item->text().toUtf8().constData());
		auto settings = obs_source_get_settings(scene);
		const int order = (int)obs_data_get_int(settings, "order");
		sceneList->insertItem(order, item);
		if (obs_data_get_bool(settings, "canvas_active")) {
			selectedItem = item;
		}
		obs_data_release(settings);
		obs_source_release(scene);
	}
	sceneList->blockSignals(false);
	if (selectedItem)
		sceneList->setCurrentItem(selectedItem);

	UpdateLinkedScenes();

	if (sceneList->count() == 0) {
		AddScene("", false);
	}

	if (sceneList->currentRow() < 0)
		sceneList->setCurrentRow(0);
}

void CanvasDock::LoadTransitions()
{
	size_t idx = 0;
	const char *id;
	while (obs_enum_transition_types(idx++, &id)) {
		if (obs_is_source_configurable(id))
			continue;
		const char *name = obs_source_get_display_name(id);

		OBSSourceAutoRelease tr = obs_source_create_private(id, name, NULL);
		transitions.emplace_back(tr);

		//signals "transition_stop" and "transition_video_stop"
		//        TransitionFullyStopped TransitionStopped
	}

	obs_data_array_t *transition_array = obs_data_get_array(settings, "transitions");
	if (transition_array) {
		size_t c = obs_data_array_count(transition_array);
		for (size_t i = 0; i < c; i++) {
			obs_data_t *td = obs_data_array_item(transition_array, i);
			if (!td)
				continue;
			OBSSourceAutoRelease transition = obs_load_private_source(td);
			if (transition)
				transitions.emplace_back(transition);

			obs_data_release(td);
		}
		obs_data_array_release(transition_array);
	}

	auto current_transition = GetTransition(obs_data_get_string(settings, "transition"));
	if (!current_transition)
		current_transition = GetTransition(obs_source_get_display_name("fade_transition"));

	SwapTransition(current_transition);

	for (auto t : transitions) {
		auto name = QString::fromUtf8(obs_source_get_name(t));
		transition->addItem(name);
		if (obs_weak_source_references_source(source, t)) {
			transition->setCurrentText(name);
		}
	}
}

void CanvasDock::UpdateLinkedScenes()
{
	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_source_t *src = scenes.sources.array[i];
		obs_data_t *settings = obs_source_get_settings(src);
		auto c = obs_data_get_array(settings, "canvas");
		if (c) {
			const auto count = obs_data_array_count(c);
			for (size_t j = 0; j < count; j++) {
				auto item = obs_data_array_item(c, j);
				if (!item)
					continue;
				if (strcmp(obs_data_get_string(item, "name"), obs_canvas_get_name(canvas)) == 0) {
					auto sn = QString::fromUtf8(obs_data_get_string(item, "scene"));
					auto sil = sceneList->findItems(sn, Qt::MatchExactly);
					foreach(auto si, sil)
					{
						si->setIcon(QIcon(":/aitum/media/linked.svg"));
					}
				} else if (strcmp(obs_data_get_string(item, "name"), "") == 0 &&
					   obs_data_get_int(item, "width") == canvas_width &&
					   obs_data_get_int(item, "height") == canvas_height) {
					obs_data_set_string(item, "name", obs_canvas_get_name(canvas));
					auto sn = QString::fromUtf8(obs_data_get_string(item, "scene"));
					auto sil = sceneList->findItems(sn, Qt::MatchExactly);
					foreach(auto si, sil)
					{
						si->setIcon(QIcon(":/aitum/media/linked.svg"));
					}
				}
				obs_data_release(item);
			}

			obs_data_array_release(c);
		}

		obs_data_release(settings);
	}
	obs_frontend_source_list_free(&scenes);
}

void CanvasDock::source_rename(void *data, calldata_t *calldata)
{
	const auto d = static_cast<CanvasDock *>(data);
	const auto prev_name = QString::fromUtf8(calldata_string(calldata, "prev_name"));
	const auto new_name = QString::fromUtf8(calldata_string(calldata, "new_name"));
	auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!source || !obs_source_is_scene(source))
		return;

	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		const obs_source_t *src = scenes.sources.array[i];
		auto ss = obs_source_get_settings(src);
		auto c = obs_data_get_array(ss, "canvas");
		obs_data_release(ss);
		if (!c)
			continue;
		const auto count = obs_data_array_count(c);
		for (size_t j = 0; j < count; j++) {
			auto item = obs_data_array_item(c, j);
			auto n = QString::fromUtf8(obs_data_get_string(item, "scene"));
			if (n == prev_name) {
				obs_data_set_string(item, "scene", calldata_string(calldata, "new_name"));
			}
			obs_data_release(item);
		}
		obs_data_array_release(c);
	}
	obs_frontend_source_list_free(&scenes);

	for (int i = 0; i < d->sceneList->count(); i++) {
		const auto item = d->sceneList->item(i);
		if (item->text() != prev_name)
			continue;
		item->setText(new_name);
	}
}

void CanvasDock::source_remove(void *data, calldata_t *calldata)
{
	const auto d = static_cast<CanvasDock *>(data);
	const auto source = (obs_source_t *)calldata_ptr(calldata, "source");
	if (!obs_source_is_scene(source))
		return;
	const auto canvas = obs_source_get_canvas(source);
	obs_canvas_release(canvas);
	if (!canvas || canvas != d->canvas)
		return;
	if (obs_weak_source_references_source(d->source, source) || source == obs_scene_get_source(d->scene)) {
		QMetaObject::invokeMethod(d, "SwitchScene", Q_ARG(QString, ""), Q_ARG(bool, false));
	}
	const auto name = QString::fromUtf8(obs_source_get_name(source));
	if (name.isEmpty())
		return;
	QMetaObject::invokeMethod(d, "SceneRemoved", Q_ARG(QString, name));
}

void CanvasDock::SceneRemoved(const QString name)
{
	for (int i = 0; i < sceneList->count(); i++) {
		auto item = sceneList->item(i);
		if (item->text() != name)
			continue;
		sceneList->takeItem(i);
	}
	auto r = sceneList->currentRow();
	auto c = sceneList->count();
	if ((r < 0 && c > 0) || r >= c) {
		sceneList->setCurrentRow(0);
	}
}

void CanvasDock::AddSceneItem(OBSSceneItem item)
{
	obs_scene_t *add_scene = obs_sceneitem_get_scene(item);

	if (scene == add_scene)
		sourceList->Add(item);

	obs_scene_enum_items(add_scene, select_one, (obs_sceneitem_t *)item);
}

bool CanvasDock::select_one(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	obs_sceneitem_t *selectedItem = reinterpret_cast<obs_sceneitem_t *>(param);
	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, select_one, param);

	obs_sceneitem_select(item, (selectedItem == item));

	UNUSED_PARAMETER(scene);
	return true;
}

void CanvasDock::RefreshSources(OBSScene refresh_scene)
{
	if (refresh_scene != scene || sourceList->IgnoreReorder())
		return;

	sourceList->RefreshItems();
}

void CanvasDock::ReorderSources(OBSScene order_scene)
{
	if (order_scene != scene || sourceList->IgnoreReorder())
		return;

	sourceList->ReorderItems();
}

void CanvasDock::SwitchBackToSelectedTransition()
{
	auto tn = transition->currentText().toUtf8();
	auto transition = GetTransition(tn.constData());
	SwapTransition(transition);
}

OBSEventFilter *CanvasDock::BuildEventFilter()
{
	return new OBSEventFilter([this](QObject *obj, QEvent *event) {
		UNUSED_PARAMETER(obj);

		if (!scene)
			return false;
		switch (event->type()) {
		case QEvent::MouseButtonPress:
			return this->HandleMousePressEvent(static_cast<QMouseEvent *>(event));
		case QEvent::MouseButtonRelease:
			return this->HandleMouseReleaseEvent(static_cast<QMouseEvent *>(event));
		//case QEvent::MouseButtonDblClick:			return this->HandleMouseClickEvent(				static_cast<QMouseEvent *>(event));
		case QEvent::MouseMove:
			return this->HandleMouseMoveEvent(static_cast<QMouseEvent *>(event));
		//case QEvent::Enter:
		case QEvent::Leave:
			return this->HandleMouseLeaveEvent(static_cast<QMouseEvent *>(event));
		case QEvent::Wheel:
			return this->HandleMouseWheelEvent(static_cast<QWheelEvent *>(event));
		//case QEvent::FocusIn:
		//case QEvent::FocusOut:
		case QEvent::KeyPress:
			return this->HandleKeyPressEvent(static_cast<QKeyEvent *>(event));
		case QEvent::KeyRelease:
			return this->HandleKeyReleaseEvent(static_cast<QKeyEvent *>(event));
		default:
			return false;
		}
	});
}

bool CanvasDock::HandleMousePressEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QPointF pos = event->position();
#else
	QPointF pos = event->localPos();
#endif

	if (scrollMode && IsFixedScaling() && event->button() == Qt::LeftButton) {
		setCursor(Qt::ClosedHandCursor);
		scrollingFrom.x = (float)pos.x();
		scrollingFrom.y = (float)pos.y();
		return true;
	}

	if (event->button() == Qt::RightButton) {
		scrollMode = false;
		setCursor(Qt::ArrowCursor);
	}

	if (locked)
		return false;

	//float pixelRatio = 1.0f;
	//float x = pos.x() - main->previewX / pixelRatio;
	//float y = pos.y() - main->previewY / pixelRatio;
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
	bool altDown = (modifiers & Qt::AltModifier);
	bool shiftDown = (modifiers & Qt::ShiftModifier);
	bool ctrlDown = (modifiers & Qt::ControlModifier);

	if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton)
		return false;

	if (event->button() == Qt::LeftButton)
		mouseDown = true;

	{
		std::lock_guard<std::mutex> lock(selectMutex);
		selectedItems.clear();
	}

	if (altDown)
		cropping = true;

	if (altDown || shiftDown || ctrlDown) {
		vec2 s;
		SceneFindBoxData sfbd(s, s);

		obs_scene_enum_items(scene, FindSelected, &sfbd);

		std::lock_guard<std::mutex> lock(selectMutex);
		selectedItems = sfbd.sceneItems;
	}
	startPos = GetMouseEventPos(event);

	//vec2_set(&startPos, mouseEvent.x, mouseEvent.y);
	//GetStretchHandleData(startPos, false);

	//vec2_divf(&startPos, &startPos, main->previewScale / pixelRatio);
	startPos.x = std::round(startPos.x);
	startPos.y = std::round(startPos.y);

	mouseOverItems = SelectedAtPos(scene, startPos);
	vec2_zero(&lastMoveOffset);

	mousePos = startPos;

	return true;
}

vec2 CanvasDock::GetMouseEventPos(QMouseEvent *event)
{

	auto s = obs_weak_source_get_source(source);
	uint32_t sourceCX = obs_source_get_width(s);
	if (sourceCX <= 0)
		sourceCX = 1;
	uint32_t sourceCY = obs_source_get_height(s);
	if (sourceCY <= 0)
		sourceCY = 1;
	obs_source_release(s);

	int x, y;
	float scale;

	auto size = preview->size();

	GetScaleAndCenterPos(sourceCX, sourceCY, size.width(), size.height(), x, y, scale);
	//auto newCX = scale * float(sourceCX);
	//auto newCY = scale * float(sourceCY);
	float pixelRatio = GetDevicePixelRatio();

	QPoint qtPos = event->pos();

	vec2 pos;
	vec2_set(&pos, ((float)qtPos.x() - (float)x / pixelRatio) / scale, ((float)qtPos.y() - (float)y / pixelRatio) / scale);

	return pos;
}

bool CanvasDock::SelectedAtPos(obs_scene_t *s, const vec2 &pos)
{
	if (!s)
		return false;

	SceneFindData sfd(pos, false);
	obs_scene_enum_items(s, CheckItemSelected, &sfd);
	return !!sfd.item;
}

bool CanvasDock::CheckItemSelected(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	SceneFindData *data = reinterpret_cast<SceneFindData *>(param);
	matrix4 transform;
	vec3 transformedPos;
	vec3 pos3;

	if (!SceneItemHasVideo(item))
		return true;
	if (obs_sceneitem_is_group(item)) {
		data->group = item;
		obs_sceneitem_group_enum_items(item, CheckItemSelected, param);
		data->group = nullptr;

		if (data->item) {
			return false;
		}
	}

	vec3_set(&pos3, data->pos.x, data->pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	if (data->group) {
		matrix4 parent_transform;
		obs_sceneitem_get_draw_transform(data->group, &parent_transform);
		matrix4_mul(&transform, &transform, &parent_transform);
	}

	matrix4_inv(&transform, &transform);
	vec3_transform(&transformedPos, &pos3, &transform);

	if (transformedPos.x >= 0.0f && transformedPos.x <= 1.0f && transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {
		if (obs_sceneitem_selected(item)) {
			data->item = item;
			return false;
		}
	}

	UNUSED_PARAMETER(scene);
	return true;
}

bool CanvasDock::HandleMouseReleaseEvent(QMouseEvent *event)
{
	if (scrollMode)
		setCursor(Qt::OpenHandCursor);

	if (!mouseDown && event->button() == Qt::RightButton) {
		QMenu popup(this);
		QAction *a =
			popup.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.Main.Preview.Disable")), [this] {
				preview_disabled = !preview_disabled;
				obs_display_set_enabled(preview->GetDisplay(), !preview_disabled);
				preview->setVisible(!preview_disabled);
				previewDisabledWidget->setVisible(preview_disabled);
			});
		auto projectorMenu = popup.addMenu(QString::fromUtf8(obs_frontend_get_locale_string("Projector.Open.Preview")));
		AddProjectorMenuMonitors(projectorMenu, this, SLOT(OpenPreviewProjector()));

		a = popup.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Projector.Window")),
				    [this] { OpenProjector(-1); });

		a = popup.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.LockPreview")), this,
				    [this] { locked = !locked; });
		a->setCheckable(true);
		a->setChecked(locked);

		popup.addAction(GetIconFromType(OBS_ICON_TYPE_IMAGE),
				QString::fromUtf8(obs_frontend_get_locale_string("Screenshot")), this, [this] {
					auto s = obs_weak_source_get_source(source);
					obs_frontend_take_source_screenshot(s);
					obs_source_release(s);
				});

		popup.addMenu(CreateAddSourcePopupMenu());

		popup.addSeparator();

		OBSSceneItem sceneItem = GetSelectedItem();
		if (sceneItem) {
			AddSceneItemMenuItems(&popup, sceneItem);
		}
		popup.exec(QCursor::pos());
		return true;
	}

	if (locked)
		return false;

	if (!mouseDown)
		return false;

	const vec2 pos = GetMouseEventPos(event);

	if (!mouseMoved)
		ProcessClick(pos);

	if (selectionBox) {
		Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();

		bool altDown = modifiers & Qt::AltModifier;
		bool shiftDown = modifiers & Qt::ShiftModifier;
		bool ctrlDown = modifiers & Qt::ControlModifier;

		std::lock_guard<std::mutex> lock(selectMutex);
		if (altDown || ctrlDown || shiftDown) {
			for (size_t i = 0; i < selectedItems.size(); i++) {
				obs_sceneitem_select(selectedItems[i], true);
			}
		}

		for (size_t i = 0; i < hoveredPreviewItems.size(); i++) {
			bool select = true;
			obs_sceneitem_t *item = hoveredPreviewItems[i];

			if (altDown) {
				select = false;
			} else if (ctrlDown) {
				select = !obs_sceneitem_selected(item);
			}

			obs_sceneitem_select(hoveredPreviewItems[i], select);
		}
	}

	if (stretchGroup) {
		obs_sceneitem_defer_group_resize_end(stretchGroup);
	}

	stretchItem = nullptr;
	stretchGroup = nullptr;
	mouseDown = false;
	mouseMoved = false;
	cropping = false;
	selectionBox = false;
	unsetCursor();

	OBSSceneItem item = GetItemAtPos(pos, true);

	std::lock_guard<std::mutex> lock(selectMutex);
	hoveredPreviewItems.clear();
	hoveredPreviewItems.push_back(item);
	selectedItems.clear();

	return true;
}

void CanvasDock::ProcessClick(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();

	if (modifiers & Qt::ControlModifier)
		DoCtrlSelect(pos);
	else
		DoSelect(pos);
}

void CanvasDock::DoSelect(const vec2 &pos)
{
	OBSSceneItem item = GetItemAtPos(pos, true);
	obs_scene_enum_items(scene, select_one, (obs_sceneitem_t *)item);
}

void CanvasDock::DoCtrlSelect(const vec2 &pos)
{
	OBSSceneItem item = GetItemAtPos(pos, false);
	if (!item)
		return;

	bool selected = obs_sceneitem_selected(item);
	obs_sceneitem_select(item, !selected);
}

OBSSceneItem CanvasDock::GetItemAtPos(const vec2 &pos, bool selectBelow)
{
	if (!scene)
		return OBSSceneItem();

	SceneFindData sfd(pos, selectBelow);
	obs_scene_enum_items(scene, FindItemAtPos, &sfd);
	return sfd.item;
}

bool CanvasDock::FindItemAtPos(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	SceneFindData *data = reinterpret_cast<SceneFindData *>(param);
	matrix4 transform;
	matrix4 invTransform;
	vec3 transformedPos;
	vec3 pos3;
	vec3 pos3_;

	if (!SceneItemHasVideo(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;

	vec3_set(&pos3, data->pos.x, data->pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	matrix4_inv(&invTransform, &transform);
	vec3_transform(&transformedPos, &pos3, &invTransform);
	vec3_transform(&pos3_, &transformedPos, &transform);

	if (CloseFloat(pos3.x, pos3_.x) && CloseFloat(pos3.y, pos3_.y) && transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	    transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {
		if (data->selectBelow && obs_sceneitem_selected(item)) {
			if (data->item)
				return false;
			else
				data->selectBelow = false;
		}

		data->item = item;
	}

	UNUSED_PARAMETER(scene);
	return true;
}

bool CanvasDock::HandleMouseMoveEvent(QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QPointF qtPos = event->position();
#else
	QPointF qtPos = event->localPos();
#endif

	float pixelRatio = GetDevicePixelRatio();

	if (scrollMode && event->buttons() == Qt::LeftButton) {
		scrollingOffset.x += pixelRatio * ((float)qtPos.x() - scrollingFrom.x);
		scrollingOffset.y += pixelRatio * ((float)qtPos.y() - scrollingFrom.y);
		scrollingFrom.x = (float)qtPos.x();
		scrollingFrom.y = (float)qtPos.y();
		//emit DisplayResized();
		return true;
	}

	if (locked)
		return true;

	bool updateCursor = false;

	if (mouseDown) {
		vec2 pos = GetMouseEventPos(event);

		if (!mouseMoved && !mouseOverItems && stretchHandle == ItemHandle::None) {
			ProcessClick(startPos);
			mouseOverItems = SelectedAtPos(scene, startPos);
		}

		pos.x = std::round(pos.x);
		pos.y = std::round(pos.y);

		if (stretchHandle != ItemHandle::None) {
			if (obs_sceneitem_locked(stretchItem))
				return true;

			selectionBox = false;

			obs_sceneitem_t *group = obs_sceneitem_get_group(scene, stretchItem);
			if (group) {
				vec3 group_pos;
				vec3_set(&group_pos, pos.x, pos.y, 0.0f);
				vec3_transform(&group_pos, &group_pos, &invGroupTransform);
				pos.x = group_pos.x;
				pos.y = group_pos.y;
			}

			if (stretchHandle == ItemHandle::Rot) {
				RotateItem(pos);
				setCursor(Qt::ClosedHandCursor);
			} else if (cropping)
				CropItem(pos);
			else
				StretchItem(pos);

		} else if (mouseOverItems) {
			if (cursor().shape() != Qt::SizeAllCursor)
				setCursor(Qt::SizeAllCursor);
			selectionBox = false;
			MoveItems(pos);
		} else {
			selectionBox = true;
			if (!mouseMoved)
				DoSelect(startPos);
			BoxItems(startPos, pos);
		}

		mouseMoved = true;
		mousePos = pos;
	} else {
		vec2 pos = GetMouseEventPos(event);
		OBSSceneItem item = GetItemAtPos(pos, true);

		std::lock_guard<std::mutex> lock(selectMutex);
		hoveredPreviewItems.clear();
		hoveredPreviewItems.push_back(item);

		if (!mouseMoved && hoveredPreviewItems.size() > 0) {
			mousePos = pos;
			//float scale = GetDevicePixelRatio();
			//float x = qtPos.x(); // - main->previewX / scale;
			//float y = qtPos.y(); // - main->previewY / scale;
			vec2_set(&startPos, pos.x, pos.y);
			updateCursor = true;
		}
	}

	if (updateCursor) {
		GetStretchHandleData(startPos, true);
		uint32_t stretchFlags = (uint32_t)stretchHandle;
		UpdateCursor(stretchFlags);
	}
	return true;
}

void CanvasDock::RotateItem(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
	bool shiftDown = (modifiers & Qt::ShiftModifier);
	bool ctrlDown = (modifiers & Qt::ControlModifier);

	vec2 pos2;
	vec2_copy(&pos2, &pos);

	float angle = atan2(pos2.y - rotatePoint.y, pos2.x - rotatePoint.x) + RAD(90);

#define ROT_SNAP(rot, thresh)                                    \
	if (abs(angle - RAD((float)rot)) < RAD((float)thresh)) { \
		angle = RAD((float)rot);                         \
	}

	if (shiftDown) {
		for (int i = 0; i <= 360 / 15; i++) {
			ROT_SNAP(i * 15 - 90, 7.5);
		}
	} else if (!ctrlDown) {
		ROT_SNAP(rotateAngle, 5)

		ROT_SNAP(-90, 5)
		ROT_SNAP(-45, 5)
		ROT_SNAP(0, 5)
		ROT_SNAP(45, 5)
		ROT_SNAP(90, 5)
		ROT_SNAP(135, 5)
		ROT_SNAP(180, 5)
		ROT_SNAP(225, 5)
		ROT_SNAP(270, 5)
		ROT_SNAP(315, 5)
	}
#undef ROT_SNAP

	vec2 pos3;
	vec2_copy(&pos3, &offsetPoint);
	RotatePos(&pos3, angle);
	pos3.x += rotatePoint.x;
	pos3.y += rotatePoint.y;

	obs_sceneitem_set_rot(stretchItem, DEG(angle));
	obs_sceneitem_set_pos(stretchItem, &pos3);
}

void CanvasDock::RotatePos(vec2 *pos, float rot)
{
	float cosR = cos(rot);
	float sinR = sin(rot);

	vec2 newPos;

	newPos.x = cosR * pos->x - sinR * pos->y;
	newPos.y = sinR * pos->x + cosR * pos->y;

	vec2_copy(pos, &newPos);
}

static float maxfunc(float x, float y)
{
	return x > y ? x : y;
}

static float minfunc(float x, float y)
{
	return x < y ? x : y;
}

void CanvasDock::CropItem(const vec2 &pos)
{
	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(stretchItem);
	uint32_t stretchFlags = (uint32_t)stretchHandle;
	uint32_t align = obs_sceneitem_get_alignment(stretchItem);
	vec3 tl, br, pos3;

	vec3_zero(&tl);
	vec3_set(&br, stretchItemSize.x, stretchItemSize.y, 0.0f);

	vec3_set(&pos3, pos.x, pos.y, 0.0f);
	vec3_transform(&pos3, &pos3, &screenToItem);

	obs_sceneitem_crop crop = startCrop;
	vec2 scale;

	obs_sceneitem_get_scale(stretchItem, &scale);

	vec2 max_tl;
	vec2 max_br;

	vec2_set(&max_tl, float(-crop.left) * scale.x, float(-crop.top) * scale.y);
	vec2_set(&max_br, stretchItemSize.x + (float)crop.right * scale.x, stretchItemSize.y + (float)crop.bottom * scale.y);

	typedef std::function<float(float, float)> minmax_func_t;

	minmax_func_t min_x = scale.x < 0.0f ? maxfunc : minfunc;
	minmax_func_t min_y = scale.y < 0.0f ? maxfunc : minfunc;
	minmax_func_t max_x = scale.x < 0.0f ? minfunc : maxfunc;
	minmax_func_t max_y = scale.y < 0.0f ? minfunc : maxfunc;

	pos3.x = min_x(pos3.x, max_br.x);
	pos3.x = max_x(pos3.x, max_tl.x);
	pos3.y = min_y(pos3.y, max_br.y);
	pos3.y = max_y(pos3.y, max_tl.y);

	if (stretchFlags & ITEM_LEFT) {
		float maxX = stretchItemSize.x - (2.0f * scale.x);
		pos3.x = tl.x = min_x(pos3.x, maxX);

	} else if (stretchFlags & ITEM_RIGHT) {
		float minX = (2.0f * scale.x);
		pos3.x = br.x = max_x(pos3.x, minX);
	}

	if (stretchFlags & ITEM_TOP) {
		float maxY = stretchItemSize.y - (2.0f * scale.y);
		pos3.y = tl.y = min_y(pos3.y, maxY);

	} else if (stretchFlags & ITEM_BOTTOM) {
		float minY = (2.0f * scale.y);
		pos3.y = br.y = max_y(pos3.y, minY);
	}

#define ALIGN_X (ITEM_LEFT | ITEM_RIGHT)
#define ALIGN_Y (ITEM_TOP | ITEM_BOTTOM)
	vec3 newPos;
	vec3_zero(&newPos);

	uint32_t align_x = (align & ALIGN_X);
	uint32_t align_y = (align & ALIGN_Y);
	if (align_x == (stretchFlags & ALIGN_X) && align_x != 0)
		newPos.x = pos3.x;
	else if (align & ITEM_RIGHT)
		newPos.x = stretchItemSize.x;
	else if (!(align & ITEM_LEFT))
		newPos.x = stretchItemSize.x * 0.5f;

	if (align_y == (stretchFlags & ALIGN_Y) && align_y != 0)
		newPos.y = pos3.y;
	else if (align & ITEM_BOTTOM)
		newPos.y = stretchItemSize.y;
	else if (!(align & ITEM_TOP))
		newPos.y = stretchItemSize.y * 0.5f;
#undef ALIGN_X
#undef ALIGN_Y

	crop = startCrop;

	if (stretchFlags & ITEM_LEFT)
		crop.left += int(std::round(tl.x / scale.x));
	else if (stretchFlags & ITEM_RIGHT)
		crop.right += int(std::round((stretchItemSize.x - br.x) / scale.x));

	if (stretchFlags & ITEM_TOP)
		crop.top += int(std::round(tl.y / scale.y));
	else if (stretchFlags & ITEM_BOTTOM)
		crop.bottom += int(std::round((stretchItemSize.y - br.y) / scale.y));

	vec3_transform(&newPos, &newPos, &itemToScreen);
	newPos.x = std::round(newPos.x);
	newPos.y = std::round(newPos.y);

#if 0
	vec3 curPos;
	vec3_zero(&curPos);
	obs_sceneitem_get_pos(stretchItem, (vec2*)&curPos);
	blog(LOG_DEBUG, "curPos {%d, %d} - newPos {%d, %d}",
			int(curPos.x), int(curPos.y),
			int(newPos.x), int(newPos.y));
	blog(LOG_DEBUG, "crop {%d, %d, %d, %d}",
			crop.left, crop.top,
			crop.right, crop.bottom);
#endif

	obs_sceneitem_defer_update_begin(stretchItem);
	obs_sceneitem_set_crop(stretchItem, &crop);
	if (boundsType == OBS_BOUNDS_NONE)
		obs_sceneitem_set_pos(stretchItem, (vec2 *)&newPos);
	obs_sceneitem_defer_update_end(stretchItem);
}

void CanvasDock::StretchItem(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
	obs_bounds_type boundsType = obs_sceneitem_get_bounds_type(stretchItem);
	uint32_t stretchFlags = (uint32_t)stretchHandle;
	bool shiftDown = (modifiers & Qt::ShiftModifier);
	vec3 tl, br, pos3;

	vec3_zero(&tl);
	vec3_set(&br, stretchItemSize.x, stretchItemSize.y, 0.0f);

	vec3_set(&pos3, pos.x, pos.y, 0.0f);
	vec3_transform(&pos3, &pos3, &screenToItem);

	if (stretchFlags & ITEM_LEFT)
		tl.x = pos3.x;
	else if (stretchFlags & ITEM_RIGHT)
		br.x = pos3.x;

	if (stretchFlags & ITEM_TOP)
		tl.y = pos3.y;
	else if (stretchFlags & ITEM_BOTTOM)
		br.y = pos3.y;

	if (!(modifiers & Qt::ControlModifier))
		SnapStretchingToScreen(tl, br);

	obs_source_t *s = obs_sceneitem_get_source(stretchItem);

	vec2 baseSize;
	vec2_set(&baseSize, float(obs_source_get_width(s)), float(obs_source_get_height(s)));

	vec2 size;
	vec2_set(&size, br.x - tl.x, br.y - tl.y);

	if (boundsType != OBS_BOUNDS_NONE) {
		if (shiftDown)
			ClampAspect(tl, br, size, baseSize);

		if (tl.x > br.x)
			std::swap(tl.x, br.x);
		if (tl.y > br.y)
			std::swap(tl.y, br.y);

		vec2_abs(&size, &size);

		obs_sceneitem_set_bounds(stretchItem, &size);
	} else {
		obs_sceneitem_crop crop;
		obs_sceneitem_get_crop(stretchItem, &crop);

		baseSize.x -= float(crop.left + crop.right);
		baseSize.y -= float(crop.top + crop.bottom);

		if (baseSize.x > 0.0 && baseSize.y > 0.0) {
			if (!shiftDown)
				ClampAspect(tl, br, size, baseSize);

			vec2_div(&size, &size, &baseSize);
			obs_sceneitem_set_scale(stretchItem, &size);
		}
	}

	pos3 = CalculateStretchPos(tl, br);
	vec3_transform(&pos3, &pos3, &itemToScreen);

	vec2 newPos;
	vec2_set(&newPos, std::round(pos3.x), std::round(pos3.y));
	obs_sceneitem_set_pos(stretchItem, &newPos);
}

void CanvasDock::MoveItems(const vec2 &pos)
{
	Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();

	vec2 offset, moveOffset;
	vec2_sub(&offset, &pos, &startPos);
	vec2_sub(&moveOffset, &offset, &lastMoveOffset);

	if (!(modifiers & Qt::ControlModifier))
		SnapItemMovement(moveOffset);

	vec2_add(&lastMoveOffset, &lastMoveOffset, &moveOffset);

	obs_scene_enum_items(scene, move_items, &moveOffset);
}

bool CanvasDock::move_items(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	if (obs_sceneitem_locked(item))
		return true;

	bool selected = obs_sceneitem_selected(item);
	vec2 *offset = reinterpret_cast<vec2 *>(param);

	if (obs_sceneitem_is_group(item) && !selected) {
		matrix4 transform;
		vec3 new_offset;
		vec3_set(&new_offset, offset->x, offset->y, 0.0f);

		obs_sceneitem_get_draw_transform(item, &transform);
		vec4_set(&transform.t, 0.0f, 0.0f, 0.0f, 1.0f);
		matrix4_inv(&transform, &transform);
		vec3_transform(&new_offset, &new_offset, &transform);
		obs_sceneitem_group_enum_items(item, move_items, &new_offset);
	}

	if (selected) {
		vec2 pos;
		obs_sceneitem_get_pos(item, &pos);
		vec2_add(&pos, &pos, offset);
		obs_sceneitem_set_pos(item, &pos);
	}

	UNUSED_PARAMETER(scene);
	return true;
}

void CanvasDock::SnapItemMovement(vec2 &offset)
{
	SelectedItemBounds sib;
	obs_scene_enum_items(scene, AddItemBounds, &sib);

	sib.tl.x += offset.x;
	sib.tl.y += offset.y;
	sib.br.x += offset.x;
	sib.br.y += offset.y;

	vec3 snapOffset = GetSnapOffset(sib.tl, sib.br);

	auto config = obs_frontend_get_user_config();
	if (!config)
		return;

	const bool snap = config_get_bool(config, "BasicWindow", "SnappingEnabled");
	const bool sourcesSnap = config_get_bool(config, "BasicWindow", "SourceSnapping");
	if (snap == false)
		return;
	if (sourcesSnap == false) {
		offset.x += snapOffset.x;
		offset.y += snapOffset.y;
		return;
	}

	const float clampDist = (float)config_get_double(config, "BasicWindow", "SnapDistance") / previewScale;

	OffsetData offsetData;
	offsetData.clampDist = clampDist;
	offsetData.tl = sib.tl;
	offsetData.br = sib.br;
	vec3_copy(&offsetData.offset, &snapOffset);

	obs_scene_enum_items(scene, GetSourceSnapOffset, &offsetData);

	if (fabsf(offsetData.offset.x) > EPSILON || fabsf(offsetData.offset.y) > EPSILON) {
		offset.x += offsetData.offset.x;
		offset.y += offsetData.offset.y;
	} else {
		offset.x += snapOffset.x;
		offset.y += snapOffset.y;
	}
}

void CanvasDock::BoxItems(const vec2 &start_pos, const vec2 &pos)
{
	if (!scene)
		return;

	if (cursor().shape() != Qt::CrossCursor)
		setCursor(Qt::CrossCursor);

	SceneFindBoxData sfbd(start_pos, pos);
	obs_scene_enum_items(scene, FindItemsInBox, &sfbd);

	std::lock_guard<std::mutex> lock(selectMutex);
	hoveredPreviewItems = sfbd.sceneItems;
}

bool CanvasDock::FindItemsInBox(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	SceneFindBoxData *data = reinterpret_cast<SceneFindBoxData *>(param);
	matrix4 transform;
	matrix4 invTransform;
	vec3 transformedPos;
	vec3 pos3;
	vec3 pos3_;

	vec2 pos_min, pos_max;
	vec2_min(&pos_min, &data->startPos, &data->pos);
	vec2_max(&pos_max, &data->startPos, &data->pos);

	const float x1 = pos_min.x;
	const float x2 = pos_max.x;
	const float y1 = pos_min.y;
	const float y2 = pos_max.y;

	if (!SceneItemHasVideo(item))
		return true;
	if (obs_sceneitem_locked(item))
		return true;
	if (!obs_sceneitem_visible(item))
		return true;

	vec3_set(&pos3, data->pos.x, data->pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	matrix4_inv(&invTransform, &transform);
	vec3_transform(&transformedPos, &pos3, &invTransform);
	vec3_transform(&pos3_, &transformedPos, &transform);

	if (CloseFloat(pos3.x, pos3_.x) && CloseFloat(pos3.y, pos3_.y) && transformedPos.x >= 0.0f && transformedPos.x <= 1.0f &&
	    transformedPos.y >= 0.0f && transformedPos.y <= 1.0f) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x > x1 && transform.t.x < x2 && transform.t.y > y1 && transform.t.y < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x + transform.x.x > x1 && transform.t.x + transform.x.x < x2 && transform.t.y + transform.x.y > y1 &&
	    transform.t.y + transform.x.y < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x + transform.y.x > x1 && transform.t.x + transform.y.x < x2 && transform.t.y + transform.y.y > y1 &&
	    transform.t.y + transform.y.y < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x + transform.x.x + transform.y.x > x1 && transform.t.x + transform.x.x + transform.y.x < x2 &&
	    transform.t.y + transform.x.y + transform.y.y > y1 && transform.t.y + transform.x.y + transform.y.y < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (transform.t.x + 0.5 * (transform.x.x + transform.y.x) > x1 &&
	    transform.t.x + 0.5 * (transform.x.x + transform.y.x) < x2 &&
	    transform.t.y + 0.5 * (transform.x.y + transform.y.y) > y1 &&
	    transform.t.y + 0.5 * (transform.x.y + transform.y.y) < y2) {

		data->sceneItems.push_back(item);
		return true;
	}

	if (IntersectBox(transform, x1, x2, y1, y2)) {
		data->sceneItems.push_back(item);
		return true;
	}

	UNUSED_PARAMETER(scene);
	return true;
}

bool CanvasDock::IntersectBox(matrix4 transform, float x1, float x2, float y1, float y2)
{
	float x3, x4, y3, y4;

	x3 = transform.t.x;
	y3 = transform.t.y;
	x4 = x3 + transform.x.x;
	y4 = y3 + transform.x.y;

	if (IntersectLine(x1, x1, x3, x4, y1, y2, y3, y4) || IntersectLine(x1, x2, x3, x4, y1, y1, y3, y4) ||
	    IntersectLine(x2, x2, x3, x4, y1, y2, y3, y4) || IntersectLine(x1, x2, x3, x4, y2, y2, y3, y4))
		return true;

	x4 = x3 + transform.y.x;
	y4 = y3 + transform.y.y;

	if (IntersectLine(x1, x1, x3, x4, y1, y2, y3, y4) || IntersectLine(x1, x2, x3, x4, y1, y1, y3, y4) ||
	    IntersectLine(x2, x2, x3, x4, y1, y2, y3, y4) || IntersectLine(x1, x2, x3, x4, y2, y2, y3, y4))
		return true;

	x3 = transform.t.x + transform.x.x;
	y3 = transform.t.y + transform.x.y;
	x4 = x3 + transform.y.x;
	y4 = y3 + transform.y.y;

	if (IntersectLine(x1, x1, x3, x4, y1, y2, y3, y4) || IntersectLine(x1, x2, x3, x4, y1, y1, y3, y4) ||
	    IntersectLine(x2, x2, x3, x4, y1, y2, y3, y4) || IntersectLine(x1, x2, x3, x4, y2, y2, y3, y4))
		return true;

	x3 = transform.t.x + transform.y.x;
	y3 = transform.t.y + transform.y.y;
	x4 = x3 + transform.x.x;
	y4 = y3 + transform.x.y;

	if (IntersectLine(x1, x1, x3, x4, y1, y2, y3, y4) || IntersectLine(x1, x2, x3, x4, y1, y1, y3, y4) ||
	    IntersectLine(x2, x2, x3, x4, y1, y2, y3, y4) || IntersectLine(x1, x2, x3, x4, y2, y2, y3, y4))
		return true;

	return false;
}

bool CanvasDock::CounterClockwise(float x1, float x2, float x3, float y1, float y2, float y3)
{
	return (y3 - y1) * (x2 - x1) > (y2 - y1) * (x3 - x1);
}

bool CanvasDock::IntersectLine(float x1, float x2, float x3, float x4, float y1, float y2, float y3, float y4)
{
	bool a = CounterClockwise(x1, x2, x3, y1, y2, y3);
	bool b = CounterClockwise(x1, x2, x4, y1, y2, y4);
	bool c = CounterClockwise(x3, x4, x1, y3, y4, y1);
	bool d = CounterClockwise(x3, x4, x2, y3, y4, y2);

	return (a != b) && (c != d);
}

void CanvasDock::GetStretchHandleData(const vec2 &pos, bool ignoreGroup)
{
	if (!scene)
		return;

	HandleFindData hfd(pos, previewScale);
	obs_scene_enum_items(scene, FindHandleAtPos, &hfd);

	stretchItem = std::move(hfd.item);
	stretchHandle = hfd.handle;

	rotateAngle = hfd.angle;
	rotatePoint = hfd.rotatePoint;
	offsetPoint = hfd.offsetPoint;

	if (stretchHandle != ItemHandle::None) {
		matrix4 boxTransform;
		vec3 itemUL;
		float itemRot;

		stretchItemSize = GetItemSize(stretchItem);

		obs_sceneitem_get_box_transform(stretchItem, &boxTransform);
		itemRot = obs_sceneitem_get_rot(stretchItem);
		vec3_from_vec4(&itemUL, &boxTransform.t);

		/* build the item space conversion matrices */
		matrix4_identity(&itemToScreen);
		matrix4_rotate_aa4f(&itemToScreen, &itemToScreen, 0.0f, 0.0f, 1.0f, RAD(itemRot));
		matrix4_translate3f(&itemToScreen, &itemToScreen, itemUL.x, itemUL.y, 0.0f);

		matrix4_identity(&screenToItem);
		matrix4_translate3f(&screenToItem, &screenToItem, -itemUL.x, -itemUL.y, 0.0f);
		matrix4_rotate_aa4f(&screenToItem, &screenToItem, 0.0f, 0.0f, 1.0f, RAD(-itemRot));

		obs_sceneitem_get_crop(stretchItem, &startCrop);
		obs_sceneitem_get_pos(stretchItem, &startItemPos);

		obs_source_t *s = obs_sceneitem_get_source(stretchItem);
		cropSize.x = float(obs_source_get_width(s) - startCrop.left - startCrop.right);
		cropSize.y = float(obs_source_get_height(s) - startCrop.top - startCrop.bottom);

		stretchGroup = obs_sceneitem_get_group(scene, stretchItem);
		if (stretchGroup && !ignoreGroup) {
			obs_sceneitem_get_draw_transform(stretchGroup, &invGroupTransform);
			matrix4_inv(&invGroupTransform, &invGroupTransform);
			obs_sceneitem_defer_group_resize_begin(stretchGroup);
		} else {
			stretchGroup = nullptr;
		}
	}
}

void CanvasDock::UpdateCursor(uint32_t &flags)
{
	if (obs_sceneitem_locked(stretchItem)) {
		unsetCursor();
		return;
	}

	if (!flags && (cursor().shape() != Qt::OpenHandCursor || !scrollMode))
		unsetCursor();
	if (cursor().shape() != Qt::ArrowCursor)
		return;

	if ((flags & ITEM_LEFT && flags & ITEM_TOP) || (flags & ITEM_RIGHT && flags & ITEM_BOTTOM))
		setCursor(Qt::SizeFDiagCursor);
	else if ((flags & ITEM_LEFT && flags & ITEM_BOTTOM) || (flags & ITEM_RIGHT && flags & ITEM_TOP))
		setCursor(Qt::SizeBDiagCursor);
	else if (flags & ITEM_LEFT || flags & ITEM_RIGHT)
		setCursor(Qt::SizeHorCursor);
	else if (flags & ITEM_TOP || flags & ITEM_BOTTOM)
		setCursor(Qt::SizeVerCursor);
	else if (flags & ITEM_ROT)
		setCursor(Qt::OpenHandCursor);
}

void CanvasDock::SnapStretchingToScreen(vec3 &tl, vec3 &br)
{
	uint32_t stretchFlags = (uint32_t)stretchHandle;
	vec3 newTL = GetTransformedPos(tl.x, tl.y, itemToScreen);
	vec3 newTR = GetTransformedPos(br.x, tl.y, itemToScreen);
	vec3 newBL = GetTransformedPos(tl.x, br.y, itemToScreen);
	vec3 newBR = GetTransformedPos(br.x, br.y, itemToScreen);
	vec3 boundingTL;
	vec3 boundingBR;

	vec3_copy(&boundingTL, &newTL);
	vec3_min(&boundingTL, &boundingTL, &newTR);
	vec3_min(&boundingTL, &boundingTL, &newBL);
	vec3_min(&boundingTL, &boundingTL, &newBR);

	vec3_copy(&boundingBR, &newTL);
	vec3_max(&boundingBR, &boundingBR, &newTR);
	vec3_max(&boundingBR, &boundingBR, &newBL);
	vec3_max(&boundingBR, &boundingBR, &newBR);

	vec3 offset = GetSnapOffset(boundingTL, boundingBR);
	vec3_add(&offset, &offset, &newTL);
	vec3_transform(&offset, &offset, &screenToItem);
	vec3_sub(&offset, &offset, &tl);

	if (stretchFlags & ITEM_LEFT)
		tl.x += offset.x;
	else if (stretchFlags & ITEM_RIGHT)
		br.x += offset.x;

	if (stretchFlags & ITEM_TOP)
		tl.y += offset.y;
	else if (stretchFlags & ITEM_BOTTOM)
		br.y += offset.y;
}

void CanvasDock::ClampAspect(vec3 &tl, vec3 &br, vec2 &size, const vec2 &baseSize)
{
	float baseAspect = baseSize.x / baseSize.y;
	float aspect = size.x / size.y;
	uint32_t stretchFlags = (uint32_t)stretchHandle;

	if (stretchHandle == ItemHandle::TopLeft || stretchHandle == ItemHandle::TopRight ||
	    stretchHandle == ItemHandle::BottomLeft || stretchHandle == ItemHandle::BottomRight) {
		if (aspect < baseAspect) {
			if ((size.y >= 0.0f && size.x >= 0.0f) || (size.y <= 0.0f && size.x <= 0.0f))
				size.x = size.y * baseAspect;
			else
				size.x = size.y * baseAspect * -1.0f;
		} else {
			if ((size.y >= 0.0f && size.x >= 0.0f) || (size.y <= 0.0f && size.x <= 0.0f))
				size.y = size.x / baseAspect;
			else
				size.y = size.x / baseAspect * -1.0f;
		}

	} else if (stretchHandle == ItemHandle::TopCenter || stretchHandle == ItemHandle::BottomCenter) {
		if ((size.y >= 0.0f && size.x >= 0.0f) || (size.y <= 0.0f && size.x <= 0.0f))
			size.x = size.y * baseAspect;
		else
			size.x = size.y * baseAspect * -1.0f;

	} else if (stretchHandle == ItemHandle::CenterLeft || stretchHandle == ItemHandle::CenterRight) {
		if ((size.y >= 0.0f && size.x >= 0.0f) || (size.y <= 0.0f && size.x <= 0.0f))
			size.y = size.x / baseAspect;
		else
			size.y = size.x / baseAspect * -1.0f;
	}

	size.x = std::round(size.x);
	size.y = std::round(size.y);

	if (stretchFlags & ITEM_LEFT)
		tl.x = br.x - size.x;
	else if (stretchFlags & ITEM_RIGHT)
		br.x = tl.x + size.x;

	if (stretchFlags & ITEM_TOP)
		tl.y = br.y - size.y;
	else if (stretchFlags & ITEM_BOTTOM)
		br.y = tl.y + size.y;
}

vec3 CanvasDock::CalculateStretchPos(const vec3 &tl, const vec3 &br)
{
	uint32_t alignment = obs_sceneitem_get_alignment(stretchItem);
	vec3 pos;

	vec3_zero(&pos);

	if (alignment & OBS_ALIGN_LEFT)
		pos.x = tl.x;
	else if (alignment & OBS_ALIGN_RIGHT)
		pos.x = br.x;
	else
		pos.x = (br.x - tl.x) * 0.5f + tl.x;

	if (alignment & OBS_ALIGN_TOP)
		pos.y = tl.y;
	else if (alignment & OBS_ALIGN_BOTTOM)
		pos.y = br.y;
	else
		pos.y = (br.y - tl.y) * 0.5f + tl.y;

	return pos;
}

bool CanvasDock::AddItemBounds(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	SelectedItemBounds *data = reinterpret_cast<SelectedItemBounds *>(param);
	vec3 t[4];

	auto add_bounds = [data, &t]() {
		for (const vec3 &v : t) {
			if (data->first) {
				vec3_copy(&data->tl, &v);
				vec3_copy(&data->br, &v);
				data->first = false;
			} else {
				vec3_min(&data->tl, &data->tl, &v);
				vec3_max(&data->br, &data->br, &v);
			}
		}
	};

	if (obs_sceneitem_is_group(item)) {
		SelectedItemBounds sib;
		obs_sceneitem_group_enum_items(item, AddItemBounds, &sib);

		if (!sib.first) {
			matrix4 xform;
			obs_sceneitem_get_draw_transform(item, &xform);

			vec3_set(&t[0], sib.tl.x, sib.tl.y, 0.0f);
			vec3_set(&t[1], sib.tl.x, sib.br.y, 0.0f);
			vec3_set(&t[2], sib.br.x, sib.tl.y, 0.0f);
			vec3_set(&t[3], sib.br.x, sib.br.y, 0.0f);
			vec3_transform(&t[0], &t[0], &xform);
			vec3_transform(&t[1], &t[1], &xform);
			vec3_transform(&t[2], &t[2], &xform);
			vec3_transform(&t[3], &t[3], &xform);
			add_bounds();
		}
	}
	if (!obs_sceneitem_selected(item))
		return true;

	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	t[0] = GetTransformedPos(0.0f, 0.0f, boxTransform);
	t[1] = GetTransformedPos(1.0f, 0.0f, boxTransform);
	t[2] = GetTransformedPos(0.0f, 1.0f, boxTransform);
	t[3] = GetTransformedPos(1.0f, 1.0f, boxTransform);
	add_bounds();

	UNUSED_PARAMETER(scene);
	return true;
}

vec3 CanvasDock::GetSnapOffset(const vec3 &tl, const vec3 &br)
{
	auto s = obs_weak_source_get_source(source);
	vec2 screenSize;
	screenSize.x = (float)obs_source_get_base_width(s);
	screenSize.y = (float)obs_source_get_base_height(s);
	obs_source_release(s);
	vec3 clampOffset;

	vec3_zero(&clampOffset);

	auto config = obs_frontend_get_user_config();
	if (!config)
		return clampOffset;

	const bool snap = config_get_bool(config, "BasicWindow", "SnappingEnabled");
	if (snap == false)
		return clampOffset;

	const bool screenSnap = config_get_bool(config, "BasicWindow", "ScreenSnapping");
	const bool centerSnap = config_get_bool(config, "BasicWindow", "CenterSnapping");

	const float clampDist = (float)config_get_double(config, "BasicWindow", "SnapDistance") / previewScale;
	const float centerX = br.x - (br.x - tl.x) / 2.0f;
	const float centerY = br.y - (br.y - tl.y) / 2.0f;

	// Left screen edge.
	if (screenSnap && fabsf(tl.x) < clampDist)
		clampOffset.x = -tl.x;
	// Right screen edge.
	if (screenSnap && fabsf(clampOffset.x) < EPSILON && fabsf(screenSize.x - br.x) < clampDist)
		clampOffset.x = screenSize.x - br.x;
	// Horizontal center.
	if (centerSnap && fabsf(screenSize.x - (br.x - tl.x)) > clampDist && fabsf(screenSize.x / 2.0f - centerX) < clampDist)
		clampOffset.x = screenSize.x / 2.0f - centerX;

	// Top screen edge.
	if (screenSnap && fabsf(tl.y) < clampDist)
		clampOffset.y = -tl.y;
	// Bottom screen edge.
	if (screenSnap && fabsf(clampOffset.y) < EPSILON && fabsf(screenSize.y - br.y) < clampDist)
		clampOffset.y = screenSize.y - br.y;
	// Vertical center.
	if (centerSnap && fabsf(screenSize.y - (br.y - tl.y)) > clampDist && fabsf(screenSize.y / 2.0f - centerY) < clampDist)
		clampOffset.y = screenSize.y / 2.0f - centerY;

	return clampOffset;
}

bool CanvasDock::GetSourceSnapOffset(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	OffsetData *data = reinterpret_cast<OffsetData *>(param);

	if (obs_sceneitem_selected(item))
		return true;

	matrix4 boxTransform;
	obs_sceneitem_get_box_transform(item, &boxTransform);

	vec3 t[4] = {GetTransformedPos(0.0f, 0.0f, boxTransform), GetTransformedPos(1.0f, 0.0f, boxTransform),
		     GetTransformedPos(0.0f, 1.0f, boxTransform), GetTransformedPos(1.0f, 1.0f, boxTransform)};

	bool first = true;
	vec3 tl, br;
	vec3_zero(&tl);
	vec3_zero(&br);
	for (const vec3 &v : t) {
		if (first) {
			vec3_copy(&tl, &v);
			vec3_copy(&br, &v);
			first = false;
		} else {
			vec3_min(&tl, &tl, &v);
			vec3_max(&br, &br, &v);
		}
	}

	// Snap to other source edges
#define EDGE_SNAP(l, r, x, y)                                                                                              \
	do {                                                                                                               \
		double dist = fabsf(l.x - data->r.x);                                                                      \
		if (dist < data->clampDist && fabsf(data->offset.x) < EPSILON && data->tl.y < br.y && data->br.y > tl.y && \
		    (fabsf(data->offset.x) > dist || data->offset.x < EPSILON))                                            \
			data->offset.x = l.x - data->r.x;                                                                  \
	} while (false)

	EDGE_SNAP(tl, br, x, y);
	EDGE_SNAP(tl, br, y, x);
	EDGE_SNAP(br, tl, x, y);
	EDGE_SNAP(br, tl, y, x);
#undef EDGE_SNAP

	UNUSED_PARAMETER(scene);
	return true;
}

bool CanvasDock::FindHandleAtPos(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	HandleFindData &data = *reinterpret_cast<HandleFindData *>(param);

	if (!obs_sceneitem_selected(item)) {
		if (obs_sceneitem_is_group(item)) {
			HandleFindData newData(data, item);
			newData.angleOffset = obs_sceneitem_get_rot(item);

			obs_sceneitem_group_enum_items(item, FindHandleAtPos, &newData);

			data.item = newData.item;
			data.handle = newData.handle;
			data.angle = newData.angle;
			data.rotatePoint = newData.rotatePoint;
			data.offsetPoint = newData.offsetPoint;
		}

		return true;
	}

	matrix4 transform;
	vec3 pos3;
	float closestHandle = data.radius;

	vec3_set(&pos3, data.pos.x, data.pos.y, 0.0f);

	obs_sceneitem_get_box_transform(item, &transform);

	auto TestHandle = [&](float x, float y, ItemHandle handle) {
		vec3 handlePos = GetTransformedPos(x, y, transform);
		vec3_transform(&handlePos, &handlePos, &data.parent_xform);

		float dist = vec3_dist(&handlePos, &pos3);
		if (dist < data.radius) {
			if (dist < closestHandle) {
				closestHandle = dist;
				data.handle = handle;
				data.item = item;
			}
		}
	};

	TestHandle(0.0f, 0.0f, ItemHandle::TopLeft);
	TestHandle(0.5f, 0.0f, ItemHandle::TopCenter);
	TestHandle(1.0f, 0.0f, ItemHandle::TopRight);
	TestHandle(0.0f, 0.5f, ItemHandle::CenterLeft);
	TestHandle(1.0f, 0.5f, ItemHandle::CenterRight);
	TestHandle(0.0f, 1.0f, ItemHandle::BottomLeft);
	TestHandle(0.5f, 1.0f, ItemHandle::BottomCenter);
	TestHandle(1.0f, 1.0f, ItemHandle::BottomRight);

	vec2 rotHandleOffset;
	vec2_set(&rotHandleOffset, 0.0f, HANDLE_RADIUS * data.radius * 1.5f - data.radius);
	RotatePos(&rotHandleOffset, atan2(transform.x.y, transform.x.x));
	RotatePos(&rotHandleOffset, RAD(data.angleOffset));

	vec3 handlePos = GetTransformedPos(0.5f, 0.0f, transform);
	vec3_transform(&handlePos, &handlePos, &data.parent_xform);
	handlePos.x -= rotHandleOffset.x;
	handlePos.y -= rotHandleOffset.y;

	float dist = vec3_dist(&handlePos, &pos3);
	if (dist < data.radius) {
		if (dist < closestHandle) {
			closestHandle = dist;
			data.item = item;
			data.angle = obs_sceneitem_get_rot(item);
			data.handle = ItemHandle::Rot;

			vec2_set(&data.rotatePoint, transform.t.x + transform.x.x / 2 + transform.y.x / 2,
				 transform.t.y + transform.x.y / 2 + transform.y.y / 2);

			obs_sceneitem_get_pos(item, &data.offsetPoint);
			data.offsetPoint.x -= data.rotatePoint.x;
			data.offsetPoint.y -= data.rotatePoint.y;

			RotatePos(&data.offsetPoint, -RAD(obs_sceneitem_get_rot(item)));
		}
	}

	UNUSED_PARAMETER(scene);
	return true;
}

bool CanvasDock::HandleMouseLeaveEvent(QMouseEvent *event)
{
	UNUSED_PARAMETER(event);
	std::lock_guard<std::mutex> lock(selectMutex);
	if (!selectionBox)
		hoveredPreviewItems.clear();
	return true;
}

bool CanvasDock::HandleMouseWheelEvent(QWheelEvent *event)
{
	UNUSED_PARAMETER(event);
	return true;
}

bool CanvasDock::HandleKeyPressEvent(QKeyEvent *event)
{
	UNUSED_PARAMETER(event);
	return true;
}

bool CanvasDock::HandleKeyReleaseEvent(QKeyEvent *event)
{
	UNUSED_PARAMETER(event);
	return true;
}

void CanvasDock::AddSourceFromAction()
{
	QAction *a = qobject_cast<QAction *>(sender());
	if (!a)
		return;

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
		if (obs_get_source_output_flags(id) & OBS_SOURCE_REQUIRES_CANVAS) {
			while ((s = obs_canvas_get_source_by_name(canvas, text.toUtf8().constData()))) {
				text = QString("%1 %2").arg(placeHolderText).arg(i++);
			}
			created_source = obs_scene_get_source(obs_canvas_scene_create(canvas, text.toUtf8().constData()));
		} else {
			while ((s = obs_get_source_by_name(text.toUtf8().constData()))) {
				text = QString("%1 %2").arg(placeHolderText).arg(i++);
			}
			created_source = obs_source_create(id, text.toUtf8().constData(), nullptr, nullptr);
		}
		obs_scene_add(scene, created_source);
		if (obs_source_configurable(created_source)) {
			obs_frontend_open_source_properties(created_source);
		}
		obs_source_release(created_source);
	}
}

void CanvasDock::MainSceneChanged()
{
	auto current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		//if (linkedButton)
		//	linkedButton->setChecked(false);
		return;
	}

	auto ss = obs_source_get_settings(current_scene);
	obs_source_release(current_scene);
	auto c = obs_data_get_array(ss, "canvas");
	obs_data_release(ss);
	if (!c) {
		//if (linkedButton)
		//	linkedButton->setChecked(false);
		return;
	}
	const auto count = obs_data_array_count(c);
	obs_data_t *found = nullptr;
	for (size_t i = 0; i < count; i++) {
		auto item = obs_data_array_item(c, i);
		if (!item)
			continue;
		if (strcmp(obs_data_get_string(item, "name"), obs_canvas_get_name(canvas)) == 0) {
			found = item;
			break;
		} else if (strcmp(obs_data_get_string(item, "name"), "") == 0 && obs_data_get_int(item, "width") == canvas_width &&
			   obs_data_get_int(item, "height") == canvas_height) {
			obs_data_set_string(item, "name", obs_canvas_get_name(canvas));
			found = item;
			break;
		}
		obs_data_release(item);
	}
	if (found) {
		auto sn = QString::fromUtf8(obs_data_get_string(found, "scene"));
		SwitchScene(sn);
		//if (linkedButton)
		//	linkedButton->setChecked(true);
	} // else if (linkedButton) {
	//	linkedButton->setChecked(false);
	//}
	obs_data_release(found);
	obs_data_array_release(c);
}

void CanvasDock::save_load(obs_data_t *save_data, bool saving, void *param)
{
	UNUSED_PARAMETER(save_data);
	if (!saving)
		return;
	CanvasDock *window = static_cast<CanvasDock *>(param);
	auto c = window->sceneList->count();
	for (int row = 0; row < c; row++) {
		auto scene_name = window->sceneList->item(row)->text();
		auto scene = obs_canvas_get_source_by_name(window->canvas, scene_name.toUtf8().constData());
		if (scene) {
			auto settings = obs_source_get_settings(scene);
			obs_data_set_int(settings, "order", row);
			obs_data_set_bool(settings, "canvas_active", scene_name == window->currentSceneName);
			obs_data_release(settings);
			obs_source_release(scene);
		}
	}
}

void CanvasDock::LoadMode(int index)
{
	auto state = "";
	if (index == 0) {
		state = obs_data_get_string(settings, "panel_split_live");
	} else if (index == 1) {
		state = obs_data_get_string(settings, "panel_split_build");
	} else if (index == 2) {
		state = obs_data_get_string(settings, "panel_split_design");
	}
	if (state[0] == '\0')
		state = obs_data_get_string(settings, "panel_split");

	if (state[0] != '\0')
		panel_split->restoreState(QByteArray::fromBase64(state));

	state = "";
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

void CanvasDock::UpdateSettings(obs_data_t *s)
{
	if (s) {
		obs_data_release(settings);
		settings = s;
	}

	auto c = color_from_int(obs_data_get_int(settings, "color"));
	setStyleSheet(QString::fromUtf8("#contextContainer { border: 2px solid %1}").arg(c.name(QColor::HexRgb)));

	canvas_width = (uint32_t)obs_data_get_int(settings, "width");
	if (canvas_width < 1)
		canvas_width = 1080;
	canvas_height = (uint32_t)obs_data_get_int(settings, "height");
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
}

void CanvasDock::reset_live_state()
{
	panel_split->setSizes({1, 0, 0});
	canvas_split->setSizes({1, 1});
}

void CanvasDock::reset_build_state()
{
	canvas_split->setSizes({1, 1});
	panel_split->setSizes({1, 1, 1});
}

void CanvasDock::LogScenes()
{
	blog(LOG_INFO, "------------------------------------------------");
	blog(LOG_INFO, "[Aitum Stream Suite] Canvas '%s' scenes:", obs_canvas_get_name(canvas));
	for (int j = 0; j < sceneList->count(); j++) {
		auto item = sceneList->item(j);
		blog(LOG_INFO, "- scene '%s':", item->text().toUtf8().constData());
		auto scene = obs_canvas_get_scene_by_name(canvas, item->text().toUtf8().constData());
		obs_scene_enum_items(scene, LogSceneItem, (void *)(intptr_t)1);
		obs_source_enum_filters(obs_scene_get_source(scene), LogFilter, (void *)(intptr_t)1);

		obs_scene_release(scene);
	}
	blog(LOG_INFO, "------------------------------------------------");
}

bool CanvasDock::LogSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *v_val)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	const char *id = obs_source_get_id(source);
	int indent_count = (int)(intptr_t)v_val;
	std::string indent;

	for (int i = 0; i < indent_count; i++)
		indent += "    ";

	blog(LOG_INFO, "%s- source: '%s' (%s)", indent.c_str(), name, id);

	if (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) {
		const uint32_t all_mixers = (1 << MAX_AUDIO_MIXES) - 1;
		uint32_t mixers = obs_source_get_audio_mixers(source);
		if (mixers == 0) {
			blog(LOG_INFO, "    %s- audio tracks: none", indent.c_str());
		} else if ((mixers & all_mixers) != all_mixers) {
			std::string tracks;
			for (uint32_t i = 0; i < MAX_AUDIO_MIXES; i++) {
				if (mixers & (1 << i)) {
					tracks += " ";
					tracks += std::to_string(i + 1);
				}
			}
			blog(LOG_INFO, "    %s- audio tracks:%s", indent.c_str(), tracks.c_str());
		}

		obs_monitoring_type monitoring_type = obs_source_get_monitoring_type(source);

		if (monitoring_type != OBS_MONITORING_TYPE_NONE) {
			const char *type = (monitoring_type == OBS_MONITORING_TYPE_MONITOR_ONLY) ? "monitor only"
												 : "monitor and output";

			blog(LOG_INFO, "    %s- monitoring: %s", indent.c_str(), type);
		}
	}
	int child_indent = 1 + indent_count;
	obs_source_enum_filters(source, LogFilter, (void *)(intptr_t)child_indent);

	obs_source_t *show_tn = obs_sceneitem_get_transition(item, true);
	obs_source_t *hide_tn = obs_sceneitem_get_transition(item, false);
	if (show_tn)
		blog(LOG_INFO, "    %s- show: '%s' (%s)", indent.c_str(), obs_source_get_name(show_tn), obs_source_get_id(show_tn));
	if (hide_tn)
		blog(LOG_INFO, "    %s- hide: '%s' (%s)", indent.c_str(), obs_source_get_name(hide_tn), obs_source_get_id(hide_tn));

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, LogSceneItem, (void *)(intptr_t)child_indent);
	return true;
}

void CanvasDock::LogFilter(obs_source_t *, obs_source_t *filter, void *v_val)
{
	const char *name = obs_source_get_name(filter);
	const char *id = obs_source_get_id(filter);
	int val = (int)(intptr_t)v_val;
	std::string indent;

	for (int i = 0; i < val; i++)
		indent += "    ";

	blog(LOG_INFO, "%s- filter: '%s' (%s)", indent.c_str(), name, id);
}

void CanvasDock::OpenPreviewProjector()
{
	int monitor = sender()->property("monitor").toInt();
	OpenProjector(monitor);
}

void CanvasDock::OpenSourceProjector()
{
	int monitor = sender()->property("monitor").toInt();
	if (monitor > 9 || monitor > QGuiApplication::screens().size() - 1)
		return;
	OBSSceneItem item = GetSelectedItem();
	if (!item)
		return;

	obs_source_t *open_source = obs_sceneitem_get_source(item);
	if (!open_source)
		return;
	if (obs_source_get_output_flags(open_source) & OBS_SOURCE_REQUIRES_CANVAS) {
		auto config = obs_frontend_get_user_config();
		if (config) {
			bool closeProjectors = config_get_bool(config, "BasicWindow", "CloseExistingProjectors");

			if (closeProjectors && monitor > -1) {
				for (size_t i = projectors.size(); i > 0; i--) {
					size_t idx = i - 1;
					if (projectors[idx]->GetMonitor() == monitor)
						DeleteProjector(projectors[idx]);
				}
			}
		}

		OBSProjector *projector =
			new OBSProjector(nullptr, open_source, monitor, [this](OBSProjector *p) { DeleteProjector(p); });

		projectors.emplace_back(projector);
	} else {
		obs_frontend_open_projector("Source", monitor, nullptr, obs_source_get_name(open_source));
	}
}

void CanvasDock::DeleteProjector(OBSProjector *projector)
{
	for (size_t i = 0; i < projectors.size(); i++) {
		if (projectors[i] == projector) {
			projectors[i]->deleteLater();
			projectors.erase(projectors.begin() + i);
			break;
		}
	}
}

OBSProjector *CanvasDock::OpenProjector(int monitor)
{
	/* seriously?  10 monitors? */
	if (monitor > 9 || monitor > QGuiApplication::screens().size() - 1)
		return nullptr;
	auto config = obs_frontend_get_user_config();
	if (!config)
		return nullptr;

	bool closeProjectors = config_get_bool(config, "BasicWindow", "CloseExistingProjectors");

	if (closeProjectors && monitor > -1) {
		for (size_t i = projectors.size(); i > 0; i--) {
			size_t idx = i - 1;
			if (projectors[idx]->GetMonitor() == monitor)
				DeleteProjector(projectors[idx]);
		}
	}

	OBSProjector *projector = new OBSProjector(canvas, nullptr, monitor, [this](OBSProjector *p) { DeleteProjector(p); });

	projectors.emplace_back(projector);

	return projector;
}
