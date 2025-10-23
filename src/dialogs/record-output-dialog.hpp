#pragma once
#include <QDialog>

class RecordOutputDialog : public QDialog {
	Q_OBJECT
private:

	QStringList otherNames;

	void validateOutputs(QPushButton *confirmButton);

public:
	RecordOutputDialog(QDialog *parent, QStringList otherNames);
	RecordOutputDialog(QDialog *parent, QString name, QString path, QString filename, QString format, QStringList otherNames);

	QString outputName;
	QString recordPath;
	QString filenameFormat;
	QString fileFormat;
};
