#include "switching-splitter.hpp"
#include <obs-module.h>
#include <QMenu>

SwitchingSplitter::SwitchingSplitter(QWidget *parent) : QSplitter(parent)
{
	this->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, &QSplitter::customContextMenuRequested, this, [this](const QPoint &pos) {
		auto handle = dynamic_cast<QSplitterHandle *>(childAt(pos));
		if (!handle)
			return;

		QMenu menu;
		auto action = menu.addAction(QString::fromUtf8(obs_module_text("AutomaticSwitchOrientation")));
		action->setCheckable(true);
		action->setChecked(automaticSwitching);
		connect(action, &QAction::triggered, this, [this] {
			automaticSwitching = !automaticSwitching;
			if (automaticSwitching)
				doSwitchOrientation();
		});
		action = menu.addAction(QString::fromUtf8(obs_module_text("SwitchOrientation")));
		action->setEnabled(!automaticSwitching);
		connect(action, &QAction::triggered, this, [this] {
			if (orientation() == Qt::Horizontal)
				setOrientation(Qt::Vertical);
			else
				setOrientation(Qt::Horizontal);
		});
		action = menu.addAction(QString::fromUtf8(obs_module_text("SwitchPanels")));
		connect(action, &QAction::triggered, this, [this, handle] {
			auto handleIndex = 0;
			while (this->handle(handleIndex) && this->handle(handleIndex) != handle)
				++handleIndex;
			if (!this->handle(handleIndex))
				return;
			handleIndex--;
			auto sizes = this->sizes();
			if (handleIndex < 0 || handleIndex >= sizes.size() - 1)
				return;
			auto widgetA = widget(handleIndex);
			auto widgetB = widget(handleIndex + 1);
			if (!widgetA || !widgetB)
				return;
			auto sizeA = sizes[handleIndex];
			auto sizeB = sizes[handleIndex + 1];
			widgetA->setParent(nullptr);
			widgetB->setParent(nullptr);
			insertWidget(handleIndex, widgetB);
			insertWidget(handleIndex + 1, widgetA);
			sizes[handleIndex] = sizeB;
			sizes[handleIndex + 1] = sizeA;
			setSizes(sizes);
		});
		bool collapsedPanels = false;
		for (auto size : sizes()) {
			if (size == 0) {
				collapsedPanels = true;
				break;
			}
		}
		action = menu.addAction(QString::fromUtf8(obs_module_text("ExpandAllPanels")));
		action->setEnabled(collapsedPanels);
		connect(action, &QAction::triggered, this, [this] {
			auto sizes = this->sizes();
			for (int i = 0; i < sizes.size(); i++) {
				sizes[i] = 1;
			}
			setSizes(sizes);
		});
		menu.exec(mapToGlobal(pos));
	});
}

SwitchingSplitter::~SwitchingSplitter() {}

void SwitchingSplitter::resizeEvent(QResizeEvent *re)
{
	QSplitter::resizeEvent(re);
	if (automaticSwitching)
		doSwitchOrientation();
}
void SwitchingSplitter::doSwitchOrientation()
{
	auto s = size();
	bool horizontal = s.width() > s.height();
	if (orientation() == Qt::Horizontal && !horizontal) {
		setOrientation(Qt::Vertical);
	} else if (orientation() == Qt::Vertical && horizontal) {
		setOrientation(Qt::Horizontal);
	}
}

QString SwitchingSplitter::savePanelOrder()
{
	QString order;
	for (int i = 0; i < count(); i++) {
		if (!order.isEmpty())
			order += ",";
		auto w = widget(i);
		if (w)
			order += w->objectName();
	}
	return order;
}

void SwitchingSplitter::restorePanelOrder(const QString &order) {
	auto names = order.split(",");
	for (int i = 0; i < names.size(); i++) {
		auto name = names[i];
		if (name.isEmpty())
			continue;
		auto w = findChild<QWidget *>(name);
		if (!w)
			continue;
		w->setParent(nullptr);
		insertWidget(i, w);
	}
}
