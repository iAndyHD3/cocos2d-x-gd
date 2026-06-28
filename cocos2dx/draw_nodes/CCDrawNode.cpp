/* Copyright (c) 2012 Scott Lembcke and Howling Moon Software
 * Copyright (c) 2012 .org
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "CCDrawNode.h"
#include "support/CCPointExtension.h"
#include "shaders/CCShaderCache.h"
#include "CCGL.h"
#include "support/CCNotificationCenter.h"
#include "CCEventType.h"

NS_CC_BEGIN

// ccVertex2F == CGPoint in 32-bits, but not in 64-bits (OS X)
// that's why the "v2f" functions are needed
static ccVertex2F v2fzero = {0.0f,0.0f};

static inline ccVertex2F v2f(float x, float y)
{
    ccVertex2F ret = {x, y};
	return ret;
}

static inline ccVertex2F v2fadd(const ccVertex2F &v0, const ccVertex2F &v1)
{
	return v2f(v0.x+v1.x, v0.y+v1.y);
}

static inline ccVertex2F v2fsub(const ccVertex2F &v0, const ccVertex2F &v1)
{
	return v2f(v0.x-v1.x, v0.y-v1.y);
}

static inline ccVertex2F v2fmult(const ccVertex2F &v, float s)
{
	return v2f(v.x * s, v.y * s);
}

static inline ccVertex2F v2fperp(const ccVertex2F &p0)
{
	return v2f(-p0.y, p0.x);
}

static inline ccVertex2F v2fneg(const ccVertex2F &p0)
{
	return v2f(-p0.x, - p0.y);
}

static inline float v2fdot(const ccVertex2F &p0, const ccVertex2F &p1)
{
	return  p0.x * p1.x + p0.y * p1.y;
}

static inline ccVertex2F v2fforangle(float _a_)
{
	return v2f(cosf(_a_), sinf(_a_));
}

static inline ccVertex2F v2fnormalize(const ccVertex2F &p)
{
	CCPoint r = ccpNormalize(ccp(p.x, p.y));
	return v2f(r.x, r.y);
}

static inline ccVertex2F __v2f(const CCPoint &v)
{
//#ifdef __LP64__
	return v2f(v.x, v.y);
// #else
// 	return * ((ccVertex2F*) &v);
// #endif
}

static inline ccTex2F __t(const ccVertex2F &v)
{
	return *(ccTex2F*)&v;
}

// implementation of CCDrawNode

CCDrawNode::CCDrawNode()
: m_uVao(0)
, m_uVbo(0)
, m_uBufferCapacity(0)
, m_nBufferCount(0)
, m_pBuffer(NULL)
, m_bDirty(false)
{
    m_sBlendFunc.src = CC_BLEND_SRC;
    m_sBlendFunc.dst = CC_BLEND_DST;
}

CCDrawNode::~CCDrawNode()
{
    free(m_pBuffer);
    m_pBuffer = NULL;
    
    glDeleteBuffers(1, &m_uVbo);
    m_uVbo = 0;
    
#if CC_TEXTURE_ATLAS_USE_VAO      
    glDeleteVertexArrays(1, &m_uVao);
    ccGLBindVAO(0);
    m_uVao = 0;
#endif

#if CC_ENABLE_CACHE_TEXTURE_DATA
    CCNotificationCenter::sharedNotificationCenter()->removeObserver(this, EVENT_COME_TO_FOREGROUND);
#endif    
}

CCDrawNode* CCDrawNode::create()
{
    CCDrawNode* pRet = new CCDrawNode();
    if (pRet && pRet->init())
    {
        pRet->autorelease();
    }
    else
    {
        CC_SAFE_DELETE(pRet);
    }
    
    return pRet;
}

void CCDrawNode::ensureCapacity(unsigned int count)
{
    if(m_nBufferCount + count > m_uBufferCapacity)
    {
		m_uBufferCapacity += MAX(m_uBufferCapacity, count);
		m_pBuffer = (ccV2F_C4B_T2F*)realloc(m_pBuffer, m_uBufferCapacity*sizeof(ccV2F_C4B_T2F));
	}
}

bool CCDrawNode::init()
{
    m_sBlendFunc.src = CC_BLEND_SRC;
    m_sBlendFunc.dst = CC_BLEND_DST;

    setShaderProgram(CCShaderCache::sharedShaderCache()->programForKey(kCCShader_PositionLengthTexureColor));
    
    ensureCapacity(512);
    
#if CC_TEXTURE_ATLAS_USE_VAO    
    glGenVertexArrays(1, &m_uVao);
    ccGLBindVAO(m_uVao);
#endif
    
    glGenBuffers(1, &m_uVbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_uVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(ccV2F_C4B_T2F)* m_uBufferCapacity, m_pBuffer, GL_STREAM_DRAW);
    
    glEnableVertexAttribArray(kCCVertexAttrib_Position);
    glVertexAttribPointer(kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE, sizeof(ccV2F_C4B_T2F), (GLvoid *)offsetof(ccV2F_C4B_T2F, vertices));
    
    glEnableVertexAttribArray(kCCVertexAttrib_Color);
    glVertexAttribPointer(kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ccV2F_C4B_T2F), (GLvoid *)offsetof(ccV2F_C4B_T2F, colors));
    
    glEnableVertexAttribArray(kCCVertexAttrib_TexCoords);
    glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, sizeof(ccV2F_C4B_T2F), (GLvoid *)offsetof(ccV2F_C4B_T2F, texCoords));
    
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
#if CC_TEXTURE_ATLAS_USE_VAO 
    ccGLBindVAO(0);
#endif
    
    CHECK_GL_ERROR_DEBUG();
    
    m_bDirty = true;

#if CC_ENABLE_CACHE_TEXTURE_DATA
    // Need to listen the event only when not use batchnode, because it will use VBO
    CCNotificationCenter::sharedNotificationCenter()->addObserver(this,
            callfuncO_selector(CCDrawNode::listenBackToForeground),
            EVENT_COME_TO_FOREGROUND,
            NULL);
#endif

    return true;
}

void CCDrawNode::render()
{
    if (m_bDirty)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_uVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(ccV2F_C4B_T2F)*m_uBufferCapacity, m_pBuffer, GL_STREAM_DRAW);
        m_bDirty = false;
    }
#if CC_TEXTURE_ATLAS_USE_VAO     
    ccGLBindVAO(m_uVao);
#else
    ccGLEnableVertexAttribs(kCCVertexAttribFlag_PosColorTex);
    glBindBuffer(GL_ARRAY_BUFFER, m_uVbo);
    // vertex
    glVertexAttribPointer(kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE, sizeof(ccV2F_C4B_T2F), (GLvoid *)offsetof(ccV2F_C4B_T2F, vertices));
    
    // color
    glVertexAttribPointer(kCCVertexAttrib_Color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ccV2F_C4B_T2F), (GLvoid *)offsetof(ccV2F_C4B_T2F, colors));
    
    // texcood
    glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, sizeof(ccV2F_C4B_T2F), (GLvoid *)offsetof(ccV2F_C4B_T2F, texCoords));
#endif

    glDrawArrays(GL_TRIANGLES, 0, m_nBufferCount);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    CC_INCREMENT_GL_DRAWS(1);
    CHECK_GL_ERROR_DEBUG();
}

void CCDrawNode::draw()
{
    CC_NODE_DRAW_SETUP();
    ccGLBlendFunc(m_sBlendFunc.src, m_sBlendFunc.dst);
    
    render();
}

bool CCDrawNode::drawDot(const CCPoint &pos, float radius, const ccColor4F &color)
{
    unsigned int vertex_count = 2*3;
    ensureCapacity(vertex_count);
	
	ccV2F_C4B_T2F a = {{pos.x - radius, pos.y - radius}, ccc4BFromccc4F(color), {-1.0, -1.0} };
	ccV2F_C4B_T2F b = {{pos.x - radius, pos.y + radius}, ccc4BFromccc4F(color), {-1.0,  1.0} };
	ccV2F_C4B_T2F c = {{pos.x + radius, pos.y + radius}, ccc4BFromccc4F(color), { 1.0,  1.0} };
	ccV2F_C4B_T2F d = {{pos.x + radius, pos.y - radius}, ccc4BFromccc4F(color), { 1.0, -1.0} };
	
	ccV2F_C4B_T2F_Triangle *triangles = (ccV2F_C4B_T2F_Triangle *)(m_pBuffer + m_nBufferCount);
    ccV2F_C4B_T2F_Triangle triangle0 = {a, b, c};
    ccV2F_C4B_T2F_Triangle triangle1 = {a, c, d};
	triangles[0] = triangle0;
	triangles[1] = triangle1;
	
	m_nBufferCount += vertex_count;
	
	m_bDirty = true;

    // TODO: implement
    return true;
}

bool CCDrawNode::drawSegment(const CCPoint &from, const CCPoint &to, float radius, const ccColor4F &color)
{
    return drawSegmentEx(from, to, radius, color, true, true);
}

bool CCDrawNode::drawSegmentEx(const CCPoint &from, const CCPoint &to, float radius, const ccColor4F &color, bool fill, bool close)
{
    if (m_bUseArea && !is_segment_on_screen(m_rDrawArea, from, to))
        return false;

    // Base body is 6 vertices (2 triangles). Each cap adds 6 more.
    unsigned int vertex_count = 6;
    if (close) vertex_count += 6;
    if (fill)  vertex_count += 6;

    ensureCapacity(vertex_count);

    ccVertex2F a = __v2f(from);
    ccVertex2F b = __v2f(to);

    ccVertex2F n = v2fnormalize(v2fperp(v2fsub(b, a)));
    ccVertex2F t = v2fperp(n);

    ccVertex2F nw = v2fmult(n, radius);
    ccVertex2F tw = v2fmult(t, radius);
    ccVertex2F v0 = v2fsub(b, v2fadd(nw, tw));
    ccVertex2F v1 = v2fadd(b, v2fsub(nw, tw));
    ccVertex2F v2 = v2fsub(b, nw);
    ccVertex2F v3 = v2fadd(b, nw);
    ccVertex2F v4 = v2fsub(a, nw);
    ccVertex2F v5 = v2fadd(a, nw);
    ccVertex2F v6 = v2fsub(a, v2fsub(nw, tw));
    ccVertex2F v7 = v2fadd(a, v2fadd(nw, tw));

    ccV2F_C4B_T2F_Triangle *triangles = (ccV2F_C4B_T2F_Triangle *)(m_pBuffer + m_nBufferCount);
    ccColor4B c4b = ccc4BFromccc4F(color);
    int triIndex = 0;

    // Draw end cap
    if (close)
    {
        triangles[triIndex++] = {
            {v0, c4b, __t(v2fneg(v2fadd(n, t)))},
            {v1, c4b, __t(v2fsub(n, t))},
            {v2, c4b, __t(v2fneg(n))},
        };
        triangles[triIndex++] = {
            {v3, c4b, __t(n)},
            {v1, c4b, __t(v2fsub(n, t))},
            {v2, c4b, __t(v2fneg(n))},
        };
    }

    // Draw segment body
    triangles[triIndex++] = {
        {v3, c4b, __t(n)},
        {v4, c4b, __t(v2fneg(n))},
        {v2, c4b, __t(v2fneg(n))},
    };
    triangles[triIndex++] = {
        {v3, c4b, __t(n)},
        {v4, c4b, __t(v2fneg(n))},
        {v5, c4b, __t(n)},
    };

    // Draw start cap
    if (fill)
    {
        triangles[triIndex++] = {
            {v6, c4b, __t(v2fsub(t, n))},
            {v4, c4b, __t(v2fneg(n))},
            {v5, c4b, __t(n)},
        };
        triangles[triIndex++] = {
            {v6, c4b, __t(v2fsub(t, n))},
            {v7, c4b, __t(v2fadd(n, t))},
            {v5, c4b, __t(n)},
        };
    }

    m_nBufferCount += vertex_count;
    m_bDirty = true;

    return true;
}

void CCDrawNode::drawArchLikeHalfCircle(const CCPoint& p1, const CCPoint& p2, float radius, unsigned int segments, const ccColor4F& color, float width) {
    CCPoint cp1;
    CCPoint cp2;

    CCPoint diff = p2 - p1;
    float length = diff.getLength(); // Maps to ccpLength

    // Epsilon check
    if (length <= 0.0001f) {
        cp1 = p1;
        cp2 = p1;
    } else {
        // Normalize the direction vector
        CCPoint dir = diff * (1.0f / length);
        
        // Calculate the normal perpendicular vector (-y, x)
        CCPoint normal(-dir.y, dir.x);

        // Control points are offset at 1/3 and 2/3 along the segment, then extruded by the radius
        CCPoint offset1 = p1 + (dir * (length * 0.33333f));
        CCPoint offset2 = p1 + (dir * (length * 0.66667f));

        cp1 = offset1 + (normal * radius);
        cp2 = offset2 + (normal * radius);
    }

    // Call the dashed cubic bezier function
    drawCubicBezierDashed(p1, cp1, cp2, p2, segments, color, width, 1, 1);
}

void cocos2d::CCDrawNode::drawCubicBezierDashed(
    cocos2d::CCPoint const& origin,
    cocos2d::CCPoint const& control1,
    cocos2d::CCPoint const& control2,
    cocos2d::CCPoint const& destination,
    unsigned int segments,
    cocos2d::_ccColor4F const& color,
    float radius,
    unsigned int dashLength,
    unsigned int dashGap)
{
    if (!segments) return;

    unsigned int count = segments + 1;
    CCPoint* points = new (std::nothrow) CCPoint[count];
    if (!points) return;

    // Compute cubic Bézier points
    float t = 0.0f;
    float step = 1.0f / static_cast<float>(segments);

    for (unsigned int i = 0; i < segments; i++) {
        float oneMinusT = 1.0f - t;
        float oneMinusT2 = oneMinusT * oneMinusT;
        float oneMinusT3 = oneMinusT2 * oneMinusT;
        float t2 = t * t;
        float t3 = t2 * t;

        points[i].x = oneMinusT3 * origin.x
                     + 3.0f * oneMinusT2 * t * control1.x
                     + 3.0f * oneMinusT * t2 * control2.x
                     + t3 * destination.x;
        points[i].y = oneMinusT3 * origin.y
                     + 3.0f * oneMinusT2 * t * control1.y
                     + 3.0f * oneMinusT * t2 * control2.y
                     + t3 * destination.y;

        t += step;
    }

    // Ensure the last point is exactly the destination
    points[segments] = destination;

    // Draw dashed segments
    unsigned int period = dashLength + dashGap;
    if (!period) period = 1;

    for (unsigned int i = 0; i < segments; i++) {
        if (i % period < dashLength) {
            this->drawSegmentEx(points[i], points[i + 1], radius, color, 1, 1);
        }
    }

    delete[] points;
}

bool CCDrawNode::drawPolygon(CCPoint *verts, unsigned int count, const ccColor4F &fillColor, float borderWidth, const ccColor4F &borderColor)
{
    struct ExtrudeVerts {ccVertex2F offset, n;};
	struct ExtrudeVerts* extrude = (struct ExtrudeVerts*)malloc(sizeof(struct ExtrudeVerts)*count);
	memset(extrude, 0, sizeof(struct ExtrudeVerts)*count);
	
	for(unsigned int i = 0; i < count; i++)
    {
		ccVertex2F v0 = __v2f(verts[(i-1+count)%count]);
		ccVertex2F v1 = __v2f(verts[i]);
		ccVertex2F v2 = __v2f(verts[(i+1)%count]);
        
		ccVertex2F n1 = v2fnormalize(v2fperp(v2fsub(v1, v0)));
		ccVertex2F n2 = v2fnormalize(v2fperp(v2fsub(v2, v1)));
		
		ccVertex2F offset = v2fmult(v2fadd(n1, n2), 1.0/(v2fdot(n1, n2) + 1.0));
        struct ExtrudeVerts tmp = {offset, n2};
		extrude[i] = tmp;
	}
	
	bool outline = (borderColor.a > 0.0 && borderWidth > 0.0);
	
	unsigned int triangle_count = 3*count - 2;
	unsigned int vertex_count = 3*triangle_count;
    ensureCapacity(vertex_count);
	
	ccV2F_C4B_T2F_Triangle *triangles = (ccV2F_C4B_T2F_Triangle *)(m_pBuffer + m_nBufferCount);
	ccV2F_C4B_T2F_Triangle *cursor = triangles;
	
	float inset = (outline == 0.0 ? 0.5 : 0.0);
	for(unsigned int i = 0; i < count-2; i++)
    {
		ccVertex2F v0 = v2fsub(__v2f(verts[0  ]), v2fmult(extrude[0  ].offset, inset));
		ccVertex2F v1 = v2fsub(__v2f(verts[i+1]), v2fmult(extrude[i+1].offset, inset));
		ccVertex2F v2 = v2fsub(__v2f(verts[i+2]), v2fmult(extrude[i+2].offset, inset));
		
        ccV2F_C4B_T2F_Triangle tmp = {
            {v0, ccc4BFromccc4F(fillColor), __t(v2fzero)},
            {v1, ccc4BFromccc4F(fillColor), __t(v2fzero)},
            {v2, ccc4BFromccc4F(fillColor), __t(v2fzero)},
        };

		*cursor++ = tmp;
	}
	
	for(unsigned int i = 0; i < count; i++)
    {
		int j = (i+1)%count;
		ccVertex2F v0 = __v2f(verts[i]);
		ccVertex2F v1 = __v2f(verts[j]);
		
		ccVertex2F n0 = extrude[i].n;
		
		ccVertex2F offset0 = extrude[i].offset;
		ccVertex2F offset1 = extrude[j].offset;
		
		if(outline)
        {
			ccVertex2F inner0 = v2fsub(v0, v2fmult(offset0, borderWidth));
			ccVertex2F inner1 = v2fsub(v1, v2fmult(offset1, borderWidth));
			ccVertex2F outer0 = v2fadd(v0, v2fmult(offset0, borderWidth));
			ccVertex2F outer1 = v2fadd(v1, v2fmult(offset1, borderWidth));
			
            ccV2F_C4B_T2F_Triangle tmp1 = {
                {inner0, ccc4BFromccc4F(borderColor), __t(v2fneg(n0))},
                {inner1, ccc4BFromccc4F(borderColor), __t(v2fneg(n0))},
                {outer1, ccc4BFromccc4F(borderColor), __t(n0)}
            };
			*cursor++ = tmp1;

            ccV2F_C4B_T2F_Triangle tmp2 = {
                {inner0, ccc4BFromccc4F(borderColor), __t(v2fneg(n0))},
                {outer0, ccc4BFromccc4F(borderColor), __t(n0)},
                {outer1, ccc4BFromccc4F(borderColor), __t(n0)}
            };
			*cursor++ = tmp2;
		}
        else {
			ccVertex2F inner0 = v2fsub(v0, v2fmult(offset0, 0.5));
			ccVertex2F inner1 = v2fsub(v1, v2fmult(offset1, 0.5));
			ccVertex2F outer0 = v2fadd(v0, v2fmult(offset0, 0.5));
			ccVertex2F outer1 = v2fadd(v1, v2fmult(offset1, 0.5));
			
            ccV2F_C4B_T2F_Triangle tmp1 = {
                {inner0, ccc4BFromccc4F(fillColor), __t(v2fzero)},
                {inner1, ccc4BFromccc4F(fillColor), __t(v2fzero)},
                {outer1, ccc4BFromccc4F(fillColor), __t(n0)}
            };
			*cursor++ = tmp1;

            ccV2F_C4B_T2F_Triangle tmp2 = {
                {inner0, ccc4BFromccc4F(fillColor), __t(v2fzero)},
                {outer0, ccc4BFromccc4F(fillColor), __t(n0)},
                {outer1, ccc4BFromccc4F(fillColor), __t(n0)}
            };
			*cursor++ = tmp2;
		}
	}
	
	m_nBufferCount += vertex_count;
	
	m_bDirty = true;

    free(extrude);

    // TODO: implement
    return true;
}

struct ExtrudeVerts {
    cocos2d::CCPoint offset;
    cocos2d::CCPoint n;
};

bool CCDrawNode::drawPolygon(cocos2d::CCPoint *verts, unsigned int count, const cocos2d::_ccColor4F &fillColor, float borderWidth, const cocos2d::_ccColor4F &borderColor, BorderAlignment borderAlignment)
{
    ExtrudeVerts* extrude = (ExtrudeVerts*)malloc(sizeof(ExtrudeVerts) * count);
    memset((void*)extrude, 0, sizeof(ExtrudeVerts) * count);

    if (count > 0)
    {
        for (unsigned int i = 0; i < count; i++)
        {
            cocos2d::CCPoint v0 = verts[(i - 1 + count) % count];
            cocos2d::CCPoint v1 = verts[i];
            cocos2d::CCPoint v2 = verts[(i + 1) % count];

            cocos2d::CCPoint n1 = cocos2d::ccpNormalize(cocos2d::ccpSub(v1, v0));
            cocos2d::CCPoint n2 = cocos2d::ccpNormalize(cocos2d::ccpSub(v2, v1));

            // Calculate perpendicular vectors
            cocos2d::CCPoint n1_p = ccp(-n1.y, n1.x);
            cocos2d::CCPoint n2_p = ccp(-n2.y, n2.x);

            float invLength = 1.0f / (cocos2d::ccpDot(n1_p, n2_p) + 1.0f);
            extrude[i].offset = cocos2d::ccpMult(cocos2d::ccpAdd(n1_p, n2_p), invLength);
            extrude[i].n = n2_p;
        }
    }

    bool outline = (borderColor.a > 0.0f && borderWidth > 0.0f);
    unsigned int requiredVertexCount = 3 * (count - 2) + 6 * count; // Resolves to: 9 * count - 6

    if (requiredVertexCount + m_nBufferCount > m_uBufferCapacity)
    {
        unsigned int newCapacity = m_uBufferCapacity;
        if (newCapacity <= requiredVertexCount)
        {
            newCapacity = requiredVertexCount;
        }
        
        m_uBufferCapacity += newCapacity;
        m_pBuffer = (cocos2d::ccV2F_C4B_T2F*)realloc(m_pBuffer, sizeof(cocos2d::ccV2F_C4B_T2F) * m_uBufferCapacity);
    }

    cocos2d::ccV2F_C4B_T2F* buffer = &m_pBuffer[m_nBufferCount];
    float inset = (outline == false) ? 0.5f : 0.0f;

    cocos2d::ccColor4B fillC4B = {
        (GLubyte)(fillColor.r * 255.0f),
        (GLubyte)(fillColor.g * 255.0f),
        (GLubyte)(fillColor.b * 255.0f),
        (GLubyte)(fillColor.a * 255.0f)
    };

    if (count != 2)
    {
        for (unsigned int i = 0; i < count - 2; i++)
        {
            cocos2d::CCPoint v0 = cocos2d::ccpSub(verts[0], cocos2d::ccpMult(extrude[0].offset, inset));
            cocos2d::CCPoint v1 = cocos2d::ccpSub(verts[i + 1], cocos2d::ccpMult(extrude[i + 1].offset, inset));
            cocos2d::CCPoint v2 = cocos2d::ccpSub(verts[i + 2], cocos2d::ccpMult(extrude[i + 2].offset, inset));

            cocos2d::ccTex2F texCoords = {0.0f, 0.0f};

            buffer[0].vertices = cocos2d::vertex2(v0.x, v0.y);
            buffer[0].colors = fillC4B;
            buffer[0].texCoords = texCoords;

            buffer[1].vertices = cocos2d::vertex2(v1.x, v1.y);
            buffer[1].colors = fillC4B;
            buffer[1].texCoords = texCoords;

            buffer[2].vertices = cocos2d::vertex2(v2.x, v2.y);
            buffer[2].colors = fillC4B;
            buffer[2].texCoords = texCoords;

            buffer += 3;
        }
    }

    float outScale = borderWidth;
    float inScale = -borderWidth;

    if (outline)
    {
        if ((int)borderAlignment == 1) // Inner alignment
        {
            outScale = 0.0f;
            inScale = borderWidth * -2.0f;
        }
        else if ((int)borderAlignment == 2) // Outer alignment
        {
            inScale = 0.0f;
            outScale = borderWidth * 2.0f;
        }
    }

    if (count > 0)
    {
        bool isFringe = !outline;
        cocos2d::ccColor4B borderC4B = {
            (GLubyte)(borderColor.r * 255.0f),
            (GLubyte)(borderColor.g * 255.0f),
            (GLubyte)(borderColor.b * 255.0f),
            (GLubyte)(borderColor.a * 255.0f)
        };

        for (unsigned int i = 0; i < count; i++)
        {
            int j = (i + 1) % count;

            cocos2d::CCPoint v0 = verts[i];
            cocos2d::CCPoint v1 = verts[j];
            cocos2d::CCPoint offset0 = extrude[i].offset;
            cocos2d::CCPoint offset1 = extrude[j].offset;

            cocos2d::ccColor4B c0, c1;
            cocos2d::CCPoint inner0, inner1, outer0, outer1;
            cocos2d::ccTex2F texCoords = {0.0f, 0.0f};

            if (isFringe)
            {
                inner0 = cocos2d::ccpSub(v0, cocos2d::ccpMult(offset0, 0.5f));
                inner1 = cocos2d::ccpSub(v1, cocos2d::ccpMult(offset1, 0.5f));
                outer0 = cocos2d::ccpAdd(v0, cocos2d::ccpMult(offset0, 0.5f));
                outer1 = cocos2d::ccpAdd(v1, cocos2d::ccpMult(offset1, 0.5f));

                c0 = fillC4B;
                c1 = { fillC4B.r, fillC4B.g, fillC4B.b, 0 };
            }
            else
            {
                inner0 = cocos2d::ccpAdd(v0, cocos2d::ccpMult(offset0, inScale));
                inner1 = cocos2d::ccpAdd(v1, cocos2d::ccpMult(offset1, inScale));
                outer0 = cocos2d::ccpAdd(v0, cocos2d::ccpMult(offset0, outScale));
                outer1 = cocos2d::ccpAdd(v1, cocos2d::ccpMult(offset1, outScale));

                c0 = borderC4B;
                c1 = borderC4B;
            }

            buffer[0].vertices = cocos2d::vertex2(inner0.x, inner0.y);
            buffer[0].colors = c0;
            buffer[0].texCoords = texCoords;

            buffer[1].vertices = cocos2d::vertex2(inner1.x, inner1.y);
            buffer[1].colors = c0;
            buffer[1].texCoords = texCoords;

            buffer[2].vertices = cocos2d::vertex2(outer1.x, outer1.y);
            buffer[2].colors = c1;
            buffer[2].texCoords = texCoords;

            buffer[3].vertices = cocos2d::vertex2(inner0.x, inner0.y);
            buffer[3].colors = c0;
            buffer[3].texCoords = texCoords;

            buffer[4].vertices = cocos2d::vertex2(outer0.x, outer0.y);
            buffer[4].colors = c1;
            buffer[4].texCoords = texCoords;

            buffer[5].vertices = cocos2d::vertex2(outer1.x, outer1.y);
            buffer[5].colors = c1;
            buffer[5].texCoords = texCoords;

            buffer += 6;
        }
    }

    m_nBufferCount += requiredVertexCount;
    m_bDirty = true;
    free(extrude);

    return true;
}


void CCDrawNode::clear()
{
    m_nBufferCount = 0;
    m_bDirty = true;
}

ccBlendFunc CCDrawNode::getBlendFunc() const
{
    return m_sBlendFunc;
}

void CCDrawNode::setBlendFunc(const ccBlendFunc &blendFunc)
{
    m_sBlendFunc = blendFunc;
}

/** listen the event that coming to foreground on Android
 */
void CCDrawNode::listenBackToForeground(CCObject *obj)
{
    init();
}

// rob

bool CCDrawNode::is_circle_on_screen(CCRect const& rect, CCPoint const& center, float radius) {
    if (center.x + radius < m_fMinAreaX) return false;
    if (center.x - radius > m_fMaxAreaX) return false;
    if (center.y + radius < m_fMinAreaY) return false;
    if (center.y - radius > m_fMaxAreaY) return false;
    return true;
}
bool CCDrawNode::is_segment_on_screen(CCRect const& rect, CCPoint const& from, CCPoint const& to) {
    if (from.x < m_fMinAreaX && to.x < m_fMinAreaX) return false;
    if (from.x > m_fMaxAreaX && to.x > m_fMaxAreaX) return false;
    if (from.y < m_fMinAreaY && to.y < m_fMinAreaY) return false;
    if (from.y > m_fMaxAreaY && to.y > m_fMaxAreaY) return false;
    return true;
}

void CCDrawNode::disableDrawArea(void) {
    m_bUseArea = false;
}
bool CCDrawNode::drawCircle(CCPoint const& center, float radius, struct _ccColor4F const& fillColor, float borderWidth, struct _ccColor4F const& borderColor, unsigned int segments) {
    if (m_bUseArea && !is_circle_on_screen(m_rDrawArea, center, radius)) return false;
    auto vertices = new CCPoint[segments];
    for (unsigned int i = 0; i < segments; i++) {
        float rads = i * (2.0f * M_PI / segments);
        vertices[i].x = radius * cosf(rads) + center.x;
        vertices[i].y = radius * sinf(rads) + center.y;
    }
    drawPolygon(vertices, segments, fillColor, borderWidth, borderColor);
    delete[] vertices;
    return true;
}
bool CCDrawNode::drawLines(CCPoint* vertices, unsigned int count, float lineWidth, struct _ccColor4F const& color) {
    for (unsigned int i = 0; i < count; i += 2) {
        drawSegment(vertices[i], vertices[i + 1], lineWidth, color);
    }
    return true;
}
bool CCDrawNode::drawRect(CCPoint const& bottomLeft, CCPoint const& topRight, struct _ccColor4F const& fillColor, float borderWidth, struct _ccColor4F const& borderColor, BorderAlignment borderAlignment) {
    CCPoint verts[4] = {
        bottomLeft,
        ccp(topRight.x, bottomLeft.y),
        topRight,
        ccp(bottomLeft.x, topRight.y)
    };
    return drawPolygon(verts, 4, fillColor, borderWidth, borderColor, borderAlignment);
}

bool CCDrawNode::drawRect(class CCRect const& rect, struct _ccColor4F const& fillColor, float borderWidth, struct _ccColor4F const& borderColor, BorderAlignment borderAlignment) {
    CCPoint verts[4] = {
        ccp(rect.getMinX(), rect.getMinY()),
        ccp(rect.getMaxX(), rect.getMinY()),
        ccp(rect.getMaxX(), rect.getMaxY()),
        ccp(rect.getMinX(), rect.getMaxY())
    };
    return drawPolygon(verts, 4, fillColor, borderWidth, borderColor, borderAlignment);
}
void CCDrawNode::enableDrawArea(class CCRect& drawArea) {
    m_bUseArea = true;
    m_rDrawArea = drawArea;
    m_fMinAreaX = m_rDrawArea.getMinX();
    m_fMinAreaY = m_rDrawArea.getMinY();
    m_fMaxAreaX = m_rDrawArea.getMaxX();
    m_fMaxAreaY = m_rDrawArea.getMaxY();
}

NS_CC_END
