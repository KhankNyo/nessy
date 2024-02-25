
#include <windows.h>
#include <CommCtrl.h>
#include <wingdi.h>

#include "Common.h"
#include "Utils.h"


#include "Nes.c"


#define COLOR_WHITE 0x00FFFFFF
#define COLOR_BLACK 0x00000000

typedef enum Win32_MenuOptions 
{
    WIN32_FILE_MENU_OPEN,
} Win32_MenuOptions;

typedef struct Win32_Rect 
{
    int X, Y, W, H;
} Win32_Rect;



static struct {
    HWND MainWindow,
         StatusWindow,
         GameWindow;
    COLORREF StatusWindowBackgroundColor;
    HFONT StatusWindowFont;
    int MainWindowWidth, 
        MainWindowHeight;
    double StatusWindowWidthRatio;
} sWin32_Gui = {
    .StatusWindowBackgroundColor = RGB(25, 25, 255),
    .StatusWindowWidthRatio = .35,
};

static double sWin32_FontSize = -16.;
static double sWin32_TimerFrequency;
static Nes_DisplayableStatus sWin32_DisplayableStatus;
static Platform_FrameBuffer sWin32_FrameBuffer;





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

static HWND Win32_CreateWindow(
    HWND ParentWindowHandle, 
    const char *Name,
    const char *StyleClass,
    DWORD ExtraStyles
)
{
    HWND Window = CreateWindowExA(WS_EX_CLIENTEDGE, 
        StyleClass, 
        Name, 
        WS_BORDER | WS_VISIBLE | ExtraStyles,
        0, 0, 0, 0,
        ParentWindowHandle, 
        NULL, NULL, NULL
    );
    if (!Window)
        Win32_Fatal("Cannot create window.");
    return Window;
}

static void Win32_ResizeGuiComponents(int NewWidth, int NewHeight)
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
    int GameWindowWidth = NewWidth - StatusWindowWidth;
    int GameWindowHeight = NewHeight;
    int GameWindowX = 0;
    int GameWindowY = 0;
    SetWindowPos(
        sWin32_Gui.GameWindow, HWND_BOTTOM,
        GameWindowX, GameWindowY,
        GameWindowWidth, GameWindowHeight, 
        SWP_NOZORDER
    );
}

static Win32_Rect Win32_FitFrameToScreen(const RECT *ScreenRegion, int FrameWidth, int FrameHeight)
{
    Win32_Rect FrameDimensions = {
        .X = ScreenRegion->left,
        .Y = ScreenRegion->top,                          
        .W = ScreenRegion->right - ScreenRegion->left,   
        .H = ScreenRegion->bottom - ScreenRegion->top,
    };
    double ScreenAspectRatio = (double)FrameDimensions.W / FrameDimensions.H;
    double FrameAspectRatio = (double)FrameWidth / (double)FrameHeight;

    /* the screen is wider than what we want to display */
    if (ScreenAspectRatio > FrameAspectRatio)
    {
        /* scale width to fit */
        FrameDimensions.W = FrameDimensions.H * FrameAspectRatio;
        int Middle = (ScreenRegion->right - ScreenRegion->left) / 2;
        FrameDimensions.X = FrameDimensions.X + Middle - FrameDimensions.W/2;
    }
    else /* what we want to display is wider */
    {
        /* scale height to fit */
        FrameDimensions.H = FrameDimensions.W / FrameAspectRatio;
        int Middle = (ScreenRegion->bottom - ScreenRegion->top) / 2;
        FrameDimensions.Y = FrameDimensions.Y + Middle - FrameDimensions.H/2;
    }
    return FrameDimensions;
}

static void Win32_InvertTextAndBackgroundColors(HDC DeviceContext)
{
    COLORREF TextColor = GetTextColor(DeviceContext);
    COLORREF BackgroundColor = SetBkColor(DeviceContext, TextColor);
    SetTextColor(DeviceContext, BackgroundColor);
}

static int Win32_DrawTextWrap(HDC DeviceContext, RECT *Region, const char *Text)
{
    return DrawTextA(DeviceContext, Text, -1, Region, DT_LEFT | DT_WORDBREAK);
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
        SetTextColor(DeviceContext, COLOR_WHITE);
        HFONT OldFont = NULL;
        if (sWin32_Gui.StatusWindowFont)
            OldFont = SelectObject(DeviceContext, sWin32_Gui.StatusWindowFont);


        char Flags[] = "nv_bdizc";
        if (sWin32_DisplayableStatus.N)
            Flags[0] -= 32;
        if (sWin32_DisplayableStatus.V)
            Flags[1] -= 32;
        if (sWin32_DisplayableStatus.B)
            Flags[3] -= 32;
        if (sWin32_DisplayableStatus.D)
            Flags[4] -= 32;
        if (sWin32_DisplayableStatus.I)
            Flags[5] -= 32;
        if (sWin32_DisplayableStatus.Z)
            Flags[6] -= 32;
        if (sWin32_DisplayableStatus.C)
            Flags[7] -= 32;

        char Tmp[4096];
        FormatString(
            Tmp, sizeof Tmp,
            "A:[{x2}] ", (u32)sWin32_DisplayableStatus.A, 
            "X:[{x2}] ", (u32)sWin32_DisplayableStatus.X,
            "Y:[{x2}] ", (u32)sWin32_DisplayableStatus.Y, 
            "PC:[{x4}] ", (u32)sWin32_DisplayableStatus.PC, 
            "SP:[{x4}] ", (u32)sWin32_DisplayableStatus.SP,
            "\n\nFlags: {s}", Flags,
            NULL
        );
        Win32_DrawTextWrap(DeviceContext, &Region, Tmp);

        Region.top = sWin32_Gui.MainWindowHeight / 3;
        Region.top += Win32_DrawTextWrap(DeviceContext, &Region, sWin32_DisplayableStatus.DisasmBeforePC);
            Win32_InvertTextAndBackgroundColors(DeviceContext);
        Region.top += Win32_DrawTextWrap(DeviceContext, &Region, sWin32_DisplayableStatus.DisasmAtPC);
            Win32_InvertTextAndBackgroundColors(DeviceContext);
        Win32_DrawTextWrap(DeviceContext, &Region, sWin32_DisplayableStatus.DisasmAfterPC);

        if (OldFont)
            SelectObject(DeviceContext, OldFont);
        EndPaint(Window, &PaintStruct);
    } break;
    default: return DefWindowProcA(Window, Msg, WParam, LParam);
    }
    return 0;
}

static LRESULT CALLBACK Win32_GameWndProc(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    switch (Msg)
    {
    case WM_PAINT:
    {
        if (!sWin32_FrameBuffer.Data) 
            return DefWindowProcA(Window, Msg, WParam, LParam);

        PAINTSTRUCT PaintStruct;
        HDC DeviceContext = BeginPaint(Window, &PaintStruct);
        RECT *ScreenRegion = &PaintStruct.rcPaint;

        int Width = sWin32_FrameBuffer.Width;
        int Height = sWin32_FrameBuffer.Height;
        const void *Buffer = sWin32_FrameBuffer.Data;

        Win32_Rect Frame = Win32_FitFrameToScreen(ScreenRegion, Width, Height);
        BITMAPINFO BufferInfo = {
            .bmiHeader = {
                .biSize = sizeof BufferInfo.bmiHeader,
                .biPlanes = 1,
                .biWidth = Width,
                .biHeight = -Height,
                .biBitCount = 32,
                .biCompression = BI_RGB,
            },
        };
        StretchDIBits(
            DeviceContext, 
            Frame.X, Frame.Y, 
            Frame.W, Frame.H, 
            0, 0, Width, Height,
            Buffer, &BufferInfo, 
            DIB_RGB_COLORS, SRCCOPY
        );
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
        Win32_RegisterWindowClass(
            GetModuleHandleA(NULL), 
            StatusClass, 
            Win32_StatusWndProc, 
            0, 
            sWin32_Gui.StatusWindowBackgroundColor
        );
        sWin32_Gui.StatusWindow = Win32_CreateWindow(
            Window, "Status", StatusClass, WS_CHILD
        );

        const char *GameClass = "nesicle";
        Win32_RegisterWindowClass(
            GetModuleHandleA(NULL), 
            GameClass, 
            Win32_GameWndProc, 
            0, 
            COLOR_BLACK
        );
        sWin32_Gui.GameWindow = Win32_CreateWindow(
            Window, "Emulator", GameClass, WS_CHILD
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
        Win32_ResizeGuiComponents(Width, Height);
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
        INITCOMMONCONTROLSEX CommCtrl = {
            .dwICC = ICC_UPDOWN_CLASS,
            .dwSize = sizeof CommCtrl,
        };
        (void)InitCommonControlsEx(&CommCtrl);
    }

    /* create main window */
    const char *ClassName = "NESSY";
    Win32_RegisterWindowClass(Instance, ClassName, Win32_MainWndProc, 0, COLOR_WHITE);
    sWin32_Gui.MainWindow = Win32_CreateWindow(NULL, "Nessy", ClassName, WS_CAPTION | WS_OVERLAPPEDWINDOW);

    /* resize window */
    {
        int DefaultWidth = 1220, 
            DefaultHeight = 720;
        int DefaultX = (GetSystemMetrics(SM_CXSCREEN) - DefaultWidth) / 2;
        int DefaultY = (GetSystemMetrics(SM_CYSCREEN) - DefaultHeight) / 2;
        SetWindowPos( 
            sWin32_Gui.MainWindow, HWND_BOTTOM,
            DefaultX, DefaultY, DefaultWidth, DefaultHeight, 
            SWP_NOZORDER
        );
        Win32_ResizeGuiComponents(DefaultWidth, DefaultHeight);
    }
    
    /* init timer */
    LARGE_INTEGER Tmp;
    QueryPerformanceFrequency(&Tmp);
    sWin32_TimerFrequency = 1000.0 / (double)Tmp.QuadPart;


    Nes_OnEntry();
    double TimePassed = Platform_GetTimeMillisec();
    while (Win32_PollInputs())
    {
        Sleep(5);
        Nes_OnLoop();

        double Now = Platform_GetTimeMillisec();
        if (Now - TimePassed > 1000.0 / 60.0)
        {
            TimePassed = Now;

            sWin32_FrameBuffer = Nes_PlatformQueryFrameBuffer();
            InvalidateRect(sWin32_Gui.GameWindow, NULL, FALSE);
        }
    }
    Nes_AtExit();

    /* don't need to clean up the window, windows does it for us */
    (void)sWin32_Gui.MainWindow;
    return 0;
}


double Platform_GetTimeMillisec(void)
{
    LARGE_INTEGER Now;
    QueryPerformanceCounter(&Now);
    return (double)Now.QuadPart * sWin32_TimerFrequency;
}

void Platform_NesNotifyChangeInStatus(const Nes_DisplayableStatus *Status)
{
    sWin32_DisplayableStatus = *Status;
    InvalidateRect(sWin32_Gui.StatusWindow, NULL, TRUE);
}

