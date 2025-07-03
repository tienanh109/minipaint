#define UNICODE
#define _UNICODE

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shellapi.h> // Required for ShellExecuteW to open URLs

#pragma comment(lib, "comctl32.lib") // Required for SysLink control
#pragma comment(lib, "shell32.lib")  // Required for ShellExecuteW

// This pragma ensures the application uses version 6 of the Common Controls library,
// which is required for modern controls like the SysLink control (WC_LINK).
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")


// --- Resource IDs ---
#define IDI_MYICON          101
#define IDD_WIDTH_DIALOG    102
#define IDB_ABOUT_BANNER    104 // New ID for the banner bitmap

// --- Control IDs for About Box ---
#define IDC_LINK_GITHUB     1001
// IDC_BANNER_STATIC is no longer needed as we will draw the banner manually

// --- Menu Item IDs ---
#define IDM_FILE_NEW        1
#define IDM_FILE_SAVE       2
#define IDM_FILE_EXIT       3
#define IDM_EDIT_UNDO       4
#define IDM_TOOL_PEN        5
#define IDM_TOOL_ERASER     6
#define IDM_TOOL_LINE       7
#define IDM_TOOL_RECTANGLE  8
#define IDM_TOOL_ELLIPSE    9
#define IDM_OPTIONS_COLOR   10
#define IDM_OPTIONS_WIDTH   11
#define IDM_HELP_ABOUT      12
#define IDC_WIDTH_EDIT      101

// --- Application State ---
enum Tool { TOOL_PEN, TOOL_ERASER, TOOL_LINE, TOOL_RECTANGLE, TOOL_ELLIPSE };
struct AppState {
    Tool currentTool;
    COLORREF currentColor;
    int penWidth;
    bool isDrawing;
    POINT startPoint;
    POINT currentPoint;
};

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
    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES | ICC_LINK_CLASS };
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
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

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
            SaveUndoState();
            g_appState.isDrawing = true;
            g_appState.startPoint.x = GET_X_LPARAM(lParam);
            g_appState.startPoint.y = GET_Y_LPARAM(lParam);
            g_appState.currentPoint = g_appState.startPoint;
            SetCapture(hwnd);
            if (g_appState.currentTool == TOOL_PEN || g_appState.currentTool == TOOL_ERASER) {
                MoveToEx(g_hdcBuffer, g_appState.startPoint.x, g_appState.startPoint.y, NULL);
            }
            break;
        }
        case WM_MOUSEMOVE: {
            POINT cursorPos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            UpdateStatusBar(cursorPos);
            if (g_appState.isDrawing) {
                g_appState.currentPoint = cursorPos;
                if (g_appState.currentTool == TOOL_PEN || g_appState.currentTool == TOOL_ERASER) {
                    COLORREF penColor = (g_appState.currentTool == TOOL_ERASER) ? RGB(255, 255, 255) : g_appState.currentColor;
                    int penWidth = (g_appState.currentTool == TOOL_ERASER) ? g_appState.penWidth * 2 : g_appState.penWidth;
                    HPEN hPen = CreatePen(PS_SOLID, penWidth, penColor);
                    HPEN hOldPen = (HPEN)SelectObject(g_hdcBuffer, hPen);
                    LineTo(g_hdcBuffer, g_appState.currentPoint.x, g_appState.currentPoint.y);
                    SelectObject(g_hdcBuffer, hOldPen);
                    DeleteObject(hPen);
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }
        case WM_LBUTTONUP: {
            if (g_appState.isDrawing) {
                g_appState.isDrawing = false;
                ReleaseCapture();
                HPEN hPen = CreatePen(PS_SOLID, g_appState.penWidth, g_appState.currentColor);
                HPEN hOldPen = (HPEN)SelectObject(g_hdcBuffer, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(g_hdcBuffer, GetStockObject(NULL_BRUSH));
                switch (g_appState.currentTool) {
                    case TOOL_LINE:
                        MoveToEx(g_hdcBuffer, g_appState.startPoint.x, g_appState.startPoint.y, NULL);
                        LineTo(g_hdcBuffer, g_appState.currentPoint.x, g_appState.currentPoint.y);
                        break;
                    case TOOL_RECTANGLE:
                        Rectangle(g_hdcBuffer, g_appState.startPoint.x, g_appState.startPoint.y, g_appState.currentPoint.x, g_appState.currentPoint.y);
                        break;
                    case TOOL_ELLIPSE:
                        Ellipse(g_hdcBuffer, g_appState.startPoint.x, g_appState.startPoint.y, g_appState.currentPoint.x, g_appState.currentPoint.y);
                        break;
                }
                SelectObject(g_hdcBuffer, hOldPen);
                SelectObject(g_hdcBuffer, hOldBrush);
                DeleteObject(hPen);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            BitBlt(hdc, 0, 0, g_canvasWidth, g_canvasHeight, g_hdcBuffer, 0, 0, SRCCOPY);
            if (g_appState.isDrawing && (g_appState.currentTool >= TOOL_LINE)) {
                HPEN hPen = CreatePen(PS_DOT, 1, RGB(0,0,0));
                HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
                HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
                int oldRop = SetROP2(hdc, R2_NOTXORPEN);
                switch (g_appState.currentTool) {
                    case TOOL_LINE:
                        MoveToEx(hdc, g_appState.startPoint.x, g_appState.startPoint.y, NULL);
                        LineTo(hdc, g_appState.currentPoint.x, g_appState.currentPoint.y);
                        break;
                    case TOOL_RECTANGLE:
                        Rectangle(hdc, g_appState.startPoint.x, g_appState.startPoint.y, g_appState.currentPoint.x, g_appState.currentPoint.y);
                        break;
                    case TOOL_ELLIPSE:
                        Ellipse(hdc, g_appState.startPoint.x, g_appState.startPoint.y, g_appState.currentPoint.x, g_appState.currentPoint.y);
                        break;
                }
                SetROP2(hdc, oldRop);
                SelectObject(hdc, hOldPen);
                SelectObject(hdc, hOldBrush);
                DeleteObject(hPen);
            }
            EndPaint(hwnd, &ps);
            break;
        }
        case WM_DESTROY:
            CleanupBuffers();
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- About Window ---
void ShowAboutWindow(HWND owner) {
    const wchar_t ABOUT_CLASS_NAME[] = L"AboutWindowClass";
    // Adjusted window size for better layout
    HWND hwndAbout = CreateWindowEx(
        WS_EX_DLGMODALFRAME, ABOUT_CLASS_NAME, L"About Mini Paint",
        WS_CAPTION | WS_SYSMENU, 
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 420, // Adjusted width and height
        owner, NULL, GetModuleHandle(NULL), (LPVOID)owner); 

    if (hwndAbout) {
        EnableWindow(owner, FALSE);
        ShowWindow(hwndAbout, SW_SHOW);
        SetForegroundWindow(hwndAbout);
    }
}

// Rewritten About Window Procedure to manually draw the banner for proper scaling
LRESULT CALLBACK AboutWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HBITMAP hBannerBitmap = NULL;
    static HFONT hTitleFont = NULL;
    static HFONT hTextFont = NULL;
    static HWND hOwner = NULL;

    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
            hOwner = (HWND)pCreate->lpCreateParams;

            hBannerBitmap = (HBITMAP)LoadImageW(GetModuleHandle(NULL), MAKEINTRESOURCEW(IDB_ABOUT_BANNER), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
            if (hBannerBitmap == NULL) {
                MessageBox(hOwner, L"Could not load banner resource (IDB_ABOUT_BANNER)!", L"Resource Error", MB_OK | MB_ICONERROR);
                return -1; 
            }

            LOGFONT lf = {};
            GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONT), &lf);
            
            lf.lfHeight = 24;
            lf.lfWeight = FW_BOLD;
            hTitleFont = CreateFontIndirect(&lf);

            lf.lfHeight = 16;
            lf.lfWeight = FW_NORMAL;
            hTextFont = CreateFontIndirect(&lf);

            // Define a fixed height for the banner area for layout calculations
            int bannerHeight = 120;
            int yPos = bannerHeight + 15; // Start controls below the banner area

            HWND hCtrl;

            hCtrl = CreateWindowW(L"STATIC", L"Mini Paint", WS_CHILD | WS_VISIBLE | SS_CENTER, 10, yPos, 340, 25, hwnd, NULL, NULL, NULL);
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)hTitleFont, TRUE);

            yPos += 40;
            hCtrl = CreateWindowW(L"STATIC", L"Version 1.0", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, yPos, 200, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)hTextFont, TRUE);

            yPos += 25;
            hCtrl = CreateWindowW(WC_LINK, L"Developer: <A HREF=\"https://github.com/tienanh109\">tienanh109</A>", WS_CHILD | WS_VISIBLE, 20, yPos, 340, 20, hwnd, (HMENU)IDC_LINK_GITHUB, NULL, NULL);
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)hTextFont, TRUE);

            yPos += 25;
            hCtrl = CreateWindowW(L"STATIC", L"Source Code: Win32 API", WS_CHILD | WS_VISIBLE | SS_LEFT, 20, yPos, 300, 20, hwnd, NULL, NULL, NULL);
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)hTextFont, TRUE);

            yPos += 50;
            hCtrl = CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 135, yPos, 100, 30, hwnd, (HMENU)IDOK, NULL, NULL);
            SendMessage(hCtrl, WM_SETFONT, (WPARAM)hTextFont, TRUE);

            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            if (hBannerBitmap) {
                RECT rcClient;
                GetClientRect(hwnd, &rcClient);

                // Define a fixed height for the banner area
                int bannerHeight = 120;

                HDC hdcMem = CreateCompatibleDC(hdc);
                HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hBannerBitmap);
                BITMAP bm;
                GetObject(hBannerBitmap, sizeof(bm), &bm);

                // Draw the banner, stretching it to fit the full width of the window
                // This will scale the image to fit the 360x120 area.
                StretchBlt(hdc, 0, 0, rcClient.right, bannerHeight, hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

                SelectObject(hdcMem, hbmOld);
                DeleteDC(hdcMem);
            }

            EndPaint(hwnd, &ps);
            break;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                DestroyWindow(hwnd);
            }
            break;
        case WM_NOTIFY: {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->code == NM_CLICK && pnmh->idFrom == IDC_LINK_GITHUB) {
                PNMLINK pNMLink = (PNMLINK)pnmh;
                ShellExecuteW(NULL, L"open", pNMLink->item.szUrl, NULL, NULL, SW_SHOWNORMAL);
            }
            break;
        }
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetStockObject(NULL_BRUSH);
        }
        case WM_CLOSE: {
            DestroyWindow(hwnd);
            break;
        }
        case WM_DESTROY: {
            if (hOwner) {
                EnableWindow(hOwner, TRUE);
                SetForegroundWindow(hOwner);
            }
            
            if (hBannerBitmap) DeleteObject(hBannerBitmap);
            if (hTitleFont) DeleteObject(hTitleFont);
            if (hTextFont) DeleteObject(hTextFont);
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

void UpdateToolMenuCheck(HWND hwnd) {
    HMENU hMenubar = GetMenu(hwnd);
    HMENU hMenuTool = GetSubMenu(hMenubar, 2);
    CheckMenuItem(hMenuTool, IDM_TOOL_PEN, MF_BYCOMMAND | (g_appState.currentTool == TOOL_PEN ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenuTool, IDM_TOOL_ERASER, MF_BYCOMMAND | (g_appState.currentTool == TOOL_ERASER ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenuTool, IDM_TOOL_LINE, MF_BYCOMMAND | (g_appState.currentTool == TOOL_LINE ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenuTool, IDM_TOOL_RECTANGLE, MF_BYCOMMAND | (g_appState.currentTool == TOOL_RECTANGLE ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenuTool, IDM_TOOL_ELLIPSE, MF_BYCOMMAND | (g_appState.currentTool == TOOL_ELLIPSE ? MF_CHECKED : MF_UNCHECKED));
}

void UpdateStatusBar(POINT cursorPos) {
    wchar_t buf[100];
    wsprintfW(buf, L"X: %d, Y: %d", cursorPos.x, cursorPos.y);
    SendMessage(g_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)buf);
}

void CreateAndSizeBuffers(HWND hwnd, int width, int height) {
    HDC hdcScreen = GetDC(hwnd);
    HDC hdcTemp = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmTemp = CreateCompatibleBitmap(hdcScreen, g_canvasWidth, g_canvasHeight);
    SelectObject(hdcTemp, hbmTemp);
    if (g_hdcBuffer) {
        BitBlt(hdcTemp, 0, 0, g_canvasWidth, g_canvasHeight, g_hdcBuffer, 0, 0, SRCCOPY);
    }
    CleanupBuffers();
    g_canvasWidth = width;
    g_canvasHeight = height;
    g_hdcBuffer = CreateCompatibleDC(hdcScreen);
    g_hbmBuffer = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(g_hdcBuffer, g_hbmBuffer);
    g_hdcUndo = CreateCompatibleDC(hdcScreen);
    g_hbmUndoBuffer = CreateCompatibleBitmap(hdcScreen, width, height);
    SelectObject(g_hdcUndo, g_hbmUndoBuffer);
    ClearCanvas(g_hdcBuffer, width, height);
    BitBlt(g_hdcBuffer, 0, 0, g_canvasWidth, g_canvasHeight, hdcTemp, 0, 0, SRCCOPY);
    DeleteDC(hdcTemp);
    DeleteObject(hbmTemp);
    ReleaseDC(hwnd, hdcScreen);
}

void CleanupBuffers() {
    if(g_hdcBuffer) DeleteDC(g_hdcBuffer);
    if(g_hbmBuffer) DeleteObject(g_hbmBuffer);
    if(g_hdcUndo) DeleteDC(g_hdcUndo);
    if(g_hbmUndoBuffer) DeleteObject(g_hbmUndoBuffer);
}

void ClearCanvas(HDC hdc, int width, int height) {
    RECT canvasRect = { 0, 0, width, height };
    FillRect(hdc, &canvasRect, (HBRUSH)(COLOR_WINDOW + 1));
}

void SaveUndoState() {
    BitBlt(g_hdcUndo, 0, 0, g_canvasWidth, g_canvasHeight, g_hdcBuffer, 0, 0, SRCCOPY);
}

void RestoreUndoState() {
    BitBlt(g_hdcBuffer, 0, 0, g_canvasWidth, g_canvasHeight, g_hdcUndo, 0, 0, SRCCOPY);
}

void SaveCanvasAsBitmap(HWND hwnd) {
    OPENFILENAMEW ofn = {};
    wchar_t szFile[260] = L"untitled.bmp";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"Bitmap (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
        BITMAPFILEHEADER bmfh = {};
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = g_canvasWidth;
        bi.bmiHeader.biHeight = -g_canvasHeight;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        DWORD dwBmpSize = g_canvasWidth * g_canvasHeight * 4;
        void* pBits = malloc(dwBmpSize);
        GetDIBits(g_hdcBuffer, g_hbmBuffer, 0, g_canvasHeight, pBits, &bi, DIB_RGB_COLORS);
        HANDLE hFile = CreateFileW(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD dwWritten;
            bmfh.bfType = 0x4D42; // 'BM'
            bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            bmfh.bfSize = bmfh.bfOffBits + dwBmpSize;
            WriteFile(hFile, &bmfh, sizeof(bmfh), &dwWritten, NULL);
            WriteFile(hFile, &bi.bmiHeader, sizeof(bi.bmiHeader), &dwWritten, NULL);
            WriteFile(hFile, pBits, dwBmpSize, &dwWritten, NULL);
            CloseHandle(hFile);
        }
        free(pBits);
        MessageBoxW(hwnd, L"File saved successfully!", L"Save File", MB_OK | MB_ICONINFORMATION);
    }
}

INT_PTR CALLBACK PenWidthDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam); 
    switch (message) {
        case WM_INITDIALOG:
            SetDlgItemInt(hDlg, IDC_WIDTH_EDIT, g_appState.penWidth, FALSE);
            return (INT_PTR)TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                BOOL bSuccess;
                int newWidth = GetDlgItemInt(hDlg, IDC_WIDTH_EDIT, &bSuccess, FALSE);
                if (bSuccess && newWidth > 0 && newWidth < 100) {
                    g_appState.penWidth = newWidth;
                }
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}
