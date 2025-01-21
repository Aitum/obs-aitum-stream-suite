#pragma once

#include <QSplitter>

class SwitchingSplitter : public QSplitter {
	Q_OBJECT
public:
	explicit SwitchingSplitter(QWidget *parent = nullptr);
	explicit SwitchingSplitter(Qt::Orientation, QWidget *parent = nullptr);
	~SwitchingSplitter();

protected:
	void resizeEvent(QResizeEvent *) override;
};
