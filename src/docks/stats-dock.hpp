#include <obs.h>
#include <QAbstractTableModel>
#include <QColor>
#include <QDateTime>
#include <QFrame>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <set>

class StatsDock : public QFrame {
	Q_OBJECT
private:
	QTableView *table = nullptr;

private slots:
	void LoadMode(QString mode);
	void SaveSettings(bool closing = false, QString mode = "");

public:
	StatsDock(QWidget *parent = nullptr);
	~StatsDock();
};

struct OutputStatsRow {
	obs_output_t *output;
	obs_encoder_t *encoder;
	obs_canvas_t *canvas;
	video_t *video;
	int audio_track = -1;
	std::string output_name;
	std::string encoder_name;
	std::string canvas_name;
	bool updated = false;
	QColor canvas_color = QColor(0, 0, 0, 0);
	uint32_t skipped_frames = 0;
	uint32_t canvas_frames = 0;
	uint32_t canvas_fps = 0;
	QImage canvas_fps_graph = QImage(1, 24, QImage::Format_ARGB32);
	int canvas_fps_graph_width = 0;
	uint32_t canvas_fps_max = 0;
	uint32_t canvas_width = 0;
	uint32_t canvas_height = 0;
	uint32_t encoded_frames = 0;
	uint32_t encoded_fps = 0;
	QImage encoded_fps_graph = QImage(1, 24, QImage::Format_ARGB32);
	int encoded_fps_graph_width = 0;
	uint32_t encoded_fps_max = 0;
	uint32_t encoded_width = 0;
	uint32_t encoded_height = 0;
	uint32_t active_delay = 0;
	uint32_t dropped_frames = 0;
	uint64_t output_bytes = 0;
	uint32_t output_bitrate = 0;
	QImage output_bitrate_graph = QImage(1, 24, QImage::Format_ARGB32);
	int output_bitrate_graph_width = 0;
	uint32_t output_bitrate_max = 0;
	uint32_t output_frames = 0;
	uint32_t output_fps = 0;
	QImage output_fps_graph = QImage(1, 24, QImage::Format_ARGB32);
	int output_fps_graph_width = 0;
	uint32_t output_fps_max = 0;
	QDateTime last_update;
	int row_height = 24;
};

struct OutputStatsColumn {
	const char *section;
	const char *name;
	bool default_visible = true;
	int alignment;
	QVariant (*get_value)(const OutputStatsRow &row);
	QImage (*get_graph)(OutputStatsRow *row, uint32_t width);
};

class OutputStatsModel : public QAbstractTableModel {
	Q_OBJECT
private:
	friend class StatsDock;
	friend class UserRoleTypeDelegate;

	const std::vector<OutputStatsColumn> columns = {
		{"Output", "Name", true, Qt::AlignLeft | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) {
			 return row.output ? QVariant(QString::fromUtf8(row.output_name)) : QVariant();
		 }},
		{"Output", "FPS", true, Qt::AlignRight | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) { return QVariant(row.output_fps); },
		 [](OutputStatsRow *row, uint32_t width) {
			 row->output_fps_graph_width = width;
			 return row->output_fps_graph;
		 }},
		{"Output", "TotalFrames", true, Qt::AlignRight | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) {
			 return QVariant(row.output_frames);
		 }},
		{"Output", "Bitrate", true, Qt::AlignRight | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) { return QVariant(row.output_bitrate); },
		 [](OutputStatsRow *row, uint32_t width) {
			 row->output_bitrate_graph_width = width;
			 return row->output_bitrate_graph;
		 }},
		{"Output", "TotalData", true, Qt::AlignRight | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) {
			 return QVariant(row.output_bytes);
		 }},
		{"Encoder", "Name", true, Qt::AlignLeft | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) {
			 return row.encoder ? QVariant(QString::fromUtf8(row.encoder_name)) : QVariant();
		 }},
		//{"Encoder", "Status", true, [](const OutputStatsRow &row) { return QVariant(); }},
		{"Encoder", "Resolution", true, Qt::AlignCenter,
		 [](const OutputStatsRow &row) {
			 return row.encoded_width && row.encoded_height
					? QVariant(QString::number(row.encoded_width) + QString("x") +
						   QString::number(row.encoded_height))
					: QVariant();
		 }},
		{"Encoder", "FPS", true, Qt::AlignRight | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) { return QVariant(row.encoded_fps); },
		 [](OutputStatsRow *row, uint32_t width) {
			 row->encoded_fps_graph_width = width;
			 return row->encoded_fps_graph;
		 }},
		{"Encoder", "TotalFrames", true, Qt::AlignRight | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) {
			 return QVariant(row.encoded_frames);
		 }},
		{"Canvas", "Name", true, Qt::AlignLeft | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) {
			 return row.canvas ? QVariant(QString::fromUtf8(row.canvas_name)) : QVariant();
		 }},
		{"Canvas", "Resolution", true, Qt::AlignCenter,
		 [](const OutputStatsRow &row) {
			 return row.canvas_width && row.canvas_height
					? QVariant(QString::number(row.canvas_width) + "x" + QString::number(row.canvas_height))
					: QVariant();
		 }},
		{"Canvas", "FPS", true, Qt::AlignRight | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) { return QVariant(row.canvas_fps); },
		 [](OutputStatsRow *row, uint32_t width) {
			 row->canvas_fps_graph_width = width;
			 return row->canvas_fps_graph;
		 }},
		{"Canvas", "TotalFrames", true, Qt::AlignRight | Qt::AlignVCenter,
		 [](const OutputStatsRow &row) {
			 return QVariant(row.canvas_frames);
		 }},
		{"Canvas", "SkippedFrames", true, Qt::AlignRight | Qt::AlignVCenter, [](const OutputStatsRow &row) {
			 return QVariant(row.skipped_frames);
		 }}};
	std::vector<OutputStatsRow> rows;

	std::set<int> rows_changed;

	QTimer updateTimer;

	std::function<int(int)> graphWidthFunc = nullptr;

	void setGraphWidthFunc(std::function<int(int)> func) { graphWidthFunc = func; }

	void updateGraph(QImage *graph, uint32_t value, uint32_t *max_value, int width, int height, uint line_color,
			 uint graph_color);

	static std::string output_name(obs_output_t *output);
	static std::string encoder_name(obs_encoder_t *encoder);
	static QColor canvas_color(obs_canvas_t *canvas);

private slots:
	void updateStats();

public:
	OutputStatsModel(QObject *parent = nullptr);
	~OutputStatsModel();

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
};

class UserRoleTypeDelegate : public QStyledItemDelegate {
	Q_OBJECT

public:
	UserRoleTypeDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) {}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
};
