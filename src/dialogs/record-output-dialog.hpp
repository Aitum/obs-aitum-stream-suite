#pragma once
#include <QDialog>

class RecordOutputDialog : public QDialog {
	Q_OBJECT
private:
	QStringList otherNames;

	void validateOutputs(QPushButton *confirmButton);

public:
	RecordOutputDialog(QDialog *parent, QStringList otherNames, bool backtrack);
	RecordOutputDialog(QDialog *parent, QString name, QString path, QString filename, QString format, long long max_size,
			   long long max_time, QStringList otherNames, bool backtrack);

	QString outputName;
	QString recordPath;
	QString filenameFormat;
	QString fileFormat;
	long long maxSize;
	long long maxTime;
};
