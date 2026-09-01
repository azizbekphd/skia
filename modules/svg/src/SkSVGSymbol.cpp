/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "modules/svg/include/SkSVGSymbol.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkRect.h"
#include "include/core/SkSize.h"
#include "include/pathops/SkPathOps.h"
#include "modules/svg/include/SkSVGAttributeParser.h"
#include "modules/svg/include/SkSVGRenderContext.h"

#include <cstring>

SkMatrix SkSVGSymbol::viewBoxTransform(SkScalar width, SkScalar height) const {
    return fViewBox.isValid()
                   ? ComputeViewboxMatrix(*fViewBox,
                                          SkRect::MakeWH(width, height),
                                          fPreserveAspectRatio)
                   : SkMatrix::I();
}

bool SkSVGSymbol::parseAndSetAttribute(const char* name, const char* value) {
    if (INHERITED::parseAndSetAttribute(name, value) ||
        this->setOverflow(
                SkSVGAttributeParser::parse<SkSVGOverflow>("overflow", name, value))) {
        return true;
    }
    if (!strcmp(name, "viewBox")) {
        SkSVGViewBoxType viewBox;
        if (SkSVGAttributeParser(value).parseViewBox(&viewBox)) {
            this->setViewBox(viewBox);
            return true;
        }
        return false;
    }
    if (!strcmp(name, "preserveAspectRatio")) {
        SkSVGPreserveAspectRatio par;
        if (SkSVGAttributeParser(value).parsePreserveAspectRatio(&par)) {
            this->setPreserveAspectRatio(par);
            return true;
        }
        return false;
    }
    return false;
}

void SkSVGSymbol::renderSymbol(const SkSVGRenderContext& ctx,
                               SkScalar width,
                               SkScalar height) const {
    if (!(width > 0 && height > 0)) {
        return;
    }

    SkSVGRenderContext symbolContext(ctx, this);
    symbolContext.writableLengthContext()->setViewPort(SkSize::Make(width, height));
    if (!this->SkSVGTransformableNode::onPrepareToRender(&symbolContext)) {
        return;
    }

    SkCanvas* canvas = symbolContext.canvas();
    symbolContext.saveOnce();
    if (fOverflow == SkSVGOverflow::kClip) {
        canvas->clipRect(SkRect::MakeWH(width, height), true);
    }
    if (fViewBox.isValid()) {
        if (fViewBox->isEmpty()) {
            return;
        }
        canvas->concat(this->viewBoxTransform(width, height));
        symbolContext.writableLengthContext()->setViewPort(
                SkSize::Make(fViewBox->width(), fViewBox->height()));
    }

    this->SkSVGContainer::onRender(symbolContext);
}

SkPath SkSVGSymbol::asPath(const SkSVGRenderContext& ctx,
                           SkScalar width,
                           SkScalar height) const {
    if (!(width > 0 && height > 0) || (fViewBox.isValid() && fViewBox->isEmpty())) {
        return {};
    }

    SkSVGRenderContext symbolContext(ctx, this);
    symbolContext.writableLengthContext()->setViewPort(SkSize::Make(width, height));
    if (!this->SkSVGTransformableNode::onPrepareToRender(&symbolContext)) {
        return {};
    }
    if (fViewBox.isValid()) {
        symbolContext.writableLengthContext()->setViewPort(
                SkSize::Make(fViewBox->width(), fViewBox->height()));
    }

    SkPath path = this->SkSVGContainer::onAsPath(symbolContext);
    if (!this->mapToLocal(&path)) {
        return {};
    }
    path.transform(this->viewBoxTransform(width, height));

    if (fOverflow == SkSVGOverflow::kClip) {
        const SkPath viewport = SkPath::Rect(SkRect::MakeWH(width, height));
        Op(path, viewport, kIntersect_SkPathOp, &path);
    }
    this->mapToParent(&path);
    return path;
}

SkRect SkSVGSymbol::objectBoundingBox(const SkSVGRenderContext& ctx,
                                      SkScalar width,
                                      SkScalar height) const {
    if (!(width > 0 && height > 0) || (fViewBox.isValid() && fViewBox->isEmpty())) {
        return SkRect::MakeEmpty();
    }

    SkSVGRenderContext symbolContext(ctx, this);
    symbolContext.writableLengthContext()->setViewPort(SkSize::Make(width, height));
    if (!this->SkSVGTransformableNode::onPrepareToRender(&symbolContext)) {
        return SkRect::MakeEmpty();
    }
    if (fViewBox.isValid()) {
        symbolContext.writableLengthContext()->setViewPort(
                SkSize::Make(fViewBox->width(), fViewBox->height()));
    }

    SkRect bounds = this->SkSVGContainer::onTransformableObjectBoundingBox(symbolContext);
    bounds = this->viewBoxTransform(width, height).mapRect(bounds);
    this->mapToParent(&bounds);
    return bounds;
}
