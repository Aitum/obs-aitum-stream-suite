
#include "stats-dock.hpp"
#include <obs-module.h>
#include <QHeaderView>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QVBoxLayout>
#include <src/utils/color.hpp>

extern obs_data_t *current_profile_config;
extern QTabBar *modesTabBar;

StatsDock::StatsDock(QWidget *parent) : QFrame(parent)
{
	auto layout = new QVBoxLayout(this);
	setLayout(layout);

	table = new QTableView(this);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->setSortingEnabled(true);

	auto model = new OutputStatsModel();
	model->setGraphWidthFunc([this](int i) { return table->columnWidth(i); });
	auto proxyModel = new QSortFilterProxyModel(this);
	proxyModel->setSourceModel(model);
	proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

	table->setModel(proxyModel);
	table->setHorizontalHeader(new GroupedHeaderView(Qt::Horizontal, table));
	table->setTextElideMode(Qt::ElideRight);
	table->horizontalHeader()->setTextElideMode(Qt::ElideRight);

	table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
	table->setItemDelegate(new UserRoleTypeDelegate(table));

	auto index = 0;
	for (auto &column : model->columns) {
		if (!column.default_visible)
			table->setColumnHidden(index, true);
		++index;
	}

	table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(table->horizontalHeader(), &QHeaderView::customContextMenuRequested, [this](const QPoint &pos) {
		QMenu menu;
		auto section = table->horizontalHeader()->logicalIndexAt(pos);
		if (section >= 0) {
			auto m = menu.addMenu(QString::fromUtf8(obs_module_text("Sort")));
			auto a1 = m->addAction(QString::fromUtf8(obs_module_text("Ascending")),
					       [this, section] { table->sortByColumn(section, Qt::SortOrder::AscendingOrder); });
			a1->setCheckable(true);
			a1->setChecked(table->horizontalHeader()->sortIndicatorSection() == section &&
				       table->horizontalHeader()->sortIndicatorOrder() == Qt::SortOrder::AscendingOrder);
			auto a2 = m->addAction(QString::fromUtf8(obs_module_text("Descending")),
					       [this, section] { table->sortByColumn(section, Qt::SortOrder::DescendingOrder); });
			a2->setCheckable(true);
			a2->setChecked(table->horizontalHeader()->sortIndicatorSection() == section &&
				       table->horizontalHeader()->sortIndicatorOrder() == Qt::SortOrder::DescendingOrder);
			menu.addSeparator();
		}
		auto count = table->model()->columnCount();
		for (auto i = 0; i < count; ++i) {
			auto a = menu.addAction(
				table->model()->headerData(i, Qt::Orientation::Horizontal, Qt::DisplayRole).toString(),
				[this, i] { table->setColumnHidden(i, !table->isColumnHidden(i)); });
			a->setCheckable(true);
			a->setChecked(!table->isColumnHidden(i));
		}

		if (section >= 0) {
			menu.exec(QCursor::pos());
		} else {
			menu.exec(table->horizontalHeader()->viewport()->mapToGlobal(table->horizontalHeader()->rect().center()));
		}
	});

	layout->addWidget(table);
}

StatsDock::~StatsDock() {}

void StatsDock::LoadMode(QString mode)
{
	std::string setting_name = "stats_state_" + mode.toStdString();
	auto state = obs_data_get_string(current_profile_config, setting_name.c_str());
	if (state[0] == '\0') {
		setting_name = "stats_state_" + mode.toLower().toStdString();
		state = obs_data_get_string(current_profile_config, setting_name.c_str());
	}
	if (state[0] != '\0')
		table->horizontalHeader()->restoreState(QByteArray::fromBase64(state));
}

void StatsDock::SaveSettings(bool closing, QString mode)
{
	if (closing || !current_profile_config)
		return;

	auto state = table->horizontalHeader()->saveState();
	auto b64 = state.toBase64();
	auto state_chars = b64.constData();
	if (mode.isEmpty() && modesTabBar) {
		auto d = modesTabBar->tabData(modesTabBar->currentIndex());
		if (!d.isNull() && d.isValid() && !d.toString().isEmpty()) {
			mode = d.toString();
		} else {
			mode = modesTabBar->tabText(modesTabBar->currentIndex());
		}
	}
	std::string setting_name = "stats_state";
	if (!mode.isEmpty())
		setting_name += "_" + mode.toStdString();
	obs_data_set_string(current_profile_config, setting_name.c_str(), state_chars);
}

OutputStatsModel::OutputStatsModel(QObject *parent) : QAbstractTableModel(parent)
{

	updateTimer.setInterval(1000);
	connect(&updateTimer, &QTimer::timeout, this, &OutputStatsModel::updateStats);
	updateTimer.start();
}

OutputStatsModel::~OutputStatsModel() {}

int OutputStatsModel::rowCount(const QModelIndex &) const
{
	return (int)rows.size();
}

int OutputStatsModel::columnCount(const QModelIndex &) const
{
	return (int)columns.size();
}

QVariant OutputStatsModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	if (role == Qt::DisplayRole) {
		auto column = columns[index.column()];
		auto row = rows[index.row()];
		return column.get_value(row);
	} else if (role == Qt::UserRole) {
		auto column = columns[index.column()];
		if (column.get_graph) {
			auto row = (OutputStatsRow *)&rows[index.row()];
			auto width = graphWidthFunc ? graphWidthFunc(index.column()) : 0;
			return column.get_graph(row, width);
		}
	} else if (role == Qt::TextAlignmentRole) {
		auto column = columns[index.column()];
		return QVariant(column.alignment);
	} else if (role == Qt::UserRole + 1) {
		return QVariant((qlonglong)&rows[index.row()]);
	}
	return QVariant();
}

QVariant OutputStatsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		auto column = columns[section];
		return QString::fromUtf8(obs_module_text(column.section)) + "\n" + QString::fromUtf8(obs_module_text(column.name));
	}
	return QVariant();
}

void OutputStatsModel::updateStats()
{
	for (auto &row : rows) {
		row.updated = false;
	}
	obs_enum_encoders(
		[](void *data, obs_encoder_t *encoder) {
			auto model = static_cast<OutputStatsModel *>(data);
			auto encoder_type = obs_encoder_get_type(encoder);
			int index = 0;
			bool found = false;
			for (auto &row : model->rows) {
				if (row.encoder == encoder) {
					found = true;
					if (encoder_type == OBS_ENCODER_VIDEO) {
						auto encoded_frames = obs_encoder_get_encoded_frames(encoder);
						row.encoded_fps = encoded_frames > row.encoded_frames
									  ? encoded_frames - row.encoded_frames
									  : 0;
						row.encoded_frames = encoded_frames;
						row.updated = true;
						model->rows_changed.insert(index);
					} else if (encoder_type == OBS_ENCODER_AUDIO) {
					}
				}
				++index;
			}

			if (found || !obs_encoder_active(encoder))
				return true;

			if (encoder_type == OBS_ENCODER_AUDIO) {

			} else if (encoder_type == OBS_ENCODER_VIDEO) {
				auto row_number = (int)model->rows.size();
				model->rows_changed.insert(row_number);
				model->beginInsertRows(QModelIndex(), row_number, row_number);
				OutputStatsRow row{
					nullptr, encoder, nullptr, obs_encoder_parent_video(encoder), -1, "", encoder_name(encoder),
					"",      true};
				row.output_bitrate_graph.fill(0);
				row.output_fps_graph.fill(0);
				row.encoded_fps_graph.fill(0);
				row.canvas_fps_graph.fill(0);
				row.encoded_width = video_output_get_width(obs_encoder_video(encoder));
				row.encoded_height = video_output_get_height(obs_encoder_video(encoder));
				model->rows.emplace_back(row);
				model->endInsertRows();
			}
			return true;
		},
		this);

	obs_enum_outputs(
		[](void *data, obs_output_t *output) {
			auto model = static_cast<OutputStatsModel *>(data);
			int index = 0;
			bool found = false;
			bool encoded = (obs_output_get_flags(output) & OBS_OUTPUT_ENCODED);
			for (auto &row : model->rows) {
				if (encoded && !row.output && row.encoder && obs_output_active(output) &&
				    obs_encoder_get_type(row.encoder) == OBS_ENCODER_VIDEO) {
					for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; ++i) {
						obs_encoder_t *encoder = obs_output_get_video_encoder2(output, i);
						if (encoder && encoder == row.encoder) {
							row.output = output;
							row.output_name = output_name(output);
							model->rows_changed.insert(index);
							break;
						}
					}
				}

				if (row.output == output) {
					row.active_delay = obs_output_get_active_delay(output);
					row.dropped_frames = obs_output_get_frames_dropped(output);
					auto output_bytes = obs_output_get_total_bytes(output);
					row.output_bitrate =
						output_bytes > row.output_bytes ? (output_bytes - row.output_bytes) * 8 / 1000 : 0;
					row.output_bytes = output_bytes;
					auto output_frames = obs_output_get_total_frames(output);
					row.output_fps = output_frames > (int)row.output_frames ? output_frames - row.output_frames
												: 0;
					row.output_frames = output_frames;
					row.updated = true;
					model->rows_changed.insert(index);
					found = true;
				}
				++index;
			}

			if (found || !obs_output_active(output))
				return true;
			if (encoded) {
				for (size_t i = 0; i < MAX_OUTPUT_VIDEO_ENCODERS; ++i) {
					obs_encoder_t *encoder = obs_output_get_video_encoder2(output, i);
					if (!encoder)
						continue;
					auto row_number = (int)model->rows.size();
					model->rows_changed.insert(row_number);
					model->beginInsertRows(QModelIndex(), row_number, row_number);
					OutputStatsRow row{
						output,
						encoder,
						nullptr,
						obs_encoder_parent_video(encoder),
						-1,
						output_name(output),
						encoder_name(encoder),
						"",
						true,
					};
					row.output_bitrate_graph.fill(0);
					row.output_fps_graph.fill(0);
					row.encoded_fps_graph.fill(0);
					row.canvas_fps_graph.fill(0);
					row.encoded_width = video_output_get_width(obs_encoder_video(encoder));
					row.encoded_height = video_output_get_height(obs_encoder_video(encoder));
					model->rows.emplace_back(row);
					model->endInsertRows();
				}
			} else {
				auto row_number = (int)model->rows.size();
				model->rows_changed.insert(row_number);
				model->beginInsertRows(QModelIndex(), row_number, row_number);
				OutputStatsRow row{
					output, nullptr, nullptr, obs_output_video(output), -1, output_name(output), "", "", true,
				};
				row.output_bitrate_graph.fill(0);
				row.output_fps_graph.fill(0);
				row.encoded_fps_graph.fill(0);
				row.canvas_fps_graph.fill(0);
				model->rows.emplace_back(row);
				model->endInsertRows();
			}
			return true;
		},
		this);

	if (rows.empty())
		return;

	obs_enum_canvases(
		[](void *data, obs_canvas_t *canvas) {
			auto model = static_cast<OutputStatsModel *>(data);
			auto video = obs_canvas_get_video(canvas);
			int index = 0;
			for (auto &row : model->rows) {
				if (!row.canvas && row.video && row.video == video) {
					row.canvas = canvas;
					row.canvas_name = obs_canvas_get_name(canvas);
					row.canvas_color = canvas_color(canvas);
					model->rows_changed.insert(index);
				}
				if (row.canvas == canvas || row.video == video) {
					row.skipped_frames = video_output_get_skipped_frames(video);
					auto canvas_frames = video_output_get_total_frames(video);
					row.canvas_fps = canvas_frames > row.canvas_frames ? canvas_frames - row.canvas_frames : 0;
					row.canvas_frames = canvas_frames;
					struct obs_video_info ovi;
					if (obs_canvas_get_video_info(canvas, &ovi)) {
						row.canvas_width = ovi.base_width;
						row.canvas_height = ovi.base_height;
					}
					model->rows_changed.insert(index);
				}
				++index;
			}
			return true;
		},
		this);

	auto now = QDateTime::currentDateTime();
	int idx = 0;
	auto palette = QPalette();
	auto line_color = palette.text().color().rgba();
	auto graph_color = palette.highlight().color().rgba();
	for (auto &row : rows) {
		updateGraph(&row.output_bitrate_graph, row.output_bitrate, &row.output_bitrate_max, row.output_bitrate_graph_width,
			    row.row_height, line_color, graph_color);
		updateGraph(&row.output_fps_graph, row.output_fps, &row.output_fps_max, row.output_fps_graph_width, row.row_height,
			    line_color, graph_color);
		updateGraph(&row.encoded_fps_graph, row.encoded_fps, &row.encoded_fps_max, row.encoded_fps_graph_width,
			    row.row_height, line_color, graph_color);
		updateGraph(&row.canvas_fps_graph, row.canvas_fps, &row.canvas_fps_max, row.canvas_fps_graph_width, row.row_height,
			    line_color, graph_color);
		if (row.updated) {
			row.last_update = now;
			++idx;
			continue;
		}
		if (row.encoded_fps || row.output_bitrate || row.output_fps || row.canvas_fps) {
			row.encoded_fps = 0;
			row.output_bitrate = 0;
			row.output_fps = 0;
			row.canvas_fps = 0;
			rows_changed.insert(idx);
		}
		++idx;
	}

	if (rows_changed.empty())
		return;
	auto min_row = *std::min_element(rows_changed.begin(), rows_changed.end());
	auto max_row = *std::max_element(rows_changed.begin(), rows_changed.end());
	rows_changed.clear();
	emit dataChanged(index(min_row, 0), index(max_row, (int)columns.size() - 1), {Qt::DisplayRole, Qt::UserRole});
}

void OutputStatsModel::updateGraph(QImage *graph, uint32_t value, uint32_t *max_value, int width, int height, uint line_color,
				   uint graph_color)
{
	if (width > 0) {
		if (value > 0 && value * 12 < *max_value * 10) {
			*graph = graph->copy(0, 1, graph->width(), graph->height() - 1)
					 .scaled(graph->width(), height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			auto diff = *max_value / height;
			*max_value -= diff;
		} else if (value > *max_value) {
			if (*max_value > 0 && height > 0) {
				auto h = graph->height() * value / *max_value;
				*graph = graph->copy(0, graph->height() - h, graph->width(), h);
			}
			*max_value = value;
		}

		if (height > 0 && height != graph->height()) {
			*graph = graph->scaled(graph->width(), height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		}
		*graph = graph->copy(graph->width() - width + 1, 0, width, graph->height());
		if (*max_value > 0) {
			auto y = height - (int)round(height * value / (*max_value * 1.2));
			auto x = graph->width() - 1;
			for (int i = 0; i < y; i++)
				graph->setPixel(x, i, 0);
			if (y < height)
				graph->setPixel(x, y, line_color);
			for (int i = y + 1; i < height; i++)
				graph->setPixel(x, i, graph_color);
		}
	} else if (graph->width() > 1) {
		*graph = graph->copy(graph->width(), 0, 1, graph->height());
		graph->fill(0);
	}
}

std::string OutputStatsModel::output_name(obs_output_t *output)
{
	auto name = QString::fromUtf8(obs_output_get_name(output));
	if (name.startsWith(QString::fromUtf8("Aitum Stream Suite Output "))) {
		name.replace(QString::fromUtf8("Aitum Stream Suite Output "), QString::fromUtf8(""));
	} else if (name.startsWith(QString::fromUtf8("aitum_multi_output_"))) {
		name.replace(QString::fromUtf8("aitum_multi_output_"), QString::fromUtf8(""));
	} else if (name == "simple_file_output" || name == "adv_file_output" || name == "simple_ffmpeg_output" ||
		   name == "adv_ffmpeg_output") {
		name = QString::fromUtf8(obs_module_text("BuiltinRecord"));
	} else if (name == "simple_stream" || name == "adv_stream" || name == "rtmp multitrack video") {
		name = QString::fromUtf8(obs_module_text("BuiltinStream"));
	} else if (name == "virtualcam_output") {
		name = QString::fromUtf8(obs_module_text("BuiltinVirtualCamera"));
	} else {
		//"mp4 multitrack video", "flv multitrack video", "test_stream", "null"
		//decklink_output, aja_output, aja_preview_output
		name.replace(QString::fromUtf8("_"), QString::fromUtf8(" "));
	}
	return name.toStdString();
}

std::string OutputStatsModel::encoder_name(obs_encoder_t *encoder)
{
	auto name = QString::fromUtf8(obs_encoder_get_name(encoder));
	if (name.startsWith(QString::fromUtf8("Aitum Stream Suite Video "))) {
		name.replace(QString::fromUtf8("Aitum Stream Suite Video "), QString::fromUtf8(""));
	} else if (name.startsWith(QString::fromUtf8("Aitum Stream Suite Audio "))) {
		name.replace(QString::fromUtf8("Aitum Stream Suite Audio "), QString::fromUtf8(""));
	} else {
		name.replace(QString::fromUtf8("_"), QString::fromUtf8(" "));
	}
	return name.toStdString();
}

extern obs_data_t *current_profile_config;

QColor OutputStatsModel::canvas_color(obs_canvas_t *canvas)
{
	QColor color(0, 0, 0, 0);
	if (!current_profile_config)
		return color;
	auto canvas_name = obs_canvas_get_name(canvas);
	auto canvases = obs_data_get_array(current_profile_config, "canvas");
	auto canvas_count = obs_data_array_count(canvases);
	for (size_t i = 0; i < canvas_count; ++i) {
		obs_data_t *c = obs_data_array_item(canvases, i);
		if (!c)
			continue;
		if (strcmp(obs_data_get_string(c, "name"), canvas_name) == 0) {
			color = color_from_int(obs_data_get_int(c, "color"));
			color.setAlpha(255);
			obs_data_release(c);
			break;
		}
		obs_data_release(c);
	}

	obs_data_array_release(canvases);

	return color;
}

void UserRoleTypeDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	auto data = index.data(Qt::UserRole);
	if (data.canConvert<QImage>()) {
		auto image = data.value<QImage>();
		if (image.height() != option.rect.height())
			image = image.scaled(image.width(), option.rect.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		if (image.width() > option.rect.width())
			painter->drawImage(option.rect.left(), option.rect.top(), image, image.width() - option.rect.width());
		else
			painter->drawImage(option.rect.right() - image.width(), option.rect.top(), image);
	}

	auto row = (struct OutputStatsRow *)index.data(Qt::UserRole + 1).toLongLong();
	if (row->row_height != option.rect.height())
		row->row_height = option.rect.height();

	QStyledItemDelegate::paint(painter, option, index);

	QPen pen = painter->pen();
	pen.setColor(row->canvas_color);
	pen.setWidth(2);
	painter->setPen(pen);
	//painter->drawRect(option.rect.left() + 1, option.rect.top() + 1, option.rect.width() - 2, option.rect.height() - 2);
	painter->drawLine(option.rect.left() + 1, option.rect.top() + 1, option.rect.right(), option.rect.top() + 1);
	painter->drawLine(option.rect.left() + 1, option.rect.bottom(), option.rect.right(), option.rect.bottom());
}

GroupedHeaderView::GroupedHeaderView(Qt::Orientation orientation, QWidget *parent) : QHeaderView(orientation, parent) {}

void GroupedHeaderView::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const
{
	if (!rect.isValid()) {
		return;
	}

	// get the state of the section
	QStyleOptionHeader opt;
	initStyleOption(&opt);

	QStyle::State state = QStyle::State_None;

	if (isEnabled()) {
		state |= QStyle::State_Enabled;
	}

	if (window()->isActiveWindow()) {
		state |= QStyle::State_Active;
	}

	//if (sectionsClickable()) {
	//	if (logicalIndex == d->hover) {
	//		state |= QStyle::State_MouseOver;
	//	}
	//
	//	if (logicalIndex == pressed()) {
	//		state |= QStyle::State_Sunken;
	//
	//	} else if (d->highlightSelected) {
	//		if (d->sectionIntersectsSelection(logicalIndex)) {
	//			state |= QStyle::State_On;
	//		}
	//
	//		if (d->isSectionSelected(logicalIndex)) {
	//			state |= QStyle::State_Sunken;
	//		}
	//	}
	//}

	if (isSortIndicatorShown() && sortIndicatorSection() == logicalIndex)
		opt.sortIndicator = (sortIndicatorOrder() == Qt::AscendingOrder) ? QStyleOptionHeader::SortDown
										 : QStyleOptionHeader::SortUp;

	// setup the style options structure
	QVariant textAlignment = model()->headerData(logicalIndex, orientation(), Qt::TextAlignmentRole);
	opt.rect = QRect(rect.left(), rect.top() + opt.rect.height() / 2, rect.width(), rect.height() / 2);
	opt.section = logicalIndex;
	opt.state |= state;

	opt.textAlignment = Qt::Alignment(textAlignment.isValid() ? Qt::Alignment(textAlignment.toInt()) : defaultAlignment());

	opt.iconAlignment = Qt::AlignVCenter;
	auto text = model()->headerData(logicalIndex, orientation(), Qt::DisplayRole).toString();
	auto t = text.split("\n");
	opt.text = t[1];

	int margin = 2 * style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, this);

	const Qt::Alignment headerArrowAlignment =
		static_cast<Qt::Alignment>(style()->styleHint(QStyle::SH_Header_ArrowAlignment, nullptr, this));
	const bool isHeaderArrowOnTheSide = headerArrowAlignment & Qt::AlignVCenter;
	if (isSortIndicatorShown() && sortIndicatorSection() == logicalIndex && isHeaderArrowOnTheSide) {
		margin += style()->pixelMetric(QStyle::PM_HeaderMarkSize, nullptr, this);
	}

	if (textElideMode() != Qt::ElideNone) {
		opt.text = opt.fontMetrics.elidedText(opt.text, textElideMode(), rect.width() - margin);
	}

	QVariant variant = model()->headerData(logicalIndex, orientation(), Qt::DecorationRole);
	opt.icon = variant.value<QIcon>();

	if (opt.icon.isNull()) {
		opt.icon = variant.value<QPixmap>();
	}

	QVariant foregroundBrush = model()->headerData(logicalIndex, orientation(), Qt::ForegroundRole);

	if (foregroundBrush.canConvert<QBrush>()) {
		opt.palette.setBrush(QPalette::ButtonText, foregroundBrush.value<QBrush>());
	}

	QPointF oldBO = painter->brushOrigin();
	QVariant backgroundBrush = model()->headerData(logicalIndex, orientation(), Qt::BackgroundRole);

	if (backgroundBrush.canConvert<QBrush>()) {
		opt.palette.setBrush(QPalette::Button, foregroundBrush.value<QBrush>());
		opt.palette.setBrush(QPalette::Window, backgroundBrush.value<QBrush>());
		painter->setBrushOrigin(opt.rect.topLeft());
	}

	// the section position
	int visual = visualIndex(logicalIndex);
	Q_ASSERT(visual != -1);

	bool first = isFirstVisibleSection(visual);
	bool last = isLastVisibleSection(visual);

	if (first && last) {
		opt.position = QStyleOptionHeader::OnlyOneSection;
	} else if (first) {
		opt.position = QStyleOptionHeader::Beginning;
	} else if (last) {
		opt.position = QStyleOptionHeader::End;
	} else {
		opt.position = QStyleOptionHeader::Middle;
	}

	opt.orientation = orientation();

	// the selected position
	//bool previousSelected = false; //d->isSectionSelected(this->logicalIndex(visual - 1));
	//bool nextSelected = false;     //d->isSectionSelected(this->logicalIndex(visual + 1));

	//if (previousSelected && nextSelected) {
	//	opt.selectedPosition = QStyleOptionHeader::NextAndPreviousAreSelected;
	//} else if (previousSelected) {
	//	opt.selectedPosition = QStyleOptionHeader::PreviousIsSelected;
	//} else if (nextSelected) {
	//	opt.selectedPosition = QStyleOptionHeader::NextIsSelected;
	//} else {
	//	opt.selectedPosition = QStyleOptionHeader::NotAdjacent;
	//}

	// draw the section
	style()->drawControl(QStyle::CE_Header, &opt, painter, this);

	auto prev_group_visual = visual;
	auto left = rect.left();
	auto li = this->logicalIndex(prev_group_visual);
	while (li >= 0 && model()->headerData(li, orientation(), Qt::DisplayRole).toString().startsWith(t[0])) {
		if (li != logicalIndex)
			left -= sectionSize(li);
		prev_group_visual--;
		li = this->logicalIndex(prev_group_visual);
		while (li >= 0 && isSectionHidden(li)) {
			prev_group_visual--;
			li = this->logicalIndex(prev_group_visual);
		}
	}
	first = li < 0;

	auto next_group_visual = prev_group_visual + 1;
	li = this->logicalIndex(next_group_visual);
	while (li >= 0 && isSectionHidden(li)) {
		next_group_visual++;
		li = this->logicalIndex(next_group_visual);
	}
	auto width = rect.width();
	while (li >= 0 && model()->headerData(li, orientation(), Qt::DisplayRole).toString().startsWith(t[0])) {
		if (li != logicalIndex)
			width += sectionSize(li);
		next_group_visual++;
		li = this->logicalIndex(next_group_visual);
		while (li >= 0 && isSectionHidden(li)) {
			next_group_visual++;
			li = this->logicalIndex(next_group_visual);
		}
	}
	last = li < 0;
	if (!opt.icon.isNull())
		opt.icon = QIcon();
	opt.textAlignment = Qt::AlignCenter;
	opt.rect = QRect(left, rect.top(), width, rect.height() / 2);
	opt.text = t[0];
	if (textElideMode() != Qt::ElideNone) {
		margin = 2 * style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr, this);
		opt.text = opt.fontMetrics.elidedText(opt.text, textElideMode(), width - margin);
	}
	opt.sortIndicator = QStyleOptionHeader::SortIndicator::None;
	if (first && last) {
		opt.position = QStyleOptionHeader::OnlyOneSection;
	} else if (first) {
		opt.position = QStyleOptionHeader::Beginning;
	} else if (last) {
		opt.position = QStyleOptionHeader::End;
	} else {
		opt.position = QStyleOptionHeader::Middle;
	}
	style()->drawControl(QStyle::CE_Header, &opt, painter, this);

	painter->setBrushOrigin(oldBO);
}

bool GroupedHeaderView::isFirstVisibleSection(int visual) const
{
	if (visual < 0)
		return false;
	auto li = logicalIndex(visual);
	if (li < 0)
		return false;
	auto vi = visual - 1;
	li = logicalIndex(vi);
	while (li >= 0 && isSectionHidden(li)) {
		vi--;
		li = logicalIndex(vi);
	}
	return vi < 0;
}

bool GroupedHeaderView::isLastVisibleSection(int visual) const
{
	if (visual < 0)
		return false;
	auto li = logicalIndex(visual);
	if (li < 0)
		return false;
	auto vi = visual + 1;
	li = logicalIndex(vi);
	while (li >= 0 && isSectionHidden(li)) {
		vi++;
		li = logicalIndex(vi);
	}
	return li < 0;
}
