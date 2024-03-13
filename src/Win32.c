


#include "Common.h"
#include "Utils.h"

#include "Nes.c"

#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <wingdi.h>



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

typedef struct Win32_Audio
{
    Platform_AudioConfig Config;
    HWAVEOUT WaveOutHandle;
    WAVEFORMATEX Format;

    void *Buffer;
    WAVEHDR *Headers;
    u32 IndvBufferSize;
    i32 QueueSize;
    i32 ReadyBufferCount;

    volatile Bool8 ThreadShouldStop;
    volatile Bool8 ThreadTerminated;
    HANDLE ThreadHandle;
} Win32_Audio;

typedef struct Win32_BufferData 
{
    void *ViewPtr;
    isize SizeBytes;
} Win32_BufferData;



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
static Platform_ThreadContext sWin32_ThreadContext;

static Win32_Audio sWin32_Audio = { 0 };
static CRITICAL_SECTION sWin32_Audio_AccessingBufferCount;


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


static Win32_BufferData Win32_ReadFileSync(const char *FileName, DWORD dwCreationDisposition)
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
    return (Win32_BufferData){
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
    return (Win32_BufferData) { 0 };
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
            Nes_OnEmulatorToggleHalt(sWin32_ThreadContext);
        } break;
        case 'R':
        {
            Nes_OnEmulatorReset(sWin32_ThreadContext);
        } break;
        case 'F':
        {
            Nes_OnEmulatorSingleFrame(sWin32_ThreadContext);
        } break;
        case 'P':
        {
            Nes_OnEmulatorTogglePalette(sWin32_ThreadContext);
        } break;
        case VK_SPACE:
        {
            Nes_OnEmulatorSingleStep(sWin32_ThreadContext);
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
                Win32_BufferData Buffer = Win32_ReadFileSync(Prompt.lpstrFile, OPEN_EXISTING);
                if (Buffer.ViewPtr)
                {
                    const char *ErrorMessage = Nes_ParseINESFile(
                        sWin32_ThreadContext,
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
    while (PeekMessageA(&Msg, NULL, 0, 0, PM_REMOVE | PM_NOYIELD))
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
    sWin32_DisplayableStatus = Nes_PlatformQueryDisplayableStatus(sWin32_ThreadContext);
    sWin32_FrameBuffer = Nes_PlatformQueryFrameBuffer(sWin32_ThreadContext);

    InvalidateRect(sWin32_Gui.StatusWindow, NULL, FALSE);
    InvalidateRect(sWin32_Gui.GameWindow, NULL, FALSE);
}





static void CALLBACK Win32_WaveOutCallback(
    HWAVEOUT WaveOut, 
    UINT Msg, 
    DWORD_PTR UserData, 
    DWORD_PTR Unused, 
    DWORD_PTR Unused1)
{
    (void)WaveOut, (void)Msg, (void)UserData, (void)Unused, (void)Unused1;
    if (Msg != WOM_DONE)
        return;


    EnterCriticalSection(&sWin32_Audio_AccessingBufferCount);
    {
        sWin32_Audio.ReadyBufferCount++;
    }
    LeaveCriticalSection(&sWin32_Audio_AccessingBufferCount);
}

static DWORD Win32_AudioThread(void *UserData)
{
    (void)UserData;
    double t = 0;
    double dt = 1.0 / sWin32_Audio.Config.SampleRate;
    u8 *Buffer = sWin32_Audio.Buffer;
    WAVEHDR *Headers = sWin32_Audio.Headers;
    isize IndvBufferSize = sWin32_Audio.IndvBufferSize;
    isize IndvBufferFrameCount = sWin32_Audio.IndvBufferSize / sizeof(int16_t) / sWin32_Audio.Config.ChannelCount;
    isize QueueIndex = 0;

    while (!sWin32_Audio.ThreadShouldStop)
    {
        while (1)
        {
            /* don't optimize and cannot take address 
             * (ie no memory location, so it has to read into a register) */
            register volatile i32 ReadyBufferCount = 0;
            EnterCriticalSection(&sWin32_Audio_AccessingBufferCount);
            {
                ReadyBufferCount = sWin32_Audio.ReadyBufferCount;
            }
            LeaveCriticalSection(&sWin32_Audio_AccessingBufferCount);
            if (ReadyBufferCount > 0)
                break;
            Sleep(1);
        }
        do {
            i16 *SamplePtr = (i16*)&Buffer[
                QueueIndex * IndvBufferSize
            ];
            WAVEHDR *Header = &Headers[QueueIndex];
            Header->lpData = (LPSTR)SamplePtr;
            Header->dwBufferLength = IndvBufferSize;

            /* fill the audio buffer */
            for (u32 i = 0; i < IndvBufferFrameCount; i++)
            {
                i16 AudioSample = Nes_OnAudioSampleRequest(sWin32_ThreadContext, t);
                t += dt;
                for (u32 c = 0; c < sWin32_Audio.Config.ChannelCount; c++)
                {
                    *SamplePtr++ = AudioSample;
                }
            }

            /* update buffer count */
            EnterCriticalSection(&sWin32_Audio_AccessingBufferCount);
            {
                sWin32_Audio.ReadyBufferCount--;
            }
            LeaveCriticalSection(&sWin32_Audio_AccessingBufferCount);

            /* create the audio buffer for waveout */
            if (Header->dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(sWin32_Audio.WaveOutHandle, Header, sizeof *Header);
            waveOutPrepareHeader(sWin32_Audio.WaveOutHandle, Header, sizeof *Header);
            waveOutWrite(sWin32_Audio.WaveOutHandle, Header, sizeof *Header);

            QueueIndex++;
            if (QueueIndex == sWin32_Audio.QueueSize)
                QueueIndex = 0;
        } while (sWin32_Audio.ReadyBufferCount > 0);
    }
    sWin32_Audio.ThreadTerminated = true;
    return 0;
}


static void Win32_DestroyAudio(void)
{
    if (!sWin32_Audio.ThreadTerminated)
    {
        sWin32_Audio.ThreadShouldStop = true;
        WaitForSingleObject(sWin32_Audio.ThreadHandle, INFINITE);
        CloseHandle(sWin32_Audio.ThreadHandle);
    }


    DeleteCriticalSection(&sWin32_Audio_AccessingBufferCount);


    /* don't need to deallocate the buffer, windows does it faster than us */
    (void)sWin32_Audio.Buffer;
    waveOutClose(sWin32_Audio.WaveOutHandle);
}


/* the reason for using waveOut instead of other libraries like WASAPI 
 * is because waveOut is more reliable (because of how old it is), 
 * and because WASAPI works on my win10 machine but not my win11 laptop (waveOut works on both) */
static Bool8 Win32_InitializeAudio(Platform_AudioConfig Config, int BitsPerSample)
{
    sWin32_Audio.Config = Config;
    int BytesPerSample = BitsPerSample / 8;
    int BytesPerFrame = BytesPerSample * Config.ChannelCount;
    sWin32_Audio.Format = (WAVEFORMATEX) {
        .cbSize = 0,
        .nChannels = Config.ChannelCount,
        .wFormatTag = WAVE_FORMAT_PCM,
        .nBlockAlign = BytesPerFrame,
        .nSamplesPerSec = Config.SampleRate, 
        .wBitsPerSample = BitsPerSample,
        .nAvgBytesPerSec = BytesPerSample * Config.SampleRate,
    };

    /* create waveout */
    MMRESULT ErrorCode = waveOutOpen(
        &sWin32_Audio.WaveOutHandle, 
        WAVE_MAPPER, 
        &sWin32_Audio.Format, 
        (DWORD_PTR)Win32_WaveOutCallback, 
        0, 
        CALLBACK_FUNCTION
    );
    if (MMSYSERR_NOERROR != ErrorCode)
        goto WaveOutOpenFailed;


    /* allocate memory for the sound buffer */
    isize TotalBufferSizeBytes = 
        (Config.BufferSizeBytes + sizeof(WAVEHDR)) * Config.BufferQueueSize;
    u8 *AudioBuffer = Win32_AllocateMemory(TotalBufferSizeBytes);
    if (NULL == AudioBuffer)
        goto MemoryAllocationFailed;


    sWin32_Audio.Buffer = AudioBuffer;
    sWin32_Audio.Headers = (WAVEHDR*)(AudioBuffer + Config.BufferSizeBytes * Config.BufferQueueSize);
    sWin32_Audio.IndvBufferSize = Config.BufferSizeBytes;
    sWin32_Audio.QueueSize = Config.BufferQueueSize;
    sWin32_Audio.ReadyBufferCount = Config.BufferQueueSize;


    InitializeCriticalSection(&sWin32_Audio_AccessingBufferCount);


    /* create a thread handle */
    sWin32_Audio.ThreadShouldStop = false;
    sWin32_Audio.ThreadTerminated = false;
    sWin32_Audio.ThreadHandle = CreateThread(NULL, 0, Win32_AudioThread, NULL, 0, NULL);
    if (NULL == sWin32_Audio.ThreadHandle)
        goto FailedToCreateAudioThread;

    return true;

FailedToCreateAudioThread:
    sWin32_Audio.ThreadTerminated = true;
    sWin32_Audio.ThreadHandle = NULL;
    /* don't want a buffer lingering around during runtime */
    Win32_DeallocateMemory(sWin32_Audio.Buffer);
    sWin32_Audio.Buffer = NULL;
MemoryAllocationFailed:
    waveOutClose(sWin32_Audio.WaveOutHandle);
WaveOutOpenFailed:
    return false;
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
    sWin32_ThreadContext.SizeBytes = Nes_PlatformQueryThreadContextSize();
    sWin32_ThreadContext.ViewPtr = Win32_AllocateMemory(sWin32_ThreadContext.SizeBytes);
    if (NULL == sWin32_ThreadContext.ViewPtr)
    {
        Win32_Fatal("Out of memory");
    }


    /* call the emulator's entry point */
    Platform_AudioConfig AudioConfig = Nes_OnEntry(sWin32_ThreadContext);
    Bool8 HasAudio = false;
    if (AudioConfig.EnableAudio)
    {
        HasAudio = Win32_InitializeAudio(AudioConfig, 16);
        if (!HasAudio)
        {
            Nes_OnAudioFailed(sWin32_ThreadContext);
            Win32_ErrorBox("Unable to initialize audio.");
        }
    }


    /* event loop */
    double TimeOrigin = Platform_GetTimeMillisec();
    while (Win32_PollInputs())
    {
        double Now = Platform_GetTimeMillisec();
        Nes_OnLoop(sWin32_ThreadContext, Now - TimeOrigin);
        //Sleep(1);
    }


    /* don't need to clean up the window, windows does it faster than us */
    (void)sWin32_Gui.MainWindow;
    /* don't need to free the memory, windows does it faster than us */
    (void)sWin32_ThreadContext;

    /* but we do need to cleanup audio devices */
    if (AudioConfig.EnableAudio && HasAudio)
    {
        Win32_DestroyAudio();
    }

    /* exiting */
    Nes_AtExit(sWin32_ThreadContext);


    /* gracefully exits */
    ExitProcess(0);
}


double Platform_GetTimeMillisec(void)
{
    LARGE_INTEGER Now;
    QueryPerformanceCounter(&Now);
    return (double)Now.QuadPart * sWin32_TimerFrequency;
}

Nes_ControllerStatus Platform_GetControllerState(void)
{
    u16 Left = GetAsyncKeyState('A') < 0 || GetAsyncKeyState(VK_LEFT) < 0; 
    u16 Right = GetAsyncKeyState('D') < 0 || GetAsyncKeyState(VK_RIGHT) < 0;
    u16 Up = GetAsyncKeyState('W') < 0 || GetAsyncKeyState(VK_UP) < 0;
    u16 Down = GetAsyncKeyState('S') < 0 || GetAsyncKeyState(VK_DOWN) < 0;
    u16 Start = GetAsyncKeyState(VK_RETURN) < 0;
    u16 Select = GetAsyncKeyState(VK_TAB) < 0;
    u16 A = GetAsyncKeyState('K') < 0;
    u16 B = GetAsyncKeyState('J') < 0;

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

