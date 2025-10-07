#include "filters-dock.hpp"
#include "properties-dock.hpp"
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QMenu>
#include <QToolBar>
#include <QVBoxLayout>
#include <src/dialogs/name-dialog.hpp>

extern PropertiesDock *properties_dock;

FiltersDock::FiltersDock(QWidget *parent) : QFrame(parent)
{
	setMinimumWidth(100);
	setMinimumHeight(50);

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	filtersList = new QListWidget();
	filtersList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	filtersList->setFrameShape(QFrame::NoFrame);
	filtersList->setFrameShadow(QFrame::Plain);
	filtersList->setSelectionMode(QAbstractItemView::SingleSelection);
	filtersList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(filtersList, &QListWidget::customContextMenuRequested,
		[this](const QPoint &pos) { ShowFiltersContextMenu(filtersList->itemAt(pos)); });

	connect(filtersList, &QListWidget::currentItemChanged, [this]() {
		if (!source)
			return;
		const auto item = filtersList->currentItem();
		if (!item)
			return;

		auto s = obs_weak_source_get_source(source);
		if (!s)
			return;

		auto f = obs_source_get_filter_by_name(s, item->text().toUtf8().constData());
		obs_source_release(s);
		if (!f)
			return;

		QMetaObject::invokeMethod(properties_dock, "LoadProperties", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(f)));
		obs_source_release(f);
		if (!item->isSelected())
			item->setSelected(true);
	});
	connect(filtersList, &QListWidget::itemSelectionChanged, [this] {
		const auto item = filtersList->currentItem();
		if (!item)
			return;
		if (!item->isSelected())
			item->setSelected(true);
	});

	QAction *renameAction = new QAction(filtersList);
#ifdef __APPLE__
	renameAction->setShortcut({Qt::Key_Return});
#else
	renameAction->setShortcut({Qt::Key_F2});
#endif
	renameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	connect(renameAction, &QAction::triggered, [this]() { RenameFilter(filtersList->currentItem()); });
	filtersList->addAction(renameAction);

	mainLayout->addWidget(filtersList, 1);

	auto toolbar = new QToolBar();
	toolbar->setObjectName(QStringLiteral("filtersToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);

	auto a = toolbar->addAction(QIcon(QString::fromUtf8(":/res/images/plus.svg")),
				    QString::fromUtf8(obs_frontend_get_locale_string("Add")), [this] {
					    QMenu addFilterMenu;
					    AddFilterMenu(&addFilterMenu);
					    addFilterMenu.exec(QCursor::pos());
				    });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-plus");

	a = toolbar->addAction(QIcon(":/res/images/minus.svg"), QString::fromUtf8(obs_frontend_get_locale_string("RemoveFilter")),
			       [this] { RemoveFilter(filtersList->currentItem()); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("removeIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-minus");
	a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
#ifdef __APPLE__
	a->setShortcut({Qt::Key_Backspace});
#else
	a->setShortcut({Qt::Key_Delete});
#endif
	filtersList->addAction(a);

	toolbar->addSeparator();

	a = toolbar->addAction(QIcon(":/settings/images/settings/general.svg"),
			       QString::fromUtf8(obs_frontend_get_locale_string("SourceProperties")), [this] {

			       });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("propertiesIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-gear");

	toolbar->addSeparator();

	a = toolbar->addAction(QIcon(":/res/images/up.svg"), QString::fromUtf8(obs_frontend_get_locale_string("MoveFilterUp")),
			       [this] { ChangeFilterIndex(filtersList->currentItem(), OBS_ORDER_MOVE_UP); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("upArrowIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-up");
	a = toolbar->addAction(QIcon(":/res/images/down.svg"), QString::fromUtf8(obs_frontend_get_locale_string("MoveFilterDown")),
			       [this] { ChangeFilterIndex(filtersList->currentItem(), OBS_ORDER_MOVE_DOWN); });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("downArrowIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-down");
	mainLayout->addWidget(toolbar, 0);

	setObjectName(QStringLiteral("contextContainer"));
	setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	setLayout(mainLayout);
}

FiltersDock::~FiltersDock()
{
	SourceChanged(nullptr);
}

void FiltersDock::ChangeFilterIndex(QListWidgetItem *item, enum obs_order_movement movement)
{
	if (!source)
		return;
	if (!item)
		return;

	auto s = obs_weak_source_get_source(source);
	if (!s)
		return;

	auto filter = obs_source_get_filter_by_name(s, item->text().toUtf8().constData());

	if (!filter) {
		obs_source_release(s);
		return;
	}
	obs_source_filter_set_order(s, filter, movement);
	obs_source_release(s);
}

void FiltersDock::ShowFiltersContextMenu(QListWidgetItem *widget_item)
{
	auto menu = QMenu(this);
	auto addFilterMenu = menu.addMenu(QString::fromUtf8(obs_frontend_get_locale_string("Add")));
	connect(addFilterMenu, &QMenu::aboutToShow, [this, addFilterMenu] {
		addFilterMenu->clear();
		AddFilterMenu(addFilterMenu);
	});
	if (!widget_item) {
		menu.exec(QCursor::pos());
		return;
	}
	menu.addSeparator();
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Duplicate")), [this] {
		if (!source)
			return;
		const auto item = filtersList->currentItem();
		if (!item)
			return;

		auto s = obs_weak_source_get_source(source);
		if (!s)
			return;

		auto f = obs_source_get_filter_by_name(s, item->text().toUtf8().constData());
		obs_source_release(s);
		if (!f)
			return;

		QMetaObject::invokeMethod(properties_dock, "LoadProperties", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(f)));
		properties_dock->parentWidget()->show();
	});
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Remove")),
		       [this, widget_item] { RemoveFilter(widget_item); });
	menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Rename")),
		       [this, widget_item] { RenameFilter(widget_item); });
	auto orderMenu = menu.addMenu(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order")));
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveUp")),
			     [this, widget_item] { ChangeFilterIndex(widget_item, OBS_ORDER_MOVE_UP); });
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveDown")),
			     [this, widget_item] { ChangeFilterIndex(widget_item, OBS_ORDER_MOVE_DOWN); });
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToTop")),
			     [this, widget_item] { ChangeFilterIndex(widget_item, OBS_ORDER_MOVE_TOP); });
	orderMenu->addAction(QString::fromUtf8(obs_frontend_get_locale_string("Basic.MainMenu.Edit.Order.MoveToBottom")),
			     [this, widget_item] { ChangeFilterIndex(widget_item, OBS_ORDER_MOVE_BOTTOM); });

	menu.exec(QCursor::pos());
}

bool FiltersDock::filter_compatible(uint32_t sourceFlags, uint32_t filterFlags)
{
	bool filterVideo = (filterFlags & OBS_SOURCE_VIDEO) != 0;
	bool filterAsync = (filterFlags & OBS_SOURCE_ASYNC) != 0;
	bool filterAudio = (filterFlags & OBS_SOURCE_AUDIO) != 0;
	bool audio = (sourceFlags & OBS_SOURCE_AUDIO) != 0;
	bool audioOnly = (sourceFlags & OBS_SOURCE_VIDEO) == 0;
	bool asyncSource = (sourceFlags & OBS_SOURCE_ASYNC) != 0;

	if (!audio && filterAudio)
		return false;
	if (!asyncSource && filterAsync)
		return false;
	if (audioOnly && filterVideo)
		return false;
	return true;
}

void FiltersDock::RemoveFilter(QListWidgetItem *item)
{
	if (!source)
		return;
	if (!item)
		return;

	auto s = obs_weak_source_get_source(source);
	if (!s)
		return;

	auto f = obs_source_get_filter_by_name(s, item->text().toUtf8().constData());
	if (!f) {
		obs_source_release(s);
		return;
	}

	obs_source_filter_remove(s, f);
	obs_source_release(f);
	obs_source_release(s);
}

void FiltersDock::RenameFilter(QListWidgetItem *item)
{
	if (!source)
		return;
	if (!item)
		return;

	auto s = obs_weak_source_get_source(source);
	if (!s)
		return;

	auto filter = obs_source_get_filter_by_name(s, item->text().toUtf8().constData());
	obs_source_release(s);
	if (!filter)
		return;

	std::string name = obs_source_get_name(filter);
	obs_source_t *f = nullptr;
	do {
		obs_source_release(f);
		if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("FilterName")), name)) {
			break;
		}
		s = obs_weak_source_get_source(source);
		if (!s)
			break;
		f = obs_source_get_filter_by_name(s, name.c_str());
		obs_source_release(s);
		if (f)
			continue;
		obs_source_set_name(filter, name.c_str());
	} while (f);
	obs_source_release(filter);
}

void FiltersDock::AddFilterMenu(QMenu *addFilterMenu)
{
	auto s = obs_weak_source_get_source(source);
	if (!s) {
		addFilterMenu->addAction(QString::fromUtf8(obs_module_text("NoSourceSelected")))->setEnabled(false);
		return;
	}
	auto sf = obs_source_get_output_flags(s);
	obs_source_release(s);

	size_t idx = 0;
	const char *type_str;
	while (obs_enum_filter_types(idx++, &type_str)) {
		uint32_t caps = obs_get_source_output_flags(type_str);
		if ((caps & OBS_SOURCE_DEPRECATED) != 0)
			continue;
		if ((caps & OBS_SOURCE_CAP_DISABLED) != 0)
			continue;
		if ((caps & OBS_SOURCE_CAP_OBSOLETE) != 0)
			continue;

		if (!filter_compatible(sf, caps))
			continue;

		auto name = QString::fromUtf8(obs_source_get_display_name(type_str));

		QList<QAction *> actions = addFilterMenu->actions();
		QAction *before = nullptr;
		for (QAction *menuAction : actions) {
			if (menuAction->text().compare(name) >= 0)
				before = menuAction;
		}
		auto na = new QAction(name, addFilterMenu);
		connect(na, &QAction::triggered, [this, name, type_str] {
			if (!source)
				return;
			auto s = obs_weak_source_get_source(source);
			if (!s)
				return;
			QString filter_name = name;
			int i = 2;
			OBSSourceAutoRelease f = nullptr;
			while ((f = obs_source_get_filter_by_name(s, filter_name.toUtf8().constData()))) {
				filter_name = QString("%1 %2").arg(name).arg(i++);
			}
			auto filter = obs_source_create(type_str, filter_name.toUtf8().constData(), nullptr, nullptr);
			obs_source_filter_add(s, filter);
			obs_source_release(s);

			QMetaObject::invokeMethod(properties_dock, "LoadProperties", Qt::QueuedConnection,
						  Q_ARG(OBSSource, OBSSource(filter)));
			obs_source_release(filter);
			//todo make sure the new item is selected
		});
		addFilterMenu->insertAction(before, na);
	}
}

void FiltersDock::SourceChanged(OBSSource s)
{
	if (source) {
		if (obs_weak_source_references_source(source, s))
			return;
		auto prev_source = obs_weak_source_get_source(source);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_source), "remove", source_remove, this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_source), "destroy", source_remove, this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_source), "filter_add", filter_add, this);
		signal_handler_disconnect(obs_source_get_signal_handler(prev_source), "filter_remove", filter_remove, this);
		obs_source_release(prev_source);

		obs_weak_source_release(source);
	}
	filtersList->clear();
	if (!s) {
		source = nullptr;
		return;
	}
	source = obs_source_get_weak_source(s);
	signal_handler_connect(obs_source_get_signal_handler(s), "remove", source_remove, this);
	signal_handler_connect(obs_source_get_signal_handler(s), "destroy", source_remove, this);
	signal_handler_connect(obs_source_get_signal_handler(s), "filter_add", filter_add, this);
	signal_handler_connect(obs_source_get_signal_handler(s), "filter_remove", filter_remove, this);

	obs_source_enum_filters(
		s,
		[](obs_source_t *parent, obs_source_t *filter, void *param) {
			UNUSED_PARAMETER(parent);
			auto this_ = static_cast<FiltersDock *>(param);
			this_->filtersList->addItem(QString::fromUtf8(obs_source_get_name(filter)));
		},
		this);
}

void FiltersDock::filter_add(void *param, calldata_t *cd)
{
	auto this_ = static_cast<FiltersDock *>(param);
	//auto source = (obs_source_t *)calldata_ptr(cd, "source");
	auto filter = (obs_source_t *)calldata_ptr(cd, "filter");

	auto filter_name = QString::fromUtf8(obs_source_get_name(filter));
	QMetaObject::invokeMethod(this_, [this_, filter_name] { this_->filtersList->addItem(filter_name); });
}

void FiltersDock::filter_remove(void *param, calldata_t *cd)
{
	UNUSED_PARAMETER(cd);
	auto this_ = static_cast<FiltersDock *>(param);
	QMetaObject::invokeMethod(this_, [this_] {
		auto s = obs_weak_source_get_source(this_->source);
		this_->SourceChanged(nullptr);
		if (s) {
			this_->SourceChanged(s);
			obs_source_release(s);
		}
	});
}

void FiltersDock::source_remove(void *param, calldata_t *cd)
{
	auto this_ = static_cast<FiltersDock *>(param);
	auto source = (obs_source_t *)calldata_ptr(cd, "source");
	if (obs_weak_source_references_source(this_->source, source)) {
		QMetaObject::invokeMethod(this_, "SourceChanged", Qt::QueuedConnection, Q_ARG(OBSSource, OBSSource(nullptr)));
	}
}
