/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "modules/svg/include/SkSVGShape.h"

#include "include/core/SkPaint.h"  // IWYU pragma: keep
#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "include/core/SkScalar.h"
#include "include/private/base/SkDebug.h"
#include "modules/svg/include/SkSVGAttribute.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "modules/svg/include/SkSVGMarker.h"
#include "modules/svg/include/SkSVGTypes.h"
#include "src/base/SkTLazy.h"

#include <cmath>
#include <vector>

class SkSVGNode;
enum class SkSVGTag;

SkSVGShape::SkSVGShape(SkSVGTag t) : INHERITED(t) {}

namespace {

struct MarkerSegment {
    SkPoint start;
    SkPoint end;
    SkVector startTangent;
    SkVector endTangent;
};

using MarkerContour = std::vector<MarkerSegment>;

bool nonzero(const SkVector& v) {
    return v.x() != 0 || v.y() != 0;
}

SkVector start_tangent(SkSpan<const SkPoint> points) {
    for (size_t i = 1; i < points.size(); ++i) {
        const SkVector tangent = points[i] - points[0];
        if (nonzero(tangent)) {
            return tangent;
        }
    }
    return {0, 0};
}

SkVector end_tangent(SkSpan<const SkPoint> points) {
    for (size_t i = points.size() - 1; i > 0; --i) {
        const SkVector tangent = points.back() - points[i - 1];
        if (nonzero(tangent)) {
            return tangent;
        }
    }
    return {0, 0};
}

std::vector<MarkerContour> marker_contours(const SkPath& path) {
    std::vector<MarkerContour> contours;
    MarkerContour contour;
    SkPoint contourStart = {0, 0};
    SkPoint current = {0, 0};

    auto flush = [&] {
        if (!contour.empty()) {
            contours.push_back(std::move(contour));
            contour = MarkerContour();
        }
    };

    SkPath::Iter iter(path, false);
    while (auto rec = iter.next()) {
        const auto points = rec->fPoints;
        switch (rec->fVerb) {
            case SkPathVerb::kMove:
                flush();
                contourStart = current = points[0];
                break;
            case SkPathVerb::kLine:
            case SkPathVerb::kQuad:
            case SkPathVerb::kConic:
            case SkPathVerb::kCubic: {
                const SkVector startTangent = start_tangent(points);
                const SkVector endTangent = end_tangent(points);
                contour.push_back({points.front(), points.back(), startTangent, endTangent});
                current = points.back();
                break;
            }
            case SkPathVerb::kClose: {
                const SkVector tangent = contourStart - current;
                contour.push_back({current, contourStart, tangent, tangent});
                current = contourStart;
                break;
            }
        }
    }
    flush();
    return contours;
}

SkScalar direction_angle(const SkVector& v) {
    return SkRadiansToDegrees(std::atan2(v.y(), v.x()));
}

SkScalar mid_angle(const SkVector& incoming, const SkVector& outgoing) {
    if (!nonzero(incoming)) {
        return direction_angle(outgoing);
    }
    if (!nonzero(outgoing)) {
        return direction_angle(incoming);
    }
    const SkScalar a = direction_angle(incoming);
    const SkScalar b = direction_angle(outgoing);
    const SkScalar ar = SkDegreesToRadians(a);
    const SkScalar br = SkDegreesToRadians(b);
    const SkScalar x = std::cos(ar) + std::cos(br);
    const SkScalar y = std::sin(ar) + std::sin(br);
    return x == 0 && y == 0 ? b : SkRadiansToDegrees(std::atan2(y, x));
}

SkVector first_tangent(const MarkerContour& contour) {
    for (const auto& segment : contour) {
        if (nonzero(segment.startTangent)) {
            return segment.startTangent;
        }
        if (nonzero(segment.endTangent)) {
            return segment.endTangent;
        }
    }
    return {0, 0};
}

SkVector last_tangent(const MarkerContour& contour) {
    for (auto segment = contour.rbegin(); segment != contour.rend(); ++segment) {
        if (nonzero(segment->endTangent)) {
            return segment->endTangent;
        }
        if (nonzero(segment->startTangent)) {
            return segment->startTangent;
        }
    }
    return {0, 0};
}

SkVector incoming_tangent(const MarkerContour& contour, size_t vertex) {
    for (size_t i = vertex; i > 0; --i) {
        const auto& segment = contour[i - 1];
        if (nonzero(segment.endTangent)) {
            return segment.endTangent;
        }
        if (nonzero(segment.startTangent)) {
            return segment.startTangent;
        }
    }
    return {0, 0};
}

SkVector outgoing_tangent(const MarkerContour& contour, size_t vertex) {
    for (size_t i = vertex; i < contour.size(); ++i) {
        const auto& segment = contour[i];
        if (nonzero(segment.startTangent)) {
            return segment.startTangent;
        }
        if (nonzero(segment.endTangent)) {
            return segment.endTangent;
        }
    }
    return {0, 0};
}

void render_marker(const SkSVGRenderContext& ctx,
                   const SkSVGFuncIRI& iri,
                   const SkPoint& position,
                   SkScalar angle,
                   bool isStart) {
    if (iri.type() != SkSVGFuncIRI::Type::kIRI) {
        return;
    }
    const auto markerNode = ctx.findNodeById(iri.iri());
    if (!markerNode || markerNode->tag() != SkSVGTag::kMarker) {
        return;
    }
    static_cast<const SkSVGMarker*>(markerNode.get())->renderMarker(ctx, position, angle, isStart);
}

void render_markers(const SkSVGRenderContext& ctx, const SkPath& path) {
    const auto& props = ctx.presentationContext().fInherited;
    const bool hasStart = props.fMarkerStart->type() == SkSVGFuncIRI::Type::kIRI;
    const bool hasMid = props.fMarkerMid->type() == SkSVGFuncIRI::Type::kIRI;
    const bool hasEnd = props.fMarkerEnd->type() == SkSVGFuncIRI::Type::kIRI;
    if (!(hasStart || hasMid || hasEnd)) {
        return;
    }

    for (const auto& contour : marker_contours(path)) {
        if (contour.empty()) {
            continue;
        }
        if (hasStart) {
            render_marker(ctx, *props.fMarkerStart, contour.front().start,
                          direction_angle(first_tangent(contour)), true);
        }
        if (hasMid) {
            for (size_t i = 1; i < contour.size(); ++i) {
                render_marker(ctx, *props.fMarkerMid, contour[i].start,
                              mid_angle(incoming_tangent(contour, i),
                                        outgoing_tangent(contour, i)), false);
            }
        }
        if (hasEnd) {
            render_marker(ctx, *props.fMarkerEnd, contour.back().end,
                          direction_angle(last_tangent(contour)), false);
        }
    }
}

bool has_markers(const SkSVGRenderContext& ctx) {
    const auto& props = ctx.presentationContext().fInherited;
    return props.fMarkerStart->type() == SkSVGFuncIRI::Type::kIRI ||
           props.fMarkerMid->type() == SkSVGFuncIRI::Type::kIRI ||
           props.fMarkerEnd->type() == SkSVGFuncIRI::Type::kIRI;
}

}  // namespace

void SkSVGShape::onRender(const SkSVGRenderContext& ctx) const {
    const auto fillType = ctx.presentationContext().fInherited.fFillRule->asFillType();

    const auto fillPaint = ctx.fillPaint(),
             strokePaint = ctx.strokePaint();

    // TODO: this approach forces duplicate geometry resolution in onDraw(); refactor to avoid.
    if (fillPaint.isValid()) {
        this->onDraw(ctx.canvas(), ctx.lengthContext(), *fillPaint, fillType);
    }

    if (strokePaint.isValid()) {
        this->onDraw(ctx.canvas(), ctx.lengthContext(), *strokePaint, fillType);
    }

    if (has_markers(ctx)) {
        SkPath markerPath = this->onAsPath(ctx);
        if (this->mapToLocal(&markerPath)) {
            render_markers(ctx, markerPath);
        }
    }
}

void SkSVGShape::appendChild(sk_sp<SkSVGNode>) {
    SkDEBUGF("cannot append child nodes to an SVG shape.\n");
}
