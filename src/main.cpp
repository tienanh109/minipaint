#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <ExDisp.h>     // Required for WebBrowser control
#include <mshtml.h>     // Required for WebBrowser control
#include <olectl.h>     // Required for WebBrowser control
#include <atlbase.h>    // Required for CComPtr
#include <atlhost.h>    // Required for AtlAxWin

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")

// --- Resource IDs ---
#define IDI_MYICON          101
#define IDD_WIDTH_DIALOG    102
#define IDR_HTML_ABOUT      103

// --- Menu Item IDs ---
#define IDM_FILE_NEW 1
#define IDM_FILE_SAVE 2
#define IDM_FILE_EXIT 3
#define IDM_EDIT_UNDO 4
#define IDM_TOOL_PEN 5
#define IDM_TOOL_ERASER 6
#define IDM_TOOL_LINE 7
#define IDM_TOOL_RECTANGLE 8
#define IDM_TOOL_ELLIPSE 9
#define IDM_OPTIONS_COLOR 10
#define IDM_OPTIONS_WIDTH 11
#define IDM_HELP_ABOUT 12
#define IDC_WIDTH_EDIT 101

// --- Application State ---
enum Tool { TOOL_PEN, TOOL_ERASER, TOOL_LINE, TOOL_RECTANGLE, TOOL_ELLIPSE };
struct AppState { Tool currentTool; COLORREF currentColor; int penWidth; bool isDrawing; POINT startPoint; POINT currentPoint; };

// --- Global Variables ---
AppState g_appState = { TOOL_PEN, RGB(0, 0, 0), 3, false, {0, 0}, {0, 0} };
HWND g_hwndStatusBar = NULL;
HBITMAP g_hbmBuffer = NULL, g_hbmUndoBuffer = NULL;
HDC g_hdcBuffer = NULL, g_hdcUndo = NULL;
int g_canvasWidth = 0, g_canvasHeight = 0;

// --- Function Prototypes ---
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK AboutWndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK PenWidthDlgProc(HWND, UINT, WPARAM, LPARAM);
void CreateMainMenu(HWND);
void ShowAboutWindow(HWND);
void UpdateStatusBar(POINT);
void CreateAndSizeBuffers(HWND, int, int);
void CleanupBuffers();
void ClearCanvas(HDC, int, int);
void SaveCanvasAsBitmap(HWND);
void SaveUndoState();
void RestoreUndoState();
void UpdateToolMenuCheck(HWND);

// --- WinMain ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    if (FAILED(CoInitialize(NULL))) {
        MessageBox(NULL, L"COM Initialization Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    if (!AtlAxWinInit()) {
        MessageBox(NULL, L"ATL Initialization Failed!", L"Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 0;
    }

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icex);

    const wchar_t MAIN_CLASS_NAME[] = L"EnhancedPaintWindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = MAIN_CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYICON));
    RegisterClass(&wc);

    const wchar_t ABOUT_CLASS_NAME[] = L"AboutWindowClass";
    WNDCLASS wcAbout = {};
    wcAbout.lpfnWndProc = AboutWndProc;
    wcAbout.hInstance = hInstance;
    wcAbout.lpszClassName = ABOUT_CLASS_NAME;
    wcAbout.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcAbout.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcAbout.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MYICON));
    RegisterClass(&wcAbout);

    HWND hwnd = CreateWindowEx(0, MAIN_CLASS_NAME, L"Mini Paint", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        CoUninitialize();
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    CoUninitialize();
    return (int)msg.wParam;
}

// --- Main Window Procedure ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            CreateMainMenu(hwnd);
            g_hwndStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL, SBARS_SIZEGRIP | WS_CHILD | WS_VISIBLE,
                0, 0, 0, 0, hwnd, NULL, GetModuleHandle(NULL), NULL);
            RECT rc;
            GetClientRect(hwnd, &rc);
            CreateAndSizeBuffers(hwnd, rc.right, rc.bottom);
            ClearCanvas(g_hdcBuffer, rc.right, rc.bottom);
            UpdateToolMenuCheck(hwnd);
            break;
        }

        case WM_SIZE: {
            SendMessage(g_hwndStatusBar, WM_SIZE, 0, 0);
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            if (width > 0 && height > 0) {
                 CreateAndSizeBuffers(hwnd, width, height);
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDM_FILE_NEW:
                    if (MessageBox(hwnd, L"Are you sure you want to start a new drawing?\nAll unsaved changes will be lost.", L"New Drawing", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
                        ClearCanvas(g_hdcBuffer, g_canvasWidth, g_canvasHeight);
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                    break;
                case IDM_FILE_SAVE: SaveCanvasAsBitmap(hwnd); break;
                case IDM_FILE_EXIT: DestroyWindow(hwnd); break;
                case IDM_EDIT_UNDO:
                    RestoreUndoState();
                    InvalidateRect(hwnd, NULL, FALSE);
                    break;
                case IDM_HELP_ABOUT:
                    ShowAboutWindow(hwnd);
                    break;
                case IDM_TOOL_PEN: g_appState.currentTool = TOOL_PEN; SetCursor(LoadCursor(NULL, IDC_CROSS)); break;
                case IDM_TOOL_ERASER: g_appState.currentTool = TOOL_ERASER; SetCursor(LoadCursor(NULL, IDC_ARROW)); break;
                case IDM_TOOL_LINE: g_appState.currentTool = TOOL_LINE; SetCursor(LoadCursor(NULL, IDC_CROSS)); break;
                case IDM_TOOL_RECTANGLE: g_appState.currentTool = TOOL_RECTANGLE; SetCursor(LoadCursor(NULL, IDC_CROSS)); break;
                case IDM_TOOL_ELLIPSE: g_appState.currentTool = TOOL_ELLIPSE; SetCursor(LoadCursor(NULL, IDC_CROSS)); break;
                case IDM_OPTIONS_COLOR: {
                    CHOOSECOLOR cc = {};
                    static COLORREF acrCustClr[16];
                    cc.lStructSize = sizeof(cc);
                    cc.hwndOwner = hwnd;
                    cc.lpCustColors = (LPDWORD)acrCustClr;
                    cc.rgbResult = g_appState.currentColor;
                    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                    if (ChooseColor(&cc)) {
                        g_appState.currentColor = cc.rgbResult;
                    }
                } break;
                case IDM_OPTIONS_WIDTH:
                    DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_WIDTH_DIALOG), hwnd, PenWidthDlgProc);
                    break;
            }
            UpdateToolMenuCheck(hwnd);
            break;
        }
        case WM_LBUTTONDOWN: {
            SaveUndoState(); g_appState.isDrawing = true; g_appState.startPoint.x = GET_X_LPARAM(lParam); g_appState.startPoint.y = GET_Y_LPARAM(lParam); g_appState.currentPoint = g_appState.startPoint; SetCapture(hwnd); if (g_appState.currentTool == TOOL_PEN || g_appState.currentTool == TOOL_ERASER) { MoveToEx(g_hdcBuffer, g_appState.startPoint.x, g_appState.startPoint.y, NULL); } break;
        }
        case WM_MOUSEMOVE: {
            POINT cursorPos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) }; UpdateStatusBar(cursorPos); if (g_appState.isDrawing) { g_appState.currentPoint = cursorPos; if (g_appState.currentTool == TOOL_PEN || g_appState.currentTool == TOOL_ERASER) { COLORREF penColor = (g_appState.currentTool == TOOL_ERASER) ? RGB(255, 255, 255) : g_appState.currentColor; int penWidth = (g_appState.currentTool == TOOL_ERASER) ? g_appState.penWidth * 2 : g_appState.penWidth; HPEN hPen = CreatePen(PS_SOLID, penWidth, penColor); HPEN hOldPen = (HPEN)SelectObject(g_hdcBuffer, hPen); LineTo(g_hdcBuffer, g_appState.currentPoint.x, g_appState.currentPoint.y); SelectObject(g_hdcBuffer, hOldPen); DeleteObject(hPen); } InvalidateRect(hwnd, NULL, FALSE); } break;
        }
        case WM_LBUTTONUP: {
            if (g_appState.isDrawing) { g_appState.isDrawing = false; ReleaseCapture(); HPEN hPen = CreatePen(PS_SOLID, g_appState.penWidth, g_appState.currentColor); HPEN hOldPen = (HPEN)SelectObject(g_hdcBuffer, hPen); HBRUSH hOldBrush = (HBRUSH)SelectObject(g_hdcBuffer, GetStockObject(NULL_BRUSH)); switch (g_appState.currentTool) { case TOOL_LINE: MoveToEx(g_hdcBuffer, g_appState.startPoint.x, g_appState.startPoint.y, NULL); LineTo(g_hdcBuffer, g_appState.currentPoint.x, g_appState.currentPoint.y); break; case TOOL_RECTANGLE: Rectangle(g_hdcBuffer, g_appState.startPoint.x, g_appState.startPoint.y, g_appState.currentPoint.x, g_appState.currentPoint.y); break; case TOOL_ELLIPSE: Ellipse(g_hdcBuffer, g_appState.startPoint.x, g_appState.startPoint.y, g_appState.currentPoint.x, g_appState.currentPoint.y); break; } SelectObject(g_hdcBuffer, hOldPen); SelectObject(g_hdcBuffer, hOldBrush); DeleteObject(hPen); InvalidateRect(hwnd, NULL, FALSE); } break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps); BitBlt(hdc, 0, 0, g_canvasWidth, g_canvasHeight, g_hdcBuffer, 0, 0, SRCCOPY); if (g_appState.isDrawing && (g_appState.currentTool >= TOOL_LINE)) { HPEN hPen = CreatePen(PS_DOT, 1, RGB(0,0,0)); HPEN hOldPen = (HPEN)SelectObject(hdc, hPen); HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH)); int oldRop = SetROP2(hdc, R2_NOTXORPEN); switch (g_appState.currentTool) { case TOOL_LINE: MoveToEx(hdc, g_appState.startPoint.x, g_appState.startPoint.y, NULL); LineTo(hdc, g_appState.currentPoint.x, g_appState.currentPoint.y); break; case TOOL_RECTANGLE: Rectangle(hdc, g_appState.startPoint.x, g_appState.startPoint.y, g_appState.currentPoint.x, g_appState.currentPoint.y); break; case TOOL_ELLIPSE: Ellipse(hdc, g_appState.startPoint.x, g_appState.startPoint.y, g_appState.currentPoint.x, g_appState.currentPoint.y); break; } SetROP2(hdc, oldRop); SelectObject(hdc, hOldPen); SelectObject(hdc, hOldBrush); DeleteObject(hPen); } EndPaint(hwnd, &ps); break;
        }
        case WM_DESTROY: CleanupBuffers(); PostQuitMessage(0); break;
        default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- About Window ---
void ShowAboutWindow(HWND owner) {
    const wchar_t ABOUT_CLASS_NAME[] = L"AboutWindowClass";
    HWND hwndAbout = CreateWindowEx(
        WS_EX_DLGMODALFRAME, ABOUT_CLASS_NAME, L"About Mini Paint",
        WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 420, 320,
        owner, NULL, GetModuleHandle(NULL), NULL);
    
    RECT rcOwner, rcDlg;
    GetWindowRect(owner, &rcOwner);
    GetWindowRect(hwndAbout, &rcDlg);
    SetWindowPos(hwndAbout, NULL, 
                 rcOwner.left + (rcOwner.right - rcOwner.left - (rcDlg.right - rcDlg.left)) / 2,
                 rcOwner.top + (rcOwner.bottom - rcOwner.top - (rcDlg.bottom - rcDlg.top)) / 2,
                 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    EnableWindow(owner, FALSE);
    SetForegroundWindow(hwndAbout);
    ShowWindow(hwndAbout, SW_SHOW);
}

LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // FINAL FIX: This version uses the standard "AtlAxWin" window class to host the
    // browser control, which is more robust. It stores the browser's HWND as window data.
    HWND hBrowserWnd = (HWND)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Build the resource URL dynamically.
            wchar_t modulePath[MAX_PATH];
            GetModuleFileNameW(NULL, modulePath, MAX_PATH);
            wchar_t resUrl[MAX_PATH + 100];
            wsprintfW(resUrl, L"res://%s/HTML/%d", modulePath, IDR_HTML_ABOUT);

            // Create the ActiveX host window using the "AtlAxWin" class.
            // The final parameter (the URL) is passed as the window name.
            // This is the standard, most reliable way to create the control.
            hBrowserWnd = CreateWindow(L"AtlAxWin", resUrl,
                WS_CHILD | WS_VISIBLE, 0, 0, rc.right, rc.bottom,
                hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);
            
            // Store the browser HWND for later use (e.g., resizing).
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)hBrowserWnd);
            
            // Optional: Suppress script errors
            CComPtr<IWebBrowser2> pBrowser;
            if (SUCCEEDED(AtlAxGetControl(hBrowserWnd, (IUnknown**)&pBrowser))) {
                pBrowser->put_Silent(VARIANT_TRUE);
            }
            break;
        }
        case WM_SIZE: {
            if (hBrowserWnd) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                MoveWindow(hBrowserWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
            }
            break;
        }
        case WM_CLOSE: {
            EnableWindow(GetParent(hwnd), TRUE);
            SetForegroundWindow(GetParent(hwnd));
            DestroyWindow(hwnd);
            break;
        }
        case WM_DESTROY: {
            // The child browser window (hBrowserWnd) is destroyed automatically with its parent.
            // COM objects are managed by the host. No manual cleanup needed here.
            break;
        }
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


// --- Helper Functions ---
void CreateMainMenu(HWND hwnd) {
    HMENU hMenubar = CreateMenu();
    HMENU hMenuFile = CreateMenu();
    HMENU hMenuEdit = CreateMenu();
    HMENU hMenuTool = CreateMenu();
    HMENU hMenuOptions = CreateMenu();
    HMENU hMenuHelp = CreateMenu();

    AppendMenuW(hMenuFile, MF_STRING, IDM_FILE_NEW, L"&New\tCtrl+N");
    AppendMenuW(hMenuFile, MF_STRING, IDM_FILE_SAVE, L"&Save\tCtrl+S");
    AppendMenuW(hMenuFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenuFile, MF_STRING, IDM_FILE_EXIT, L"&Exit\tAlt+F4");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenuFile, L"&File");

    AppendMenuW(hMenuEdit, MF_STRING, IDM_EDIT_UNDO, L"&Undo\tCtrl+Z");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenuEdit, L"&Edit");

    AppendMenuW(hMenuTool, MF_STRING, IDM_TOOL_PEN, L"&Pen");
    AppendMenuW(hMenuTool, MF_STRING, IDM_TOOL_ERASER, L"&Eraser");
    AppendMenuW(hMenuTool, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenuTool, MF_STRING, IDM_TOOL_LINE, L"&Line");
    AppendMenuW(hMenuTool, MF_STRING, IDM_TOOL_RECTANGLE, L"&Rectangle");
    AppendMenuW(hMenuTool, MF_STRING, IDM_TOOL_ELLIPSE, L"&Ellipse");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenuTool, L"&Tools");

    AppendMenuW(hMenuOptions, MF_STRING, IDM_OPTIONS_COLOR, L"&Color...");
    AppendMenuW(hMenuOptions, MF_STRING, IDM_OPTIONS_WIDTH, L"Pen &Width...");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenuOptions, L"&Options");

    AppendMenuW(hMenuHelp, MF_STRING, IDM_HELP_ABOUT, L"&About Mini Paint...");
    AppendMenuW(hMenubar, MF_POPUP, (UINT_PTR)hMenuHelp, L"&Help");

    SetMenu(hwnd, hMenubar);
}
void UpdateToolMenuCheck(HWND hwnd) { HMENU hMenubar = GetMenu(hwnd); HMENU hMenuTool = GetSubMenu(hMenubar, 2); CheckMenuItem(hMenuTool, IDM_TOOL_PEN, MF_BYCOMMAND | (g_appState.currentTool == TOOL_PEN ? MF_CHECKED : MF_UNCHECKED)); CheckMenuItem(hMenuTool, IDM_TOOL_ERASER, MF_BYCOMMAND | (g_appState.currentTool == TOOL_ERASER ? MF_CHECKED : MF_UNCHECKED)); CheckMenuItem(hMenuTool, IDM_TOOL_LINE, MF_BYCOMMAND | (g_appState.currentTool == TOOL_LINE ? MF_CHECKED : MF_UNCHECKED)); CheckMenuItem(hMenuTool, IDM_TOOL_RECTANGLE, MF_BYCOMMAND | (g_appState.currentTool == TOOL_RECTANGLE ? MF_CHECKED : MF_UNCHECKED)); CheckMenuItem(hMenuTool, IDM_TOOL_ELLIPSE, MF_BYCOMMAND | (g_appState.currentTool == TOOL_ELLIPSE ? MF_CHECKED : MF_UNCHECKED)); }
void UpdateStatusBar(POINT cursorPos) { wchar_t buf[100]; wsprintfW(buf, L"X: %d, Y: %d", cursorPos.x, cursorPos.y); SendMessage(g_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)buf); }
void CreateAndSizeBuffers(HWND hwnd, int width, int height) { HDC hdcScreen = GetDC(hwnd); HDC hdcTemp = CreateCompatibleDC(hdcScreen); HBITMAP hbmTemp = CreateCompatibleBitmap(hdcScreen, g_canvasWidth, g_canvasHeight); SelectObject(hdcTemp, hbmTemp); if (g_hdcBuffer) { BitBlt(hdcTemp, 0, 0, g_canvasWidth, g_canvasHeight, g_hdcBuffer, 0, 0, SRCCOPY); } CleanupBuffers(); g_canvasWidth = width; g_canvasHeight = height; g_hdcBuffer = CreateCompatibleDC(hdcScreen); g_hbmBuffer = CreateCompatibleBitmap(hdcScreen, width, height); SelectObject(g_hdcBuffer, g_hbmBuffer); g_hdcUndo = CreateCompatibleDC(hdcScreen); g_hbmUndoBuffer = CreateCompatibleBitmap(hdcScreen, width, height); SelectObject(g_hdcUndo, g_hbmUndoBuffer); ClearCanvas(g_hdcBuffer, width, height); BitBlt(g_hdcBuffer, 0, 0, g_canvasWidth, g_canvasHeight, hdcTemp, 0, 0, SRCCOPY); DeleteDC(hdcTemp); DeleteObject(hbmTemp); ReleaseDC(hwnd, hdcScreen); }
void CleanupBuffers() { if(g_hdcBuffer) DeleteDC(g_hdcBuffer); if(g_hbmBuffer) DeleteObject(g_hbmBuffer); if(g_hdcUndo) DeleteDC(g_hdcUndo); if(g_hbmUndoBuffer) DeleteObject(g_hbmUndoBuffer); }
void ClearCanvas(HDC hdc, int width, int height) { RECT canvasRect = { 0, 0, width, height }; FillRect(hdc, &canvasRect, (HBRUSH)(COLOR_WINDOW + 1)); }
void SaveUndoState() { BitBlt(g_hdcUndo, 0, 0, g_canvasWidth, g_canvasHeight, g_hdcBuffer, 0, 0, SRCCOPY); }
void RestoreUndoState() { BitBlt(g_hdcBuffer, 0, 0, g_canvasWidth, g_canvasHeight, g_hdcUndo, 0, 0, SRCCOPY); }
void SaveCanvasAsBitmap(HWND hwnd) { OPENFILENAMEW ofn = {}; wchar_t szFile[260] = L"untitled.bmp"; ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd; ofn.lpstrFile = szFile; ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t); ofn.lpstrFilter = L"Bitmap (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0"; ofn.nFilterIndex = 1; ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT; if (GetSaveFileNameW(&ofn)) { BITMAPFILEHEADER bmfh = {}; BITMAPINFO bi = {}; bi.bmiHeader.biSize = sizeof(bi.bmiHeader); bi.bmiHeader.biWidth = g_canvasWidth; bi.bmiHeader.biHeight = -g_canvasHeight; bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB; DWORD dwBmpSize = g_canvasWidth * g_canvasHeight * 4; void* pBits = malloc(dwBmpSize); GetDIBits(g_hdcBuffer, g_hbmBuffer, 0, g_canvasHeight, pBits, &bi, DIB_RGB_COLORS); HANDLE hFile = CreateFileW(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); if (hFile != INVALID_HANDLE_VALUE) { DWORD dwWritten; bmfh.bfType = 0x4D42; bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER); bmfh.bfSize = bmfh.bfOffBits + dwBmpSize; WriteFile(hFile, &bmfh, sizeof(bmfh), &dwWritten, NULL); WriteFile(hFile, &bi.bmiHeader, sizeof(bi.bmiHeader), &dwWritten, NULL); WriteFile(hFile, pBits, dwBmpSize, &dwWritten, NULL); CloseHandle(hFile); } free(pBits); MessageBoxW(hwnd, L"File saved successfully!", L"Save File", MB_OK | MB_ICONINFORMATION); } }
INT_PTR CALLBACK PenWidthDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) { switch (message) { case WM_INITDIALOG: SetDlgItemInt(hDlg, IDC_WIDTH_EDIT, g_appState.penWidth, FALSE); return (INT_PTR)TRUE; case WM_COMMAND: if (LOWORD(wParam) == IDOK) { BOOL bSuccess; int newWidth = GetDlgItemInt(hDlg, IDC_WIDTH_EDIT, &bSuccess, FALSE); if (bSuccess && newWidth > 0 && newWidth < 100) { g_appState.penWidth = newWidth; } EndDialog(hDlg, LOWORD(wParam)); return (INT_PTR)TRUE; } if (LOWORD(wParam) == IDCANCEL) { EndDialog(hDlg, LOWORD(wParam)); return (INT_PTR)TRUE; } break; } return (INT_PTR)FALSE; }
