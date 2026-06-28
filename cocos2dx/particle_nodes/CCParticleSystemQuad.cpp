/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2009      Leonardo Kasperavičius
Copyright (c) 2011      Zynga Inc.

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

#include "CCGL.h"
#include "CCParticleSystemQuad.h"
#include "sprite_nodes/CCSpriteFrame.h"
#include "CCDirector.h"
#include "CCParticleBatchNode.h"
#include "textures/CCTextureAtlas.h"
#include "shaders/CCShaderCache.h"
#include "shaders/ccGLStateCache.h"
#include "shaders/CCGLProgram.h"
#include "support/TransformUtils.h"
#include "support/CCNotificationCenter.h"
#include "CCEventType.h"

#include <math.h>

NS_CC_BEGIN

//implementation CCParticleSystemQuad
// overriding the init method
bool CCParticleSystemQuad::initWithTotalParticles(unsigned int numberOfParticles, bool unk)
{
    // base initialization
    if (!CCParticleSystem::initWithTotalParticles(numberOfParticles, unk))
    {
        return false;
    }

    // allocating data space
    if (!this->allocMemory())
    {
        this->release();
        return false;
    }

    initIndices();
    setupVBO();

    setShaderProgram(CCShaderCache::sharedShaderCache()->programForKey("ShaderPositionTextureColor"));

    CCNotificationCenter::sharedNotificationCenter()->addObserver(
        this,
        (SEL_CallFuncO)&CCParticleSystemQuad::listenBackToForeground,
        EVENT_COME_TO_FOREGROUND,
        nullptr);

    return true;
}

CCParticleSystemQuad::CCParticleSystemQuad()
: m_pQuads(NULL)
, m_pIndices(NULL)
{
    m_tTextureRect = CCRectMake(0, 0, 0, 0);
    m_uParticleIdx = 0;
    m_uOpacity = 255;
    m_pBuffersVBO[0] = 0;
    m_pBuffersVBO[1] = 0;
}

CCParticleSystemQuad::~CCParticleSystemQuad()
{
    if (NULL == m_pBatchNode)
    {
        if (m_pQuads)
        {
            free(m_pQuads);
            m_pQuads = NULL;
        }
        if (m_pIndices)
        {
            free(m_pIndices);
            m_pIndices = NULL;
        }
        glDeleteBuffers(2, &m_pBuffersVBO[0]);
    }
}

// implementation CCParticleSystemQuad

CCParticleSystemQuad * CCParticleSystemQuad::create(const char *plistFile)
{
    CCParticleSystemQuad *pRet = new CCParticleSystemQuad();
    if (pRet && pRet->initWithFile(plistFile))
    {
        pRet->autorelease();
        return pRet;
    }
    CC_SAFE_DELETE(pRet);
    return NULL;
}

CCParticleSystemQuad * CCParticleSystemQuad::create(const char *plistFile, bool unk)
{
    CCParticleSystemQuad *pRet = new CCParticleSystemQuad();
    if (pRet && pRet->initWithFile(plistFile, unk))
    {
        pRet->autorelease();
        return pRet;
    }
    CC_SAFE_DELETE(pRet);
    return NULL;
}

CCParticleSystemQuad * CCParticleSystemQuad::createWithTotalParticles(unsigned int numberOfParticles, bool unk)
{
    CCParticleSystemQuad *pRet = new CCParticleSystemQuad();
    if (pRet && pRet->initWithTotalParticles(numberOfParticles, unk))
    {
        pRet->autorelease();
        return pRet;
    }
    CC_SAFE_DELETE(pRet);
    return NULL;
}

CCParticleSystemQuad * CCParticleSystemQuad::create()
{
    CCParticleSystemQuad *pParticleSystemQuad = new CCParticleSystemQuad();
    if (pParticleSystemQuad && pParticleSystemQuad->init())
    {
        pParticleSystemQuad->autorelease();
        return pParticleSystemQuad;
    }
    CC_SAFE_DELETE(pParticleSystemQuad);
    return NULL;
}


// pointRect should be in Texture coordinates, not pixel coordinates
void CCParticleSystemQuad::initTexCoordsWithRect(const CCRect& pointRect)
{
    // store the rect - RobTop stores the requested rect for later reuse
    m_tTextureRect = pointRect;

    // convert to Tex coords
    CCRect rect = CCRectMake(
        pointRect.origin.x * CC_CONTENT_SCALE_FACTOR(),
        pointRect.origin.y * CC_CONTENT_SCALE_FACTOR(),
        pointRect.size.width * CC_CONTENT_SCALE_FACTOR(),
        pointRect.size.height * CC_CONTENT_SCALE_FACTOR());

    GLfloat wide = (GLfloat) pointRect.size.width;
    GLfloat high = (GLfloat) pointRect.size.height;

    if (m_pTexture)
    {
        wide = (GLfloat)m_pTexture->getPixelsWide();
        high = (GLfloat)m_pTexture->getPixelsHigh();
    }

    GLfloat left = rect.origin.x / wide;
    GLfloat bottom = rect.origin.y / high;
    GLfloat right = left + rect.size.width / wide;
    GLfloat top = bottom + rect.size.height / high;

    // RobTop: no CC_SWAP; Y axis is not flipped (top stays top, bottom stays bottom)

    ccV3F_C4B_T2F_Quad *quads = NULL;
    unsigned int start = 0, end = 0;
    if (m_pBatchNode)
    {
        quads = m_pBatchNode->getTextureAtlas()->getQuads();
        start = m_uAtlasIndex;
        end = m_uAtlasIndex + m_uTotalParticles;
    }
    else
    {
        quads = m_pQuads;
        start = 0;
        end = m_uTotalParticles;
    }

    for (unsigned int i = start; i < end; i++)
    {
        // cocos2d-x 2.2.3 Y-flipped layout (matches RobTop's binary):
        //   bl.v = top, br.v = top, tl.v = bottom, tr.v = bottom
        quads[i].bl.texCoords.u = left;
        quads[i].bl.texCoords.v = top;
        quads[i].br.texCoords.u = right;
        quads[i].br.texCoords.v = top;
        quads[i].tl.texCoords.u = left;
        quads[i].tl.texCoords.v = bottom;
        quads[i].tr.texCoords.u = right;
        quads[i].tr.texCoords.v = bottom;
    }
}

void CCParticleSystemQuad::setTextureWithRect(CCTexture2D *texture, const CCRect& rect)
{
    // Only update the texture if is different from the current one
    if (!m_pTexture || texture->getName() != m_pTexture->getName())
    {
        CCParticleSystem::setTexture(texture);
    }

    this->initTexCoordsWithRect(rect);
}

void CCParticleSystemQuad::setTexture(CCTexture2D* texture)
{
    const CCSize& s = texture->getContentSize();
    this->setTextureWithRect(texture, CCRectMake(0, 0, s.width, s.height));
}

void CCParticleSystemQuad::setDisplayFrame(CCSpriteFrame *spriteFrame)
{
    if (spriteFrame)
    {
        spriteFrame->getOffsetInPixels();
        CCTexture2D *texture = spriteFrame->getTexture();
        this->setTextureWithRect(texture, spriteFrame->getRect());
    }
}

void CCParticleSystemQuad::initIndices()
{
    for (unsigned int i = 0; i < m_uTotalParticles; ++i)
    {
        const unsigned int i6 = i * 6;
        const unsigned int i4 = i * 4;
        m_pIndices[i6 + 0] = (GLushort) i4 + 0;
        m_pIndices[i6 + 1] = (GLushort) i4 + 1;
        m_pIndices[i6 + 2] = (GLushort) i4 + 2;
        m_pIndices[i6 + 5] = (GLushort) i4 + 1;
        m_pIndices[i6 + 4] = (GLushort) i4 + 2;
        m_pIndices[i6 + 3] = (GLushort) i4 + 3;
    }
}

void CCParticleSystemQuad::updateQuadWithParticle(tCCParticle* particle, const CCPoint& newPosition)
{
    ccV3F_C4B_T2F_Quad *quad;

    if (m_pBatchNode)
    {
        ccV3F_C4B_T2F_Quad *batchQuads = m_pBatchNode->getTextureAtlas()->getQuads();
        quad = &(batchQuads[m_uAtlasIndex + particle->atlasIndex]);
    }
    else
    {
        quad = &(m_pQuads[CCParticleSystem::m_uParticleIdx]);
    }

    // RobTop color computation: use m_uOpacity and m_tQuadColor as scratchpad
    float opacity = (float)m_uOpacity;
    float alpha = opacity * particle->color.a;
    float y = particle->modeA.dir.y;
    float fadeVar = particle->modeA.radialAccel;

    if (fadeVar > y || (y = particle->timeToLive, fadeVar = particle->modeA.tangentialAccel, fadeVar > y))
    {
        if (m_bOpacityModifyRGB)
            alpha *= y / fadeVar;
        else
            opacity *= y / fadeVar;
    }

    if (m_bOpacityModifyRGB)
    {
        m_tQuadColor.r = (GLubyte)(alpha * particle->color.r);
        m_tQuadColor.g = (GLubyte)(alpha * particle->color.g);
        m_tQuadColor.b = (GLubyte)(alpha * particle->color.b);
        m_tQuadColor.a = (GLubyte)alpha;
    }
    else
    {
        m_tQuadColor.r = (GLubyte)(opacity * particle->color.r);
        m_tQuadColor.g = (GLubyte)(opacity * particle->color.g);
        m_tQuadColor.b = (GLubyte)(opacity * particle->color.b);
        m_tQuadColor.a = (GLubyte)(opacity * particle->color.a);
    }

    quad->bl.colors = m_tQuadColor;
    quad->br.colors = m_tQuadColor;
    quad->tl.colors = m_tQuadColor;
    quad->tr.colors = m_tQuadColor;

    // vertices
    GLfloat size_2 = particle->size * 0.5f;
    if (particle->rotation != 0.0f)
    {
        GLfloat x1 = -size_2;
        GLfloat y1 = -size_2;
        GLfloat x2 = size_2;
        GLfloat y2 = size_2;
        GLfloat x = newPosition.x;
        GLfloat y_pos = newPosition.y;

        GLfloat r = (GLfloat)-CC_DEGREES_TO_RADIANS(particle->rotation);
        GLfloat cr = cosf(r);
        GLfloat sr = sinf(r);
        GLfloat ax = x1 * cr - y1 * sr + x;
        GLfloat ay = x1 * sr + y1 * cr + y_pos;
        GLfloat bx = x2 * cr - y1 * sr + x;
        GLfloat by = x2 * sr + y1 * cr + y_pos;
        GLfloat cx = x2 * cr - y2 * sr + x;
        GLfloat cy = x2 * sr + y2 * cr + y_pos;
        GLfloat dx = x1 * cr - y2 * sr + x;
        GLfloat dy = x1 * sr + y2 * cr + y_pos;

        quad->bl.vertices.x = ax;
        quad->bl.vertices.y = ay;
        quad->br.vertices.x = bx;
        quad->br.vertices.y = by;
        quad->tl.vertices.x = dx;
        quad->tl.vertices.y = dy;
        quad->tr.vertices.x = cx;
        quad->tr.vertices.y = cy;
    }
    else
    {
        quad->bl.vertices.x = newPosition.x - size_2;
        quad->bl.vertices.y = newPosition.y - size_2;
        quad->br.vertices.x = newPosition.x + size_2;
        quad->br.vertices.y = newPosition.y - size_2;
        quad->tl.vertices.x = newPosition.x - size_2;
        quad->tl.vertices.y = newPosition.y + size_2;
        quad->tr.vertices.x = newPosition.x + size_2;
        quad->tr.vertices.y = newPosition.y + size_2;
    }
}

void CCParticleSystemQuad::postStep()
{
    glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_pQuads[0]) * m_uTotalParticles, m_pQuads);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// overriding draw method
void CCParticleSystemQuad::draw()
{
    if (!m_pTexture)
        return;

    if (!m_uParticleCount)
        return;

    CC_NODE_DRAW_SETUP();

    ccGLBindTexture2D(m_pTexture->getName());
    ccGLBlendFunc(m_tBlendFunc.src, m_tBlendFunc.dst);

    ccGLEnableVertexAttribs(kCCVertexAttribFlag_PosColorTex);

    glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
    glVertexAttribPointer(kCCVertexAttrib_Position, 3, GL_FLOAT, GL_FALSE, sizeof(m_pQuads[0].bl), (GLvoid*)offsetof(ccV3F_C4B_T2F, vertices));
    glVertexAttribPointer(kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(m_pQuads[0].bl), (GLvoid*)offsetof(ccV3F_C4B_T2F, colors));
    glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, sizeof(m_pQuads[0].bl), (GLvoid*)offsetof(ccV3F_C4B_T2F, texCoords));

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);

    glDrawElements(GL_TRIANGLES, (GLsizei)CCParticleSystem::m_uParticleIdx * 6, GL_UNSIGNED_SHORT, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    CC_INCREMENT_GL_DRAWS(1);
}

void CCParticleSystemQuad::setTotalParticles(unsigned int tp)
{
    if (tp > m_uAllocatedParticles)
    {
        tCCParticle* particlesNew = (tCCParticle*)realloc(m_pParticles, tp * sizeof(tCCParticle));
        ccV3F_C4B_T2F_Quad* quadsNew = (ccV3F_C4B_T2F_Quad*)realloc(m_pQuads, sizeof(m_pQuads[0]) * tp);
        GLushort* indicesNew = (GLushort*)realloc(m_pIndices, sizeof(m_pIndices[0]) * tp * 6);

        if (particlesNew && quadsNew && indicesNew)
        {
            m_pParticles = particlesNew;
            m_pQuads = quadsNew;
            m_pIndices = indicesNew;

            memset(m_pParticles, 0, tp * sizeof(tCCParticle));
            memset(m_pQuads, 0, sizeof(m_pQuads[0]) * tp);
            memset(m_pIndices, 0, sizeof(m_pIndices[0]) * tp * 6);

            m_uAllocatedParticles = tp;
            m_uTotalParticles = tp;

            if (m_pBatchNode)
            {
                for (unsigned int i = 0; i < m_uTotalParticles; i++)
                {
                    m_pParticles[i].atlasIndex = i;
                }
            }

            initIndices();
            setupVBO();

            if (m_pTexture)
                initTexCoordsWithRect(m_tTextureRect);
        }
        else
        {
            if (particlesNew) m_pParticles = particlesNew;
            if (quadsNew) m_pQuads = quadsNew;
            if (indicesNew) m_pIndices = indicesNew;
            return;
        }
    }
    else
    {
        m_uTotalParticles = tp;
    }

    resetSystem();
}

void CCParticleSystemQuad::setupVBO()
{
    glDeleteBuffers(2, &m_pBuffersVBO[0]);

    glGenBuffers(2, &m_pBuffersVBO[0]);

    glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(m_pQuads[0]) * m_uTotalParticles, m_pQuads, GL_STREAM_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_pIndices[0]) * m_uTotalParticles * 6, m_pIndices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void CCParticleSystemQuad::listenBackToForeground(CCObject *obj)
{
    setupVBO();
}

bool CCParticleSystemQuad::allocMemory()
{
    if (m_pQuads)
    {
        free(m_pQuads);
        m_pQuads = NULL;
    }
    if (m_pIndices)
    {
        free(m_pIndices);
        m_pIndices = NULL;
    }

    m_pQuads = (ccV3F_C4B_T2F_Quad*)malloc(m_uTotalParticles * sizeof(ccV3F_C4B_T2F_Quad));
    m_pIndices = (GLushort*)malloc(m_uTotalParticles * 6 * sizeof(GLushort));

    if (m_pQuads && m_pIndices)
    {
        memset(m_pQuads, 0, m_uTotalParticles * sizeof(ccV3F_C4B_T2F_Quad));
        memset(m_pIndices, 0, m_uTotalParticles * 6 * sizeof(GLushort));
        return true;
    }

    if (m_pQuads)
    {
        free(m_pQuads);
        m_pQuads = NULL;
    }
    if (m_pIndices)
    {
        free(m_pIndices);
        m_pIndices = NULL;
    }
    return false;
}

void CCParticleSystemQuad::setBatchNode(CCParticleBatchNode *batchNode)
{
    if (m_pBatchNode != batchNode)
    {
        CCParticleBatchNode *oldBatch = m_pBatchNode;

        CCParticleSystem::setBatchNode(batchNode);

        if (batchNode)
        {
            if (!oldBatch)
            {
                ccV3F_C4B_T2F_Quad *batchQuads = m_pBatchNode->getTextureAtlas()->getQuads();
                memcpy(batchQuads + m_uAtlasIndex, m_pQuads, sizeof(m_pQuads[0]) * m_uTotalParticles);

                if (m_pQuads)
                {
                    free(m_pQuads);
                    m_pQuads = NULL;
                }
                if (m_pIndices)
                {
                    free(m_pIndices);
                    m_pIndices = NULL;
                }
                glDeleteBuffers(2, &m_pBuffersVBO[0]);
                m_pBuffersVBO[0] = 0;
                m_pBuffersVBO[1] = 0;
            }
        }
        else
        {
            allocMemory();
            initIndices();
            this->setTexture(oldBatch->getTexture());
            setupVBO();
        }
    }
}

void CCParticleSystemQuad::setOpacity(GLubyte opacity)
{
    m_uOpacity = opacity;
}

GLubyte CCParticleSystemQuad::getOpacity()
{
    return m_uOpacity;
}

void CCParticleSystemQuad::updateTexCoords()
{
    if (m_pTexture)
    {
        initTexCoordsWithRect(m_tTextureRect);
    }
}

NS_CC_END
