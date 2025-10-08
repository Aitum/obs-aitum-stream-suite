#include "live-scenes-dock.hpp"

#include <QVBoxLayout>
#include <QToolBar>
#include <obs-frontend-api.h>

LiveScenesDock::LiveScenesDock(QWidget *parent) : QFrame(parent)
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

	connect(sceneList, &QListWidget::currentItemChanged, [this]() {
		const auto item = sceneList->currentItem();
		if (!item)
			return;
		auto scene = obs_get_source_by_uuid(item->data(Qt::UserRole).toString().toUtf8().constData());
		if (!scene)
			scene = obs_get_source_by_name(item->text().toUtf8().constData());
		if (scene) {
			obs_frontend_set_current_scene(scene);
			obs_source_release(scene);
		}
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

	mainLayout->addWidget(sceneList, 1);

	auto toolbar = new QToolBar();
	toolbar->setObjectName(QStringLiteral("scenesToolbar"));
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setFloatable(false);
	auto a = toolbar->addAction(QIcon(QString::fromUtf8(":/res/images/plus.svg")),
				    QString::fromUtf8(obs_frontend_get_locale_string("Add")), [this] {
					    auto scene = obs_frontend_get_current_scene();
					    auto sli =
						    new QListWidgetItem(QString::fromUtf8(obs_source_get_name(scene)), sceneList);
					    sli->setData(Qt::UserRole, QString::fromUtf8(obs_source_get_uuid(scene)));
					    sceneList->addItem(sli);
					    obs_source_release(scene);
				    });
	toolbar->widgetForAction(a)->setProperty("themeID", QVariant(QString::fromUtf8("addIconSmall")));
	toolbar->widgetForAction(a)->setProperty("class", "icon-plus");

	a = toolbar->addAction(QIcon(":/res/images/minus.svg"), QString::fromUtf8(obs_frontend_get_locale_string("RemoveScene")),
			       [this] {
				       auto item = sceneList->currentItem();
				       if (!item)
					       return;
				       sceneList->removeItemWidget(item);
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

	obs_frontend_add_save_callback(save_load, this);
}

LiveScenesDock::~LiveScenesDock()
{
	obs_frontend_remove_save_callback(save_load, this);
}

void LiveScenesDock::save_load(obs_data_t *save_data, bool saving, void *private_data)
{
	auto lsd = (LiveScenesDock *)private_data;

	if (saving) {
		auto ls = obs_data_array_create();
		for (int idx = 0; idx < lsd->sceneList->count(); idx++) {
			auto sn = lsd->sceneList->item(idx)->text().toUtf8();
			auto uuid = lsd->sceneList->item(idx)->data(Qt::UserRole).toString().toUtf8();
			auto item = obs_data_create();
			obs_data_set_string(item, "name", sn.constData());
			obs_data_set_string(item, "uuid", uuid.constData());
			obs_data_array_push_back(ls, item);
			obs_data_release(item);
		}
		obs_data_set_array(save_data, "live_scenes", ls);
		obs_data_array_release(ls);
	} else {
		lsd->sceneList->clear();
		auto ls = obs_data_get_array(save_data, "live_scenes");
		if (ls) {
			auto count = obs_data_array_count(ls);
			for (size_t idx = 0; idx < count; idx++) {
				auto item = obs_data_array_item(ls, idx);
				if (!item)
					continue;
				auto sli =
					new QListWidgetItem(QString::fromUtf8(obs_data_get_string(item, "name")), lsd->sceneList);
				sli->setData(Qt::UserRole, QString::fromUtf8(obs_data_get_string(item, "uuid")));
				lsd->sceneList->addItem(sli);
				obs_data_release(item);
			}
			obs_data_array_release(ls);
		}
		auto main_canvas = obs_get_main_canvas();
		if (main_canvas) {
			auto sh = obs_canvas_get_signal_handler(main_canvas);
			signal_handler_disconnect(sh, "source_rename", source_rename, private_data);
			signal_handler_connect(sh, "source_rename", source_rename, private_data);
			obs_canvas_release(main_canvas);
		}
	}
}
void LiveScenesDock::source_rename(void *data, calldata_t *call_data)
{
	auto lsd = (LiveScenesDock *)data;
	QString new_name = calldata_string(call_data, "new_name");
	QString prev_name = calldata_string(call_data, "prev_name");
	for (int i = 0; i < lsd->sceneList->count(); i++) {
		auto item = lsd->sceneList->item(i);
		if (!item)
			continue;
		if (item->text() == prev_name) {
			item->setText(new_name);
		}
	}
}

void LiveScenesDock::MainSceneChanged()
{
	auto current_scene = obs_frontend_get_current_scene();
	if (!current_scene) {
		if (sceneList->currentItem())
			sceneList->setCurrentItem(nullptr);
		return;
	}
	QString scene_name = QString::fromUtf8(obs_source_get_name(current_scene));
	QString scene_uuid = QString::fromUtf8(obs_source_get_uuid(current_scene));
	obs_source_release(current_scene);
	bool found = false;
	for (int i = 0; i < sceneList->count(); i++) {
		auto item = sceneList->item(i);
		if (!item)
			continue;
		if (item->text() == scene_name) {
			sceneList->setCurrentItem(item);
			item->setSelected(true);
			found = true;
		} else if (item->data(Qt::UserRole).toString() == scene_uuid) {
			sceneList->setCurrentItem(item);
			item->setSelected(true);
			item->setText(scene_name);
			found = true;
		} else if (item->isSelected()) {
			item->setSelected(false);
		}
	}
	if (!found && sceneList->currentItem()) {
		sceneList->setCurrentItem(nullptr);
	}
}

void LiveScenesDock::ChangeSceneIndex(bool relative, int offset, int invalidIdx)
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
