#include <obs-module.h>
#include <QMainWindow>
#include <QWindow>
#include <QMouseEvent>
#include <QDockWidget>

#include "browser-dock.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#include <util/platform.h>

BrowserDock::BrowserDock(const char *name, const char *url, QWidget *parent, Qt::WindowFlags flags)
	: QWidget(parent, flags),
	  eventFilter(BuildEventFilter())
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);
	installEventFilter(eventFilter.get());

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", false);
	obs_data_set_string(settings, "url", url);
	obs_data_set_string(settings, "css", "");
	browser_source = obs_source_create_private("browser_source", name, settings);
	obs_source_inc_showing(browser_source);

	auto windowVisible = [this](bool visible) {
		if (!visible) {
#if !defined(_WIN32) && !defined(__APPLE__)
			display = nullptr;
#endif
			return;
		}

		if (!display) {
			CreateDisplay();
		} else {
			QSize s = size() * devicePixelRatioF();
			obs_display_resize(display, s.width(), s.height());
		}
	};

	auto screenChanged = [this](QScreen *) {
		CreateDisplay();

		QSize s = size() * devicePixelRatioF();
		obs_display_resize(display, s.width(), s.height());
	};

	connect(windowHandle(), &QWindow::visibleChanged, windowVisible);
	connect(windowHandle(), &QWindow::screenChanged, screenChanged);

	windowHandle()->installEventFilter(new BrowserSurfaceEventFilter(this));
	obs_frontend_add_event_callback(frontend_event, this);
}

void BrowserDock::paintEvent(QPaintEvent *event)
{
	CreateDisplay();
	QWidget::paintEvent(event);
}

void BrowserDock::moveEvent(QMoveEvent *event)
{
	QWidget::moveEvent(event);
	if (display)
		obs_display_update_color_space(display);
}

void BrowserDock::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	CreateDisplay();

	QSize s = size() * devicePixelRatioF();
	if (isVisible() && display) {
		obs_display_resize(display, s.width(), s.height());
	}
	if (browser_source) {
		OBSDataAutoRelease bs = obs_data_create();
		obs_data_set_int(bs, "width", s.width());
		obs_data_set_int(bs, "height", s.height());
		obs_source_update(browser_source, bs);
	}

	//emit DisplayResized();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool BrowserDock::nativeEvent(const QByteArray &, void *message, qintptr *)
#else
bool BrowserDock::nativeEvent(const QByteArray &, void *message, long *)
#endif
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	switch (msg.message) {
	case WM_DISPLAYCHANGE:
		if (display)
			obs_display_update_color_space(display);
	}
#else
	UNUSED_PARAMETER(message);
#endif

	return false;
}

bool QTToGSWindow(QWindow *window, gs_window &gswindow)
{
	bool success = true;

#ifdef _WIN32
	gswindow.hwnd = (HWND)window->winId();
#elif __APPLE__
	gswindow.view = (id)window->winId();
#else
	switch (obs_get_nix_platform()) {
	case OBS_NIX_PLATFORM_X11_EGL:
		gswindow.id = window->winId();
		gswindow.display = obs_get_nix_platform_display();
		break;
#ifdef ENABLE_WAYLAND
	case OBS_NIX_PLATFORM_WAYLAND: {
		QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
		gswindow.display = native->nativeResourceForWindow("surface", window);
		success = gswindow.display != nullptr;
		break;
	}
#endif
	default:
		success = false;
		break;
	}
#endif
	return success;
}

void BrowserDock::CreateDisplay(bool force)
{
	if (display)
		return;

	if (!windowHandle()->isExposed() && !force)
		return;

	if (!isVisible() && !force)
		return;

	QSize s = size() * devicePixelRatioF();

	gs_init_data info = {};
	info.cx = s.width();
	info.cy = s.height();
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;

	if (!QTToGSWindow(windowHandle(), info.window))
		return;

	display = obs_display_create(&info, 0xFF4C4C4C);

	obs_display_add_draw_callback(display, Draw, this);
}

static inline void GetScaleAndCenterPos(int baseCX, int baseCY, int windowCX, int windowCY, int &x, int &y, float &scale)
{
	int newCX, newCY;

	const double windowAspect = double(windowCX) / double(windowCY);
	const double baseAspect = double(baseCX) / double(baseCY);

	if (windowAspect > baseAspect) {
		scale = float(windowCY) / float(baseCY);
		newCX = int(double(windowCY) * baseAspect);
		newCY = windowCY;
	} else {
		scale = float(windowCX) / float(baseCX);
		newCX = windowCX;
		newCY = int(float(windowCX) / baseAspect);
	}

	x = windowCX / 2 - newCX / 2;
	y = windowCY / 2 - newCY / 2;
}

void BrowserDock::Draw(void *data, uint32_t cx, uint32_t cy)
{
	BrowserDock *window = static_cast<BrowserDock *>(data);

	if (!window->browser_source)
		return;

	uint32_t sourceCX = obs_source_get_width(window->browser_source);
	if (sourceCX <= 0)
		sourceCX = 1;
	uint32_t sourceCY = obs_source_get_height(window->browser_source);
	if (sourceCY <= 0)
		sourceCY = 1;

	int x, y;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, cx, cy, x, y, scale);
	auto newCX = scale * float(sourceCX);
	auto newCY = scale * float(sourceCY);

	if (scale != 1.0f) {
		OBSDataAutoRelease bs = obs_data_create();
		obs_data_set_int(bs, "width", cx);
		obs_data_set_int(bs, "height", cy);
		obs_source_update(window->browser_source, bs);
	}

	gs_viewport_push();
	gs_projection_push();
	const bool previous = gs_set_linear_srgb(true);

	gs_ortho(0.0f, float(sourceCX), 0.0f, float(sourceCY), -100.0f, 100.0f);
	gs_set_viewport(x, y, newCX, newCY);
	obs_source_video_render(window->browser_source);

	gs_set_linear_srgb(previous);
	gs_projection_pop();
	gs_viewport_pop();
}

void BrowserDock::frontend_event(enum obs_frontend_event event, void *data)
{
	BrowserDock *window = static_cast<BrowserDock *>(data);
	if (event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		obs_frontend_remove_event_callback(frontend_event, data);
		obs_source_release(window->browser_source);
		window->browser_source = nullptr;
	}
}

OBSEventFilter *BrowserDock::BuildEventFilter()
{
	return new OBSEventFilter([this](QObject *obj, QEvent *event) {
		UNUSED_PARAMETER(obj);

		switch (event->type()) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonRelease:
		case QEvent::MouseButtonDblClick:
			return this->HandleMouseClickEvent(static_cast<QMouseEvent *>(event));
		case QEvent::MouseMove:
		case QEvent::Enter:
		case QEvent::Leave:
			return this->HandleMouseMoveEvent(static_cast<QMouseEvent *>(event));

		case QEvent::Wheel:
			return this->HandleMouseWheelEvent(static_cast<QWheelEvent *>(event));
		case QEvent::FocusIn:
		case QEvent::FocusOut:
			return this->HandleFocusEvent(static_cast<QFocusEvent *>(event));
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
			return this->HandleKeyEvent(static_cast<QKeyEvent *>(event));
		default:
			return false;
		}
	});
}

static int TranslateQtKeyboardEventModifiers(QInputEvent *event, bool mouseEvent)
{
	int obsModifiers = INTERACT_NONE;

	if (event->modifiers().testFlag(Qt::ShiftModifier))
		obsModifiers |= INTERACT_SHIFT_KEY;
	if (event->modifiers().testFlag(Qt::AltModifier))
		obsModifiers |= INTERACT_ALT_KEY;
#ifdef __APPLE__
	// Mac: Meta = Control, Control = Command
	if (event->modifiers().testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_COMMAND_KEY;
	if (event->modifiers().testFlag(Qt::MetaModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
#else
	// Handle windows key? Can a browser even trap that key?
	if (event->modifiers().testFlag(Qt::ControlModifier))
		obsModifiers |= INTERACT_CONTROL_KEY;
#endif

	if (!mouseEvent) {
		if (event->modifiers().testFlag(Qt::KeypadModifier))
			obsModifiers |= INTERACT_IS_KEY_PAD;
	}

	return obsModifiers;
}

static int TranslateQtMouseEventModifiers(QMouseEvent *event)
{
	int modifiers = TranslateQtKeyboardEventModifiers(event, true);

	if (event->buttons().testFlag(Qt::LeftButton))
		modifiers |= INTERACT_MOUSE_LEFT;
	if (event->buttons().testFlag(Qt::MiddleButton))
		modifiers |= INTERACT_MOUSE_MIDDLE;
	if (event->buttons().testFlag(Qt::RightButton))
		modifiers |= INTERACT_MOUSE_RIGHT;

	return modifiers;
}

bool BrowserDock::HandleMouseClickEvent(QMouseEvent *event)
{
	const bool mouseUp = event->type() == QEvent::MouseButtonRelease;
	uint32_t clickCount = 1;
	if (event->type() == QEvent::MouseButtonDblClick)
		clickCount = 2;

	struct obs_mouse_event mouseEvent = {};

	mouseEvent.modifiers = TranslateQtMouseEventModifiers(event);

	int32_t button = 0;

	switch (event->button()) {
	case Qt::LeftButton:
		button = MOUSE_LEFT;
		break;
	case Qt::MiddleButton:
		button = MOUSE_MIDDLE;
		break;
	case Qt::RightButton:
		button = MOUSE_RIGHT;
		break;
	default:
		blog(LOG_WARNING, "unknown button type %d", event->button());
		return false;
	}

	// Why doesn't this work?
	//if (event->flags().testFlag(Qt::MouseEventCreatedDoubleClick))
	//	clickCount = 2;

	const bool insideSource = GetSourceRelativeXY(event->pos().x(), event->pos().y(), mouseEvent.x, mouseEvent.y);

	if (browser_source && (mouseUp || insideSource))
		obs_source_send_mouse_click(browser_source, &mouseEvent, button, mouseUp, clickCount);

	return true;
}

bool BrowserDock::GetSourceRelativeXY(int mouseX, int mouseY, int &relX, int &relY)
{
	float pixelRatio = devicePixelRatioF();

	int mouseXscaled = (int)roundf(mouseX * pixelRatio);
	int mouseYscaled = (int)roundf(mouseY * pixelRatio);

	QSize s = size() * pixelRatio;

	uint32_t sourceCX = browser_source ? obs_source_get_width(browser_source) : 1;
	if (sourceCX <= 0)
		sourceCX = 1;
	uint32_t sourceCY = browser_source ? obs_source_get_height(browser_source) : 1;
	if (sourceCY <= 0)
		sourceCY = 1;

	int x, y;
	float scale;

	GetScaleAndCenterPos(sourceCX, sourceCY, s.width(), s.height(), x, y, scale);

	if (x > 0) {
		relX = int(float(mouseXscaled - x) / scale);
		relY = int(float(mouseYscaled) / scale);
	} else {
		relX = int(float(mouseXscaled) / scale);
		relY = int(float(mouseYscaled - y) / scale);
	}

	// Confirm mouse is inside the source
	if (relX < 0 || relX > int(sourceCX))
		return false;
	if (relY < 0 || relY > int(sourceCY))
		return false;

	return true;
}

bool BrowserDock::HandleMouseMoveEvent(QMouseEvent *event)
{
	if (!event)
		return false;
	if (!browser_source)
		return true;
	struct obs_mouse_event mouseEvent = {};

	bool mouseLeave = event->type() == QEvent::Leave;

	if (!mouseLeave) {
		mouseEvent.modifiers = TranslateQtMouseEventModifiers(event);
		mouseLeave = !GetSourceRelativeXY(event->pos().x(), event->pos().y(), mouseEvent.x, mouseEvent.y);
	}

	obs_source_send_mouse_move(browser_source, &mouseEvent, mouseLeave);
	return true;
}

bool BrowserDock::HandleMouseWheelEvent(QWheelEvent *event)
{
	if (!browser_source)
		return true;
	struct obs_mouse_event mouseEvent = {};

	mouseEvent.modifiers = TranslateQtKeyboardEventModifiers(event, true);

	int xDelta = 0;
	int yDelta = 0;

	const QPoint angleDelta = event->angleDelta();
	if (!event->pixelDelta().isNull()) {
		if (angleDelta.x())
			xDelta = event->pixelDelta().x();
		else
			yDelta = event->pixelDelta().y();
	} else {
		if (angleDelta.x())
			xDelta = angleDelta.x();
		else
			yDelta = angleDelta.y();
	}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	const QPointF position = event->position();
	const int x = position.x();
	const int y = position.y();
#else
	const int x = event->pos().x();
	const int y = event->pos().y();
#endif

	const bool insideSource = GetSourceRelativeXY(x, y, mouseEvent.x, mouseEvent.y);
	if (insideSource)
		obs_source_send_mouse_wheel(browser_source, &mouseEvent, xDelta, yDelta);

	return true;
}

bool BrowserDock::HandleFocusEvent(QFocusEvent *event)
{
	bool focus = event->type() == QEvent::FocusIn;

	if (browser_source)
		obs_source_send_focus(browser_source, focus);

	return true;
}

bool BrowserDock::HandleKeyEvent(QKeyEvent *event)
{
	if (!browser_source)
		return true;
	struct obs_key_event keyEvent;

	QByteArray text = event->text().toUtf8();
	keyEvent.modifiers = TranslateQtKeyboardEventModifiers(event, false);
	keyEvent.text = text.data();
	keyEvent.native_modifiers = event->nativeModifiers();
	keyEvent.native_scancode = event->nativeScanCode();
	keyEvent.native_vkey = event->nativeVirtualKey();

	bool keyUp = event->type() == QEvent::KeyRelease;

	obs_source_send_key_click(browser_source, &keyEvent, keyUp);

	return true;
}
