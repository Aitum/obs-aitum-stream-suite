#include "../dialogs/name-dialog.hpp"
#include "transitions-dock.hpp"
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QMainWindow>

extern TransitionsDock *transitions_dock;

TransitionsDock::TransitionsDock(QWidget *parent) : QFrame(parent)
{
	auto transitionsGroupLayout = new QVBoxLayout();
	transitionsGroupLayout->setContentsMargins(0, 0, 0, 0);
	transitionsGroupLayout->setSpacing(0);
	setLayout(transitionsGroupLayout);

	transition = new QComboBox();
	transitionsGroupLayout->addWidget(transition);
	auto hl = new QHBoxLayout();
	hl->setContentsMargins(0, 0, 0, 0);
	hl->setSpacing(4);

	auto durationLabel = new QLabel(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransitionDuration")));
	duration = new QSpinBox();
	duration->setMinimum(50);
	duration->setMaximum(20000);
	duration->setSuffix(QStringLiteral("ms"));
	connect(duration, QOverload<int>::of(&QSpinBox::valueChanged), [this](int d) { obs_frontend_set_transition_duration(d); });
	durationLabel->setBuddy(duration);
	hl->addWidget(durationLabel);
	hl->addWidget(duration);
	transitionsGroupLayout->addLayout(hl);

	hl = new QHBoxLayout();
	hl->addStretch();
	auto addButton = new QPushButton();
	addButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	addButton->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.AddTransition")));
	addButton->setToolTip(QString::fromUtf8(obs_frontend_get_locale_string("Basic.AddTransition")));
	addButton->setIcon(QIcon(":/res/images/add.png"));
	addButton->setProperty("themeID", "addIconSmall");
	addButton->setProperty("class", "icon-plus");
	addButton->setProperty("toolButton", true);
	addButton->setFlat(false);

	connect(addButton, &QPushButton::clicked, [this] {
		auto menu = QMenu(this);
		if (canvasDock) {
			auto subMenu = menu.addMenu(QString::fromUtf8(obs_module_text("CopyFromMain")));
			struct obs_frontend_source_list frontend_transitions = {};
			obs_frontend_get_transitions(&frontend_transitions);
			for (size_t i = 0; i < frontend_transitions.sources.num; i++) {
				auto tr = frontend_transitions.sources.array[i];
				const char *name = obs_source_get_name(tr);
				auto action = subMenu->addAction(QString::fromUtf8(name));
				if (!obs_is_source_configurable(obs_source_get_id(tr))) {
					action->setEnabled(false);
				}
				if (canvasDock) {
					for (auto t : canvasDock->transitions) {
						if (strcmp(name, obs_source_get_name(t)) == 0) {
							action->setEnabled(false);
							break;
						}
					}
				}
				connect(action, &QAction::triggered, [this, tr] {
					OBSDataAutoRelease d = obs_save_source(tr);
					OBSSourceAutoRelease t = obs_load_private_source(d);
					if (t) {
						auto n = QString::fromUtf8(obs_source_get_name(t));
						transition->addItem(n);
						if (canvasDock) {
							canvasDock->transitions.emplace_back(t);
							canvasDock->transition->addItem(n);
						}
						transition->setCurrentText(n);
						if (canvasDock) {
							canvasDock->transition->setCurrentText(n);
						}
					}
				});
			}
			obs_frontend_source_list_free(&frontend_transitions);
			menu.addSeparator();
		}
		size_t idx = 0;
		const char *id;
		while (obs_enum_transition_types(idx++, &id)) {
			if (!obs_is_source_configurable(id)) {
				continue;
			}
			const char *display_name = obs_source_get_display_name(id);

			auto action = menu.addAction(QString::fromUtf8(display_name));
			connect(action, &QAction::triggered, [this, id] {
				if (!canvasDock) {
					auto main_window = (QMainWindow *)obs_frontend_get_main_window();
					QMetaObject::invokeMethod(main_window, "AddTransition", Q_ARG(const char *, id));
					return;
				}
				OBSSourceAutoRelease t = obs_source_create_private(id, obs_source_get_display_name(id), nullptr);
				if (t) {
					std::string name = obs_source_get_name(t);
					while (true) {
						if (!NameDialog::AskForName(
							    this, QString::fromUtf8(obs_module_text("TransitionName")), name)) {
							obs_source_release(t);
							return;
						}
						if (name.empty()) {
							continue;
						}
						bool found = false;
						if (canvasDock) {
							for (auto tr : canvasDock->transitions) {
								if (strcmp(obs_source_get_name(tr), name.c_str()) == 0) {
									found = true;
									break;
								}
							}
						} else {
						}
						if (found) {
							continue;
						}

						obs_source_set_name(t, name.c_str());
						break;
					}
					auto n = QString::fromUtf8(obs_source_get_name(t));
					transition->addItem(n);
					if (canvasDock) {
						canvasDock->transitions.emplace_back(t);
						canvasDock->transition->addItem(n);
					}
					transition->setCurrentText(n);
					if (canvasDock) {
						canvasDock->transition->setCurrentText(n);
					}
					obs_frontend_open_source_properties(t);
				}
			});
		}
		menu.exec(QCursor::pos());
	});

	hl->addWidget(addButton);

	auto removeButton = new QPushButton();
	removeButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	removeButton->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.RemoveTransition")));
	removeButton->setToolTip(QString::fromUtf8(obs_frontend_get_locale_string("Basic.RemoveTransition")));
	removeButton->setIcon(QIcon(":/res/images/list_remove.png"));
	removeButton->setProperty("themeID", "removeIconSmall");
	removeButton->setProperty("class", "icon-minus");
	removeButton->setProperty("toolButton", true);
	removeButton->setFlat(false);

	connect(removeButton, &QPushButton::clicked, [this] {
		QMessageBox mb(
			QMessageBox::Question, QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Title")),
			QString::fromUtf8(obs_frontend_get_locale_string("ConfirmRemove.Text")).arg(transition->currentText()),
			QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No));
		mb.setDefaultButton(QMessageBox::NoButton);
		if (mb.exec() != QMessageBox::Yes) {
			return;
		}

		auto n = transition->currentText().toUtf8();
		if (canvasDock) {
			for (auto it = canvasDock->transitions.begin(); it != canvasDock->transitions.end(); ++it) {
				if (strcmp(n.constData(), obs_source_get_name(it->Get())) == 0) {
					if (!obs_is_source_configurable(obs_source_get_id(it->Get()))) {
						return;
					}
					canvasDock->transitions.erase(it);
					break;
				}
			}
			for (auto idx = 0; idx < canvasDock->transition->count(); ++idx) {
				if (canvasDock->transition->itemText(idx) == transition->currentText()) {
					canvasDock->transition->removeItem(idx);
					if (canvasDock->transition->currentIndex() < 0) {
						canvasDock->transition->setCurrentIndex(0);
					}
				}
			}
		} else {
		}
		transition->removeItem(transition->currentIndex());
		if (transition->currentIndex() < 0) {
			transition->setCurrentIndex(0);
		}
	});

	hl->addWidget(removeButton);

	auto propsButton = new QPushButton();
	propsButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	propsButton->setAccessibleName(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransitionProperties")));
	propsButton->setToolTip(QString::fromUtf8(obs_frontend_get_locale_string("Basic.TransitionProperties")));
	propsButton->setIcon(QIcon(":/settings/images/settings/general.svg"));
	propsButton->setProperty("themeID", "menuIconSmall");
	propsButton->setProperty("class", "icon-dots-vert");
	propsButton->setProperty("toolButton", true);
	propsButton->setFlat(false);

	connect(propsButton, &QPushButton::clicked, [this] {
		auto menu = QMenu(this);
		auto action = menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Rename")));
		connect(action, &QAction::triggered, [this] {
			auto tn = transition->currentText().toUtf8();
			obs_source_t *t = GetTransition(tn.constData());
			if (!t) {
				return;
			}
			std::string name = obs_source_get_name(t);
			while (true) {
				if (!NameDialog::AskForName(this, QString::fromUtf8(obs_module_text("TransitionName")), name)) {
					return;
				}
				if (name.empty()) {
					continue;
				}
				bool found = false;
				if (canvasDock) {
					for (auto tr : canvasDock->transitions) {
						if (strcmp(obs_source_get_name(tr), name.c_str()) == 0) {
							found = true;
							break;
						}
					}
				} else {
					struct obs_frontend_source_list frontend_transitions = {};
					obs_frontend_get_transitions(&frontend_transitions);
					for (size_t i = 0; i < frontend_transitions.sources.num; i++) {
						obs_source_t *tr = frontend_transitions.sources.array[i];
						if (strcmp(obs_source_get_name(tr), name.c_str()) == 0) {
							found = true;
							break;
						}
					}
					obs_frontend_source_list_free(&frontend_transitions);
				}
				if (found) {
					continue;
				}
				auto qn = QString::fromUtf8(name.c_str());
				auto old_name = QString::fromUtf8(obs_source_get_name(t));
				transition->setItemText(transition->currentIndex(), qn);
				obs_source_set_name(t, name.c_str());
				if (canvasDock) {
					for (auto idx = 0; idx < canvasDock->transition->count(); ++idx) {
						if (canvasDock->transition->itemText(idx) == old_name) {
							canvasDock->transition->setItemText(idx, qn);
						}
					}
				}
				break;
			}
		});
		action = menu.addAction(QString::fromUtf8(obs_frontend_get_locale_string("Properties")));
		connect(action, &QAction::triggered, [this] {
			auto tn = transition->currentText().toUtf8();
			auto t = GetTransition(tn.constData());
			if (!t) {
				return;
			}
			obs_frontend_open_source_properties(t);
		});
		menu.exec(QCursor::pos());
	});

	hl->addWidget(propsButton);

	transitionsGroupLayout->addLayout(hl);
	transitionsGroupLayout->addStretch();

	connect(transition, &QComboBox::currentTextChanged, [this, removeButton, propsButton] {
		auto tn = transition->currentText().toUtf8();
		auto t = GetTransition(tn.constData());
		if (!t) {
			return;
		}
		if (canvasDock) {
			//canvasDock->SwapTransition(t);
			for (auto idx = 0; idx < canvasDock->transition->count(); ++idx) {
				if (canvasDock->transition->itemText(idx) == transition->currentText()) {
					canvasDock->transition->setCurrentIndex(idx);
					break;
				}
			}
		} else {
			obs_frontend_set_current_transition(t);
		}
		bool config = obs_is_source_configurable(obs_source_get_id(t));
		removeButton->setEnabled(config);
		propsButton->setEnabled(config);
	});
}

TransitionsDock::~TransitionsDock()
{
	obs_weak_canvas_release(canvas);
	transitions_dock = nullptr;
}

obs_source_t *TransitionsDock::GetTransition(const char *transition_name)
{
	if (!transition_name || !strlen(transition_name)) {
		return nullptr;
	}
	obs_source_t *t = nullptr;
	if (canvasDock) {
		for (auto transition : canvasDock->transitions) {
			if (strcmp(transition_name, obs_source_get_name(transition)) == 0) {
				t = transition;
				break;
			}
		}
	} else {
		struct obs_frontend_source_list frontend_transitions = {};
		obs_frontend_get_transitions(&frontend_transitions);
		for (size_t i = 0; i < frontend_transitions.sources.num; i++) {
			obs_source_t *transition = frontend_transitions.sources.array[i];
			if (strcmp(transition_name, obs_source_get_name(transition)) == 0) {
				t = transition;
				break;
			}
		}
		obs_frontend_source_list_free(&frontend_transitions);
	}
	return t;
}

void TransitionsDock::SetCanvas(obs_canvas_t *new_canvas, CanvasDock *new_canvas_dock)
{
	obs_weak_canvas_release(canvas);
	canvas = obs_canvas_get_weak_canvas(new_canvas);
	canvasDock = new_canvas_dock;

	QSignalBlocker tb(transition);
	transition->clear();
	if (canvasDock) {
		for (auto &t : canvasDock->transitions) {
			transition->addItem(QString::fromUtf8(obs_source_get_name(t)));
		}
	} else {
		auto mc = obs_get_main_canvas();
		if (new_canvas == mc) {
			struct obs_frontend_source_list frontend_transitions = {};
			obs_frontend_get_transitions(&frontend_transitions);
			for (size_t i = 0; i < frontend_transitions.sources.num; i++) {
				transition->addItem(QString::fromUtf8(obs_source_get_name(frontend_transitions.sources.array[i])));
			}
			obs_frontend_source_list_free(&frontend_transitions);

			auto current = obs_frontend_get_current_transition();
			if (current) {
				auto cn = QString::fromUtf8(obs_source_get_name(current));
				transition->setCurrentText(cn);
				obs_source_release(current);
			}
		} else {
		}
		obs_canvas_release(mc);
	}
	TransitionDurationChanged();
}

void TransitionsDock::UpdateCurrentTransition()
{
	if (canvasDock) {
		transition->setCurrentText(canvasDock->transition->currentText());
	} else {
		auto ct = obs_frontend_get_current_transition();
		if (ct) {
			transition->setCurrentText(obs_source_get_name(ct));
			obs_source_release(ct);
		}
	}
}

void TransitionsDock::TransitionDurationChanged()
{
	QSignalBlocker sb(duration);
	duration->setValue(obs_frontend_get_transition_duration());
}

void TransitionsDock::TransitionChanged()
{
	UpdateCurrentTransition();
}

void TransitionsDock::TransitionListChanged()
{
	if (canvasDock) {
		return;
	}
	auto c = obs_weak_canvas_get_canvas(canvas);
	if (!c) {
		return;
	}
	SetCanvas(c, nullptr);
	obs_canvas_release(c);
}
