#include "CCKeyboardDispatcher.h"

namespace cocos2d {

CCKeyboardDispatcher::CCKeyboardDispatcher() {
    m_pHandlers = CCArray::create();
    m_pHandlers->retain();
    m_bDispatching = false;
    m_bDeferredAdd = false;
    m_bDeferredRemove = false;
    m_pDeferredAddDelegates = ccCArrayNew(100);
    m_pDeferredRemoveDelegates = ccCArrayNew(100);
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
        ccCArrayAppendValueWithResize(m_pDeferredAddDelegates, pDelegate);
        m_bDeferredAdd = true;
    } else {
        forceAddDelegate(pDelegate);
    }
}
void CCKeyboardDispatcher::removeDelegate(CCKeyboardDelegate* pDelegate) {
    if (m_bDispatching) {
        ccCArrayAppendValueWithResize(m_pDeferredRemoveDelegates, pDelegate);
        m_bDeferredRemove = true;
    } else {
        forceRemoveDelegate(pDelegate);
    }
}
void cocos2d::CCKeyboardDispatcher::forceAddDelegate(CCKeyboardDelegate* pDelegate) {
    if (!pDelegate) return;

    // Guard: Prevent duplicate entries of the same delegate from accumulating
    cocos2d::CCObject* pObj = nullptr;
    CCARRAY_FOREACH(m_pHandlers, pObj) {
        auto pHandler = static_cast<cocos2d::CCKeyboardHandler*>(pObj);
        if (pHandler && pHandler->getDelegate() == pDelegate) {
            CCLOG("Delegate %p already exists, skipping duplicate addition.", pDelegate);
            return;
        }
    }

    CCLOG("Forcing addition of delegate: %p", pDelegate);
    m_pHandlers->addObject(CCKeyboardHandler::handlerWithDelegate(pDelegate));
}

void cocos2d::CCKeyboardDispatcher::forceRemoveDelegate(CCKeyboardDelegate* pDelegate) {
    if (!pDelegate || !m_pHandlers) return;

    CCLOG("Forcing removal of delegate: %p", pDelegate);
    bool removedAny = false;

    // Loop backwards to safely clear out ALL instances wrapping this delegate
    for (int i = static_cast<int>(m_pHandlers->count()) - 1; i >= 0; --i) {
        auto pHandler = static_cast<cocos2d::CCKeyboardHandler*>(m_pHandlers->objectAtIndex(i));
        if (pHandler && pHandler->getDelegate() == pDelegate) {
            m_pHandlers->removeObjectAtIndex(i);
            removedAny = true;
        }
    }

    if (removedAny) {
        CCLOG("Delegate removed: %p", pDelegate);
    } else {
        CCLOG("Delegate not found for removal: %p", pDelegate);
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


bool cocos2d::CCKeyboardDispatcher::dispatchKeyboardMSG(
    cocos2d::enumKeyCodes a_key,
    bool isKeyDown,
    bool isKeyRepeat,
    double unk)
{
    if (isKeyRepeat && m_bBlockRepeat)
        return false;

    cocos2d::enumKeyCodes key = convertKeyCode(a_key);
    if ((key >= KEY_Shift && key <= KEY_Alt) || key == CONTROLLER_Back)
        return false;

    m_bDispatching = true;

    // Dispatch exclusively to the top-most handler to prevent background event leakage
    if (m_pHandlers && m_pHandlers->count() > 0)
    {
        unsigned int count = m_pHandlers->count();
        auto handler = static_cast<CCKeyboardHandler*>(m_pHandlers->objectAtIndex(count - 1));
        if (handler)
        {
            CCKeyboardDelegate* delegate = handler->getDelegate();
            if (delegate)
            {
                if (isKeyDown)
                    delegate->keyDown(key, unk);
                else
                    delegate->keyUp(key, unk);
            }
        }
    }

    m_bDispatching = false;

    // Process deferred removals safely
    if (m_bDeferredRemove && m_pDeferredRemoveDelegates && m_pDeferredRemoveDelegates->num > 0)
    {
        m_bDeferredRemove = false;
        unsigned int numRemovals = m_pDeferredRemoveDelegates->num;
        for (unsigned int i = 0; i < numRemovals; ++i)
        {
            auto delegate = static_cast<CCKeyboardDelegate*>(m_pDeferredRemoveDelegates->arr[i]);
            forceRemoveDelegate(delegate);
        }
        ccCArrayRemoveAllValues(m_pDeferredRemoveDelegates);
    }

    // Process deferred additions safely
    if (m_bDeferredAdd && m_pDeferredAddDelegates && m_pDeferredAddDelegates->num > 0)
    {
        m_bDeferredAdd = false;
        unsigned int numAdditions = m_pDeferredAddDelegates->num;
        for (unsigned int i = 0; i < numAdditions; ++i)
        {
            auto delegate = static_cast<CCKeyboardDelegate*>(m_pDeferredAddDelegates->arr[i]);
            forceAddDelegate(delegate);
        }
        ccCArrayRemoveAllValues(m_pDeferredAddDelegates);
    }

    return true;
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

CCKeyboardHandler::~CCKeyboardHandler() {

}

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

    return true;
}

void CCKeyboardHandler::setDelegate(CCKeyboardDelegate* pDelegate) {

    m_pDelegate = pDelegate;
}

}