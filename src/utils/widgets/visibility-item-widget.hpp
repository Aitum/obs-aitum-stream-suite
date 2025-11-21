#pragma once

#include <obs.hpp>

#include <QListWidget>
#include <QWidget>

class QLabel;
class QCheckBox;

class VisibilityItemWidget : public QWidget {
	Q_OBJECT

private:
	OBSSource source;
	QLabel *label = nullptr;
	QCheckBox *vis = nullptr;

	OBSSignal enabledSignal;
	OBSSignal renameSignal;

	bool active = false;
	bool selected = false;

	static void OBSSourceEnabled(void *param, calldata_t *data);
	static void OBSSourceRenamed(void *param, calldata_t *data);

private slots:
	void SourceEnabled(bool enabled);
	void SourceRenamed(QString name);

public:
	VisibilityItemWidget(obs_source_t *source);
};
