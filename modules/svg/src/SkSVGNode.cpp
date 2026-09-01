/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "modules/svg/include/SkSVGNode.h"

#include "include/core/SkBlendMode.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorFilter.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkM44.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPicture.h"
#include "include/core/SkPictureRecorder.h"
#include "include/core/SkRRect.h"
#include "include/core/SkShader.h"
#include "include/effects/SkImageFilters.h"
#include "include/pathops/SkPathOps.h"
#include "include/private/base/SkAssert.h"
#include "include/private/base/SkTPin.h"
#include "include/utils/SkNoDrawCanvas.h"
#include "include/utils/SkPaintFilterCanvas.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "src/base/SkTLazy.h"  // IWYU pragma: keep

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace {

class DropShadowPaintAnalysisCanvas final : public SkPaintFilterCanvas {
public:
    explicit DropShadowPaintAnalysisCanvas(SkCanvas* canvas) : SkPaintFilterCanvas(canvas) {}

    bool requiresRasterShadow() const { return fRequiresRasterShadow; }

private:
    bool onFilter(SkPaint& paint) const override {
        const SkShader* shader = paint.getShader();
        const SkColorFilter* colorFilter = paint.getColorFilter();
        fRequiresRasterShadow |=
                (shader && !shader->isOpaque()) ||
                (colorFilter && !colorFilter->isAlphaUnchanged()) ||
                paint.getImageFilter() || paint.getMaskFilter() || paint.getBlender();
        return true;
    }

    mutable bool fRequiresRasterShadow = false;
};

bool drop_shadow_requires_raster(const SkPicture& picture) {
    SkNoDrawCanvas target(picture.cullRect().roundOut());
    DropShadowPaintAnalysisCanvas analysis(&target);
    picture.playback(&analysis);
    return analysis.requiresRasterShadow();
}

class DropShadowPaintCanvas final : public SkPaintFilterCanvas {
public:
    DropShadowPaintCanvas(SkCanvas* canvas,
                          SkColor color,
                          SkScalar featherRadius,
                          SkScalar imageSigma,
                          bool drawVectors,
                          bool drawImages)
        : SkPaintFilterCanvas(canvas)
        , fColor(SkColor4f::FromColor(color))
        , fFeatherRadius(featherRadius)
        , fImageSigma(imageSigma)
        , fDrawVectors(drawVectors)
        , fDrawImages(drawImages) {}

private:
    static bool is_empty_fill(const SkPaint& paint, const SkRect& bounds) {
        return paint.getStyle() == SkPaint::kFill_Style && bounds.isEmpty();
    }

    void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
        if (!is_empty_fill(paint, rect)) {
            this->SkPaintFilterCanvas::onDrawRect(rect, paint);
        }
    }

    void onDrawRRect(const SkRRect& rrect, const SkPaint& paint) override {
        if (!is_empty_fill(paint, rrect.rect())) {
            this->SkPaintFilterCanvas::onDrawRRect(rrect, paint);
        }
    }

    void onDrawDRRect(const SkRRect& outer,
                      const SkRRect& inner,
                      const SkPaint& paint) override {
        if (!is_empty_fill(paint, outer.rect())) {
            this->SkPaintFilterCanvas::onDrawDRRect(outer, inner, paint);
        }
    }

    void onDrawOval(const SkRect& rect, const SkPaint& paint) override {
        if (!is_empty_fill(paint, rect)) {
            this->SkPaintFilterCanvas::onDrawOval(rect, paint);
        }
    }

    void onDrawArc(const SkRect& rect,
                   SkScalar startAngle,
                   SkScalar sweepAngle,
                   bool useCenter,
                   const SkPaint& paint) override {
        if (!is_empty_fill(paint, rect) &&
            !(paint.getStyle() == SkPaint::kFill_Style && sweepAngle == 0)) {
            this->SkPaintFilterCanvas::onDrawArc(
                    rect, startAngle, sweepAngle, useCenter, paint);
        }
    }

    void onDrawPath(const SkPath& path, const SkPaint& paint) override {
        SkPoint line[2];
        if (paint.getStyle() == SkPaint::kFill_Style &&
            (path.isEmpty() || path.isLine(line))) {
            return;
        }
        this->SkPaintFilterCanvas::onDrawPath(path, paint);
    }

    bool onFilter(SkPaint& paint) const override {
        if (!fDrawVectors) {
            return false;
        }
        const SkScalar alpha = SkTPin(paint.getAlphaf(), 0.0f, 1.0f);
        paint.setShader(nullptr);
        paint.setColorFilter(nullptr);
        paint.setImageFilter(nullptr);
        paint.setColor4f({fColor.fR, fColor.fG, fColor.fB, alpha});

        // Approximate the Gaussian feather by growing the source geometry in place. Replaying
        // translated copies makes thin lines and glyphs look like repeated silhouettes, while
        // progressively wider round outlines produce a continuous vector halo.
        if (fFeatherRadius > 0) {
            const SkScalar featherWidth = 2 * fFeatherRadius;
            switch (paint.getStyle()) {
                case SkPaint::kFill_Style:
                    paint.setStyle(SkPaint::kStrokeAndFill_Style);
                    paint.setStrokeWidth(featherWidth);
                    break;
                case SkPaint::kStroke_Style:
                case SkPaint::kStrokeAndFill_Style:
                    paint.setStrokeWidth(paint.getStrokeWidth() + featherWidth);
                    break;
            }
            paint.setStrokeJoin(SkPaint::kRound_Join);
            paint.setStrokeCap(SkPaint::kRound_Cap);
        }
        return alpha > 0;
    }

    void onDrawImage2(const SkImage* image,
                      SkScalar left,
                      SkScalar top,
                      const SkSamplingOptions& sampling,
                      const SkPaint* paint) override {
        if (!fDrawImages) {
            return;
        }
        SkPaint shadowPaint = paint ? *paint : SkPaint();
        this->filterImagePaint(&shadowPaint);
        this->SkNWayCanvas::onDrawImage2(image, left, top, sampling, &shadowPaint);
    }

    void onDrawImageRect2(const SkImage* image,
                          const SkRect& src,
                          const SkRect& dst,
                          const SkSamplingOptions& sampling,
                          const SkPaint* paint,
                          SrcRectConstraint constraint) override {
        if (!fDrawImages) {
            return;
        }
        SkPaint shadowPaint = paint ? *paint : SkPaint();
        this->filterImagePaint(&shadowPaint);
        this->SkNWayCanvas::onDrawImageRect2(
                image, src, dst, sampling, &shadowPaint, constraint);
    }

    void filterImagePaint(SkPaint* paint) const {
        const SkColor4f color = {fColor.fR,
                                 fColor.fG,
                                 fColor.fB,
                                 1.0f};
        paint->setImageFilter(nullptr);
        paint->setColorFilter(SkColorFilters::Blend(color, nullptr, SkBlendMode::kSrcIn));
        if (fImageSigma > 0) {
            paint->setImageFilter(SkImageFilters::Blur(fImageSigma, fImageSigma, nullptr));
        }
    }

    const SkColor4f fColor;
    const SkScalar fFeatherRadius;
    const SkScalar fImageSigma;
    const bool fDrawVectors;
    const bool fDrawImages;
};

void render_vector_drop_shadow(const SkSVGNode* node,
                               const SkSVGRenderContext& ctx,
                               const SkSVGDropShadow& shadow) {
    // Record without this node's filter, then replay the drawing commands through a paint filter.
    // Unlike an SkImageFilter saveLayer, PDF keeps the recolored paths and glyphs as vector content.
    SkPictureRecorder recorder;
    constexpr SkScalar kPictureExtent = 1 << 20;
    SkCanvas* recordingCanvas = recorder.beginRecording(
            SkRect::MakeLTRB(-kPictureExtent, -kPictureExtent,
                             kPictureExtent,  kPictureExtent));
    {
        SkSVGRenderContext recordingContext(ctx, recordingCanvas);
        recordingContext.suppressFilterForNode(node);
        node->render(recordingContext);
    }
    const sk_sp<SkPicture> picture = recorder.finishRecordingAsPicture();

    // Recoloring a vector paint is exact only when its shader/filter cannot change source alpha.
    // Fall back to the normal image-filter implementation for alpha-varying paints. CanvasKit's
    // validation canvas observes this saveLayer and rejects the document before a PDF render,
    // rather than accepting a vector approximation with an incorrect shadow mask.
    if (drop_shadow_requires_raster(*picture)) {
        SkPaint filterPaint;
        filterPaint.setImageFilter(SkImageFilters::DropShadow(shadow.fDx,
                                                              shadow.fDy,
                                                              shadow.fSigma,
                                                              shadow.fSigma,
                                                              shadow.fColor,
                                                              nullptr));
        ctx.canvas()->saveLayer(nullptr, &filterPaint);
        picture->playback(ctx.canvas());
        ctx.canvas()->restore();
        return;
    }

    // Record a second copy with the filter offset inserted after this node establishes its local
    // coordinate system. Applying a translation on the destination canvas would put it before the
    // node transform, so scaled or rotated elements would receive an incorrect device-space offset.
    SkPictureRecorder shadowRecorder;
    SkCanvas* shadowRecordingCanvas = shadowRecorder.beginRecording(
            SkRect::MakeLTRB(-kPictureExtent, -kPictureExtent,
                             kPictureExtent,  kPictureExtent));
    {
        SkSVGRenderContext shadowRecordingContext(ctx, shadowRecordingCanvas);
        shadowRecordingContext.suppressFilterForNode(node);
        shadowRecordingContext.setDropShadowOffsetForNode(node, shadow.fDx, shadow.fDy);
        node->render(shadowRecordingContext);
    }
    const sk_sp<SkPicture> shadowPicture = shadowRecorder.finishRecordingAsPicture();

    // A Gaussian is not a native PDF primitive. Approximate its edge profile with nested vector
    // outlines. A blurred opaque edge has half the source alpha at its boundary and follows the
    // complementary error function outside it. Keep that feather separate from the displaced
    // shadow core: using the full source alpha for every expanded outline creates a bright halo.
    constexpr int kFeatherSteps = 16;
    constexpr SkScalar kSigmaExtent = 3;
    constexpr SkScalar kInvSqrtTwo = 0.70710678118f;
    const SkScalar colorAlpha = SkColorGetA(shadow.fColor) / 255.0f;
    const SkScalar maxRadius = shadow.fSigma * kSigmaExtent;
    const auto playbackShadow = [&](SkScalar opacity,
                                    SkScalar featherRadius,
                                    SkScalar imageSigma,
                                    bool drawVectors,
                                    bool drawImages) {
        if (opacity <= 0) {
            return;
        }
        SkPaint opacityPaint;
        opacityPaint.setAlphaf(opacity);
        ctx.canvas()->saveLayer(nullptr, &opacityPaint);
        DropShadowPaintCanvas shadowCanvas(ctx.canvas(),
                                           shadow.fColor,
                                           featherRadius,
                                           imageSigma,
                                           drawVectors,
                                           drawImages);
        shadowPicture->playback(&shadowCanvas);
        ctx.canvas()->restore();
    };

    SkScalar accumulatedAlpha = 0;
    for (int step = kFeatherSteps; step >= 0 && shadow.fSigma > 0 && colorAlpha > 0;
         --step) {
        const SkScalar radius = maxRadius * static_cast<SkScalar>(step) / kFeatherSteps;
        const SkScalar sigmaDistance = radius / shadow.fSigma;
        const SkScalar targetAlpha =
                colorAlpha * 0.5f * std::erfc(sigmaDistance * kInvSqrtTwo);
        const SkScalar remainingAlpha = 1 - accumulatedAlpha;
        const SkScalar layerAlpha = remainingAlpha > 0
                                            ? (targetAlpha - accumulatedAlpha) / remainingAlpha
                                            : 0;
        playbackShadow(layerAlpha,
                       radius,
                       /*imageSigma=*/0,
                       /*drawVectors=*/true,
                       /*drawImages=*/false);

        accumulatedAlpha = layerAlpha + accumulatedAlpha * (1 - layerAlpha);
    }

    // Finish the interior at the requested shadow alpha without increasing the feather outside
    // the source geometry. Raster icons are colored only in this core pass so they remain a single
    // PDF image object rather than being duplicated for every feather layer.
    if (colorAlpha > 0) {
        const SkScalar remainingAlpha = 1 - accumulatedAlpha;
        const SkScalar coreAlpha = remainingAlpha > 0
                                           ? (colorAlpha - accumulatedAlpha) / remainingAlpha
                                           : 0;
        playbackShadow(coreAlpha,
                       /*featherRadius=*/0,
                       /*imageSigma=*/0,
                       /*drawVectors=*/true,
                       /*drawImages=*/false);

        // Raster image alpha must be blurred as a single source rather than expanded as vector
        // geometry. It did not participate in the vector feather layers, so apply the full shadow
        // opacity here instead of the residual vector-core opacity.
        playbackShadow(colorAlpha,
                       /*featherRadius=*/0,
                       shadow.fSigma,
                       /*drawVectors=*/false,
                       /*drawImages=*/true);
    }

    picture->playback(ctx.canvas());
}

}  // namespace

SkSVGNode::SkSVGNode(SkSVGTag t) : fTag(t) {
    // Uninherited presentation attributes need a non-null default value.
    fPresentationAttributes.fStopColor.set(SkSVGColor(SK_ColorBLACK));
    fPresentationAttributes.fStopOpacity.set(SkSVGNumberType(1.0f));
    fPresentationAttributes.fFloodColor.set(SkSVGColor(SK_ColorBLACK));
    fPresentationAttributes.fFloodOpacity.set(SkSVGNumberType(1.0f));
    fPresentationAttributes.fLightingColor.set(SkSVGColor(SK_ColorWHITE));
}

SkSVGNode::~SkSVGNode() { }

void SkSVGNode::render(const SkSVGRenderContext& ctx) const {
    if (!ctx.isFilterSuppressedForNode(this) && fPresentationAttributes.fFilter.isValue() &&
        fPresentationAttributes.fFilter->type() == SkSVGFuncIRI::Type::kDropShadow) {
        render_vector_drop_shadow(this, ctx, fPresentationAttributes.fFilter->dropShadow());
        return;
    }

    SkSVGRenderContext localContext(ctx, this);

    if (this->onPrepareToRender(&localContext)) {
        this->onRender(localContext);
    }
}

bool SkSVGNode::asPaint(const SkSVGRenderContext& ctx, SkPaint* paint) const {
    SkSVGRenderContext localContext(ctx);

    return this->onPrepareToRender(&localContext) && this->onAsPaint(localContext, paint);
}

SkPath SkSVGNode::asPath(const SkSVGRenderContext& ctx) const {
    SkSVGRenderContext localContext(ctx);
    if (!this->onPrepareToRender(&localContext)) {
        return SkPath();
    }

    SkPath path = this->onAsPath(localContext);

    if (const auto* clipPath = localContext.clipPath()) {
        // There is a clip-path present on the current node.
        Op(path, *clipPath, kIntersect_SkPathOp, &path);
    }

    return path;
}

SkRect SkSVGNode::objectBoundingBox(const SkSVGRenderContext& ctx) const {
    return this->onObjectBoundingBox(ctx);
}

bool SkSVGNode::onPrepareToRender(SkSVGRenderContext* ctx) const {
    ctx->applyDropShadowOffsetForNode(this);

    // Shape opacity normally uses the leaf fast path and is folded into its fill/stroke paints.
    // Markers are additional descendants for opacity purposes even though shape nodes cannot have
    // literal children. Check both the inherited and local values here; treating a local "none"
    // override as non-leaf is harmless and keeps the decision conservative.
    const auto hasMarker = [](const SkSVGFuncIRI& iri) {
        return iri.type() == SkSVGFuncIRI::Type::kIRI;
    };
    const auto& inherited = ctx->presentationContext().fInherited;
    const bool hasMarkers = hasMarker(*inherited.fMarkerStart) ||
                            hasMarker(*inherited.fMarkerMid) ||
                            hasMarker(*inherited.fMarkerEnd) ||
                            (fPresentationAttributes.fMarkerStart.isValue() &&
                             hasMarker(*fPresentationAttributes.fMarkerStart)) ||
                            (fPresentationAttributes.fMarkerMid.isValue() &&
                             hasMarker(*fPresentationAttributes.fMarkerMid)) ||
                            (fPresentationAttributes.fMarkerEnd.isValue() &&
                             hasMarker(*fPresentationAttributes.fMarkerEnd));
    ctx->applyPresentationAttributes(fPresentationAttributes,
                                     this->hasChildren() || hasMarkers
                                             ? 0
                                             : SkSVGRenderContext::kLeaf);

    // visibility:hidden and display:none disable rendering.
    // TODO: if display is not a value (true when display="inherit"), we currently
    //   ignore it. Eventually we should be able to add SkASSERT(display.isValue()).
    const auto visibility = ctx->presentationContext().fInherited.fVisibility->type();
    const auto display = fPresentationAttributes.fDisplay;  // display is uninherited
    return visibility != SkSVGVisibility::Type::kHidden &&
           (!display.isValue() || *display != SkSVGDisplay::kNone);
}

void SkSVGNode::setAttribute(SkSVGAttribute attr, const SkSVGValue& v) {
    this->onSetAttribute(attr, v);
}

template <typename T>
void SetInheritedByDefault(SkTLazy<T>& presentation_attribute, const T& value) {
    if (value.type() != T::Type::kInherit) {
        presentation_attribute.set(value);
    } else {
        // kInherited values are semantically equivalent to
        // the absence of a local presentation attribute.
        presentation_attribute.reset();
    }
}

bool SkSVGNode::parseAndSetAttribute(const char* n, const char* v) {
#define PARSE_AND_SET(svgName, attrName)                                                        \
    this->set##attrName(                                                                        \
            SkSVGAttributeParser::parseProperty<decltype(fPresentationAttributes.f##attrName)>( \
                    svgName, n, v))

    return PARSE_AND_SET(   "clip-path"                  , ClipPath)
           || PARSE_AND_SET("clip-rule"                  , ClipRule)
           || PARSE_AND_SET("color"                      , Color)
           || PARSE_AND_SET("color-interpolation"        , ColorInterpolation)
           || PARSE_AND_SET("color-interpolation-filters", ColorInterpolationFilters)
           || PARSE_AND_SET("display"                    , Display)
           || PARSE_AND_SET("fill"                       , Fill)
           || PARSE_AND_SET("fill-opacity"               , FillOpacity)
           || PARSE_AND_SET("fill-rule"                  , FillRule)
           || PARSE_AND_SET("filter"                     , Filter)
           || PARSE_AND_SET("flood-color"                , FloodColor)
           || PARSE_AND_SET("flood-opacity"              , FloodOpacity)
           || PARSE_AND_SET("font-family"                , FontFamily)
           || PARSE_AND_SET("font-size"                  , FontSize)
           || PARSE_AND_SET("font-style"                 , FontStyle)
           || PARSE_AND_SET("font-weight"                , FontWeight)
           || PARSE_AND_SET("lighting-color"             , LightingColor)
           || PARSE_AND_SET("marker-end"                 , MarkerEnd)
           || PARSE_AND_SET("marker-mid"                 , MarkerMid)
           || PARSE_AND_SET("marker-start"               , MarkerStart)
           || PARSE_AND_SET("mask"                       , Mask)
           || PARSE_AND_SET("opacity"                    , Opacity)
           || PARSE_AND_SET("stop-color"                 , StopColor)
           || PARSE_AND_SET("stop-opacity"               , StopOpacity)
           || PARSE_AND_SET("stroke"                     , Stroke)
           || PARSE_AND_SET("stroke-dasharray"           , StrokeDashArray)
           || PARSE_AND_SET("stroke-dashoffset"          , StrokeDashOffset)
           || PARSE_AND_SET("stroke-linecap"             , StrokeLineCap)
           || PARSE_AND_SET("stroke-linejoin"            , StrokeLineJoin)
           || PARSE_AND_SET("stroke-miterlimit"          , StrokeMiterLimit)
           || PARSE_AND_SET("stroke-opacity"             , StrokeOpacity)
           || PARSE_AND_SET("stroke-width"               , StrokeWidth)
           || PARSE_AND_SET("text-anchor"                , TextAnchor)
           || PARSE_AND_SET("text-decoration"            , TextDecoration)
           || PARSE_AND_SET("visibility"                 , Visibility);

#undef PARSE_AND_SET
}

// https://www.w3.org/TR/SVG11/coords.html#PreserveAspectRatioAttribute
SkMatrix SkSVGNode::ComputeViewboxMatrix(const SkRect& viewBox,
                                         const SkRect& viewPort,
                                         SkSVGPreserveAspectRatio par) {
    if (viewBox.isEmpty() || viewPort.isEmpty()) {
        return SkMatrix::Scale(0, 0);
    }

    auto compute_scale = [&]() -> SkV2 {
        const auto sx = viewPort.width()  / viewBox.width(),
                   sy = viewPort.height() / viewBox.height();

        if (par.fAlign == SkSVGPreserveAspectRatio::kNone) {
            // none -> anisotropic scaling, regardless of fScale
            return {sx, sy};
        }

        // isotropic scaling
        const auto s = par.fScale == SkSVGPreserveAspectRatio::kMeet
                            ? std::min(sx, sy)
                            : std::max(sx, sy);
        return {s, s};
    };

    auto compute_trans = [&](const SkV2& scale) -> SkV2 {
        static constexpr float gAlignCoeffs[] = {
                0.0f, // Min
                0.5f, // Mid
                1.0f  // Max
        };

        const size_t x_coeff = par.fAlign >> 0 & 0x03,
                     y_coeff = par.fAlign >> 2 & 0x03;

        SkASSERT(x_coeff < std::size(gAlignCoeffs) &&
                 y_coeff < std::size(gAlignCoeffs));

        const auto tx = -viewBox.x() * scale.x,
                   ty = -viewBox.y() * scale.y,
                   dx = viewPort.width()  - viewBox.width() * scale.x,
                   dy = viewPort.height() - viewBox.height() * scale.y;

        return {
            tx + dx * gAlignCoeffs[x_coeff],
            ty + dy * gAlignCoeffs[y_coeff]
        };
    };

    const auto s = compute_scale(),
               t = compute_trans(s);

    return SkMatrix::Translate(t.x, t.y) *
           SkMatrix::Scale(s.x, s.y);
}
