#pragma once
#include "../utils/widgets/alignment-selector.hpp"
#include <obs.hpp>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QFrame>
#include <QCheckBox>
#include <QLabel>
#include <QAction>

class TransformDock : public QFrame {
	Q_OBJECT
private:
	OBSSceneItem item;
	std::vector<OBSSignal> sigs;

	bool ignoreTransformSignal = false;
	bool ignoreItemChange = false;

	QWidget *transformWidget;

	QLabel *sourceLabel;
	QDoubleSpinBox* positionX;
	QDoubleSpinBox* positionY;
	QDoubleSpinBox* sizeX;
	QDoubleSpinBox* sizeY;
	QDoubleSpinBox* rotation;
	AlignmentSelector* positionAlignment;
	QComboBox* boundsType;
	AlignmentSelector* boundsAlignment;
	QDoubleSpinBox *boundsWidth;
	QDoubleSpinBox *boundsHeight;
	QCheckBox* cropToBounds;

	QSpinBox *cropLeft;
	QSpinBox *cropRight;
	QSpinBox *cropTop;
	QSpinBox *cropBottom;

	QComboBox *showTransition;
	QSpinBox *showTransitionDuration;
	QComboBox *hideTransition;
	QSpinBox *hideTransitionDuration;

	QAction *pasteAction = nullptr;

	struct obs_transform_info copiedTransformInfo;
	struct obs_sceneitem_crop copiedCropInfo;
	std::string copiedShowTransition;
	OBSData copiedShowTransitionSettings;
	int copiedShowTransitionDuration;
	std::string copiedHideTransition;
	OBSData copiedHideTransitionSettings;
	int copiedHideTransitionDuration;

	static int alignToIndex(uint32_t align);
	static vec2 getAlignmentConversion(uint32_t alignment);

private slots:
	void refreshControls();
	void setItem(OBSSceneItem newItem);
	void unsetItem(OBSSceneItem unsetItem);
	void sceneItemTransform(OBSSceneItem transformItem);
	void sceneItemLocked(OBSSceneItem lockedItem);
	void onAlignChanged(int index);
	void onBoundsType(int index);
	void onControlChanged();
	void onCropChanged();
	//void setEnabled(bool enable);

public:
	TransformDock(QWidget *parent = nullptr);
	~TransformDock();
};
