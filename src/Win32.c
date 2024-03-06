
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wingdi.h>

#include "Common.h"
#include "Utils.h"


#include "Nes.c"


#define COLOR_WHITE 0x00FFFFFF
#define COLOR_BLACK 0x00000000
#define OPEN_FILE(Fname, Permission, ExtraPermission, FileAttr)\
    CreateFileA(Fname, Permission, 0, NULL, ExtraPermission, FILE_ATTRIBUTE_NORMAL | (FileAttr), NULL)

typedef enum Win32_MenuOptions 
{
    WIN32_FILE_MENU_OPEN = 0x100,
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

static OVERLAPPED sWin32_AsyncFileReader;
static HANDLE sWin32_FileHandle = INVALID_HANDLE_VALUE;
static void *sWin32_FileBuffer;
static isize sWin32_FileBufferSize;






static void Win32_Fatal(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Fatal Error", MB_ICONERROR);
    ExitProcess(1);
}

static void Win32_ErrorBox(const char *ErrorMessage)
{
    MessageBoxA(NULL, ErrorMessage, "Error", MB_ICONERROR);
}

static void Win32_SystemError(const char *Caption)
{
    DWORD ErrorCode = GetLastError();
    LPSTR ErrorText = NULL;

    FormatMessage(
        /* use system message tables to retrieve error text */
        FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_IGNORE_INSERTS,  
        NULL,
        ErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&ErrorText,
        0,      /* minimum size for output buffer */
        NULL
    );
    if (NULL != ErrorText)
    {
        MessageBoxA(NULL, ErrorText, Caption, MB_ICONERROR);
        /* release memory allocated by FormatMessage() */
        LocalFree(ErrorText);
    }
}

static void *Win32_AllocateMemory(isize SizeBytes)
{
    DEBUG_ASSERT(SizeBytes > 0);
    void *Buffer = VirtualAlloc(NULL, SizeBytes, MEM_COMMIT, PAGE_READWRITE);
    if (!Buffer)
    {
        Win32_Fatal("Out of memory.");
    }
    return Buffer;
}

static void Win32_DeallocateMemory(void *Ptr)
{
    VirtualFree(Ptr, 0, MEM_RELEASE);
}

static BITMAPINFO Win32_DefaultBitmapInfo(int Width, int Height)
{
     BITMAPINFO BitmapInfo = {
        .bmiHeader = {
            .biSize = sizeof BitmapInfo.bmiHeader,
            .biPlanes = 1,
            .biWidth = Width,
            .biHeight = -Height,
            .biBitCount = 32,
            .biCompression = BI_RGB,
        },
    };
    return BitmapInfo;
}


static void Win32_RegisterWindowClass(
    HINSTANCE Instance, 
    const char *ClassName, 
    WNDPROC WndProc, 
    UINT ExtraStyles, 
    COLORREF Color)
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



static OPENFILENAMEA Win32_CreateFileSelectionPrompt(DWORD PromptFlags)
{
    static char FileNameBuffer[MAX_PATH];
    /* get the current file name to show in the dialogue */
    OPENFILENAMEA DialogueConfig = {
        .lStructSize = sizeof(DialogueConfig),
        .hwndOwner = sWin32_Gui.MainWindow,
        .hInstance = 0, /* ignored if OFN_OPENTEMPLATEHANDLE is not set */
        .lpstrFilter = "Text documents (*.txt)\0*.txt\0All files (*)\0*\0",
        /* no custom filter */
        /* no custom filter size */
        /* this is how you get hacked */
        .nFilterIndex = 2, /* the second pair of strings in lpstrFilter separated by the null terminator???? */
        .lpstrFile = FileNameBuffer,

        .nMaxFile = sizeof FileNameBuffer,
        /* no file title, optional */
        /* no size of the above, optional */
        /* initial directory is current directory */
        /* default title ('Save as' or 'Open') */
        .Flags = PromptFlags,
        .nFileOffset = 0,
        .nFileExtension = 0, /* ??? */
        /* no default extension */
        /* no extra flags */
    };
    return DialogueConfig;
}

static HANDLE Win32_ReadFileAsync(const char *FileName, DWORD dwCreationDisposition)
{
    HANDLE FileHandle = OPEN_FILE(FileName, GENERIC_READ, dwCreationDisposition, FILE_FLAG_OVERLAPPED);
    if (INVALID_HANDLE_VALUE == FileHandle)
    {
        Win32_SystemError("Unable to open file");
        return INVALID_HANDLE_VALUE;
    }

    LARGE_INTEGER ArchaicFileSize;
    if (!GetFileSizeEx(FileHandle, &ArchaicFileSize))
    {
        Win32_SystemError("Unable to retrieve file size");
        goto CloseFile;
    }

    sWin32_AsyncFileReader = (OVERLAPPED){
        .Offset = 0,
        .OffsetHigh = 0,
        .hEvent = CreateEventA(NULL, FALSE, FALSE, "AsyncReader")
    };
    if (INVALID_HANDLE_VALUE == sWin32_AsyncFileReader.hEvent)
    {
        Win32_SystemError("Unable to read file asynchronously");
        goto CloseFile;
    }

    sWin32_FileBufferSize = ArchaicFileSize.QuadPart;
    sWin32_FileBuffer = Win32_AllocateMemory(sWin32_FileBufferSize);
    DWORD BytesRead;
    if (!ReadFile(
            FileHandle, 
            sWin32_FileBuffer, 
            sWin32_FileBufferSize, 
            &BytesRead, 
            &sWin32_AsyncFileReader
        ) && GetLastError() != ERROR_IO_PENDING)
    {
        Win32_SystemError("Unable to read file");
        goto CancelIO;
    }
    return FileHandle;
CancelIO:
    CancelIo(FileHandle);
CloseFile:
    CloseHandle(FileHandle);
    return INVALID_HANDLE_VALUE;
}




static LRESULT CALLBACK Win32_StatusWndProc(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    switch (Msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT PaintStruct;
        HDC MainDC = BeginPaint(Window, &PaintStruct);
        RECT Region = PaintStruct.rcPaint;
        int Width = Region.right - Region.left, 
            Height = Region.bottom - Region.top;

        HDC DeviceContext = CreateCompatibleDC(MainDC);
        HBITMAP BackBuffer = CreateCompatibleBitmap(MainDC, Width, Height);
        SelectObject(DeviceContext, BackBuffer);
        {
            HBRUSH BackgroundColor = CreateSolidBrush(sWin32_Gui.StatusWindowBackgroundColor);
            FillRect(DeviceContext, &Region, BackgroundColor);
            DeleteObject(BackgroundColor);
        }
        {
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
                "A:[{x2}]\n", (u32)sWin32_DisplayableStatus.A, 
                "X:[{x2}]\n", (u32)sWin32_DisplayableStatus.X,
                "Y:[{x2}]\n", (u32)sWin32_DisplayableStatus.Y, 
                "PC:[{x4}]\n", (u32)sWin32_DisplayableStatus.PC, 
                "SP:[{x4}]: {x4}\n", (u32)sWin32_DisplayableStatus.SP, (u32)sWin32_DisplayableStatus.StackValue,
                "Flags: {s}", Flags,
                NULL
            );
            Win32_DrawTextWrap(DeviceContext, &Region, Tmp);

            Region.top = sWin32_Gui.MainWindowHeight / 5;
            Region.top += Win32_DrawTextWrap(DeviceContext, &Region, sWin32_DisplayableStatus.DisasmBeforePC);
                Win32_InvertTextAndBackgroundColors(DeviceContext);
            Region.top += Win32_DrawTextWrap(DeviceContext, &Region, sWin32_DisplayableStatus.DisasmAtPC);
                Win32_InvertTextAndBackgroundColors(DeviceContext);
            Win32_DrawTextWrap(DeviceContext, &Region, sWin32_DisplayableStatus.DisasmAfterPC);


            int PalettesToDisplay = NES_PALETTE_SIZE;
            for (int i = 0; i < PalettesToDisplay; i++)
            {
                int x = Region.left + 10 + F32_LERP(Region.left, Region.right - 20, (double)i/PalettesToDisplay);
                int y = sWin32_Gui.MainWindowHeight * .95;
                int w = F32_LERP(Region.left, Region.right - 20, (double)1/PalettesToDisplay);
                int h = 10;
                RECT r = {
                    .left = x,
                    .top = y,
                    .bottom = y + h,
                    .right = x + w,
                };
                HBRUSH Color = CreateSolidBrush(sWin32_DisplayableStatus.Palette[i]);
                FillRect(DeviceContext, &r, Color);
                DeleteObject(Color);
            }

            BITMAPINFO PatternTableBitmap = Win32_DefaultBitmapInfo(
                NES_PATTERN_TABLE_WIDTH_PIX, 
                NES_PATTERN_TABLE_HEIGHT_PIX
            );
            int TableSize = sWin32_Gui.MainWindowWidth * 220. / 1350;
            int TableY = Height * .67;
            int TableX = Region.left + 10;
            StretchDIBits(DeviceContext, 
                TableX, TableY, 
                TableSize, TableSize,
                0, 0, 
                NES_PATTERN_TABLE_WIDTH_PIX, 
                NES_PATTERN_TABLE_HEIGHT_PIX, 
                sWin32_DisplayableStatus.LeftPatternTable, 
                &PatternTableBitmap, 
                DIB_RGB_COLORS, 
                SRCCOPY
            );
            StretchDIBits(DeviceContext, 
                TableX + TableSize + 5, TableY, 
                TableSize, TableSize,
                0, 0, 
                NES_PATTERN_TABLE_WIDTH_PIX, 
                NES_PATTERN_TABLE_HEIGHT_PIX, 
                sWin32_DisplayableStatus.RightPatternTable, 
                &PatternTableBitmap, 
                DIB_RGB_COLORS, 
                SRCCOPY
            );

            if (OldFont)
                SelectObject(DeviceContext, OldFont);
        }
        BitBlt(MainDC, 0, 0, Width, Height, DeviceContext, 0, 0, SRCCOPY);
        DeleteDC(DeviceContext);
        DeleteObject(BackBuffer);
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
        BITMAPINFO BitmapInfo = Win32_DefaultBitmapInfo(Width, Height);
        StretchDIBits(
            DeviceContext, 
            Frame.X, Frame.Y, 
            Frame.W, Frame.H, 
            0, 0, Width, Height,
            Buffer, &BitmapInfo, 
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
            AppendMenuA(FileMenu, MF_STRING, WIN32_FILE_MENU_OPEN, "&Open\tCtrl+O");
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
    case WM_KEYDOWN:
    {
        switch (LOWORD(WParam))
        {
        case 'H':
        {
            Nes_OnEmulatorToggleHalt();
        } break;
        case 'R':
        {
            Nes_OnEmulatorReset();
        } break;
        case 'F':
        {
            Nes_OnEmulatorSingleFrame();
        } break;
        case 'P':
        {
            Nes_OnEmulatorTogglePalette();
        } break;
        case VK_SPACE:
        {
            Nes_OnEmulatorSingleStep();
        } break;
        }
    } break;
    case WM_COMMAND:
    {
        switch ((Win32_MenuOptions)LOWORD(WParam))
        {
        case WIN32_FILE_MENU_OPEN:
        {
            if (INVALID_HANDLE_VALUE != sWin32_FileHandle)
            {
                Win32_SystemError("Cannot open file at this time.");
                break;
            }

            OPENFILENAMEA Prompt = Win32_CreateFileSelectionPrompt(0);
            if (GetOpenFileNameA(&Prompt))
            {
                sWin32_FileHandle = Win32_ReadFileAsync(Prompt.lpstrFile, OPEN_EXISTING);
            }
        } break;
        }
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

    /* check async file operation */
    if (sWin32_FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesReadSoFar = sWin32_FileBufferSize;
        if (GetOverlappedResult(
            sWin32_FileHandle, 
            &sWin32_AsyncFileReader, 
            &BytesReadSoFar, 
            FALSE
        ) || GetLastError() != ERROR_IO_INCOMPLETE)
        {
            CloseHandle(sWin32_FileHandle);

            const char *ErrorMessage = Nes_ParseINESFile(sWin32_FileBuffer, sWin32_FileBufferSize);
            if (NULL != ErrorMessage)
            {
                Win32_ErrorBox(ErrorMessage);
            }
            Win32_DeallocateMemory(sWin32_FileBuffer);
            sWin32_FileBuffer = NULL;
            sWin32_FileBufferSize = 0;
            sWin32_FileHandle = INVALID_HANDLE_VALUE;
        }
        else if (BytesReadSoFar != sWin32_FileBufferSize)
        {
            CancelIo(sWin32_FileHandle);
            CloseHandle(sWin32_FileHandle);
            sWin32_FileHandle = INVALID_HANDLE_VALUE;
        }
    }
    return true;
}

static void Win32_UpdateWindowTimer(HWND Window, UINT DontCare, UINT_PTR DontCare2, DWORD DontCare3)
{
    (void)Window, (void)DontCare, (void)DontCare2, (void)DontCare3;
    sWin32_DisplayableStatus = Nes_PlatformQueryDisplayableStatus();
    sWin32_FrameBuffer = Nes_PlatformQueryFrameBuffer();

    InvalidateRect(sWin32_Gui.StatusWindow, NULL, FALSE);
    InvalidateRect(sWin32_Gui.GameWindow, NULL, FALSE);
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
    SetTimer(sWin32_Gui.MainWindow, 999, 1000 / 100, Win32_UpdateWindowTimer);

    /* resize window */
    {
        int DefaultWidth = 1300, 
            DefaultHeight = 845;
        int DefaultX = (GetSystemMetrics(SM_CXSCREEN) - DefaultWidth) / 2;
        int DefaultY = (GetSystemMetrics(SM_CYSCREEN) - DefaultHeight) / 2;
        SetWindowPos( 
            sWin32_Gui.MainWindow, HWND_BOTTOM,
            DefaultX, DefaultY, DefaultWidth, DefaultHeight, 
            SWP_NOZORDER
        );
        RECT ClientRect;
        GetClientRect(sWin32_Gui.MainWindow, &ClientRect);
        Win32_ResizeGuiComponents(
            ClientRect.right - ClientRect.left, 
            ClientRect.bottom - ClientRect.top
        );
    }
    
    /* init timer */
    LARGE_INTEGER Tmp;
    QueryPerformanceFrequency(&Tmp);
    sWin32_TimerFrequency = 1000.0 / (double)Tmp.QuadPart;


    Nes_OnEntry();
    double TimeOrigin = Platform_GetTimeMillisec();
    while (Win32_PollInputs())
    {
        double Now = Platform_GetTimeMillisec();
        Nes_OnLoop(Now - TimeOrigin);
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

Nes_ControllerStatus Platform_GetControllerState(void)
{
    u16 Left = GetKeyState('A') < 0 || GetKeyState(VK_LEFT) < 0; 
    u16 Right = GetKeyState('D') < 0 || GetKeyState(VK_RIGHT) < 0;
    u16 Up = GetKeyState('W') < 0 || GetKeyState(VK_UP) < 0;
    u16 Down = GetKeyState('S') < 0 || GetKeyState(VK_DOWN) < 0;
    u16 Start = GetKeyState(VK_RETURN) < 0;
    u16 Select = GetKeyState(VK_TAB) < 0;
    u16 A = GetKeyState('K') < 0;
    u16 B = GetKeyState('J') < 0;

    Nes_ControllerStatus ControllerStatus = 
        (A << 0)
        | (B << 1)
        | (Select << 2) 
        | (Start << 3) 
        | (Up << 4) 
        | (Down << 5) 
        | (Left << 6) 
        | (Right << 7);
    return ControllerStatus;
}

