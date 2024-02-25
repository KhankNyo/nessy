
#include <windows.h>
#include <CommCtrl.h>

#include "Common.h"
#include "Utils.h"

#include "Nes.c"




typedef enum Win32_MenuOptions 
{
    WIN32_FILE_MENU_OPEN,
} Win32_MenuOptions;
static struct {
    HWND MainWindow,
         StatusWindow;
    COLORREF StatusWindowBackgroundColor;
    HFONT StatusWindowFont;
    int MainWindowWidth, 
        MainWindowHeight;
    double StatusWindowWidthRatio;
} sWin32_Gui;

static double sWin32_FontSize = -16.;





static void Win32_Fatal(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Fatal", MB_ICONERROR);
    ExitProcess(1);
}


static void Win32_RegisterWindowClass(HINSTANCE Instance, const char *ClassName, WNDPROC WndProc, UINT ExtraStyles, COLORREF Color)
{
    WNDCLASSEXA WindowClass = {
        .cbSize = sizeof WindowClass,
        .style = CS_HREDRAW | CS_VREDRAW | ExtraStyles,
        .lpfnWndProc = WndProc,
        .lpszClassName = ClassName,
        .hInstance = Instance,
        .hCursor = LoadCursorA(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)CreateSolidBrush(Color),
    };
    ATOM Atom = RegisterClassExA(&WindowClass);
    if (!Atom)
    {
        Win32_Fatal("Failed to register window class.");
    }
}

static HWND Win32_CreateChildWindow(
    HWND ParentWindowHandle, 
    const char *Name,
    const char *StyleClass,
    DWORD ExtraStyles
)
{
    HWND Window = CreateWindowExA(WS_EX_CLIENTEDGE, 
        StyleClass, Name, 
        WS_BORDER | WS_VISIBLE | WS_CHILD | ExtraStyles,
        0, 0, 100, 50,
        ParentWindowHandle, 
        NULL, NULL, NULL
    );
    if (!Window)
        Win32_Fatal("Cannot create child window.");
    return Window;
}

static void Win32_ResizeGui(int NewWidth, int NewHeight)
{
    sWin32_Gui.MainWindowWidth = NewWidth;
    sWin32_Gui.MainWindowHeight = NewHeight;

    int StatusWindowWidth = NewWidth * sWin32_Gui.StatusWindowWidthRatio;
    int StatusWindowHeight = NewHeight;
    int StatusWindowX = NewWidth - StatusWindowWidth;
    int StatusWindowY = 0;
    SetWindowPos(
        sWin32_Gui.StatusWindow, HWND_TOP, 
        StatusWindowX, StatusWindowY, 
        StatusWindowWidth, StatusWindowHeight,
        SWP_NOZORDER
    );
}

static LRESULT CALLBACK Win32_StatusWndProc(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    switch (Msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT PaintStruct;
        HDC DeviceContext = BeginPaint(Window, &PaintStruct);
        RECT Region = PaintStruct.rcPaint;
        SetBkColor(DeviceContext, sWin32_Gui.StatusWindowBackgroundColor);
        SetTextColor(DeviceContext, RGB(255, 255, 255));
        HFONT OldFont = NULL;
        if (sWin32_Gui.StatusWindowFont)
            OldFont = SelectObject(DeviceContext, sWin32_Gui.StatusWindowFont);

        Nes_DisplayableStatus Status = Nes_QueryStatus();
        char Flags[] = "nv_bdizc";
        if (Status.N)
            Flags[0] -= 32;
        if (Status.V)
            Flags[1] -= 32;
        if (Status.B)
            Flags[3] -= 32;
        if (Status.D)
            Flags[4] -= 32;
        if (Status.I)
            Flags[5] -= 32;
        if (Status.Z)
            Flags[6] -= 32;
        if (Status.C)
            Flags[7] -= 32;

        char Tmp[4096];
        FormatString(
            Tmp, sizeof Tmp,
            "A:[{x2}] ", (u32)Status.A, 
            "X:[{x2}] ", (u32)Status.X,
            "Y:[{x2}] ", (u32)Status.Y, 
            "PC:[{x4}] ", (u32)Status.PC, 
            "SP:[{x4}] ", (u32)Status.SP,
            "\n\nFlags: {s}", Flags,
            NULL
        );
        int TextHeight = DrawText(DeviceContext, Tmp, -1, &Region, DT_LEFT | DT_WORDBREAK);
        Region.top += TextHeight * 3 / 2;

        if (OldFont)
            SelectObject(DeviceContext, OldFont);
        EndPaint(Window, &PaintStruct);
    } break;
    default: return DefWindowProcA(Window, Msg, WParam, LParam);
    }
    return 0;
}

static LRESULT CALLBACK Win32_MainWndProc(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    switch (Msg)
    {
    case WM_CREATE:
    {
        HMENU FileMenu = CreateMenu();
            AppendMenuA(FileMenu, MF_STRING, WIN32_FILE_MENU_OPEN, "Open");
        HMENU MainMenu = CreateMenu();
            AppendMenuA(MainMenu, MF_POPUP, (UINT_PTR)FileMenu, "File");
        SetMenu(Window, MainMenu);

        const char *StatusClass = "Status";
        sWin32_Gui.StatusWindowBackgroundColor = RGB(50, 50, 255);
        Win32_RegisterWindowClass(
            GetModuleHandleA(NULL), 
            StatusClass, 
            Win32_StatusWndProc, 
            0, 
            sWin32_Gui.StatusWindowBackgroundColor
        );
        sWin32_Gui.StatusWindow = Win32_CreateChildWindow(
            Window, "Status", StatusClass, 0
        );
        sWin32_Gui.StatusWindowFont = CreateFont(
            sWin32_FontSize, sWin32_FontSize,
            0, 
            0, 
            FW_MEDIUM, 
            0, 
            0, 
            0,
            OEM_CHARSET,
            OUT_DEVICE_PRECIS,
            CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY,
            DEFAULT_PITCH,
            "Terminal"
        );
        sWin32_Gui.StatusWindowWidthRatio = .35;
    } break;
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    } break;
    case WM_SIZE:
    {
        RECT WindowDimensions;
        GetClientRect(Window, &WindowDimensions);
        int Width = WindowDimensions.right - WindowDimensions.left;
        int Height = WindowDimensions.bottom - WindowDimensions.top;
        Win32_ResizeGui(Width, Height);
    } break;
    default: return DefWindowProcA(Window, Msg, WParam, LParam);
    }
    return 0;
}


static Bool8 Win32_PollInputs(void)
{
    MSG Msg;
    while (PeekMessageA(&Msg, NULL, 0, 0, PM_REMOVE))
    {
        if (Msg.message == WM_QUIT)
            return false;
        TranslateMessage(&Msg);
        DispatchMessageA(&Msg);
    }
    return true;
}

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PCHAR CmdLine, int CmdShow)
{
    (void)PrevInstance, (void)CmdLine, (void)CmdShow;
    {
        static INITCOMMONCONTROLSEX CommCtrl = {
            .dwICC = ICC_UPDOWN_CLASS,
            .dwSize = sizeof CommCtrl,
        };
        (void)InitCommonControlsEx(&CommCtrl);
    }

    const char *ClassName = "NESSY";
    Win32_RegisterWindowClass(Instance, ClassName, Win32_MainWndProc, 0, RGB(255, 255, 255));
    sWin32_Gui.MainWindow = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        ClassName,
        "Nessy",
        WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_OVERLAPPEDWINDOW ,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1080, 720,
        NULL, NULL, NULL, NULL
    );
    if (!sWin32_Gui.MainWindow)
    {
        Win32_Fatal("Failed to create window.");
    }
    Win32_ResizeGui(1080, 720);

    Nes_OnEntry();
    double TimePassed = GetTickCount64();
    while (Win32_PollInputs())
    {
        Nes_OnLoop();

        RECT WindowRect = {
            .top = 0,
            .left = 0,
            .right = sWin32_Gui.MainWindowWidth, 
            .bottom = sWin32_Gui.MainWindowHeight,
        };
        double Now = GetTickCount64();
        if (Now - TimePassed > 1000.0 / 60.0)
        {
            InvalidateRect(sWin32_Gui.MainWindow, &WindowRect, FALSE);
            TimePassed = Now;
        }
    }
    Nes_AtExit();

    return 0;
}

