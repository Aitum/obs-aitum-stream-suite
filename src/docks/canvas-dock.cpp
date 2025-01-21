#include "canvas-dock.hpp"
#include <QSplitter>
#include <QListView>
#include <QListWidget>
#include <QGroupBox>
#include "../utils/widgets/switching-splitter.hpp"
#include <obs-module.h>

CanvasDock::CanvasDock(obs_data_t *settings, QWidget *parent)
	: QFrame(parent),
	  preview(new OBSQTDisplay(this))
{
	UNUSED_PARAMETER(settings);
	setObjectName(QStringLiteral("contextContainer"));
	setContentsMargins(0, 0, 0, 0);
	auto split = new SwitchingSplitter;
	auto l = new QBoxLayout(QBoxLayout::TopToBottom, this);
	setLayout(l);
	l->addWidget(split);
	split->setOrientation(Qt::Vertical);

	split->addWidget(preview);

	//setLayout(mainLayout);

	preview->setObjectName(QStringLiteral("preview"));
	preview->setMinimumSize(QSize(24, 24));
	QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
	sizePolicy1.setHorizontalStretch(0);
	sizePolicy1.setVerticalStretch(0);
	sizePolicy1.setHeightForWidth(preview->sizePolicy().hasHeightForWidth());
	preview->setSizePolicy(sizePolicy1);

	preview->setMouseTracking(true);
	preview->setFocusPolicy(Qt::StrongFocus);
	//preview->installEventFilter(eventFilter.get());

	preview->show(); 
	connect(preview, &OBSQTDisplay::DisplayCreated,
		[this]() { obs_display_add_draw_callback(preview->GetDisplay(), DrawPreview, this); });

	//obs_display_set_enabled(preview->GetDisplay(), !preview_disabled);

	auto panels = new SwitchingSplitter;
	//auto panelsLayout = new QVBoxLayout();
	//auto panelsLayout = new FlowLayout();
	//panels->setLayout(panelsLayout);
	auto scenesGroup = new QGroupBox(QString::fromUtf8(obs_module_text("Scenes")));
	auto scenesGroupLayout = new QVBoxLayout();
	scenesGroup->setLayout(scenesGroupLayout);
	auto sceneList = new QListWidget();
	//sceneList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	sceneList->setFrameShape(QFrame::NoFrame);
	sceneList->setFrameShadow(QFrame::Plain);
	sceneList->setSelectionMode(QAbstractItemView::SingleSelection);
	sceneList->setViewMode(QListView::ListMode);
	//sceneList->setResizeMode(QListView::Fixed);
	sceneList->addItem("Scene 1");
	sceneList->addItem("Scene 2");
	sceneList->addItem("Scene 3");
	sceneList->addItem("Scene 4");
	sceneList->addItem("Scene 5");
	scenesGroupLayout->addWidget(sceneList);

	panels->addWidget(scenesGroup);
	//panelsLayout->addWidget(scenesGroup);

	auto sourcesGroup = new QGroupBox(QString::fromUtf8(obs_module_text("Sources")));
	auto sourcesGroupLayout = new QVBoxLayout();
	sourcesGroup->setLayout(sourcesGroupLayout);
	auto sourceList = new QListWidget(); //new QListView;
	//sourceList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
	sourceList->setFrameShape(QFrame::NoFrame);
	sourceList->setFrameShadow(QFrame::Plain);
	sourceList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	sourceList->addItem("Source 1");
	sourceList->addItem("Source 2");
	sourceList->addItem("Source 3");
	sourceList->addItem("Source 4");
	sourceList->addItem("Source 5");
	sourcesGroupLayout->addWidget(sourceList);

	panels->addWidget(sourcesGroup);
	//panelsLayout->addWidget(sourcesGroup);
	split->addWidget(panels);
}

void CanvasDock::DrawPreview(void *data, uint32_t cx, uint32_t cy)
{
	UNUSED_PARAMETER(cx);
	UNUSED_PARAMETER(cy);
	CanvasDock *window = static_cast<CanvasDock *>(data);
	if (!window)
		return;

}

CanvasDock::~CanvasDock() {

}
