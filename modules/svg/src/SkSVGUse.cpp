/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "modules/svg/include/SkSVGUse.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkScalar.h"
#include "include/private/base/SkDebug.h"
#include "modules/svg/include/SkSVGAttributeParser.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "modules/svg/include/SkSVGSymbol.h"

SkSVGUse::SkSVGUse() : INHERITED(SkSVGTag::kUse) {}

void SkSVGUse::appendChild(sk_sp<SkSVGNode>) {
    SkDEBUGF("cannot append child nodes to this element.\n");
}

bool SkSVGUse::parseAndSetAttribute(const char* n, const char* v) {
    return INHERITED::parseAndSetAttribute(n, v) ||
           this->setX(SkSVGAttributeParser::parse<SkSVGLength>("x", n, v)) ||
           this->setY(SkSVGAttributeParser::parse<SkSVGLength>("y", n, v)) ||
           this->setWidth(SkSVGAttributeParser::parse<SkSVGLength>("width", n, v)) ||
           this->setHeight(SkSVGAttributeParser::parse<SkSVGLength>("height", n, v)) ||
           this->setHref(SkSVGAttributeParser::parse<SkSVGIRI>("href", n, v)) ||
           this->setHref(SkSVGAttributeParser::parse<SkSVGIRI>("xlink:href", n, v));
}

bool SkSVGUse::onPrepareToRender(SkSVGRenderContext* ctx) const {
    if (fHref.iri().isEmpty() || !INHERITED::onPrepareToRender(ctx)) {
        return false;
    }

    const auto& lctx = ctx->lengthContext();
    const SkScalar x = lctx.resolve(fX, SkSVGLengthContext::LengthType::kHorizontal);
    const SkScalar y = lctx.resolve(fY, SkSVGLengthContext::LengthType::kVertical);
    if (x || y) {
        // Restored when the local SkSVGRenderContext leaves scope.
        ctx->saveOnce();
        ctx->canvas()->translate(x, y);
    }

    // TODO: width/height override for <svg> targets.

    return true;
}

void SkSVGUse::onRender(const SkSVGRenderContext& ctx) const {
    const auto ref = ctx.findNodeById(fHref);
    if (!ref) {
        return;
    }

    SkSVGRenderContext useContext(ctx);
    useContext.setContextPaints(ctx);

    if (ref->tag() == SkSVGTag::kSymbol) {
        const auto& lctx = useContext.lengthContext();
        const SkScalar width = lctx.resolve(fWidth,
                                            SkSVGLengthContext::LengthType::kHorizontal);
        const SkScalar height = lctx.resolve(fHeight,
                                             SkSVGLengthContext::LengthType::kVertical);
        static_cast<const SkSVGSymbol*>(ref.get())->renderSymbol(useContext, width, height);
        return;
    }

    ref->render(useContext);
}

SkPath SkSVGUse::onAsPath(const SkSVGRenderContext& ctx) const {
    const auto ref = ctx.findNodeById(fHref);
    if (!ref) {
        return SkPath();
    }

    SkPath path;
    if (ref->tag() == SkSVGTag::kSymbol) {
        const auto& lctx = ctx.lengthContext();
        const SkScalar width = lctx.resolve(fWidth,
                                            SkSVGLengthContext::LengthType::kHorizontal);
        const SkScalar height = lctx.resolve(fHeight,
                                             SkSVGLengthContext::LengthType::kVertical);
        path = static_cast<const SkSVGSymbol*>(ref.get())->asPath(ctx, width, height);
    } else {
        path = ref->asPath(ctx);
    }

    const auto& lctx = ctx.lengthContext();
    path.offset(lctx.resolve(fX, SkSVGLengthContext::LengthType::kHorizontal),
                lctx.resolve(fY, SkSVGLengthContext::LengthType::kVertical));
    this->mapToParent(&path);
    return path;
}

SkRect SkSVGUse::onTransformableObjectBoundingBox(const SkSVGRenderContext& ctx) const {
    const auto ref = ctx.findNodeById(fHref);
    if (!ref) {
        return SkRect::MakeEmpty();
    }

    const SkSVGLengthContext& lctx = ctx.lengthContext();
    const SkScalar x = lctx.resolve(fX, SkSVGLengthContext::LengthType::kHorizontal);
    const SkScalar y = lctx.resolve(fY, SkSVGLengthContext::LengthType::kVertical);

    SkRect bounds;
    if (ref->tag() == SkSVGTag::kSymbol) {
        const SkScalar width = lctx.resolve(fWidth,
                                            SkSVGLengthContext::LengthType::kHorizontal);
        const SkScalar height = lctx.resolve(fHeight,
                                             SkSVGLengthContext::LengthType::kVertical);
        bounds = static_cast<const SkSVGSymbol*>(ref.get())
                         ->objectBoundingBox(ctx, width, height);
    } else {
        bounds = ref->objectBoundingBox(ctx);
    }
    bounds.offset(x, y);

    return bounds;
}
