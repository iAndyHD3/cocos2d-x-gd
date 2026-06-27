/****************************************************************************
Copyright (c) 2010 cocos2d-x.org

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "CCEGLView.h"
#include "cocoa/CCSet.h"
#include "ccMacros.h"
#include "CCDirector.h"
#include "touch_dispatcher/CCTouch.h"
#include "touch_dispatcher/CCTouchDispatcher.h"
#include "text_input_node/CCIMEDispatcher.h"
#include "keypad_dispatcher/CCKeypadDispatcher.h"
#include "support/CCPointExtension.h"
#include "CCApplication.h"
#include "robtop/keyboard_dispatcher/CCKeyboardDispatcher.h"
#include "robtop/mouse_dispatcher/CCMouseDispatcher.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include "robtop/glfw/glfw3native.h"

#include <mutex>
#include <deque>
#include <unordered_set>
#include <thread>
#include <process.h>

NS_CC_BEGIN

#pragma pack(push, 1)
struct RawInputEvent {
    uint8_t type;
    uint8_t pad0[7];
    double timestamp;
    union {
        struct {
            int keyCode;
            uint8_t flags;
            uint8_t isRepeat;
            uint16_t pad2;
            int unknown;
        } keyboard;
        struct {
            int touchID;
            uint16_t isDown;
            uint16_t pad3;
            int buttonType;
        } mouse;
    };
};
#pragma pack(pop)

static bool g_bInputCaptureEnabled = false;
static bool g_bRawInputInited = false;
static uintptr_t g_uRawInputThread = 0;
static uint32_t g_nRawInputThreadId = 0;
static HWND g_hMainWindow = NULL;
static HWND g_hRawInputWnd = NULL;
static std::mutex g_queueMutex;
static std::deque<RawInputEvent> g_eventQueue;
static std::unordered_set<int> g_keyDownSet;

static LRESULT CALLBACK rawInputWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_INPUT)
    {
        RAWINPUT raw;
        UINT cbSize = sizeof(raw);
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &raw, &cbSize, sizeof(RAWINPUTHEADER));

        if (raw.header.dwType == RIM_TYPEKEYBOARD)
        {
            USHORT vkey = raw.data.keyboard.VKey;
            UINT flags = raw.data.keyboard.Flags;
            CCLog("rawInput: WM_INPUT KEY vkey=%d flags=0x%X", vkey, flags);
            int convKey = vkey;
            if (vkey >= VK_NUMPAD0 && vkey <= VK_NUMPAD9)
                convKey = vkey - (VK_NUMPAD0 - 0x30);

            bool isDown = !(flags & RI_KEY_BREAK);

            LARGE_INTEGER freq, count;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&count);
            double timestamp = (double)(int)count.LowPart / (double)(int)freq.LowPart;

            bool isRepeat = false;
            if (isDown)
            {
                isRepeat = (g_keyDownSet.find(convKey) != g_keyDownSet.end());
                g_keyDownSet.insert(convKey);
            }
            else
            {
                g_keyDownSet.erase(convKey);
            }

            RawInputEvent ev = {};
            ev.type = 0;
            ev.timestamp = timestamp;
            ev.keyboard.keyCode = convKey;
            ev.keyboard.flags = isDown;
            ev.keyboard.isRepeat = isRepeat;
            ev.keyboard.unknown = -1;

            std::lock_guard<std::mutex> lock(g_queueMutex);
            g_eventQueue.push_back(ev);
        }
        else if (raw.header.dwType == RIM_TYPEMOUSE)
        {
            USHORT btnFlags = raw.data.mouse.usButtonFlags;
            LARGE_INTEGER freq, count;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&count);
            double timestamp = (double)(int)count.LowPart / (double)(int)freq.LowPart;
            int touchID = 0;

            if (btnFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
            {
                RawInputEvent ev = {};
                ev.type = 1;
                ev.timestamp = timestamp;
                ev.mouse.touchID = touchID;
                ev.mouse.isDown = 1;
                ev.mouse.buttonType = 0;
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_eventQueue.push_back(ev);
            }
            if (btnFlags & RI_MOUSE_LEFT_BUTTON_UP)
            {
                RawInputEvent ev = {};
                ev.type = 1;
                ev.timestamp = timestamp;
                ev.mouse.touchID = touchID;
                ev.mouse.isDown = 0;
                ev.mouse.buttonType = 0;
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_eventQueue.push_back(ev);
            }
            if (btnFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
            {
                RawInputEvent ev = {};
                ev.type = 1;
                ev.timestamp = timestamp;
                ev.mouse.touchID = touchID;
                ev.mouse.isDown = 1;
                ev.mouse.buttonType = 1;
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_eventQueue.push_back(ev);
            }
            if (btnFlags & RI_MOUSE_RIGHT_BUTTON_UP)
            {
                RawInputEvent ev = {};
                ev.type = 1;
                ev.timestamp = timestamp;
                ev.mouse.touchID = touchID;
                ev.mouse.isDown = 0;
                ev.mouse.buttonType = 1;
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_eventQueue.push_back(ev);
            }
            if (btnFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN)
            {
                RawInputEvent ev = {};
                ev.type = 1;
                ev.timestamp = timestamp;
                ev.mouse.touchID = touchID;
                ev.mouse.isDown = 1;
                ev.mouse.buttonType = 2;
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_eventQueue.push_back(ev);
            }
            if (btnFlags & RI_MOUSE_MIDDLE_BUTTON_UP)
            {
                RawInputEvent ev = {};
                ev.type = 1;
                ev.timestamp = timestamp;
                ev.mouse.touchID = touchID;
                ev.mouse.isDown = 0;
                ev.mouse.buttonType = 2;
                std::lock_guard<std::mutex> lock(g_queueMutex);
                g_eventQueue.push_back(ev);
            }
        }
        return 0;
    }
    return DefWindowProcA(hWnd, Msg, wParam, lParam);
}

static unsigned __stdcall rawInputThread(void* arg)
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = rawInputWndProc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "GD_RawInput";
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0, "GD_RawInput", "GD Raw Input Window", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, wc.hInstance, 0);
    if (!hWnd)
    {
        CCLog("rawInputThread: CreateWindowExA failed");
        return 0;
    }

    g_hRawInputWnd = hWnd;

    RAWINPUTDEVICE rid[2] = {};
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x06;
    rid[0].dwFlags = RIDEV_INPUTSINK;
    rid[0].hwndTarget = hWnd;
    rid[1].usUsagePage = 0x01;
    rid[1].usUsage = 0x02;
    rid[1].dwFlags = RIDEV_INPUTSINK;
    rid[1].hwndTarget = hWnd;

    if (!RegisterRawInputDevices(rid, 2, sizeof(RAWINPUTDEVICE)))
    {
        CCLog("rawInputThread: RegisterRawInputDevices failed");
        g_hRawInputWnd = NULL;
        DestroyWindow(hWnd);
        return 0;
    }

    CCLog("rawInputThread: started successfully");

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    MSG msg;
    while (GetMessageA(&msg, hWnd, 0, 0) > 0)
        DispatchMessageA(&msg);

    DestroyWindow(hWnd);
    g_hRawInputWnd = NULL;
    return 0;
}

static bool glew_dynamic_binding()
{
	const char *gl_extensions = (const char*)glGetString(GL_EXTENSIONS);

	// If the current opengl driver doesn't have framebuffers methods, check if an extension exists
	if (glGenFramebuffers == NULL)
	{
		CCLog("OpenGL: glGenFramebuffers is NULL, try to detect an extension");
		if (strstr(gl_extensions, "ARB_framebuffer_object"))
		{
			CCLog("OpenGL: ARB_framebuffer_object is supported");

			glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC) wglGetProcAddress("glIsRenderbuffer");
			glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) wglGetProcAddress("glBindRenderbuffer");
			glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) wglGetProcAddress("glDeleteRenderbuffers");
			glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) wglGetProcAddress("glGenRenderbuffers");
			glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) wglGetProcAddress("glRenderbufferStorage");
			glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) wglGetProcAddress("glGetRenderbufferParameteriv");
			glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC) wglGetProcAddress("glIsFramebuffer");
			glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) wglGetProcAddress("glBindFramebuffer");
			glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) wglGetProcAddress("glDeleteFramebuffers");
			glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) wglGetProcAddress("glGenFramebuffers");
			glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) wglGetProcAddress("glCheckFramebufferStatus");
			glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC) wglGetProcAddress("glFramebufferTexture1D");
			glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) wglGetProcAddress("glFramebufferTexture2D");
			glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC) wglGetProcAddress("glFramebufferTexture3D");
			glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) wglGetProcAddress("glFramebufferRenderbuffer");
			glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) wglGetProcAddress("glGetFramebufferAttachmentParameteriv");
			glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) wglGetProcAddress("glGenerateMipmap");
		}
		else
		if (strstr(gl_extensions, "EXT_framebuffer_object"))
		{
			CCLog("OpenGL: EXT_framebuffer_object is supported");
			glIsRenderbuffer = (PFNGLISRENDERBUFFERPROC) wglGetProcAddress("glIsRenderbufferEXT");
			glBindRenderbuffer = (PFNGLBINDRENDERBUFFERPROC) wglGetProcAddress("glBindRenderbufferEXT");
			glDeleteRenderbuffers = (PFNGLDELETERENDERBUFFERSPROC) wglGetProcAddress("glDeleteRenderbuffersEXT");
			glGenRenderbuffers = (PFNGLGENRENDERBUFFERSPROC) wglGetProcAddress("glGenRenderbuffersEXT");
			glRenderbufferStorage = (PFNGLRENDERBUFFERSTORAGEPROC) wglGetProcAddress("glRenderbufferStorageEXT");
			glGetRenderbufferParameteriv = (PFNGLGETRENDERBUFFERPARAMETERIVPROC) wglGetProcAddress("glGetRenderbufferParameterivEXT");
			glIsFramebuffer = (PFNGLISFRAMEBUFFERPROC) wglGetProcAddress("glIsFramebufferEXT");
			glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC) wglGetProcAddress("glBindFramebufferEXT");
			glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC) wglGetProcAddress("glDeleteFramebuffersEXT");
			glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC) wglGetProcAddress("glGenFramebuffersEXT");
			glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC) wglGetProcAddress("glCheckFramebufferStatusEXT");
			glFramebufferTexture1D = (PFNGLFRAMEBUFFERTEXTURE1DPROC) wglGetProcAddress("glFramebufferTexture1DEXT");
			glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC) wglGetProcAddress("glFramebufferTexture2DEXT");
			glFramebufferTexture3D = (PFNGLFRAMEBUFFERTEXTURE3DPROC) wglGetProcAddress("glFramebufferTexture3DEXT");
			glFramebufferRenderbuffer = (PFNGLFRAMEBUFFERRENDERBUFFERPROC) wglGetProcAddress("glFramebufferRenderbufferEXT");
			glGetFramebufferAttachmentParameteriv = (PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC) wglGetProcAddress("glGetFramebufferAttachmentParameterivEXT");
			glGenerateMipmap = (PFNGLGENERATEMIPMAPPROC) wglGetProcAddress("glGenerateMipmapEXT");
		}
		else
		{
			CCLog("OpenGL: No framebuffers extension is supported");
			CCLog("OpenGL: Any call to Fbo will crash!");
			return false;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
// impliment CCEGLView
//////////////////////////////////////////////////////////////////////////

CCEGLView::CCEGLView()
: m_bCaptured(false)
, m_pMainWindow(NULL)
, m_pPrimaryMonitor(NULL)
, m_fFrameZoomFactor(1.0f)
, m_bSupportTouch(false)
{
    strcpy(m_szViewName, "Cocos2dxWin32");
}

CCEGLView::~CCEGLView()
{
    g_bRawInputInited = false;
    if (g_hRawInputWnd)
        PostMessageA(g_hRawInputWnd, WM_QUIT, 0, 0);
    if (g_nRawInputThreadId)
    {
        if (g_nRawInputThreadId != GetThreadId(GetCurrentThread()))
        {
            WaitForSingleObject((HANDLE)g_uRawInputThread, INFINITE);
            CloseHandle((HANDLE)g_uRawInputThread);
        }
        g_uRawInputThread = 0;
        g_nRawInputThreadId = 0;
    }
    g_hRawInputWnd = NULL;
}

bool CCEGLView::isOpenGLReady()
{
    return !!m_pMainWindow;
}

void CCEGLView::end()
{
    if (m_pMainWindow) {
        glfwDestroyWindow(m_pMainWindow);
        m_pMainWindow = NULL;
    }
    // glfwTerminate();
    delete this;
}

void CCEGLView::swapBuffers()
{
    if (m_pMainWindow) {
        glfwSwapBuffers(m_pMainWindow);
    }
}


void CCEGLView::setIMEKeyboardState(bool /*bOpen*/)
{

}

void CCEGLView::resizeWindow(int width, int height)
{
    if (m_pMainWindow) {
        glfwSetWindowSize(m_pMainWindow, width, height);
    }
}

void CCEGLView::setFrameZoomFactor(float fZoomFactor)
{
    m_fFrameZoomFactor = fZoomFactor;
    // resize(m_obScreenSize.width * fZoomFactor, m_obScreenSize.height * fZoomFactor);
    // centerWindow();
    // CCDirector::sharedDirector()->setProjection(CCDirector::sharedDirector()->getProjection());
    updateFrameSize();
}

float CCEGLView::getFrameZoomFactor()
{
    return m_fFrameZoomFactor;
}

void CCEGLView::setFrameSize(float width, float height)
{
    CCEGLViewProtocol::setFrameSize(width, height);

    updateFrameSize();
}

void CCEGLView::centerWindow()
{
    if (!m_pMainWindow) {
        return;
    }

    glfwSetWindowPos(m_pMainWindow, 10, 10);
}

void CCEGLView::setViewPortInPoints(float x , float y , float w , float h)
{
    glViewport((GLint)(x * m_fScaleX * m_fFrameZoomFactor + m_obViewPortRect.origin.x * m_fFrameZoomFactor),
        (GLint)(y * m_fScaleY  * m_fFrameZoomFactor + m_obViewPortRect.origin.y * m_fFrameZoomFactor),
        (GLsizei)(w * m_fScaleX * m_fFrameZoomFactor),
        (GLsizei)(h * m_fScaleY * m_fFrameZoomFactor));
}

void CCEGLView::setScissorInPoints(float x , float y , float w , float h)
{
    glScissor((GLint)(x * m_fScaleX * m_fFrameZoomFactor + m_obViewPortRect.origin.x * m_fFrameZoomFactor),
              (GLint)(y * m_fScaleY * m_fFrameZoomFactor + m_obViewPortRect.origin.y * m_fFrameZoomFactor),
              (GLsizei)(w * m_fScaleX * m_fFrameZoomFactor),
              (GLsizei)(h * m_fScaleY * m_fFrameZoomFactor));
}

CCEGLView* CCEGLView::sharedOpenGLView() {
    return CCDirector::get()->getOpenGLView();
}

CCEGLView* CCEGLView::create(const std::string& name) {
    return createWithRect(name, {0, 0, 960, 640}, 1.0f);
}

CCEGLView* CCEGLView::createWithRect(const std::string& name, cocos2d::CCRect rect, float unk) {
    auto view = new CCEGLView();
    view->initWithRect(name, rect, unk);
    return view;
}

bool CCEGLView::initWithRect(const std::string& name, cocos2d::CCRect rect, float unk) {
    if (!glfwInit()) {
        return false;
    }

    m_pMainWindow = glfwCreateWindow(rect.size.width, rect.size.height, name.c_str(), NULL, NULL);
    if (!m_pMainWindow) {
        return false;
    }
    this->setFrameSize(rect.size.width, rect.size.height);
    this->setFrameZoomFactor(unk);

    glfwMakeContextCurrent(m_pMainWindow);

    g_hMainWindow = glfwGetWin32Window(m_pMainWindow);
    g_bInputCaptureEnabled = true;

    if (!g_bRawInputInited)
    {
        g_bRawInputInited = true;
        unsigned int thrdAddr;
        g_uRawInputThread = _beginthreadex(0, 0, rawInputThread, &g_hMainWindow, 0, &thrdAddr);
        g_nRawInputThreadId = thrdAddr;
    }

    glfwSetCharCallback(m_pMainWindow, [](GLFWwindow* window, unsigned int codepoint) {
        CCEGLView::sharedOpenGLView()->onGLFWCharCallback(window, codepoint);
    });
    glfwSetMouseButtonCallback(m_pMainWindow, [](GLFWwindow* window, int button, int action, int mods) {
        CCEGLView::sharedOpenGLView()->onGLFWMouseCallBack(window, button, action, mods);
    });
    glfwSetCursorPosCallback(m_pMainWindow, [](GLFWwindow* window, double x, double y) {
        CCEGLView::sharedOpenGLView()->onGLFWMouseMoveCallBack(window, x, y);
    });

    this->initGlew();

    return true;
}

void CCEGLView::onGLFWKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {

}

void CCEGLView::onGLFWCharCallback(GLFWwindow* window, unsigned int codepoint) {
    if (codepoint == 8)
    {
        CCIMEDispatcher::sharedDispatcher()->dispatchDeleteBackward();
    }
    else
    {
        char buf[4] = {};
        int len = 0;
        if (codepoint <= 0x7F) {
            buf[0] = (char)codepoint;
            len = 1;
        } else if (codepoint <= 0x7FF) {
            buf[0] = 0xC0 | (codepoint >> 6);
            buf[1] = 0x80 | (codepoint & 0x3F);
            len = 2;
        } else if (codepoint <= 0xFFFF) {
            buf[0] = 0xE0 | (codepoint >> 12);
            buf[1] = 0x80 | ((codepoint >> 6) & 0x3F);
            buf[2] = 0x80 | (codepoint & 0x3F);
            len = 3;
        } else {
            buf[0] = 0xF0 | (codepoint >> 18);
            buf[1] = 0x80 | ((codepoint >> 12) & 0x3F);
            buf[2] = 0x80 | ((codepoint >> 6) & 0x3F);
            buf[3] = 0x80 | (codepoint & 0x3F);
            len = 4;
        }
        CCIMEDispatcher::sharedDispatcher()->dispatchInsertText(buf, len, KEY_Unknown);
    }
}

void CCEGLView::pumpRawInput() {
    HWND hMainWnd = g_hMainWindow;
    bool mouseInWindow = false;
    if (g_bInputCaptureEnabled && hMainWnd)
    {
        POINT pt;
        if (GetCursorPos(&pt))
        {
            HWND wnd = WindowFromPoint(pt);
            if (wnd && GetAncestor(wnd, GA_ROOT) == hMainWnd)
                mouseInWindow = true;
        }
    }

    if (mouseInWindow)
    {
        POINT pt;
        if (GetCursorPos(&pt) && ScreenToClient(hMainWnd, &pt))
        {
            m_fMouseX = (float)pt.x;
            m_fMouseY = (float)pt.y;
        }
        m_fMouseX /= m_fFrameZoomFactor;
        m_fMouseY /= m_fFrameZoomFactor;
    }

    std::deque<RawInputEvent> localQueue;
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        localQueue.swap(g_eventQueue);
    }

    if (!localQueue.empty())
        CCLog("pumpRawInput: draining %zu events", localQueue.size());

    for (const auto& ev : localQueue)
    {
        if (ev.type == 0 && g_bInputCaptureEnabled)
        {
            CCLog("pumpRawInput: KEY event keyCode=%d flags=%d repeat=%d", ev.keyboard.keyCode, ev.keyboard.flags, ev.keyboard.isRepeat);
            enumKeyCodes key = CCKeyboardDispatcher::convertKeyCode((enumKeyCodes)ev.keyboard.keyCode);
            bool isShift = GetKeyState(VK_SHIFT) < 0;
            bool isCtrl = GetKeyState(VK_CONTROL) < 0;
            bool isAlt = GetKeyState(VK_MENU) < 0;

            auto* dir = CCDirector::sharedDirector();
            auto* dispatcher = dir->getKeyboardDispatcher();
            dispatcher->updateModifierKeys(isShift, isCtrl, isAlt, false);

            bool isKeyDown = ev.keyboard.flags || ev.keyboard.isRepeat;

            CCIMEDispatcher* ime = CCIMEDispatcher::sharedDispatcher();
            bool hasImeDelegate = ime->hasDelegate();

            if (hasImeDelegate && isKeyDown)
            {
                if (key == KEY_Left || key == KEY_Right)
                {
                    char a = 'a';
                    ime->dispatchInsertText(&a, 1, key);
                }
                else if (key == KEY_Delete)
                {
                    ime->dispatchDeleteForward();
                }
            }

            if (ev.keyboard.flags && ev.keyboard.keyCode == 8)
            {
                ime->dispatchDeleteBackward();
            }

            if (!hasImeDelegate || key == KEY_Escape || key == KEY_Enter)
                dispatcher->dispatchKeyboardMSG(key, isKeyDown, ev.keyboard.isRepeat, 0.0);

            if (ev.keyboard.flags && !ev.keyboard.isRepeat && key == KEY_V && isCtrl)
            {
                // clipboard paste flag
            }
        }
        else if (ev.type == 1 && mouseInWindow)
        {
            if (ev.mouse.isDown)
            {
                m_bCaptured = true;
                int touchID = 0;
                handleTouchesBegin(1, &touchID, &m_fMouseX, &m_fMouseY, ev.timestamp);
            }
            else
            {
                if (m_bCaptured)
                {
                    m_bCaptured = false;
                    int touchID = 0;
                    handleTouchesEnd(1, &touchID, &m_fMouseX, &m_fMouseY, ev.timestamp);
                }
            }
        }
    }
}

void CCEGLView::onGLFWMouseCallBack(GLFWwindow* window, int button, int action, int mods) {
    // auto* dir = CCDirector::sharedDirector();
    // auto* mouse = dir->getMouseDispatcher();

    // TODO: some stuff with set window pos??

    int touchID = 0;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            m_bCaptured = true;
            // TODO: mouse locked stuff
            this->handleTouchesBegin(1, &touchID, &m_fMouseX, &m_fMouseY, 0.0);
        } else if (action == GLFW_RELEASE) {
            if (m_bCaptured) {
                m_bCaptured = false;
                this->handleTouchesEnd(1, &touchID, &m_fMouseX, &m_fMouseY, 0.0);
            }
        }
    }
}

void CCEGLView::onGLFWMouseMoveCallBack(GLFWwindow* window, double x, double y) {
    float fx = x;
    float fy = y;
    int touchID = 0;

    m_fMouseX = x;
    m_fMouseY = y;

    if (m_bCaptured) {
        this->handleTouchesMove(1, &touchID, &fx, &fy, 0.0);
    }
}

bool CCEGLView::initGlew() {
    if (glewInit() != GLEW_OK) {
        return false;
    }

    if (!glew_dynamic_binding()) {
        return false;
    }

    return true;
}

void CCEGLView::updateFrameSize() {
    if (m_pMainWindow) {
        glfwSetWindowSize(m_pMainWindow, m_obScreenSize.width, m_obScreenSize.height);
    }
}

bool CCEGLView::windowShouldClose() {
    return m_pMainWindow ? glfwWindowShouldClose(m_pMainWindow) : true;
}

void CCEGLView::pollEvents() {
    double x, y;
    glfwGetCursorPos(m_pMainWindow, &x, &y);
    m_fMouseX = x;
    m_fMouseY = y;

    glfwPollEvents();
}

CCEGLView* CCEGLView::createWithFullScreen(std::string const& name, bool borderless, bool fix) {
    return CCEGLView::create(name);
}
CCSize CCEGLView::getDisplaySize() {
    auto hwnd = glfwGetWin32Window(m_pMainWindow);
    auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFOEX mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(monitor, &mi);
    float width = mi.rcMonitor.right - mi.rcMonitor.left;
    float height = mi.rcMonitor.bottom - mi.rcMonitor.top;
    return {width, height};
}
void CCEGLView::makeBorderlessTop() {
    ROB_UNIMPLEMENTED();
}
void CCEGLView::showCursor(bool show) {
    // ROB_UNIMPLEMENTED();
}
void CCEGLView::toggleFullScreen(bool, bool, bool) {
    ROB_UNIMPLEMENTED();
}
void CCEGLView::toggleLockCursor(bool) {
    ROB_UNIMPLEMENTED();
}
void CCEGLView::iconify() {
    ROB_UNIMPLEMENTED();
}

bool CCEGLView::getShouldHideCursor() const {
    return m_bShouldHideCursor;
}
bool CCEGLView::getCursorLocked() const {
    return m_bCursorLocked;
}
GLFWwindow* CCEGLView::getWindow() const {
    return m_pMainWindow;
}


NS_CC_END
