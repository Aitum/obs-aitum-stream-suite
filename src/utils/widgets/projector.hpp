#pragma once

#include <obs.hpp>
#include "qt-display.hpp"
#include <src/docks/canvas-dock.hpp>

bool IsAlwaysOnTop(QWidget *window);
void SetAlwaysOnTop(QWidget *window, bool enable);

class OBSProjector : public OBSQTDisplay {
	Q_OBJECT

private:
	obs_canvas_t *canvas = nullptr;
	obs_source_t *source = nullptr;
	std::vector<OBSSignal> sigs;

	static void OBSRender(void *data, uint32_t cx, uint32_t cy);

	void mousePressEvent(QMouseEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

	bool isAlwaysOnTop;
	bool isAlwaysOnTopOverridden = false;
	int savedMonitor = -1;

	bool ready = false;

	void UpdateProjectorTitle(QString name = "");

	QRect prevGeometry;
	void SetMonitor(int monitor);

	QScreen *screen = nullptr;

	std::function<void(OBSProjector *projector)> deleteProjector = nullptr;

	static void OBSSourceRenamed(void *data, calldata_t *params);
	static void OBSSourceDestroyed(void *data, calldata_t *params);

private slots:
	void EscapeTriggered();
	void OpenFullScreenProjector();
	void ResizeToContent();
	void OpenWindowedProjector();
	void AlwaysOnTopToggled(bool alwaysOnTop);
	void ScreenRemoved(QScreen *screen_);

public:
	OBSProjector(obs_canvas_t *canvas_, obs_source_t *source_, int monitor,
		     std::function<void(OBSProjector *projector)> deleteProjector_);
	~OBSProjector();

	int GetMonitor();
	void RenameProjector(QString oldName, QString newName);
	void SetHideCursor();

	bool IsAlwaysOnTop() const;
	bool IsAlwaysOnTopOverridden() const;
	void SetIsAlwaysOnTop(bool isAlwaysOnTop, bool isOverridden);
};
