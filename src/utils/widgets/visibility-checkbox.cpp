#include "visibility-checkbox.hpp"

VisibilityCheckBox::VisibilityCheckBox()
{
	setProperty("visibilityCheckBox", true);
	setProperty("class", "indicator-visibility");
}

VisibilityCheckBox::VisibilityCheckBox(QWidget *parent) : QCheckBox(parent) {}
