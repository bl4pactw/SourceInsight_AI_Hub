#ifndef SIHUB_WEBVIEW2_C_H
#define SIHUB_WEBVIEW2_C_H

#include <windows.h>

#define WEBVIEW2_SDK_VERSION "1.0.3650.58"
#define WEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC 0

typedef struct ICoreWebView2Environment ICoreWebView2Environment;
typedef struct ICoreWebView2Controller ICoreWebView2Controller;
typedef struct ICoreWebView2 ICoreWebView2;
typedef struct ICoreWebView2Settings ICoreWebView2Settings;
typedef struct ICoreWebView2WebMessageReceivedEventArgs ICoreWebView2WebMessageReceivedEventArgs;
typedef struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler;
typedef struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler ICoreWebView2CreateCoreWebView2ControllerCompletedHandler;
typedef struct ICoreWebView2WebMessageReceivedEventHandler ICoreWebView2WebMessageReceivedEventHandler;

struct ICoreWebView2Environment_vtbl
{
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2Environment *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2Environment *self);
	ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2Environment *self);
	HRESULT (STDMETHODCALLTYPE *CreateCoreWebView2Controller)(ICoreWebView2Environment *self, HWND parent_window, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *handler);
	void *CreateWebResourceResponse;
	void *BrowserVersionString;
	void *add_NewBrowserVersionAvailable;
	void *remove_NewBrowserVersionAvailable;
};

struct ICoreWebView2Environment
{
	const struct ICoreWebView2Environment_vtbl *lpVtbl;
};

struct ICoreWebView2Controller_vtbl
{
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2Controller *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2Controller *self);
	ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2Controller *self);
	void *IsVisible;
	HRESULT (STDMETHODCALLTYPE *SetIsVisible)(ICoreWebView2Controller *self, BOOL is_visible);
	void *Bounds;
	HRESULT (STDMETHODCALLTYPE *SetBounds)(ICoreWebView2Controller *self, RECT bounds);
	void *ZoomFactor;
	void *SetZoomFactor;
	void *add_ZoomFactorChanged;
	void *remove_ZoomFactorChanged;
	void *SetBoundsAndZoomFactor;
	HRESULT (STDMETHODCALLTYPE *MoveFocus)(ICoreWebView2Controller *self, INT32 reason);
	void *add_MoveFocusRequested;
	void *remove_MoveFocusRequested;
	void *add_GotFocus;
	void *remove_GotFocus;
	void *add_LostFocus;
	void *remove_LostFocus;
	void *add_AcceleratorKeyPressed;
	void *remove_AcceleratorKeyPressed;
	void *ParentWindow;
	void *SetParentWindow;
	void *NotifyParentWindowPositionChanged;
	HRESULT (STDMETHODCALLTYPE *Close)(ICoreWebView2Controller *self);
	HRESULT (STDMETHODCALLTYPE *CoreWebView2)(ICoreWebView2Controller *self, ICoreWebView2 **core_webview);
};

struct ICoreWebView2Controller
{
	const struct ICoreWebView2Controller_vtbl *lpVtbl;
};

struct ICoreWebView2_vtbl
{
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2 *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2 *self);
	ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2 *self);
	HRESULT (STDMETHODCALLTYPE *Settings)(ICoreWebView2 *self, ICoreWebView2Settings **settings);
	void *Source;
	void *Navigate;
	HRESULT (STDMETHODCALLTYPE *NavigateToString)(ICoreWebView2 *self, LPCWSTR html_content);
	void *add_NavigationStarting;
	void *remove_NavigationStarting;
	void *add_ContentLoading;
	void *remove_ContentLoading;
	void *add_SourceChanged;
	void *remove_SourceChanged;
	void *add_HistoryChanged;
	void *remove_HistoryChanged;
	void *add_NavigationCompleted;
	void *remove_NavigationCompleted;
	void *add_FrameNavigationStarting;
	void *remove_FrameNavigationStarting;
	void *add_FrameNavigationCompleted;
	void *remove_FrameNavigationCompleted;
	void *add_ScriptDialogOpening;
	void *remove_ScriptDialogOpening;
	void *add_PermissionRequested;
	void *remove_PermissionRequested;
	void *add_ProcessFailed;
	void *remove_ProcessFailed;
	void *AddScriptToExecuteOnDocumentCreated;
	void *RemoveScriptToExecuteOnDocumentCreated;
	void *ExecuteScript;
	void *CapturePreview;
	void *Reload;
	HRESULT (STDMETHODCALLTYPE *PostWebMessageAsJson)(ICoreWebView2 *self, LPCWSTR web_message_as_json);
	void *PostWebMessageAsString;
	HRESULT (STDMETHODCALLTYPE *add_WebMessageReceived)(ICoreWebView2 *self, ICoreWebView2WebMessageReceivedEventHandler *handler, INT64 *token);
	void *remove_WebMessageReceived;
	void *CallDevToolsProtocolMethod;
	void *BrowserProcessId;
	void *CanGoBack;
	void *CanGoForward;
	void *GoBack;
	void *GoForward;
	void *GetDevToolsProtocolEventReceiver;
	void *Stop;
	void *add_NewWindowRequested;
	void *remove_NewWindowRequested;
	void *add_DocumentTitleChanged;
	void *remove_DocumentTitleChanged;
	void *DocumentTitle;
	void *AddHostObjectToScript;
	void *RemoveHostObjectFromScript;
	void *OpenDevToolsWindow;
	void *add_ContainsFullScreenElementChanged;
	void *remove_ContainsFullScreenElementChanged;
	void *ContainsFullScreenElement;
	void *add_WebResourceRequested;
	void *remove_WebResourceRequested;
	void *AddWebResourceRequestedFilter;
	void *RemoveWebResourceRequestedFilter;
	void *add_WindowCloseRequested;
	void *remove_WindowCloseRequested;
};

struct ICoreWebView2
{
	const struct ICoreWebView2_vtbl *lpVtbl;
};

struct ICoreWebView2Settings_vtbl
{
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2Settings *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2Settings *self);
	ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2Settings *self);
	void *IsScriptEnabled;
	void *SetIsScriptEnabled;
	void *IsWebMessageEnabled;
	void *SetIsWebMessageEnabled;
	void *AreDefaultScriptDialogsEnabled;
	HRESULT (STDMETHODCALLTYPE *SetAreDefaultScriptDialogsEnabled)(ICoreWebView2Settings *self, BOOL enabled);
	void *IsStatusBarEnabled;
	HRESULT (STDMETHODCALLTYPE *SetIsStatusBarEnabled)(ICoreWebView2Settings *self, BOOL enabled);
	void *AreDevToolsEnabled;
	HRESULT (STDMETHODCALLTYPE *SetAreDevToolsEnabled)(ICoreWebView2Settings *self, BOOL enabled);
	void *AreDefaultContextMenusEnabled;
	void *SetAreDefaultContextMenusEnabled;
	void *AreHostObjectsAllowed;
	void *SetAreHostObjectsAllowed;
	void *IsZoomControlEnabled;
	HRESULT (STDMETHODCALLTYPE *SetIsZoomControlEnabled)(ICoreWebView2Settings *self, BOOL enabled);
	void *IsBuiltInErrorPageEnabled;
	void *SetIsBuiltInErrorPageEnabled;
};

struct ICoreWebView2Settings
{
	const struct ICoreWebView2Settings_vtbl *lpVtbl;
};

struct ICoreWebView2WebMessageReceivedEventArgs_vtbl
{
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2WebMessageReceivedEventArgs *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2WebMessageReceivedEventArgs *self);
	ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2WebMessageReceivedEventArgs *self);
	void *Source;
	void *WebMessageAsJson;
	HRESULT (STDMETHODCALLTYPE *TryGetWebMessageAsString)(ICoreWebView2WebMessageReceivedEventArgs *self, LPWSTR *message);
};

struct ICoreWebView2WebMessageReceivedEventArgs
{
	const struct ICoreWebView2WebMessageReceivedEventArgs_vtbl *lpVtbl;
};

struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_vtbl
{
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self);
	ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self);
	HRESULT (STDMETHODCALLTYPE *Invoke)(ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *self, HRESULT error_code, ICoreWebView2Environment *environment);
};

struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler
{
	const struct ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler_vtbl *lpVtbl;
};

struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_vtbl
{
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self);
	ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self);
	HRESULT (STDMETHODCALLTYPE *Invoke)(ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *self, HRESULT error_code, ICoreWebView2Controller *controller);
};

struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler
{
	const struct ICoreWebView2CreateCoreWebView2ControllerCompletedHandler_vtbl *lpVtbl;
};

struct ICoreWebView2WebMessageReceivedEventHandler_vtbl
{
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ICoreWebView2WebMessageReceivedEventHandler *self, REFIID riid, void **object);
	ULONG (STDMETHODCALLTYPE *AddRef)(ICoreWebView2WebMessageReceivedEventHandler *self);
	ULONG (STDMETHODCALLTYPE *Release)(ICoreWebView2WebMessageReceivedEventHandler *self);
	HRESULT (STDMETHODCALLTYPE *Invoke)(ICoreWebView2WebMessageReceivedEventHandler *self, ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args);
};

struct ICoreWebView2WebMessageReceivedEventHandler
{
	const struct ICoreWebView2WebMessageReceivedEventHandler_vtbl *lpVtbl;
};

static const GUID IID_ICoreWebView2Environment = { 0xb96d755e, 0x0319, 0x4e92, { 0xa2, 0x96, 0x23, 0x43, 0x6f, 0x46, 0xa1, 0xfc } };
static const GUID IID_ICoreWebView2Controller = { 0x4d00c0d1, 0x9434, 0x4eb6, { 0x80, 0x78, 0x86, 0x97, 0xa5, 0x60, 0x33, 0x4f } };
static const GUID IID_ICoreWebView2 = { 0x76eceacb, 0x0462, 0x4d94, { 0xac, 0x83, 0x42, 0x3a, 0x67, 0x93, 0x77, 0x5e } };
static const GUID IID_ICoreWebView2Settings = { 0xe562e4f0, 0xd7fa, 0x43ac, { 0x8d, 0x71, 0xc0, 0x51, 0x50, 0x49, 0x9f, 0x00 } };
static const GUID IID_ICoreWebView2WebMessageReceivedEventArgs = { 0x0f99a40c, 0xe962, 0x4207, { 0x9e, 0x92, 0xe3, 0xd5, 0x42, 0xef, 0xf8, 0x49 } };
static const GUID IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler = { 0x4e8a3389, 0xc9d8, 0x4bd2, { 0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d } };
static const GUID IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler = { 0x6c4819f3, 0xc9b7, 0x4260, { 0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c } };
static const GUID IID_ICoreWebView2WebMessageReceivedEventHandler = { 0x57213f19, 0x00e6, 0x49fa, { 0x8e, 0x07, 0x89, 0x8e, 0xa0, 0x1e, 0xcb, 0xd2 } };

typedef HRESULT (STDAPICALLTYPE *CreateCoreWebView2EnvironmentWithOptionsFunc)(LPCWSTR browser_executable_folder, LPCWSTR user_data_folder, void *environment_options, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *handler);

#endif
