/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGSymbol_DEFINED
#define SkSVGSymbol_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/private/base/SkAPI.h"
#include "modules/svg/include/SkSVGHiddenContainer.h"
#include "modules/svg/include/SkSVGTypes.h"

class SkSVGRenderContext;
class SkMatrix;
class SkPath;
struct SkRect;

class SK_API SkSVGSymbol final : public SkSVGHiddenContainer {
public:
    static constexpr SkSVGTag tag = SkSVGTag::kSymbol;
    static sk_sp<SkSVGSymbol> Make() { return sk_sp<SkSVGSymbol>(new SkSVGSymbol()); }

    SVG_OPTIONAL_ATTR(ViewBox, SkSVGViewBoxType)
    SVG_ATTR(PreserveAspectRatio, SkSVGPreserveAspectRatio, SkSVGPreserveAspectRatio())
    SVG_ATTR(Overflow, SkSVGOverflow, SkSVGOverflow::kClip)

    void renderSymbol(const SkSVGRenderContext&, SkScalar width, SkScalar height) const;
    SkPath asPath(const SkSVGRenderContext&, SkScalar width, SkScalar height) const;
    SkRect objectBoundingBox(const SkSVGRenderContext&, SkScalar width, SkScalar height) const;

protected:
    bool parseAndSetAttribute(const char*, const char*) override;

private:
    SkSVGSymbol() : INHERITED(SkSVGTag::kSymbol) {}

    SkMatrix viewBoxTransform(SkScalar width, SkScalar height) const;

    using INHERITED = SkSVGHiddenContainer;
};

#endif  // SkSVGSymbol_DEFINED
