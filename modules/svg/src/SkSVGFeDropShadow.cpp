/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "modules/svg/include/SkSVGFeDropShadow.h"

#include "include/core/SkColor.h"
#include "include/core/SkM44.h"
#include "include/core/SkScalar.h"
#include "include/effects/SkImageFilters.h"
#include "include/private/base/SkTPin.h"
#include "modules/svg/include/SkSVGAttributeParser.h"
#include "modules/svg/include/SkSVGFilterContext.h"
#include "modules/svg/include/SkSVGRenderContext.h"

#include <vector>

template <>
bool SkSVGAttributeParser::parse<SkSVGFeDropShadow::StdDeviation>(
        SkSVGFeDropShadow::StdDeviation*);

bool SkSVGFeDropShadow::parseAndSetAttribute(const char* name, const char* value) {
    return INHERITED::parseAndSetAttribute(name, value) ||
           this->setDx(SkSVGAttributeParser::parse<SkSVGNumberType>("dx", name, value)) ||
           this->setDy(SkSVGAttributeParser::parse<SkSVGNumberType>("dy", name, value)) ||
           this->setStdDeviation(
                   SkSVGAttributeParser::parse<SkSVGFeDropShadow::StdDeviation>(
                           "stdDeviation", name, value));
}

sk_sp<SkImageFilter> SkSVGFeDropShadow::onMakeImageFilter(
        const SkSVGRenderContext& ctx, const SkSVGFilterContext& fctx) const {
    const auto transform = ctx.transformForCurrentOBB(fctx.primitiveUnits()).scale;
    const auto offset = SkV2{this->getDx(), this->getDy()} * transform;
    const auto sigma = SkV2{this->getStdDeviation().fX, this->getStdDeviation().fY} * transform;

    const auto floodColor = this->getFloodColor();
    const auto floodOpacity = this->getFloodOpacity();
    SkColor color = SK_ColorBLACK;
    if (floodColor.isValue() && floodOpacity.isValue()) {
        color = ctx.resolveSvgColor(*floodColor);
        const SkScalar opacity = SkTPin(static_cast<SkScalar>(*floodOpacity), 0.0f, 1.0f);
        color = SkColorSetA(color, SkScalarRoundToInt(SkColorGetA(color) * opacity));
    }

    return SkImageFilters::DropShadow(
            offset.x, offset.y, sigma.x, sigma.y, color,
            fctx.resolveInput(ctx, this->getIn(), this->resolveColorspace(ctx, fctx)),
            this->resolveFilterSubregion(ctx, fctx));
}

template <>
bool SkSVGAttributeParser::parse<SkSVGFeDropShadow::StdDeviation>(
        SkSVGFeDropShadow::StdDeviation* stdDeviation) {
    std::vector<SkSVGNumberType> values;
    if (!this->parse(&values)) {
        return false;
    }

    stdDeviation->fX = values[0];
    stdDeviation->fY = values.size() > 1 ? values[1] : values[0];
    return true;
}
