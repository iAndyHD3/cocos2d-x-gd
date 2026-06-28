#ifndef __CCKEYBOARD_DISPATCHER_H__
#define __CCKEYBOARD_DISPATCHER_H__

#include "CCKeyboardDelegate.h"
#include "../../cocoa/CCArray.h"
#include "support/data_support/ccCArray.h"


NS_CC_BEGIN

// @note RobTop Addition
class CC_DLL CCKeyboardDispatcher : public CCObject 
{
public:
	CCKeyboardDispatcher();
	virtual ~CCKeyboardDispatcher();

	void addDelegate(CCKeyboardDelegate* pDelegate);
	void removeDelegate(CCKeyboardDelegate* pDelegate);

	void forceAddDelegate(CCKeyboardDelegate* pDelegate);
	void forceRemoveDelegate(CCKeyboardDelegate* pDelegate);

	static enumKeyCodes convertKeyCode(enumKeyCodes key);

	bool dispatchKeyboardMSG(enumKeyCodes key, bool isKeyDown, bool isKeyRepeat, double unk);

	inline bool getAltKeyPressed() const {
		return m_bAltPressed;
	}
	inline bool getCommandKeyPressed() const {
		return m_bCommandPressed;
	}
	inline bool getControlKeyPressed() const {
		return m_bControlPressed;
	}
	inline bool getShiftKeyPressed() const {
		return m_bShiftPressed;
	}

	static GEODE_DLL CCKeyboardDispatcher* get();

	const char* keyToString(enumKeyCodes key);

	void updateModifierKeys(bool shft, bool ctrl, bool alt, bool cmd);

	inline bool getBlockRepeat() const {
		return m_bBlockRepeat;
	}

	inline void setBlockRepeat(bool blockRepeat) {
		this->m_bBlockRepeat = blockRepeat;
	}

protected:
	CCArray* m_pHandlers;	// 0x38
	bool m_bDispatching;			// 0x40
    bool m_bDeferredAdd;			// 0x41
    bool m_bDeferredRemove;		// 0x42
    char pad0[5];
	ccCArray* m_pDeferredAddDelegates; // 0x48
	ccCArray* m_pDeferredRemoveDelegates;	// 0x50
	bool m_bShiftPressed;		// 0x58
	bool m_bControlPressed;	// 0x59
	bool m_bAltPressed;			// 0x5a
	bool m_bCommandPressed;	// 0x5b
	bool m_bBlockRepeat;		// 0x5c
    char pad1[3];
};

NS_CC_END

#endif
