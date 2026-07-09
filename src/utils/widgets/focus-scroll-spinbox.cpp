#include "focus-scroll-spinbox.hpp"
#include <QWheelEvent>

FocusScrollSpinBox::FocusScrollSpinBox(QWidget *parent) : QSpinBox(parent) {
	setFocusPolicy(Qt::StrongFocus);
}

void FocusScrollSpinBox::wheelEvent(QWheelEvent* event) {
	if (!hasFocus()) {
		event->ignore();
	} else {
		QSpinBox::wheelEvent(event);
	}
}

FocusScrollDoubleSpinBox::FocusScrollDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent)
{
	setFocusPolicy(Qt::StrongFocus);
}

void FocusScrollDoubleSpinBox::wheelEvent(QWheelEvent *event)
{
	if (!hasFocus()) {
		event->ignore();
	} else {
		QDoubleSpinBox::wheelEvent(event);
	}
}
