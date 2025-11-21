#include "visibility-item-widget.hpp"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>

VisibilityItemWidget::VisibilityItemWidget(obs_source_t *source_)
	: source(source_),
	  enabledSignal(obs_source_get_signal_handler(source), "enable", OBSSourceEnabled, this),
	  renameSignal(obs_source_get_signal_handler(source), "rename", OBSSourceRenamed, this)
{
	bool enabled = obs_source_enabled(source);

	vis = new QCheckBox();
	vis->setProperty("class", "checkbox-icon indicator-visibility");
	vis->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	vis->setChecked(enabled);

	label = new QLabel(QString::fromUtf8(obs_source_get_name(source)));
	label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	QHBoxLayout *itemLayout = new QHBoxLayout();
	itemLayout->addWidget(vis);
	itemLayout->addWidget(label);
	itemLayout->setContentsMargins(0, 0, 0, 0);

	setLayout(itemLayout);

	connect(vis, &QCheckBox::clicked, [this](bool visible) { obs_source_set_enabled(source, visible); });
}

void VisibilityItemWidget::OBSSourceEnabled(void *param, calldata_t *data)
{
	VisibilityItemWidget *window = static_cast<VisibilityItemWidget *>(param);
	bool enabled = calldata_bool(data, "enabled");

	QMetaObject::invokeMethod(window, "SourceEnabled", Q_ARG(bool, enabled));
}

void VisibilityItemWidget::SourceEnabled(bool enabled)
{
	if (vis->isChecked() != enabled)
		vis->setChecked(enabled);
}

void VisibilityItemWidget::OBSSourceRenamed(void *param, calldata_t *data)
{
	VisibilityItemWidget *window = static_cast<VisibilityItemWidget *>(param);
	auto name = QString::fromUtf8(calldata_string(data, "new_name"));

	QMetaObject::invokeMethod(window, "SourceRenamed", Q_ARG(QString, name));
}

void VisibilityItemWidget::SourceRenamed(QString name)
{
	label->setText(name);
}
