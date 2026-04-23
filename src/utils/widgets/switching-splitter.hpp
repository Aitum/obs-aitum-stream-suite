#pragma once

#include <QSplitter>

class SwitchingSplitter : public QSplitter {
	Q_OBJECT
private:
	void doSwitchOrientation();

public:
	explicit SwitchingSplitter(QWidget *parent = nullptr);
	~SwitchingSplitter();

	bool automaticSwitching = true;
	QString savePanelOrder();
	void restorePanelOrder(const QString &order);

protected:
	void resizeEvent(QResizeEvent *) override;
};
