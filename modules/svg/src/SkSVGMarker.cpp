/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "modules/svg/include/SkSVGMarker.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkRect.h"
#include "include/core/SkSize.h"
#include "modules/svg/include/SkSVGAttributeParser.h"
#include "modules/svg/include/SkSVGRenderContext.h"

#include <cstring>

SkSVGMarker::SkSVGMarker() : INHERITED(SkSVGTag::kMarker) {}

bool SkSVGMarker::parseAndSetAttribute(const char* name, const char* value) {
    if (INHERITED::parseAndSetAttribute(name, value) ||
        this->setRefX(SkSVGAttributeParser::parse<SkSVGLength>("refX", name, value)) ||
        this->setRefY(SkSVGAttributeParser::parse<SkSVGLength>("refY", name, value)) ||
        this->setMarkerWidth(
                SkSVGAttributeParser::parse<SkSVGLength>("markerWidth", name, value)) ||
        this->setMarkerHeight(
                SkSVGAttributeParser::parse<SkSVGLength>("markerHeight", name, value)) ||
        this->setOverflow(
                SkSVGAttributeParser::parse<SkSVGOverflow>("overflow", name, value))) {
        return true;
    }

    if (!strcmp(name, "markerUnits")) {
        if (!strcmp(value, "strokeWidth")) {
            this->setMarkerUnits(SkSVGMarkerUnits::kStrokeWidth);
            return true;
        }
        if (!strcmp(value, "userSpaceOnUse")) {
            this->setMarkerUnits(SkSVGMarkerUnits::kUserSpaceOnUse);
            return true;
        }
        return false;
    }

    if (!strcmp(name, "orient")) {
        if (!strcmp(value, "auto")) {
            this->setOrient(SkSVGOrient(SkSVGOrient::Type::kAuto));
            return true;
        }
        if (!strcmp(value, "auto-start-reverse")) {
            this->setOrient(SkSVGOrient(SkSVGOrient::Type::kAutoStartReverse));
            return true;
        }
        auto angle = SkSVGAttributeParser::parse<SkSVGNumberType>(value);
        if (angle.isValid()) {
            this->setOrient(SkSVGOrient(*angle));
            return true;
        }
        return false;
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

void SkSVGMarker::renderMarker(const SkSVGRenderContext& ctx,
                               const SkPoint& position,
                               SkScalar autoAngle,
                               bool isStart) const {
    const auto& lctx = ctx.lengthContext();
    const SkScalar width = lctx.resolve(fMarkerWidth,
                                        SkSVGLengthContext::LengthType::kHorizontal);
    const SkScalar height = lctx.resolve(fMarkerHeight,
                                         SkSVGLengthContext::LengthType::kVertical);
    if (!(width > 0 && height > 0)) {
        return;
    }

    SkScalar angle = fOrient.angle();
    if (fOrient.type() != SkSVGOrient::Type::kAngle) {
        angle = autoAngle;
        if (isStart && fOrient.type() == SkSVGOrient::Type::kAutoStartReverse) {
            angle += 180;
        }
    }

    SkSVGRenderContext markerContext(ctx);
    // Marker content inherits from the marker definition, never from the referencing shape.
    // Context paints remain available explicitly through context-fill/context-stroke.
    markerContext.setInheritedPresentation(fInheritedPresentationAttributes);
    markerContext.setContextPaints(ctx);
    markerContext.writableLengthContext()->setViewPort(SkSize::Make(width, height));
    const auto& markerLengthContext = markerContext.lengthContext();
    SkSVGLengthContext referenceLengthContext(markerLengthContext);

    if (!this->SkSVGTransformableNode::onPrepareToRender(&markerContext)) {
        return;
    }

    SkCanvas* canvas = markerContext.canvas();
    markerContext.saveOnce();
    canvas->translate(position.x(), position.y());
    canvas->rotate(angle);

    if (fMarkerUnits == SkSVGMarkerUnits::kStrokeWidth) {
        const SkScalar strokeWidth = lctx.resolve(
                *ctx.presentationContext().fInherited.fStrokeWidth,
                SkSVGLengthContext::LengthType::kOther);
        canvas->scale(strokeWidth, strokeWidth);
    }

    SkMatrix contentMatrix = SkMatrix::I();
    SkPoint mappedRef;
    if (fViewBox.isValid()) {
        if (fViewBox->isEmpty()) {
            return;
        }
        contentMatrix = ComputeViewboxMatrix(*fViewBox,
                                              SkRect::MakeWH(width, height),
                                              fPreserveAspectRatio);
        referenceLengthContext.setViewPort(
                SkSize::Make(fViewBox->width(), fViewBox->height()));
        const SkPoint ref = SkPoint::Make(
                referenceLengthContext.resolve(fRefX,
                                               SkSVGLengthContext::LengthType::kHorizontal),
                referenceLengthContext.resolve(fRefY,
                                               SkSVGLengthContext::LengthType::kVertical));
        contentMatrix.mapPoints({&mappedRef, 1}, {&ref, 1});
    } else {
        mappedRef = SkPoint::Make(
                markerLengthContext.resolve(fRefX,
                                            SkSVGLengthContext::LengthType::kHorizontal),
                markerLengthContext.resolve(fRefY,
                                            SkSVGLengthContext::LengthType::kVertical));
    }

    canvas->translate(-mappedRef.x(), -mappedRef.y());
    if (fOverflow == SkSVGOverflow::kClip) {
        canvas->clipRect(SkRect::MakeWH(width, height), true);
    }
    canvas->concat(contentMatrix);

    this->SkSVGContainer::onRender(markerContext);
}
