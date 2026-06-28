#include "CCKeyboardDispatcher.h"

namespace cocos2d {

CCKeyboardDispatcher::CCKeyboardDispatcher() {
    m_pHandlers = CCArray::create();
    m_pHandlers->retain();
    m_bDispatching = false;
    m_bDeferredAdd = false;
    m_bDeferredRemove = false;
    m_pDeferredAddDelegates = ccCArrayNew(8);
    m_pDeferredRemoveDelegates = ccCArrayNew(8);
    m_bShiftPressed = false;
    m_bControlPressed = false;
    m_bAltPressed = false;
    m_bCommandPressed = false;
    m_bBlockRepeat = false;
}
CCKeyboardDispatcher::~CCKeyboardDispatcher() {
    CC_SAFE_RELEASE(m_pHandlers);
    ccCArrayFree(m_pDeferredAddDelegates);
    ccCArrayFree(m_pDeferredRemoveDelegates);
}

void CCKeyboardDispatcher::addDelegate(CCKeyboardDelegate* pDelegate) {
    CCLOG("Adding delegate: %p", pDelegate);
    if (m_bDispatching) {
        ccCArrayAppendValue(m_pDeferredAddDelegates, pDelegate);
        m_bDeferredAdd = true;
    } else {
        forceAddDelegate(pDelegate);
    }
}
void CCKeyboardDispatcher::removeDelegate(CCKeyboardDelegate* pDelegate) {
    if (m_bDispatching) {
        ccCArrayAppendValue(m_pDeferredRemoveDelegates, pDelegate);
        m_bDeferredRemove = true;
    } else {
        forceRemoveDelegate(pDelegate);
    }
}

void CCKeyboardDispatcher::forceAddDelegate(CCKeyboardDelegate* pDelegate) {
    CCLOG("Forcing addition of delegate: %p", pDelegate);
    m_pHandlers->addObject(CCKeyboardHandler::handlerWithDelegate(pDelegate));
}
void CCKeyboardDispatcher::forceRemoveDelegate(CCKeyboardDelegate* pDelegate) {
    CCLOG("Forcing removal of delegate: %p", pDelegate);
    CCKeyboardHandler* pHandler = nullptr;
    CCObject* pObj = nullptr;
    CCARRAY_FOREACH(m_pHandlers, pObj) {
        pHandler = (CCKeyboardHandler*)pObj;
        if (pHandler && pHandler->getDelegate() == pDelegate) {
            m_pHandlers->removeObject(pHandler);
            break;
        }
    }
}

enumKeyCodes CCKeyboardDispatcher::convertKeyCode(enumKeyCodes key) {
    switch (key) {
        case KEY_ArrowUp:
            key = KEY_Up;
            break;
        case KEY_ArrowDown:
            key = KEY_Down;
            break;
        case KEY_ArrowLeft:
            key = KEY_Left;
            break;
        case KEY_ArrowRight:
            key = KEY_Right;
            break;
        case KEY_Shift:
        case KEY_Control:
        case KEY_Alt:
        case CONTROLLER_Back:
            return KEY_Unknown;
    }
    return key;
}
bool CCKeyboardDispatcher::dispatchKeyboardMSG(
    enumKeyCodes a_key,
    bool isKeyDown,
    bool isKeyRepeat,
    double unk)
{
    // Block repeated keys if configured
    if (isKeyRepeat && m_bBlockRepeat)
        return false;

    // Convert key code and filter out modifier / special keys
    enumKeyCodes key = convertKeyCode(a_key);
    if(key == KEY_Unknown)
        return false;

    // Signal that we are currently dispatching
    m_bDispatching = true;

    // Deliver the event to all delegates in reverse order

    unsigned int count = m_pHandlers->count();
    for (unsigned int i = 0; i < count; ++i)
    {
        CCKeyboardHandler* handler = static_cast<CCKeyboardHandler*>(m_pHandlers->objectAtIndex(i));
        if (!handler)
            continue;

        CCKeyboardDelegate* delegate = handler->getDelegate();
        if(!dynamic_cast<CCKeyboardDelegate*>(delegate))
            continue;
        CCLOG("Dispatching keyboard event to delegate: %p", delegate);
        if (isKeyDown)
            delegate->keyDown(key, unk);
        else
            delegate->keyUp(key, unk);
    }

    m_bDispatching = false;

    // Process deferred removals
    if (m_bDeferredRemove)
    {
        m_bDeferredRemove = false;
        for (unsigned int i = 0; i < m_pDeferredRemoveDelegates->num; ++i)
        {
            forceRemoveDelegate(static_cast<CCKeyboardDelegate*>(m_pDeferredRemoveDelegates->arr[i]));
        }
        ccCArrayRemoveAllValues(m_pDeferredRemoveDelegates);
    }

    // Process deferred additions
    if (m_bDeferredAdd)
    {
        m_bDeferredAdd = false;
        for (unsigned int i = 0; i < m_pDeferredAddDelegates->num; ++i)
        {
            forceAddDelegate(static_cast<CCKeyboardDelegate*>(m_pDeferredAddDelegates->arr[i]));
        }
        ccCArrayRemoveAllValues(m_pDeferredAddDelegates);
        return true;
    }
    return false;
}

const char* CCKeyboardDispatcher::keyToString(enumKeyCodes key) {
    // TODO: possibly implement this
    return "PLACEHOLDER_KEY";
}

void CCKeyboardDispatcher::updateModifierKeys(bool shft, bool ctrl, bool alt, bool cmd) {
    m_bShiftPressed = shft;
    m_bControlPressed = ctrl || cmd;
    m_bAltPressed = alt;
    m_bCommandPressed = cmd;
}
// CCKeyboardHandler

CCKeyboardHandler::~CCKeyboardHandler() {}

CCKeyboardHandler& CCKeyboardHandler::operator=(const CCKeyboardHandler&) = default;

CCKeyboardDelegate* CCKeyboardHandler::getDelegate() {
    return m_pDelegate;
}

CCKeyboardHandler* CCKeyboardHandler::handlerWithDelegate(CCKeyboardDelegate* pDelegate) {
    auto pRet = new CCKeyboardHandler();
    if (pRet && pRet->initWithDelegate(pDelegate)) {
        pRet->autorelease();
        return pRet;
    } else {
        CC_SAFE_DELETE(pRet);
        return nullptr;
    }
}

bool CCKeyboardHandler::initWithDelegate(CCKeyboardDelegate* pDelegate) {
    m_pDelegate = pDelegate;
    if(auto delegate = dynamic_cast<CCObject*>(pDelegate)) {
        delegate->retain();
    }
    return true;
}

void CCKeyboardHandler::setDelegate(CCKeyboardDelegate* pDelegate) {
    if(auto delegate = dynamic_cast<CCObject*>(m_pDelegate)) {
        delegate->release();
    }
    if(auto delegate = dynamic_cast<CCObject*>(pDelegate)) {
        delegate->retain();

    }
    m_pDelegate = pDelegate;
}

}