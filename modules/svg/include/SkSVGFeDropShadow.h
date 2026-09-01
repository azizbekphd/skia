/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkSVGFeDropShadow_DEFINED
#define SkSVGFeDropShadow_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/private/base/SkAPI.h"
#include "modules/svg/include/SkSVGFe.h"
#include "modules/svg/include/SkSVGNode.h"
#include "modules/svg/include/SkSVGTypes.h"

#include <vector>

class SkImageFilter;
class SkSVGFilterContext;
class SkSVGRenderContext;

class SK_API SkSVGFeDropShadow : public SkSVGFe {
public:
    struct StdDeviation {
        SkSVGNumberType fX;
        SkSVGNumberType fY;
    };

    static sk_sp<SkSVGFeDropShadow> Make() {
        return sk_sp<SkSVGFeDropShadow>(new SkSVGFeDropShadow());
    }

    SVG_ATTR(Dx, SkSVGNumberType, SkSVGNumberType(2))
    SVG_ATTR(Dy, SkSVGNumberType, SkSVGNumberType(2))
    SVG_ATTR(StdDeviation, StdDeviation, StdDeviation({2, 2}))

protected:
    sk_sp<SkImageFilter> onMakeImageFilter(const SkSVGRenderContext&,
                                           const SkSVGFilterContext&) const override;

    std::vector<SkSVGFeInputType> getInputs() const override { return {this->getIn()}; }

    bool parseAndSetAttribute(const char*, const char*) override;

private:
    SkSVGFeDropShadow() : INHERITED(SkSVGTag::kFeDropShadow) {}

    using INHERITED = SkSVGFe;
};

#endif  // SkSVGFeDropShadow_DEFINED
