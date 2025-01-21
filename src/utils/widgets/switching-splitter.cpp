#include "switching-splitter.hpp"


SwitchingSplitter::SwitchingSplitter(QWidget *parent): QSplitter(parent) {

}
SwitchingSplitter::SwitchingSplitter(Qt::Orientation o, QWidget *parent) : QSplitter(parent) {

}
SwitchingSplitter::~SwitchingSplitter() {

}

void SwitchingSplitter::resizeEvent(QResizeEvent* re) {
	QSplitter::resizeEvent(re);
	auto s = size();
	bool horizontal = s.width() > s.height();
	if (orientation() == Qt::Horizontal && !horizontal) {
		setOrientation(Qt::Vertical);
	} else if (orientation() == Qt::Vertical && horizontal) {
		setOrientation(Qt::Horizontal);
	}
}
