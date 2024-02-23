#include "Common.h"
#include "Nes.c"
#include <windows.h>
#include <CommCtrl.h>


typedef enum Win32_MenuOptions 
{
    WIN32_FILE_MENU_OPEN,
} Win32_MenuOptions;
static struct {
    HWND MainWindow,
         CPUStatusWindow;
    int MainWindowWidth, 
        MainWindowHeight;
    double CPUStatusWindowWidthRatio;
} sWin32_Gui;


void Platform_Fatal(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Fatal", MB_ICONERROR);
    ExitProcess(1);
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
        Platform_Fatal("Cannot create child window.");
    return Window;
}

static void Win32_ResizeGui(int NewWidth, int NewHeight)
{
    sWin32_Gui.MainWindowWidth = NewWidth;
    sWin32_Gui.MainWindowHeight = NewHeight;

    int CPUStatusWindowWidth = NewWidth * sWin32_Gui.CPUStatusWindowWidthRatio;
    int CPUStatusWindowHeight = NewHeight;
    int CPUStatusWindowX = NewWidth - CPUStatusWindowWidth;
    int CPUStatusWindowY = 0;
    SetWindowPos(
        sWin32_Gui.CPUStatusWindow, HWND_TOP, 
        CPUStatusWindowX, CPUStatusWindowY, 
        CPUStatusWindowWidth, CPUStatusWindowHeight,
        SWP_NOZORDER
    );
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

        sWin32_Gui.CPUStatusWindow = Win32_CreateChildWindow(
            Window, "Status", "STATIC", WS_TILED | WS_OVERLAPPED
        );
        Win32_CreateChildWindow(sWin32_Gui.CPUStatusWindow, 
            "00", "EDIT", 0
        );
        sWin32_Gui.CPUStatusWindowWidthRatio = .35;
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

    WNDCLASSEXA WindowClass = {
        .cbSize = sizeof WindowClass,
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = Win32_MainWndProc,
        .lpszClassName = "NES",
        .hInstance = Instance,
        .hCursor = LoadCursorA(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
    };
    ATOM Atom = RegisterClassExA(&WindowClass);
    if (!Atom)
    {
        Platform_Fatal("Failed to register window class.");
    }

    sWin32_Gui.MainWindow = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        WindowClass.lpszClassName,
        "Nessy",
        WS_BORDER | WS_VISIBLE | WS_CAPTION | WS_OVERLAPPEDWINDOW ,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1080, 720,
        NULL, NULL, NULL, NULL
    );
    if (!sWin32_Gui.MainWindow)
    {
        Platform_Fatal("Failed to create window.");
    }
    Win32_ResizeGui(1080, 720);

    Nes_OnEntry();
    while (Win32_PollInputs())
    {
        Nes_OnLoop();
    }
    Nes_AtExit();

    return 0;
}

