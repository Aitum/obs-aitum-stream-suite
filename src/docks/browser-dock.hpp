#pragma once
#include "browser-panel.hpp"
#include <obs.hpp>
#include <QVBoxLayout>
#include <QInputEvent>
#include <QWidget>
#include <QWindow>
#include "src/utils/event-filter.hpp"

class BrowserDock : public QWidget {
	Q_OBJECT

	friend class BrowserSurfaceEventFilter;

	virtual void paintEvent(QPaintEvent *event) override;
	virtual void moveEvent(QMoveEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	virtual bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#else
	virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
	bool wayland = false;
	std::unique_ptr<OBSEventFilter> eventFilter;
	OBSDisplay display;
	OBSSource browser_source;
	QCefWidget *cefWidget = nullptr;
	QVBoxLayout *layout = nullptr;
	std::string url;

	OBSEventFilter *BuildEventFilter();

	bool GetSourceRelativeXY(int mouseX, int mouseY, int &x, int &y);

	bool HandleMouseClickEvent(QMouseEvent *event);
	bool HandleMouseMoveEvent(QMouseEvent *event);
	bool HandleMouseWheelEvent(QWheelEvent *event);
	bool HandleFocusEvent(QFocusEvent *event);
	bool HandleKeyEvent(QKeyEvent *event);

	void CreateDisplay(bool force = false);
	void DestroyDisplay() { display = nullptr; };

	static void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY, int &x, int &y, float &scale);
	static int TranslateQtMouseEventModifiers(QMouseEvent *event);
	static int TranslateQtKeyboardEventModifiers(QInputEvent *event, bool mouseEvent);
	static bool QTToGSWindow(QWindow *window, gs_window &gswindow);
	static void Draw(void *data, uint32_t cx, uint32_t cy);

public:
	BrowserDock(const char* name, const char *url, QWidget *parent = nullptr);
	~BrowserDock();

	void Refresh();
	void Reset();
};

class BrowserSurfaceEventFilter : public QObject {
	BrowserDock *display;
	int mTimerId;

public:
	BrowserSurfaceEventFilter(BrowserDock *src) : display(src), mTimerId(0) {}

protected:
	bool eventFilter(QObject *obj, QEvent *event) override
	{
		bool result = QObject::eventFilter(obj, event);
		QPlatformSurfaceEvent *surfaceEvent;

		switch (event->type()) {
		case QEvent::PlatformSurface:
			surfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);

			switch (surfaceEvent->surfaceEventType()) {
			case QPlatformSurfaceEvent::SurfaceCreated:
				if (display->windowHandle()->isExposed()) {
					createOBSDisplay();
				} else {
					mTimerId = startTimer(67); // Arbitrary
				}
				break;
			case QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed:
				display->DestroyDisplay();
				break;
			default:
				break;
			}

			break;
		case QEvent::Expose:
			createOBSDisplay();
			break;
		default:
			break;
		}

		return result;
	}

	void timerEvent(QTimerEvent *) override { createOBSDisplay(display->isVisible()); }

private:
	void createOBSDisplay(bool force = false)
	{
		display->CreateDisplay(force);
		if (mTimerId > 0) {
			killTimer(mTimerId);
			mTimerId = 0;
		}
	}
};
