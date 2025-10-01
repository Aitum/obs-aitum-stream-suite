#pragma once

#include <QWidget>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QWindow>
#include <obs-frontend-api.h>
#include <obs.hpp>
#include "../utils/event-filter.hpp"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <obs-nix-platform.h>
#endif

#ifdef ENABLE_WAYLAND
#include <QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#endif

class BrowserDock : public QWidget {
	Q_OBJECT

	virtual void paintEvent(QPaintEvent *event) override;
	virtual void moveEvent(QMoveEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	virtual bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#else
	virtual bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif
private:
	OBSDisplay display;
	OBSSource browser_source;

	std::unique_ptr<OBSEventFilter> eventFilter;

	bool GetSourceRelativeXY(int mouseX, int mouseY, int &x, int &y);

	bool HandleMouseClickEvent(QMouseEvent *event);
	bool HandleMouseMoveEvent(QMouseEvent *event);
	bool HandleMouseWheelEvent(QWheelEvent *event);
	bool HandleFocusEvent(QFocusEvent *event);
	bool HandleKeyEvent(QKeyEvent *event);

	OBSEventFilter *BuildEventFilter();

	static void Draw(void *data, uint32_t cx, uint32_t cy);
	static void frontend_event(enum obs_frontend_event event, void *data);

public:
	BrowserDock(const char *name, const char *url, QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
	~BrowserDock() { display = nullptr; }

	void CreateDisplay(bool force = false);
	void DestroyDisplay() { display = nullptr; };
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
				if (display->windowHandle()->isExposed())
					createOBSDisplay();
				else
					mTimerId = startTimer(67); // Arbitrary
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
