#ifndef MC6502_H
#define MC6502_H


#include "Common.h"

typedef struct MC6502 MC6502;
struct MC6502 
{
    u8 A, X, Y;
    u8 SP;
    u8 Flags;
    u16 PC;
};


#endif /* MC6502_H */



#ifdef MC6502_IMPLEMENTATION

#ifdef STANDALONE
#   undef STANDALONE
#   include <windows.h>
#   include <commctrl.h>
#   define DISASSEMBLER_IMPLEMENTATION
#       include "Disassembler.h"
#   undef DISASSEMBLER_IMPLEMENTATION

static HWND sWin32_MainWindow;
static WNDCLASSEXA sWin32_WindowClass;
static HFONT sWin32_FontHandle;


static int CStringLength(const char *CString)
{
    int Length = 0;
    while (*CString++)
        Length++;
    return Length;
}

static void MemCopy(void *Dst, const void *Src, size_t SizeBytes)
{
    uint8_t *DstPtr = Dst;
    const uint8_t *SrcPtr = Src;
    while (SizeBytes--)
    {
        *DstPtr++ = *SrcPtr++;
    }
}


static void Platform_FatalError(const char *Message)
{
    MessageBoxA(NULL, Message, "Fatal Error", MB_ICONERROR);
    ExitProcess(1);
}


static Bool8 Win32_LoadFont(int Width, int Height, int Weight)
{
    LOGFONTA FontInfo = {
        .lfHeight = Height,
        .lfWidth = Width,
        .lfWeight = Weight,
        .lfCharSet = OEM_CHARSET, 
        .lfOutPrecision = OUT_DEVICE_PRECIS,
        .lfClipPrecision = CLIP_DEFAULT_PRECIS, 
        .lfQuality = DEFAULT_QUALITY,
        .lfPitchAndFamily = DEFAULT_PITCH, 
        .lfFaceName = "Terminal",
    };

    HFONT FontHandle = CreateFontIndirect(&FontInfo);
    if (NULL == FontHandle)
        return false;

    if (NULL != sWin32_FontHandle)
        DeleteObject(sWin32_FontHandle);
    sWin32_FontHandle = FontHandle;
    return true;
}



static LRESULT CALLBACK Win32_WndProc(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam)
{
    switch (Msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
    } break;
    case WM_PAINT:
    {
        PAINTSTRUCT PaintStruct;
        HDC DeviceContext = BeginPaint(Window, &PaintStruct);

        static char Text[] = "Hello, world";
        HFONT Old = SelectObject(DeviceContext, sWin32_FontHandle);
        DrawTextA(DeviceContext, Text, sizeof Text, &PaintStruct.rcPaint, DT_WORDBREAK);
        SelectObject(DeviceContext, Old);

        DeleteDC(DeviceContext);
        EndPaint(Window, &PaintStruct);
    } break;
    default: 
    {
        return DefWindowProcA(Window, Msg, WParam, LParam);
    } break;
    }
    return 0;
}


static Bool8 Win32_PollInputs(void)
{
    MSG Msg;
    while (PeekMessageA(&Msg, 0, 0, 0, PM_REMOVE))
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
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    sWin32_WindowClass = (WNDCLASSEXA){
        .style = CS_HREDRAW | CS_VREDRAW,
        .cbSize = sizeof sWin32_WindowClass,
        .hInstance = Instance,
        .lpfnWndProc = Win32_WndProc,
        .lpszClassName = "Invader", 
        .hCursor = LoadCursorA(NULL, IDC_ARROW),
        .hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH),
    };
    RegisterClassExA(&sWin32_WindowClass);

    Win32_LoadFont(-8, -12, FW_BOLD);

    sWin32_MainWindow = CreateWindowExA(
        WS_EX_CLIENTEDGE,
        sWin32_WindowClass.lpszClassName,
        "Debugger",
        WS_OVERLAPPEDWINDOW | WS_BORDER | WS_CAPTION,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1080, 720,
        NULL, NULL, NULL, NULL
    );
    if (NULL == sWin32_MainWindow)
    {
        Platform_FatalError("Cannot create window");
    }
    ShowWindow(sWin32_MainWindow, SW_SHOW);

    while (Win32_PollInputs())
    {
    }

    /* don't need to cleanup window */
    return 0;
}
#  endif /* STANDALONE */
#endif /* MC6502_IMPLEMENTATION */

