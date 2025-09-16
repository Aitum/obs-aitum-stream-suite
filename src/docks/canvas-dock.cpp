#include "../dialogs/name-dialog.hpp"
#include "../utils/widgets/source-tree.hpp"
#include "../utils/widgets/switching-splitter.hpp"
#include "canvas-dock.hpp"
#include <QComboBox>
#include <QDockWidget>
#include <QGuiApplication>
#include <QGroupBox>
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

#define HANDLE_RADIUS 4.0f
#define HELPER_ROT_BREAKPONT 45.0f
#define SPACER_LABEL_MARGIN 6.0f

CanvasDock::CanvasDock(obs_data_t *settings_, QWidget *parent)
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
	if (!canvas_width)
		canvas_width = 1080;
	canvas_height = (uint32_t)obs_data_get_int(settings, "height");
	if (!canvas_height)
		canvas_height = 1920;

	canvas_name = obs_data_get_string(settings, "name");
	if (canvas_name.empty())
		canvas_name = "Vertical";

	canvas = obs_get_canvas_by_uuid(obs_data_get_string(settings, "uuid"));
	if (canvas) {
		std::string name = obs_canvas_get_name(canvas);
		if (name != canvas_name) {
			obs_canvas_set_name(canvas, canvas_name.c_str());
		}
	} else {
		canvas = obs_get_canvas_by_name(canvas_name.c_str());
	}
	if (canvas)
	{
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
		canvas = obs_frontend_add_canvas(canvas_name.c_str(), &ovi, PROGRAM);
		if (canvas) {
			obs_data_set_string(settings, "uuid", obs_canvas_get_uuid(canvas));
			obs_data_set_int(settings, "width", canvas_width);
			obs_data_set_int(settings, "height", canvas_height);
		}
	}

	canvas_split->addWidget(preview);

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
	//preview->installEventFilter(eventFilter.get());

	preview->show();
	connect(preview, &OBSQTDisplay::DisplayCreated,
		[this]() { obs_display_add_draw_callback(preview->GetDisplay(), DrawPreview, this); });

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
		obs_source_t *source = obs_get_source_by_name(item->text().toUtf8().constData());
		if (!source)
			return;
		std::string name = obs_source_get_name(source);
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SceneName")), name)) {
				break;
			}
			s = obs_get_source_by_name(name.c_str());
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
				       auto s = obs_get_source_by_name(item->text().toUtf8().constData());
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
		&selectMutex, &hoveredPreviewItems, [](void *param) { return ((CanvasDock *)param)->scene; }, this, this);
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
		std::string name = obs_source_get_name(source);
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SourceName")), name)) {
				break;
			}
			s = obs_get_source_by_name(name.c_str());
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

	auto state = obs_data_get_string(settings, "panel_split");
	if (state and strlen(state) > 0)
		panel_split->restoreState(QByteArray::fromBase64(state));

	state = obs_data_get_string(settings, "canvas_split");
	if (state and strlen(state) > 0)
		canvas_split->restoreState(QByteArray::fromBase64(state));

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

	for (auto t : transitions) {
		auto name = QString::fromUtf8(obs_source_get_name(t));
		transition->addItem(name);
		if (obs_weak_source_references_source(source, t)) {
			transition->setCurrentText(name);
			bool config = obs_is_source_configurable(obs_source_get_unversioned_id(t));
			removeButton->setEnabled(config);
			propsButton->setEnabled(config);
		}
	}
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
	if (!window)
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
	SaveSettings();
	obs_data_release(settings);
	obs_enter_graphics();
	gs_vertexbuffer_destroy(box);
	obs_leave_graphics();
	obs_canvas_release(canvas);
}

void CanvasDock::SaveSettings()
{
	auto state = canvas_split->saveState();
	auto b64 = state.toBase64();
	auto state_chars = b64.constData();
	obs_data_set_string(settings, "canvas_split", state_chars);
	state = panel_split->saveState();
	b64 = state.toBase64();
	state_chars = b64.constData();
	obs_data_set_string(settings, "panel_split", state_chars);
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

inline QColor CanvasDock::color_from_int(long long val)
{
	return QColor(val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
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
	auto sl = GetGlobalScenesList();
	int row = -1;
	bool hidden = false;
	bool selected = false;
	for (int i = 0; i < sl->count(); i++) {
		auto item = sl->item(i);
		if (item->text() == canvasItem->text()) {
			row = i;
			hidden = item->isHidden();
			selected = item->isSelected();
			break;
		}
	}
	if (row < 0 || row >= sl->count())
		return;

	sl->blockSignals(true);
	QListWidgetItem *item = sl->takeItem(row);
	if (relative) {
		sl->insertItem(row + offset, item);
	} else if (offset == 0) {
		sl->insertItem(offset, item);
	} else {
		sl->insertItem(sl->count(), item);
	}
	item->setHidden(hidden);
	item->setSelected(selected);
	sl->blockSignals(false);

	if (idx == invalidIdx)
		return;

	sceneList->blockSignals(true);
	item = sceneList->takeItem(idx);
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

QIcon create2StateIcon(QString fileOn, QString fileOff);

void CanvasDock::AddScene(QString duplicate, bool ask_name)
{
	std::string name = duplicate.isEmpty() ? obs_module_text("Scene") : duplicate.toUtf8().constData();
	obs_source_t *s = obs_get_source_by_name(name.c_str());
	int i = 0;
	while (s) {
		obs_source_release(s);
		i++;
		name = obs_module_text("Scene");
		name += " ";
		name += std::to_string(i);
		s = obs_get_source_by_name(name.c_str());
	}
	do {
		obs_source_release(s);
		if (ask_name && !NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SceneName")), name)) {
			break;
		}
		s = obs_get_source_by_name(name.c_str());
		if (s)
			continue;

		obs_source_t *new_scene = nullptr;
		if (!duplicate.isEmpty()) {
			auto origSceneSource = obs_get_source_by_name(duplicate.toUtf8().constData());
			if (origSceneSource) {
				auto origScene = obs_scene_from_source(origSceneSource);
				if (origScene) {
					new_scene = obs_scene_get_source(
						obs_scene_duplicate(origScene, name.c_str(), OBS_SCENE_DUP_REFS));
				}
				obs_source_release(origSceneSource);
				if (new_scene) {
					obs_source_save(new_scene);
					obs_data_t *settings = obs_source_get_settings(new_scene);
					obs_data_set_bool(settings, "custom_size", true);
					obs_data_set_int(settings, "cx", canvas_width);
					obs_data_set_int(settings, "cy", canvas_height);
					obs_source_load(new_scene);
					obs_data_release(settings);
				}
			}
		}
		if (!new_scene) {
			obs_data_t *settings = obs_data_create();
			obs_data_set_bool(settings, "custom_size", true);
			obs_data_set_int(settings, "cx", canvas_width);
			obs_data_set_int(settings, "cy", canvas_height);
			obs_data_array_t *items = obs_data_array_create();
			obs_data_set_array(settings, "items", items);
			obs_data_array_release(items);
			new_scene = obs_source_create("scene", name.c_str(), settings, nullptr);
			obs_canvas_move_scene(obs_scene_from_source(new_scene), canvas);
			obs_data_release(settings);
			obs_source_load(new_scene);
		}
		auto sn = QString::fromUtf8(obs_source_get_name(new_scene));
		//if (scenesCombo)
		//	scenesCombo->addItem(sn);
		auto sli = new QListWidgetItem(sn, sceneList);
		sli->setIcon(create2StateIcon(":/aitum/media/linked.svg", ":/aitum/media/unlinked.svg"));
		sceneList->addItem(sli);

		SwitchScene(sn);
		obs_source_release(new_scene);

		auto sl = GetGlobalScenesList();

		if (hideScenes) {
			for (int j = 0; j < sl->count(); j++) {
				auto item = sl->item(j);
				if (item->text() == sn) {
					item->setHidden(true);
				}
			}
		}
	} while (ask_name && s);
}

void CanvasDock::RemoveScene(const QString &sceneName)
{
	auto s = obs_get_source_by_name(sceneName.toUtf8().constData());
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

void CanvasDock::SwitchScene(const QString &scene_name, bool transition)
{
	auto s = scene_name.isEmpty() ? nullptr : obs_get_source_by_name(scene_name.toUtf8().constData());
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
	/* if (vendor && oldName != currentSceneName) {
		const auto d = obs_data_create();
		obs_data_set_int(d, "width", canvas_width);
		obs_data_set_int(d, "height", canvas_height);
		obs_data_set_string(d, "old_scene", oldName.toUtf8().constData());
		obs_data_set_string(d, "new_scene", currentSceneName.toUtf8().constData());
		obs_websocket_vendor_emit_event(vendor, "switch_scene", d);
		obs_data_release(d);
	}*/
}

void CanvasDock::SceneItemAdded(void *data, calldata_t *params)
{
	CanvasDock *window = static_cast<CanvasDock *>(data);

	obs_sceneitem_t *item = (obs_sceneitem_t *)calldata_ptr(params, "item");

	QMetaObject::invokeMethod(window, "AddSceneItem", Q_ARG(OBSSceneItem, OBSSceneItem(item)));
}

void CanvasDock::SceneReordered(void *data, calldata_t *params)
{
	CanvasDock *window = static_cast<CanvasDock *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");

	QMetaObject::invokeMethod(window, "ReorderSources", Q_ARG(OBSScene, OBSScene(scene)));
}

void CanvasDock::SceneRefreshed(void *data, calldata_t *params)
{
	CanvasDock *window = static_cast<CanvasDock *>(data);

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(params, "scene");

	QMetaObject::invokeMethod(window, "RefreshSources", Q_ARG(OBSScene, OBSScene(scene)));
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
		obs_source_release(oldTransition);
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

	auto addSource = [this](QMenu *popup, const char *source_type, const char *name) {
		QString qname = QString::fromUtf8(name);
		QAction *popupItem = new QAction(qname, this);
		if (strcmp(source_type, "scene") == 0) {
			popupItem->setIcon(GetSceneIcon());
		} else if (strcmp(source_type, "group") == 0) {
			popupItem->setIcon(GetGroupIcon());
		} else {
			popupItem->setIcon(GetIconFromType(obs_source_get_icon_type(source_type)));
		}
		popupItem->setData(QString::fromUtf8(source_type));
		QMenu *menu = new QMenu(this);
		popupItem->setMenu(menu);
		QObject::connect(menu, &QMenu::aboutToShow, [this, menu, source_type] { LoadSourceTypeMenu(menu, source_type); });
		QList<QAction *> actions = menu->actions();
		QAction *after = nullptr;
		for (QAction *menuAction : actions) {
			if (menuAction->text().compare(name) >= 0) {
				after = menuAction;
				break;
			}
		}
		popup->insertAction(after, popupItem);
	};

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
			addSource(popup, unversioned_type, name);
		} else {
			addSource(deprecated, unversioned_type, name);
			foundDeprecated = true;
		}
		foundValues = true;
	}

	addSource(popup, "scene", obs_frontend_get_locale_string("Basic.Scene"));
	addSource(popup, "group", obs_frontend_get_locale_string("Group"));

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

QIcon CanvasDock::GetIconFromType(enum obs_icon_type icon_type) const
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());

	switch (icon_type) {
	case OBS_ICON_TYPE_IMAGE:
		return main_window->property("imageIcon").value<QIcon>();
	case OBS_ICON_TYPE_COLOR:
		return main_window->property("colorIcon").value<QIcon>();
	case OBS_ICON_TYPE_SLIDESHOW:
		return main_window->property("slideshowIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return main_window->property("audioInputIcon").value<QIcon>();
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return main_window->property("audioOutputIcon").value<QIcon>();
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return main_window->property("desktopCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return main_window->property("windowCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return main_window->property("gameCapIcon").value<QIcon>();
	case OBS_ICON_TYPE_CAMERA:
		return main_window->property("cameraIcon").value<QIcon>();
	case OBS_ICON_TYPE_TEXT:
		return main_window->property("textIcon").value<QIcon>();
	case OBS_ICON_TYPE_MEDIA:
		return main_window->property("mediaIcon").value<QIcon>();
	case OBS_ICON_TYPE_BROWSER:
		return main_window->property("browserIcon").value<QIcon>();
	case OBS_ICON_TYPE_CUSTOM:
		//TODO: Add ability for sources to define custom icons
		return main_window->property("defaultIcon").value<QIcon>();
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return main_window->property("audioProcessOutputIcon").value<QIcon>();
	default:
		return main_window->property("defaultIcon").value<QIcon>();
	}
}

QIcon CanvasDock::GetSceneIcon() const
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return main_window->property("sceneIcon").value<QIcon>();
}

QIcon CanvasDock::GetGroupIcon() const
{
	const auto main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	return main_window->property("groupIcon").value<QIcon>();
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
	CanvasDock *cd = static_cast<CanvasDock *>(menu->parent());
	auto a = menu->menuAction();
	auto t = a->data().toString();
	auto idUtf8 = t.toUtf8();
	const char *id = idUtf8.constData();
	if (strcmp(obs_source_get_unversioned_id(source), id) == 0) {
		auto name = QString::fromUtf8(obs_source_get_name(source));
		QList<QAction *> actions = menu->actions();
		QAction *before = nullptr;
		for (QAction *menuAction : actions) {
			if (menuAction->text().compare(name) >= 0)
				before = menuAction;
		}
		auto na = new QAction(name, menu);
		connect(na, &QAction::triggered, [cd, source] { cd->AddSourceToScene(source); });
		menu->insertAction(before, na);
		struct descendant_info info = {false, cd->source, obs_scene_get_source(cd->scene)};
		obs_source_enum_full_tree(source, check_descendant, &info);
		na->setEnabled(!info.exists);
	}
	return true;
}

void CanvasDock::LoadSourceTypeMenu(QMenu *menu, const char *type)
{
	menu->clear();
	if (strcmp(type, "scene") == 0) {
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
		obs_source_t *source = obs_get_source_by_name(name.c_str());
		if (!source)
			return;
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SceneName")), name)) {
				break;
			}
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
		auto s = obs_get_source_by_name(item->text().toUtf8().constData());
		if (s) {
			obs_frontend_take_source_screenshot(s);
			obs_source_release(s);
		}
	});
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Filters")), [this] {
		auto item = sceneList->currentItem();
		if (!item)
			return;
		auto s = obs_get_source_by_name(item->text().toUtf8().constData());
		if (s) {
			obs_frontend_open_source_filters(s);
			obs_source_release(s);
		}
	});

	auto tom = menu.addMenu(QString::fromUtf8(obs_frontend_get_locale_string("TransitionOverride")));
	std::string scene_name = widget_item->text().toUtf8().constData();
	OBSSourceAutoRelease scene_source = obs_get_source_by_name(scene_name.c_str());
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

	connect(duration, (void(QSpinBox::*)(int)) & QSpinBox::valueChanged, [scene_name](int dur) {
		OBSSourceAutoRelease source = obs_get_source_by_name(scene_name.c_str());
		OBSDataAutoRelease ps = obs_source_get_private_settings(source);

		obs_data_set_int(ps, "transition_duration", dur);
	});

	auto action = tom->addAction(QString::fromUtf8(obs_frontend_get_locale_string("None")));
	action->setCheckable(true);
	action->setChecked(!curTransition || !strlen(curTransition));
	connect(action, &QAction::triggered, [scene_name] {
		OBSSourceAutoRelease source = obs_get_source_by_name(scene_name.c_str());
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
		connect(a, &QAction::triggered, [scene_name, a2] {
			OBSSourceAutoRelease source = obs_get_source_by_name(scene_name.c_str());
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
			if (!obs_data_get_bool(settings, "custom_size")) {
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
						if (obs_data_get_int(item, "width") == canvas_width &&
						    obs_data_get_int(item, "height") == canvas_height) {
							auto sn = QString::fromUtf8(obs_data_get_string(item, "scene"));
							if (sn == sceneList->currentItem()->text()) {
								checkBox->setChecked(true);
							}
						}
						obs_data_release(item);
					}

					obs_data_array_release(c);
				}
			}
			obs_data_release(settings);
		}
		obs_frontend_source_list_free(&scenes);
	});
	if (hideScenes) {
		menu.addAction(QString::fromUtf8(obs_module_text("OnMainCanvas")), [this] {
			auto item = sceneList->currentItem();
			if (!item)
				return;
			auto s = obs_get_source_by_name(item->text().toUtf8().constData());
			if (!s)
				return;

			if (obs_frontend_preview_program_mode_active())
				obs_frontend_set_current_preview_scene(s);
			else
				obs_frontend_set_current_scene(s);
			obs_source_release(s);
		});
	}
	a = menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("ShowInMultiview")), [scene_name](bool checked) {
		OBSSourceAutoRelease source = obs_get_source_by_name(scene_name.c_str());
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
		if (obs_data_get_int(item, "width") == canvas_width && obs_data_get_int(item, "height") == canvas_height) {
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
			obs_data_set_int(found, "width", canvas_width);
			obs_data_set_int(found, "height", canvas_height);
			obs_data_array_push_back(c, found);
		}
		obs_data_set_string(found, "scene", linkedScene.toUtf8().constData());
	}
	obs_data_release(ss);
	obs_data_release(found);
	obs_data_array_release(c);
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
		std::string name = obs_source_get_name(item_source);
		obs_source_t *s = nullptr;
		do {
			obs_source_release(s);
			if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("SourceName")), name)) {
				break;
			}
			s = obs_get_source_by_name(name.c_str());
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

	auto projectorMenu = popup->addMenu(QString::fromUtf8(obs_frontend_get_locale_string("SourceProjector")));
	AddProjectorMenuMonitors(projectorMenu, this, SLOT(OpenSourceProjector()));
	popup->addAction(QString::fromUtf8(obs_frontend_get_locale_string("SourceWindow")), [sceneItem] {
		obs_source_t *s = obs_source_get_ref(obs_sceneitem_get_source(sceneItem));
		if (!s)
			return;
		obs_frontend_open_projector("Source", -1, nullptr, obs_source_get_name(s));
		obs_source_release(s);
	});

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
	connect(duration, (void(QSpinBox::*)(int)) & QSpinBox::valueChanged, setDuration);

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
