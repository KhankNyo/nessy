


#include "Common.h"
#include "Utils.h"

#include "Nes.c"

#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wingdi.h>
#include <mmdeviceapi.h>
#include <audioclient.h>



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
static BufferData sWin32_StaticBuffer;

static WAVEFORMATEX sWin32_AudioFormat = { 0 };


#define GUID_SECT

#define __DEFINE_GUID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const GUID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
#define __DEFINE_IID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const IID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
#define __DEFINE_CLSID(n,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) static const CLSID n GUID_SECT = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
#define PA_DEFINE_CLSID(className, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        __DEFINE_CLSID(s_CLS_##className, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)
#define PA_DEFINE_IID(interfaceName, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
        __DEFINE_IID(s_IID_##interfaceName, 0x##l, 0x##w1, 0x##w2, 0x##b1, 0x##b2, 0x##b3, 0x##b4, 0x##b5, 0x##b6, 0x##b7, 0x##b8)

// "1CB9AD4C-DBFA-4c32-B178-C2F568A703B2"
PA_DEFINE_IID(IAudioClient, 1cb9ad4c, dbfa, 4c32, b1, 78, c2, f5, 68, a7, 03, b2);
// "726778CD-F60A-4EDA-82DE-E47610CD78AA"
PA_DEFINE_IID(IAudioClient2, 726778cd, f60a, 4eda, 82, de, e4, 76, 10, cd, 78, aa);
// "7ED4EE07-8E67-4CD4-8C1A-2B7A5987AD42"
PA_DEFINE_IID(IAudioClient3, 7ed4ee07, 8e67, 4cd4, 8c, 1a, 2b, 7a, 59, 87, ad, 42);
// "1BE09788-6894-4089-8586-9A2A6C265AC5"
PA_DEFINE_IID(IMMEndpoint, 1be09788, 6894, 4089, 85, 86, 9a, 2a, 6c, 26, 5a, c5);
// "A95664D2-9614-4F35-A746-DE8DB63617E6"
PA_DEFINE_IID(IMMDeviceEnumerator, a95664d2, 9614, 4f35, a7, 46, de, 8d, b6, 36, 17, e6);
// "BCDE0395-E52F-467C-8E3D-C4579291692E"
PA_DEFINE_CLSID(IMMDeviceEnumerator, bcde0395, e52f, 467c, 8e, 3d, c4, 57, 92, 91, 69, 2e);
// "F294ACFC-3146-4483-A7BF-ADDCA7C260E2"
PA_DEFINE_IID(IAudioRenderClient, f294acfc, 3146, 4483, a7, bf, ad, dc, a7, c2, 60, e2);
// "C8ADBD64-E71E-48a0-A4DE-185C395CD317"
PA_DEFINE_IID(IAudioCaptureClient, c8adbd64, e71e, 48a0, a4, de, 18, 5c, 39, 5c, d3, 17);
// *2A07407E-6497-4A18-9787-32F79BD0D98F*  Or this??
PA_DEFINE_IID(IDeviceTopology, 2A07407E, 6497, 4A18, 97, 87, 32, f7, 9b, d0, d9, 8f);
// *AE2DE0E4-5BCA-4F2D-AA46-5D13F8FDB3A9*
PA_DEFINE_IID(IPart, AE2DE0E4, 5BCA, 4F2D, aa, 46, 5d, 13, f8, fd, b3, a9);
// *4509F757-2D46-4637-8E62-CE7DB944F57B*
PA_DEFINE_IID(IKsJackDescription, 4509F757, 2D46, 4637, 8e, 62, ce, 7d, b9, 44, f5, 7b);

#undef GUID_SECT




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


static BufferData Win32_ReadFileSync(const char *FileName, DWORD dwCreationDisposition)
{
    HANDLE FileHandle = OPEN_FILE(FileName, GENERIC_READ, dwCreationDisposition, 0);
    if (INVALID_HANDLE_VALUE == FileHandle)
        goto CreateFileFailed;

    LARGE_INTEGER ArchaicFileSize;
    if (!GetFileSizeEx(FileHandle, &ArchaicFileSize))
        goto GetFileSizeFailed;

    isize FileBufferSize = ArchaicFileSize.QuadPart;
    void *Buffer = Win32_AllocateMemory(FileBufferSize);
    if (NULL == Buffer)
        goto AllocateMemoryFailed;

    DWORD ReadSize;
    if (!ReadFile(FileHandle, Buffer, FileBufferSize, &ReadSize, NULL) || ReadSize != FileBufferSize)
        goto ReadFileFailed;

    CloseHandle(FileHandle);
    return (BufferData){
        .ViewPtr = Buffer,
        .SizeBytes = FileBufferSize
    };

ReadFileFailed:
    Win32_DeallocateMemory(Buffer);
AllocateMemoryFailed:
GetFileSizeFailed:
    CloseHandle(FileHandle);
CreateFileFailed:
    Win32_SystemError("Unable to open file");
    return (BufferData) { 0 };
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
            Nes_OnEmulatorToggleHalt(sWin32_StaticBuffer);
        } break;
        case 'R':
        {
            Nes_OnEmulatorReset(sWin32_StaticBuffer);
        } break;
        case 'F':
        {
            Nes_OnEmulatorSingleFrame(sWin32_StaticBuffer);
        } break;
        case 'P':
        {
            Nes_OnEmulatorTogglePalette(sWin32_StaticBuffer);
        } break;
        case VK_SPACE:
        {
            Nes_OnEmulatorSingleStep(sWin32_StaticBuffer);
        } break;
        }
    } break;
    case WM_COMMAND:
    {
        switch ((Win32_MenuOptions)LOWORD(WParam))
        {
        case WIN32_FILE_MENU_OPEN:
        {
            OPENFILENAMEA Prompt = Win32_CreateFileSelectionPrompt(0);
            if (GetOpenFileNameA(&Prompt))
            {
                BufferData Buffer = Win32_ReadFileSync(Prompt.lpstrFile, OPEN_EXISTING);
                if (Buffer.ViewPtr)
                {
                    const char *ErrorMessage = Nes_ParseINESFile(
                        sWin32_StaticBuffer,
                        Buffer.ViewPtr, 
                        Buffer.SizeBytes
                    );
                    if (ErrorMessage)
                    {
                        Win32_ErrorBox(ErrorMessage);
                    }
                    Win32_DeallocateMemory(Buffer.ViewPtr);
                }
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
    return true;
}

static void Win32_UpdateWindowTimer(HWND Window, UINT DontCare, UINT_PTR DontCare2, DWORD DontCare3)
{
    (void)Window, (void)DontCare, (void)DontCare2, (void)DontCare3;
    sWin32_DisplayableStatus = Nes_PlatformQueryDisplayableStatus(sWin32_StaticBuffer);
    sWin32_FrameBuffer = Nes_PlatformQueryFrameBuffer(sWin32_StaticBuffer);

    InvalidateRect(sWin32_Gui.StatusWindow, NULL, FALSE);
    InvalidateRect(sWin32_Gui.GameWindow, NULL, FALSE);
}


typedef struct AudioState
{
    IMMDevice* Device;
    IAudioClient* Client;
    IAudioRenderClient* Renderer;
    UINT32 BufferSizeFrame;
    HANDLE BufferRefillEvent;
    WAVEFORMATEX Format;
} AudioState;

static const char *Win32_InitializeAudio(int SamplesPerSec, int Channels, int BitsPerSample, int BufferSizeBytes)
{
#define SEC_TO_100NS(sec) (sec)*10000000

    int FrameSize = Channels * BitsPerSample / 8;
    AudioState Audio = { 
        .Format = {
            .wFormatTag = WAVE_FORMAT_PCM,
            .wBitsPerSample = BitsPerSample,
            .nChannels = Channels,
            .nSamplesPerSec = SamplesPerSec,
            .nAvgBytesPerSec = SamplesPerSec * FrameSize,
            .nBlockAlign = FrameSize,
        },
    };
    HRESULT hr;

    /* get audio device */
    {
        IMMDeviceEnumerator* DeviceEnumerator;
        hr = CoCreateInstance(&s_CLS_IMMDeviceEnumerator, 
            NULL, 
            CLSCTX_INPROC_SERVER, 
            &s_IID_IMMDeviceEnumerator, 
            (void **)&DeviceEnumerator
        ); 
        WIN32_NO_ERR(hr, "CoCreateInstance");

        hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(DeviceEnumerator, eRender, eMultimedia, &Audio.Device);
        WIN32_NO_ERR(hr, "GetDefaultAudioEndpoint");

        IMMDeviceEnumerator_Release(DeviceEnumerator);
    }

    /* audio client bs */
    hr = IMMDevice_Activate(
            Audio.Device, 
            &s_IID_IAudioClient, 
            CLSCTX_INPROC_SERVER, 
            NULL, 
            (void **)&Audio.Client
            );
    WIN32_NO_ERR(hr, "Device->Activate");


    /* setup sound sample */
    REFERENCE_TIME BufferDuration100ns = SEC_TO_100NS(BufferSizeBytes) / SamplesPerSec;
    hr = IAudioClient_Initialize(
            Audio.Client, 
            AUDCLNT_SHAREMODE_SHARED,   
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK, 
            BufferDuration100ns, 
            0, 
            &Audio.Format, 
            NULL
            );
    WIN32_NO_ERR(hr, "AudioClient->Initialize");


    /* create an event handler to fill the buffer when needed */
    Audio.BufferRefillEvent = CreateEventExA(NULL, NULL, 0, SYNCHRONIZE | EVENT_MODIFY_STATE);
    /* retarded function */
    DEBUG_ASSERT(Audio.BufferRefillEvent && Audio.BufferRefillEvent != INVALID_HANDLE_VALUE, 
            "CreateEventExA"
    );
    /* set the event */
    hr = IAudioClient_SetEventHandle(Audio.Client, Audio.BufferRefillEvent);
    WIN32_NO_ERR(hr, "SetEventHandle");


    /* get the audio render service */
    hr = IAudioClient_GetService(Audio.Client, &s_IID_IAudioRenderClient, &Audio.Renderer);
    WIN32_NO_ERR(hr, "GetService");


    /* get the buffer size (in frame count: channel * sampleSizeBytes */
    hr = IAudioClient_GetBufferSize(Audio.Client, &Audio.BufferSizeFrame);
    WIN32_NO_ERR(hr, "GetBufferSize (init)");
    {
        BYTE* Tmp;
        hr = IAudioRenderClient_GetBuffer(Audio.Renderer, Audio.BufferSizeFrame, &Tmp);
        WIN32_NO_ERR(hr, "GetBuffer (init)");

        /* clear the buffer to 0 */
        hr = IAudioRenderClient_ReleaseBuffer(Audio.Renderer, Audio.BufferSizeFrame, AUDCLNT_BUFFERFLAGS_SILENT);
        WIN32_NO_ERR(hr, "ReleaseBuffer (init)");
    }
    return Audio;
}





int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PCHAR CmdLine, int CmdShow)
{
    (void)PrevInstance, (void)CmdLine, (void)CmdShow;
    {
        INITCOMMONCONTROLSEX CommCtrl = {
            .dwICC = ICC_UPDOWN_CLASS,
            .dwSize = sizeof CommCtrl,
        };
        BOOL CommCtrlOk = InitCommonControlsEx(&CommCtrl);
        if (!CommCtrlOk)
        {
            Win32_SystemError("Warning: InitCommonControlsEx failed");
        }
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


    /* ask the game for the static buffer size, and then */
    /* allocate game memory up front, the platform owns it */
    sWin32_StaticBuffer.SizeBytes = Nes_RequestStaticBufferSize();
    sWin32_StaticBuffer.ViewPtr = Win32_AllocateMemory(sWin32_StaticBuffer.SizeBytes);
    if (NULL == sWin32_StaticBuffer.ViewPtr)
    {
        Win32_Fatal("Out of memory");
    }


    /* call the emulator's entry point */
    Platform_AudioConfig AudioConfig = Nes_OnEntry(sWin32_StaticBuffer);
    if (AudioConfig.EnableAudio)
    {
        const char *ErrorMessage = Win32_InitializeAudio(
            AudioConfig.SampleRate, 
            AudioConfig.ChannelCount, 
            16, 
            AudioConfig.BufferSizeBytes * AudioConfig.QueueSize
        );
        if (ErrorMessage)
        {
            Nes_OnAudioInitializationFailed(sWin32_StaticBuffer);
            Win32_ErrorBox(ErrorMessage);
        }
    }


    /* event loop */
    double TimeOrigin = Platform_GetTimeMillisec();
    while (Win32_PollInputs())
    {
        double Now = Platform_GetTimeMillisec();
        Nes_OnLoop(sWin32_StaticBuffer, Now - TimeOrigin);
    }


    /* exiting */
    Nes_AtExit(sWin32_StaticBuffer);


    /* don't need to clean up the window, windows does it faster than us */
    (void)sWin32_Gui.MainWindow;
    /* don't need to free the memory, windows does it faster than us */
    (void)sWin32_StaticBuffer;

    /* but we do need to cleanup audio devices */
    if (AudioConfig.EnableAudio)
        Win32_DestroyAudio();


    /* gracefully exits */
    ExitProcess(0);
}


double Platform_GetTimeMillisec(void)
{
    LARGE_INTEGER Now;
    QueryPerformanceCounter(&Now);
    return (double)Now.QuadPart * sWin32_TimerFrequency;
}

/* TODO: the GetKeyState function gets the physical state of the keyboard at any given moment, 
 * but we only care about the state of the keyboard when the game window is being focused */
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

