/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGMarker_DEFINED
#define SkSVGMarker_DEFINED

#include "include/core/SkPoint.h"
#include "include/core/SkRefCnt.h"
#include "include/private/base/SkAPI.h"
#include "modules/svg/include/SkSVGHiddenContainer.h"
#include "modules/svg/include/SkSVGTypes.h"

class SkSVGRenderContext;

class SK_API SkSVGMarker final : public SkSVGHiddenContainer {
public:
    static sk_sp<SkSVGMarker> Make() { return sk_sp<SkSVGMarker>(new SkSVGMarker()); }

    SVG_ATTR(RefX, SkSVGLength, SkSVGLength(0))
    SVG_ATTR(RefY, SkSVGLength, SkSVGLength(0))
    SVG_ATTR(MarkerWidth, SkSVGLength, SkSVGLength(3))
    SVG_ATTR(MarkerHeight, SkSVGLength, SkSVGLength(3))
    SVG_ATTR(MarkerUnits, SkSVGMarkerUnits, SkSVGMarkerUnits::kStrokeWidth)
    SVG_ATTR(Orient, SkSVGOrient, SkSVGOrient())
    SVG_OPTIONAL_ATTR(ViewBox, SkSVGViewBoxType)
    SVG_ATTR(PreserveAspectRatio, SkSVGPreserveAspectRatio, SkSVGPreserveAspectRatio())
    SVG_ATTR(Overflow, SkSVGOverflow, SkSVGOverflow::kClip)

    void setInheritedPresentationAttributes(const SkSVGPresentationAttributes& inherited) {
        fInheritedPresentationAttributes = inherited;
    }

    void renderMarker(const SkSVGRenderContext&,
                      const SkPoint& position,
                      SkScalar autoAngle,
                      bool isStart) const;

protected:
    bool parseAndSetAttribute(const char*, const char*) override;

private:
    SkSVGMarker();

    SkSVGPresentationAttributes fInheritedPresentationAttributes =
            SkSVGPresentationAttributes::MakeInitial();

    using INHERITED = SkSVGHiddenContainer;
};

#endif  // SkSVGMarker_DEFINED
