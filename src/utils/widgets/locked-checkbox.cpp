#include "locked-checkbox.hpp"

LockedCheckBox::LockedCheckBox()
{
	setProperty("lockCheckBox", true);
	setProperty("class", "indicator-lock");
}

LockedCheckBox::LockedCheckBox(QWidget *parent) : QCheckBox(parent) {}
