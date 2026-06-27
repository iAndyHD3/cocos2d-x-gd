#include "CCEGLViewProtocol.h"
#include "touch_dispatcher/CCTouchDispatcher.h"
#include "touch_dispatcher/CCTouch.h"
#include "CCDirector.h"
#include "cocoa/CCSet.h"
#include "cocoa/CCDictionary.h"
#include "cocoa/CCInteger.h"

NS_CC_BEGIN

static CCTouch* s_pTouches[CC_MAX_TOUCHES] = { NULL };
static unsigned int s_indexBitsUsed = 0;
static CCDictionary s_TouchesIntergerDict;

static int getUnUsedIndex()
{
    int temp = s_indexBitsUsed;

    for (int i = 0; i < CC_MAX_TOUCHES; i++)
    {
        if (!(temp & 1))
        {
            s_indexBitsUsed |= (1 << i);
            return i;
        }
        temp >>= 1;
    }

    return -1;
}

static void removeUsedIndexBit(int index)
{
    if (index >= 0 && index < CC_MAX_TOUCHES)
    {
        unsigned int temp = ~(1 << index);
        s_indexBitsUsed &= temp;
    }
}

CCEGLViewProtocol::CCEGLViewProtocol()
    : m_pDelegate(NULL)
    , m_fScaleX(1.0f)
    , m_fScaleY(1.0f)
    , m_eResolutionPolicy(kResolutionUnKnown)
    , m_fBackingScaleFactor(1.0f)
{
}

CCEGLViewProtocol::~CCEGLViewProtocol()
{
}

void CCEGLViewProtocol::setDesignResolutionSize(float width, float height, ResolutionPolicy resolutionPolicy)
{
    if (width == 0.0f || height == 0.0f)
        return;

    m_obDesignResolutionSize.setSize(width, height);

    float screenW = m_obScreenSize.width;
    float screenH = m_obScreenSize.height;
    float designW = m_obDesignResolutionSize.width;
    float designH = m_obDesignResolutionSize.height;

    float scaleX = screenW / designW;
    float scaleY = screenH / designH;

    m_fScaleX = scaleX;
    m_fScaleY = scaleY;

    switch (resolutionPolicy)
    {
    case kResolutionNoBorder:
        m_fScaleX = m_fScaleY = fmaxf(scaleX, scaleY);
        break;
    case kResolutionShowAll:
        m_fScaleX = m_fScaleY = fminf(scaleX, scaleY);
        break;
    case kResolutionFixedHeight:
        m_fScaleX = m_fScaleY;
        m_obDesignResolutionSize.width = ceilf(screenW / m_fScaleX);
        break;
    case kResolutionFixedWidth:
        m_fScaleY = m_fScaleX;
        m_obDesignResolutionSize.height = ceilf(screenH / m_fScaleY);
        break;
    }

    float viewPortW = m_obDesignResolutionSize.width * m_fScaleX;
    float viewPortH = m_obDesignResolutionSize.height * m_fScaleY;

    m_obViewPortRect.setRect(
        (m_obScreenSize.width - viewPortW) * 0.5f,
        (m_obScreenSize.height - viewPortH) * 0.5f,
        viewPortW,
        viewPortH
    );

    m_eResolutionPolicy = resolutionPolicy;

    CCDirector::sharedDirector()->m_obWinSizeInPoints = getDesignResolutionSize();
    CCDirector::sharedDirector()->createStatsLabel();
    CCDirector::sharedDirector()->setGLDefaultValues();
}

const CCSize& CCEGLViewProtocol::getDesignResolutionSize() const 
{
    return m_obDesignResolutionSize;
}

const CCSize& CCEGLViewProtocol::getFrameSize() const
{
    return m_obScreenSize;
}

void CCEGLViewProtocol::setFrameSize(float width, float height)
{
    CCSize size(width, height);
    m_obScreenSize = size;
    m_obDesignResolutionSize = size;
}

CCSize CCEGLViewProtocol::getVisibleSize() const
{
    if (m_eResolutionPolicy == kResolutionNoBorder)
        return CCSize(m_obScreenSize.width / m_fScaleX, m_obScreenSize.height / m_fScaleY);
    else
        return m_obDesignResolutionSize;
}

CCPoint CCEGLViewProtocol::getVisibleOrigin() const
{
    if (m_eResolutionPolicy == kResolutionNoBorder)
        return CCPoint(
            (m_obDesignResolutionSize.height - m_obScreenSize.width / m_fScaleX) * 0.5f,
            (m_obDesignResolutionSize.width - m_obScreenSize.height / m_fScaleY) * 0.5f);
    else
        return CCPointZero;
}

void CCEGLViewProtocol::setTouchDelegate(EGLTouchDelegate * pDelegate)
{
    m_pDelegate = pDelegate;
}

void CCEGLViewProtocol::setViewPortInPoints(float x , float y , float w , float h)
{

    glViewport((GLint)(x * m_fScaleX + m_obViewPortRect.origin.x),
               (GLint)(y * m_fScaleY + m_obViewPortRect.origin.y),
               (GLsizei)(w * m_fScaleX),
               (GLsizei)(h * m_fScaleY));
}

void CCEGLViewProtocol::setScissorInPoints(float x , float y , float w , float h)
{
    glScissor((GLint)(x * m_fScaleX + m_obViewPortRect.origin.x),
              (GLint)(y * m_fScaleY + m_obViewPortRect.origin.y),
              (GLsizei)(w * m_fScaleX),
              (GLsizei)(h * m_fScaleY));
}

bool CCEGLViewProtocol::isScissorEnabled()
{
    return glIsEnabled(GL_SCISSOR_TEST) != GL_FALSE;
}

CCRect CCEGLViewProtocol::getScissorRect()
{
	GLfloat params[4];
	glGetFloatv(GL_SCISSOR_BOX, params);
	float x = (params[0] - m_obViewPortRect.origin.x) / m_fScaleX;
	float y = (params[1] - m_obViewPortRect.origin.y) / m_fScaleY;
	float w = params[2] / m_fScaleX;
	float h = params[3] / m_fScaleY;
	return CCRectMake(x, y, w, h);
}

void CCEGLViewProtocol::setViewName(const char* pszViewName)
{
    if (pszViewName)
    {
        size_t len = strlen(pszViewName);
        if (len > 0)
            strncpy(m_szViewName, pszViewName, sizeof(m_szViewName));
    }
}

const char* CCEGLViewProtocol::getViewName()
{
    return m_szViewName;
}

void CCEGLViewProtocol::handleTouchesBegin(int num, int ids[], float xs[], float ys[], double timestamp)
{
    CCSet set;

    for (int i = 0; i < num; ++i)
    {
        int id = ids[i];
        float x = xs[i];
        float y = ys[i];

        CCInteger* pIndex = (CCInteger*)s_TouchesIntergerDict.objectForKey(id);

        if (!pIndex)
        {
            int nUnusedIndex = getUnUsedIndex();

            if (nUnusedIndex == -1)
                continue;

            CCTouch* pTouch = s_pTouches[nUnusedIndex] = new CCTouch();
            pTouch->setTouchInfo(nUnusedIndex, (x - m_obViewPortRect.origin.x) / m_fScaleX,
                                 (y - m_obViewPortRect.origin.y) / m_fScaleY);
            pTouch->m_fPrevTime = timestamp;

            CCInteger* pInterObj = new CCInteger(nUnusedIndex);
            s_TouchesIntergerDict.setObject(pInterObj, id);
            set.addObject(pTouch);
            pInterObj->release();
        }
    }

    if (set.count() > 0)
        m_pDelegate->touchesBegan(&set, NULL);
}

void CCEGLViewProtocol::handleTouchesMove(int num, int ids[], float xs[], float ys[], double timestamp)
{
    CCSet set;

    for (int i = 0; i < num; ++i)
    {
        int id = ids[i];
        float x = xs[i];
        float y = ys[i];

        CCInteger* pIndex = (CCInteger*)s_TouchesIntergerDict.objectForKey(id);
        if (!pIndex)
            continue;

        CCTouch* pTouch = s_pTouches[pIndex->getValue()];
        if (pTouch)
        {
            pTouch->setTouchInfo(pIndex->getValue(), (x - m_obViewPortRect.origin.x) / m_fScaleX,
                                 (y - m_obViewPortRect.origin.y) / m_fScaleY);
            pTouch->m_fPrevTime = timestamp;

            set.addObject(pTouch);
        }
        else
        {
            return;
        }
    }

    if (set.count() > 0)
        m_pDelegate->touchesMoved(&set, NULL);
}

void CCEGLViewProtocol::getSetOfTouchesEndOrCancel(CCSet& set, int num, int ids[], float xs[], float ys[], double timestamp)
{
    for (int i = 0; i < num; ++i)
    {
        int id = ids[i];
        float x = xs[i];
        float y = ys[i];

        CCInteger* pIndex = (CCInteger*)s_TouchesIntergerDict.objectForKey(id);
        if (!pIndex)
            continue;

        CCTouch* pTouch = s_pTouches[pIndex->getValue()];
        if (pTouch)
        {
            pTouch->setTouchInfo(pIndex->getValue(), (x - m_obViewPortRect.origin.x) / m_fScaleX,
                                 (y - m_obViewPortRect.origin.y) / m_fScaleY);
            pTouch->m_fPrevTime = timestamp;

            set.addObject(pTouch);

            pTouch->release();
            s_pTouches[pIndex->getValue()] = NULL;
            removeUsedIndexBit(pIndex->getValue());

            s_TouchesIntergerDict.removeObjectForKey(id);
        }
        else
        {
            return;
        }
    }
}

void CCEGLViewProtocol::handleTouchesEnd(int num, int ids[], float xs[], float ys[], double timestamp)
{
    CCSet set;
    getSetOfTouchesEndOrCancel(set, num, ids, xs, ys, timestamp);
    m_pDelegate->touchesEnded(&set, NULL);
}

void CCEGLViewProtocol::handleTouchesCancel(int num, int ids[], float xs[], float ys[], double timestamp)
{
    CCSet set;
    getSetOfTouchesEndOrCancel(set, num, ids, xs, ys, timestamp);
    m_pDelegate->touchesCancelled(&set, NULL);
}

const CCRect& CCEGLViewProtocol::getViewPortRect() const
{
    return m_obViewPortRect;
}

float CCEGLViewProtocol::getScaleX() const
{
    return m_fScaleX;
}

float CCEGLViewProtocol::getScaleY() const
{
    return m_fScaleY;
}

float CCEGLViewProtocol::getBackingScaleFactor() const
{
    return m_fBackingScaleFactor;
}

void CCEGLViewProtocol::setBackingScaleFactor(float fScaleFactor)
{
    m_fBackingScaleFactor = fScaleFactor;
}

double CCEGLViewProtocol::getPlatformTimestamp()
{
    return 0.0;
}

void CCEGLViewProtocol::updateDesignResolutionSize()
{
    float screenW = m_obScreenSize.width;
    if (screenW <= 0.0f)
        return;

    float screenH = m_obScreenSize.height;
    if (screenH <= 0.0f)
        return;

    float designW = m_obDesignResolutionSize.width;
    if (designW <= 0.0f)
        return;

    float designH = m_obDesignResolutionSize.height;
    if (designH <= 0.0f)
        return;

    float scaleX = screenW / designW;
    float scaleY = screenH / designH;

    m_fScaleX = scaleX;
    m_fScaleY = scaleY;

    switch (m_eResolutionPolicy)
    {
    case kResolutionNoBorder:
        m_fScaleX = m_fScaleY = fmaxf(scaleX, scaleY);
        break;
    case kResolutionShowAll:
        m_fScaleX = m_fScaleY = fminf(scaleX, scaleY);
        break;
    case kResolutionFixedHeight:
        m_fScaleX = m_fScaleY;
        m_obDesignResolutionSize.width = ceilf(screenW / m_fScaleX);
        break;
    case kResolutionFixedWidth:
        m_fScaleY = m_fScaleX;
        m_obDesignResolutionSize.height = ceilf(screenH / m_fScaleY);
        break;
    }

    float viewPortW = m_obDesignResolutionSize.width * m_fScaleX;
    float viewPortH = m_obDesignResolutionSize.height * m_fScaleY;

    m_obViewPortRect.setRect(
        (m_obScreenSize.width - viewPortW) * 0.5f,
        (m_obScreenSize.height - viewPortH) * 0.5f,
        viewPortW,
        viewPortH
    );

    CCDirector::sharedDirector()->m_obWinSizeInPoints = getDesignResolutionSize();
    CCDirector::sharedDirector()->createStatsLabel();
    CCDirector::sharedDirector()->setGLDefaultValues();
}

void CCEGLViewProtocol::pollInputEvents()
{
}


NS_CC_END
