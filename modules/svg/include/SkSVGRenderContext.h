/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGRenderContext_DEFINED
#define SkSVGRenderContext_DEFINED

#include "include/core/SkFontMgr.h"
#include "include/core/SkFourByteTag.h"
#include "include/core/SkM44.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSize.h"
#include "include/core/SkTypes.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skshaper/include/SkShaper_factory.h"
#include "modules/svg/include/SkSVGAttribute.h"
#include "modules/svg/include/SkSVGIDMapper.h"
#include "modules/svg/include/SkSVGNode.h"
#include "modules/svg/include/SkSVGTypes.h"
#include "src/base/SkTLazy.h"
#include "src/core/SkTHash.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

class SkCanvas;
class SkPaint;
class SkString;
class SkUnicode;
namespace skresources { class ResourceProvider; }

class SK_API SkSVGLengthContext {
public:
    SkSVGLengthContext(const SkSize& viewport, SkScalar dpi = 90)
        : fViewport(viewport), fDPI(dpi) {}

    enum class LengthType {
        kHorizontal,
        kVertical,
        kOther,
    };

    const SkSize& viewPort() const { return fViewport; }
    void setViewPort(const SkSize& viewport) { fViewport = viewport; }

    SkScalar resolve(const SkSVGLength&, LengthType) const;
    SkRect   resolveRect(const SkSVGLength& x, const SkSVGLength& y,
                         const SkSVGLength& w, const SkSVGLength& h) const;

private:
    SkSize   fViewport;
    SkScalar fDPI;
};

struct SK_API SkSVGPresentationContext {
    SkSVGPresentationContext();
    SkSVGPresentationContext(const SkSVGPresentationContext&)            = default;
    SkSVGPresentationContext& operator=(const SkSVGPresentationContext&) = default;

    const skia_private::THashMap<SkString, SkSVGColorType>* fNamedColors = nullptr;

    // Inherited presentation attributes, computed for the current node.
    SkSVGPresentationAttributes fInherited;
    // Context paints are resolved in the context element's coordinate and OBB scope before
    // entering a marker or use instance. Besides preserving paint-server coordinates, storing
    // concrete paints makes recursive context-fill/context-stroke references terminate when the
    // outermost context element has no context paint of its own. The source CTM lets shaders
    // compensate for transforms subsequently established by the instance subtree.
    SkTLazy<SkPaint> fContextFill;
    SkTLazy<SkPaint> fContextStroke;
    SkMatrix         fContextPaintCTM = SkMatrix::I();
};

class SK_API SkSVGRenderContext {
public:
    // Captures data required for object bounding box resolution.
    struct OBBScope {
        const SkSVGNode*          fNode;
        const SkSVGRenderContext* fCtx;
    };

    SkSVGRenderContext(SkCanvas*,
                       const sk_sp<SkFontMgr>&,
                       const sk_sp<skresources::ResourceProvider>&,
                       const SkSVGIDMapper&,
                       const SkSVGLengthContext&,
                       const SkSVGPresentationContext&,
                       const OBBScope&,
                       const sk_sp<SkShapers::Factory>&);
    SkSVGRenderContext(const SkSVGRenderContext&);
    SkSVGRenderContext(const SkSVGRenderContext&, SkCanvas*);
    // Establish a new OBB scope.  Normally used when entering a node's render scope.
    SkSVGRenderContext(const SkSVGRenderContext&, const SkSVGNode*);
    ~SkSVGRenderContext();

    const SkSVGLengthContext& lengthContext() const { return *fLengthContext; }
    SkSVGLengthContext* writableLengthContext() { return fLengthContext.writable(); }

    const SkSVGPresentationContext& presentationContext() const { return *fPresentationContext; }

    void setInheritedPresentation(const SkSVGPresentationAttributes& inherited) {
        fPresentationContext.writable()->fInherited = inherited;
    }

    void setContextPaints(const SkSVGRenderContext& contextElement);

    SkCanvas* canvas() const { return fCanvas; }
    void saveOnce();

    // Used while replaying a CSS drop-shadow as vector drawing commands. Suppression is scoped
    // to one node so filters on descendants still render normally.
    void suppressFilterForNode(const SkSVGNode* node) { fSuppressedFilterNode = node; }
    bool isFilterSuppressedForNode(const SkSVGNode* node) const {
        return fSuppressedFilterNode == node;
    }

    // Records a CSS filter offset after the target node has established its local coordinate
    // system. This keeps filter lengths subject to the node's transform and viewport mapping.
    void setDropShadowOffsetForNode(const SkSVGNode* node, SkScalar dx, SkScalar dy) {
        fDropShadowOffsetNode = node;
        fDropShadowOffsetX = dx;
        fDropShadowOffsetY = dy;
    }
    void applyDropShadowOffsetForNode(const SkSVGNode* node);

    enum ApplyFlags {
        kLeaf = 1 << 0, // the target node doesn't have descendants
    };
    void applyPresentationAttributes(const SkSVGPresentationAttributes&, uint32_t flags);

    // Scoped wrapper that temporarily clears the original node reference.
    class BorrowedNode {
    public:
        explicit BorrowedNode(sk_sp<SkSVGNode>* node)
            : fOwner(node) {
            if (fOwner) {
                fBorrowed = std::move(*fOwner);
                *fOwner = nullptr;
            }
        }

        ~BorrowedNode() {
            if (fOwner) {
                *fOwner = std::move(fBorrowed);
            }
        }

        const SkSVGNode* get() const { return fBorrowed.get(); }
        const SkSVGNode* operator->() const { return fBorrowed.get(); }
        const SkSVGNode& operator*() const { return *fBorrowed; }

        explicit operator bool() const { return !!fBorrowed; }

    private:
        // noncopyable
        BorrowedNode(const BorrowedNode&)      = delete;
        BorrowedNode& operator=(BorrowedNode&) = delete;

        sk_sp<SkSVGNode>* fOwner;
        sk_sp<SkSVGNode>  fBorrowed;
    };

    // Note: the id->node association is cleared for the lifetime of the returned value
    // (effectively breaks reference cycles, assuming appropriate return value scoping).
    BorrowedNode findNodeById(const SkSVGIRI&) const;

    SkTLazy<SkPaint> fillPaint() const;
    SkTLazy<SkPaint> strokePaint() const;

    SkSVGColorType resolveSvgColor(const SkSVGColor&) const;

    // The local computed clip path (not inherited).
    const SkPath* clipPath() const { return fClipPath.getMaybeNull(); }

    const sk_sp<skresources::ResourceProvider>& resourceProvider() const {
        return fResourceProvider;
    }

    sk_sp<SkFontMgr> fontMgr() const {
        // It is probably an oversight to try to render <text> without having set the SkFontMgr.
        // We will assert this in debug mode, but fallback to an empty fontmgr in release builds.
        SkASSERT(fFontMgr);
        return fFontMgr ? fFontMgr : SkFontMgr::RefEmpty();
    }

    // Returns the translate/scale transformation required to map into the current OBB scope,
    // with the specified units.
    struct OBBTransform {
        SkV2 offset, scale;
    };
    OBBTransform transformForCurrentOBB(SkSVGObjectBoundingBoxUnits) const;

    SkRect resolveOBBRect(const SkSVGLength& x, const SkSVGLength& y,
                          const SkSVGLength& w, const SkSVGLength& h,
                          SkSVGObjectBoundingBoxUnits) const;

    const OBBScope& currentOBBScope() const { return fOBBScope; }

    std::unique_ptr<SkShaper> makeShaper() const {
        SkASSERT(fTextShapingFactory);
        return fTextShapingFactory->makeShaper(this->fontMgr());
    }

    SkUnicode* unicode() const {
        SkASSERT(fTextShapingFactory);
        return fTextShapingFactory->getUnicode();
    }

    std::unique_ptr<SkShaper::BiDiRunIterator> makeBidiRunIterator(const char* utf8,
                                                                   size_t utf8Bytes,
                                                                   uint8_t bidiLevel) const {
        SkASSERT(fTextShapingFactory);
        return fTextShapingFactory->makeBidiRunIterator(utf8, utf8Bytes, bidiLevel);
    }

    std::unique_ptr<SkShaper::ScriptRunIterator> makeScriptRunIterator(const char* utf8,
                                                                       size_t utf8Bytes) const {
        SkASSERT(fTextShapingFactory);
        constexpr SkFourByteTag unknownScript = SkSetFourByteTag('Z', 'z', 'z', 'z');
        return fTextShapingFactory->makeScriptRunIterator(utf8, utf8Bytes, unknownScript);
    }

private:
    // Stack-only
    void* operator new(size_t)                               = delete;
    void* operator new(size_t, void*)                        = delete;
    SkSVGRenderContext& operator=(const SkSVGRenderContext&) = delete;

    void applyOpacity(SkScalar opacity, uint32_t flags, bool hasFilter);
    void applyFilter(const SkSVGFuncIRI&);
    void applyClip(const SkSVGFuncIRI&);
    void applyMask(const SkSVGFuncIRI&);

    SkTLazy<SkPaint> commonPaint(const SkSVGPaint&, float opacity) const;

    const sk_sp<SkFontMgr>&                       fFontMgr;
    const sk_sp<SkShapers::Factory>&              fTextShapingFactory;
    const sk_sp<skresources::ResourceProvider>&   fResourceProvider;
    const SkSVGIDMapper&                          fIDMapper;
    SkTCopyOnFirstWrite<SkSVGLengthContext>       fLengthContext;
    SkTCopyOnFirstWrite<SkSVGPresentationContext> fPresentationContext;
    SkCanvas*                                     fCanvas;
    // The save count on 'fCanvas' at construction time.
    // A restoreToCount() will be issued on destruction.
    int                                           fCanvasSaveCount;

    // clipPath, if present for the current context (not inherited).
    SkTLazy<SkPath>                               fClipPath;

    const SkSVGNode*                              fSuppressedFilterNode = nullptr;
    const SkSVGNode*                              fDropShadowOffsetNode = nullptr;
    SkScalar                                      fDropShadowOffsetX = 0;
    SkScalar                                      fDropShadowOffsetY = 0;

    // Deferred opacity optimization for leaf nodes.
    float                                         fDeferredPaintOpacity = 1;

    // Current object bounding box scope.
    const OBBScope                                fOBBScope;
};

#endif // SkSVGRenderContext_DEFINED
