import re
import sys

USED_METHODS = {
    "ICoreWebView2Environment": {
        "CreateCoreWebView2Controller": "HWND parent_window, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *handler",
    },
    "ICoreWebView2Controller": {
        "SetIsVisible": "BOOL is_visible",
        "SetBounds": "RECT bounds",
        "MoveFocus": "INT32 reason",
        "Close": "",
        "CoreWebView2": "ICoreWebView2 **core_webview",
    },
    "ICoreWebView2": {
        "Settings": "ICoreWebView2Settings **settings",
        "NavigateToString": "LPCWSTR html_content",
        "PostWebMessageAsJson": "LPCWSTR web_message_as_json",
        "add_WebMessageReceived": "ICoreWebView2WebMessageReceivedEventHandler *handler, INT64 *token",
    },
    "ICoreWebView2Settings": {
        "SetAreDevToolsEnabled": "BOOL enabled",
        "SetIsStatusBarEnabled": "BOOL enabled",
        "SetIsZoomControlEnabled": "BOOL enabled",
        "SetAreDefaultScriptDialogsEnabled": "BOOL enabled",
    },
    "ICoreWebView2WebMessageReceivedEventArgs": {
        "TryGetWebMessageAsString": "LPWSTR *message",
    },
    "ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler": {
        "Invoke": "HRESULT error_code, ICoreWebView2Environment *environment",
    },
    "ICoreWebView2CreateCoreWebView2ControllerCompletedHandler": {
        "Invoke": "HRESULT error_code, ICoreWebView2Controller *controller",
    },
    "ICoreWebView2WebMessageReceivedEventHandler": {
        "Invoke": "ICoreWebView2 *sender, ICoreWebView2WebMessageReceivedEventArgs *args",
    },
}

UNKNOWN_METHODS = [
    ("QueryInterface", "REFIID riid, void **object"),
    ("AddRef", None),
    ("Release", None),
]


def parse_vtables(source):
    vtables = {}
    for match in re.finditer(r"pub struct (\w+)_Vtbl \{(.*?)\n\}", source, re.S):
        name, body = match.group(1), match.group(2)
        fields = re.findall(r"\n    pub (\w+):", body)
        vtables[name] = fields
    return vtables


def parse_iids(source):
    iids = {}
    pattern = r"define_interface!\(\s*(\w+),\s*\w+_Vtbl,\s*0x([0-9a-f]{8})_([0-9a-f]{4})_([0-9a-f]{4})_([0-9a-f]{4})_([0-9a-f]{12})\s*\)"
    for match in re.finditer(pattern, source):
        iids[match.group(1)] = match.groups()[1:]
    return iids


def guid_initializer(parts):
    data1, data2, data3, data4a, data4b = parts
    data4 = data4a + data4b
    data4_bytes = ", ".join("0x" + data4[index:index + 2] for index in range(0, 16, 2))
    return "{ 0x%s, 0x%s, 0x%s, { %s } }" % (data1, data2, data3, data4_bytes)


def emit(vtables, iids, sdk_version):
    lines = []
    lines.append("#ifndef SIHUB_WEBVIEW2_C_H")
    lines.append("#define SIHUB_WEBVIEW2_C_H")
    lines.append("")
    lines.append("#include <windows.h>")
    lines.append("")
    lines.append("#define WEBVIEW2_SDK_VERSION \"%s\"" % sdk_version)
    lines.append("#define WEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC 0")
    lines.append("")
    for name in USED_METHODS:
        lines.append("typedef struct %s %s;" % (name, name))
    lines.append("")
    for name, used in USED_METHODS.items():
        fields = vtables[name]
        if fields[0] != "base__":
            raise SystemExit("unexpected base for " + name)
        unknown_fields = [field for field in fields[1:]]
        for method in used:
            if method not in unknown_fields:
                raise SystemExit("method %s not in %s" % (method, name))
        lines.append("struct %s_vtbl" % name)
        lines.append("{")
        for method, params in UNKNOWN_METHODS:
            extra = ", " + params if params else ""
            return_type = "HRESULT" if method == "QueryInterface" else "ULONG"
            lines.append("\t%s (STDMETHODCALLTYPE *%s)(%s *self%s);" % (return_type, method, name, extra))
        for method in unknown_fields:
            if method in used:
                params = used[method]
                extra = ", " + params if params else ""
                lines.append("\tHRESULT (STDMETHODCALLTYPE *%s)(%s *self%s);" % (method, name, extra))
            else:
                lines.append("\tvoid *%s;" % method)
        lines.append("};")
        lines.append("")
        lines.append("struct %s" % name)
        lines.append("{")
        lines.append("\tconst struct %s_vtbl *lpVtbl;" % name)
        lines.append("};")
        lines.append("")
    for name in USED_METHODS:
        lines.append("static const GUID IID_%s = %s;" % (name, guid_initializer(iids[name])))
    lines.append("")
    lines.append("typedef HRESULT (STDAPICALLTYPE *CreateCoreWebView2EnvironmentWithOptionsFunc)(LPCWSTR browser_executable_folder, LPCWSTR user_data_folder, void *environment_options, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *handler);")
    lines.append("")
    lines.append("#endif")
    return "\n".join(lines) + "\n"


def main():
    bindings_path, sdk_version, output_path = sys.argv[1], sys.argv[2], sys.argv[3]
    with open(bindings_path, encoding="utf-8") as bindings_file:
        source = bindings_file.read()
    header = emit(parse_vtables(source), parse_iids(source), sdk_version)
    with open(output_path, "w", encoding="ascii", newline="\n") as output_file:
        output_file.write(header)


main()
