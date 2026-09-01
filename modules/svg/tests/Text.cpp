/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkStream.h"
#include "include/utils/SkNoDrawCanvas.h"
#include "modules/skresources/include/SkResources.h"
#include "modules/skshaper/utils/FactoryHelpers.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "modules/svg/include/SkSVGIDMapper.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "modules/svg/include/SkSVGText.h"
#include "modules/svg/include/SkSVGTypes.h"
#include "modules/svg/src/SkSVGTextPriv.h"
#include "tests/Test.h"
#include "tools/fonts/FontToolUtils.h"

#include <algorithm>
#include <string>
#include <vector>

DEF_TEST(Svg_Text_MermaidTSpanPositionParsing, r) {
    static constexpr char kSVG[] =
            "<svg xmlns='http://www.w3.org/2000/svg'>"
            "<text y='-10.1'><tspan id='label' x='0' y='-0.1em' dy='1.1em'>Idea</tspan></text>"
            "</svg>";
    SkMemoryStream stream(kSVG, sizeof(kSVG) - 1);
    const auto dom = SkSVGDOM::Builder().make(stream);
    REPORTER_ASSERT(r, dom);
    const auto node = dom->findNodeById("label");
    REPORTER_ASSERT(r, node && *node && (*node)->tag() == SkSVGTag::kTSpan);
    const auto* tspan = static_cast<const SkSVGTSpan*>(node->get());
    REPORTER_ASSERT(r, tspan->getY().size() == 1);
    REPORTER_ASSERT(r, tspan->getY()[0].unit() == SkSVGLength::Unit::kEMS);
    REPORTER_ASSERT(r, tspan->getY()[0].value() == -0.1f);
    REPORTER_ASSERT(r, tspan->getDy().size() == 1);
    REPORTER_ASSERT(r, tspan->getDy()[0].unit() == SkSVGLength::Unit::kEMS);
    REPORTER_ASSERT(r, tspan->getDy()[0].value() == 1.1f);
}

DEF_TEST(Svg_Text_PosProvider, r) {
    const auto L = [](float x) { return SkSVGLength(x); };
    const auto EM = [](float x) { return SkSVGLength(x, SkSVGLength::Unit::kEMS); };
    const float N = SkSVGTextContext::PosAttrs()[SkSVGTextContext::PosAttrs::kX];

    static const struct PosTestDesc {
        size_t                   offseta;
        std::vector<SkSVGLength> xa, ya;

        size_t                   offsetb;
        std::vector<SkSVGLength> xb, yb;

        std::vector<SkPoint>     expected;
    } gTests[] = {
        {
            0, {}, {},
            0, {}, {},

            { {N,N} }
        },

        {
            0, { L(1) }, {},
            0, {      }, {},

            { {1,N}, {N,N} }
        },
        {
            0, {       }, {},
            0, { L(10) }, {},

            { {10,N}, {N,N} }
        },
        {
            0, { L( 1) }, {},
            0, { L(10) }, {},

            { {10,N}, {N,N} }
        },
        {
            0, { L( 1), L(2) }, {},
            0, { L(10)       }, {},

            { {10,N}, {2,N}, {N,N} }
        },
        {
            0, { L(1), L( 2) }, {},
            1, {       L(20) }, {},

            { {1,N}, {20,N}, {N,N} }
        },
        {
            0, { L(1), L( 2), L(3) }, {},
            1, {       L(20)       }, {},

            { {1,N}, {20,N}, {3,N}, {N,N} }
        },
        {
            0, { L(1), L(2), L( 3) }, {},
            2, {             L(30) }, {},

            { {1,N}, {2,N}, {30,N}, {N,N} }
        },
        {
            0, { L(1)              }, {},
            2, {             L(30) }, {},

            { {1,N}, {N,N}, {30,N}, {N,N} }
        },


        {
            0, {}, { L(4) },
            0, {}, {      },

            { {N,4}, {N,N} }
        },
        {
            0, {}, {       },
            0, {}, { L(40) },

            { {N,40}, {N,N} }
        },
        {
            0, {}, { L( 4) },
            0, {}, { L(40) },

            { {N,40}, {N,N} }
        },
        {
            0, {}, { L( 4), L(5) },
            0, {}, { L(40)       },

            { {N,40}, {N,5}, {N,N} }
        },
        {
            0, {}, { L(4), L( 5) },
            1, {}, {       L(50) },

            { {N,4}, {N,50}, {N,N} }
        },
        {
            0, {}, { L(4), L( 5), L(6) },
            1, {}, {       L(50)       },

            { {N,4}, {N,50}, {N,6}, {N,N} }
        },
        {
            0, {}, { L(4), L(5), L( 6) },
            2, {}, {             L(60) },

            { {N,4}, {N,5}, {N,60}, {N,N} }
        },
        {
            0, {}, { L(4)              },
            2, {}, {             L(60) },

            { {N,4}, {N,N}, {N,60}, {N,N} }
        },

        {
            0, { L( 1), L(2)}, { L( 4)        },
            0, { L(10)      }, { L(40), L(50) },

            { {10,40}, {2,50}, {N,N} }
        },
        {
            0, { L(1), L( 2), L(3) }, { L(4), L( 5)        },
            1, {       L(20)       }, {       L(50), L(60) },

            { {1,4}, {20,50}, {3,60}, {N,N} }
        },
        {
            0, {}, { EM(0.5f) },
            0, {}, {},

            { {N,12}, {N,N} }
        },
        {
            0, {}, { L(-10.1f) },
            0, {}, { EM(-0.1f) },

            { {N,-2.4f}, {N,N} }
        },
    };

    const SkSVGTextContext::ShapedTextCallback mock_cb =
        [](const SkSVGRenderContext&,
           const sk_sp<SkTextBlob>&,
           const SkPaint*,
           const SkPaint*,
           const SkSVGTextDecoration&) {};

    auto test = [&](const PosTestDesc& tst) {
        auto a = SkSVGText::Make();
        auto b = SkSVGTSpan::Make();
        a->appendChild(b);

        a->setX(tst.xa);
        a->setY(tst.ya);
        b->setX(tst.xb);
        b->setY(tst.yb);

        const SkSVGIDMapper mapper;
        const SkSVGLengthContext lctx({0,0});
        const SkSVGPresentationContext pctx;
        SkNoDrawCanvas canvas(0, 0);
        sk_sp<SkFontMgr> fmgr = ToolUtils::TestFontMgr();
        sk_sp<skresources::ResourceProvider> rp;
        sk_sp<SkShapers::Factory> shaping = SkShapers::BestAvailable();
        const SkSVGRenderContext ctx(&canvas,
                                     fmgr,
                                     rp,
                                     mapper,
                                     lctx,
                                     pctx,
                                     {nullptr, nullptr},
                                     shaping);

        SkSVGTextContext tctx(ctx, mock_cb);
        SkSVGTextContext::ScopedPosResolver pa(*a, ctx, &tctx, tst.offseta);
        SkSVGTextContext::ScopedPosResolver pb(*b, ctx, &tctx, tst.offsetb);

        for (size_t i = 0; i < tst.expected.size(); ++i) {
            const auto& exp = tst.expected[i];
            auto pos = i >= tst.offsetb ? pb.resolve(i) : pa.resolve(i);

            REPORTER_ASSERT(r, pos[SkSVGTextContext::PosAttrs::kX] == exp.fX);
            REPORTER_ASSERT(r, pos[SkSVGTextContext::PosAttrs::kY] == exp.fY);
        }
    };

    for (const auto& tst : gTests) {
        test(tst);
    }
}

DEF_TEST(Svg_TextPath_AppliesDominantBaseline, reporter) {
    const auto render = [&](const char* baselineAttribute, SkBitmap* bitmap) {
        const std::string svgText =
                std::string("<svg width='60' height='40'>") +
                "<defs><path id='line' d='M5 25H55'/></defs>" +
                "<text font-size='12' " + baselineAttribute + ">" +
                "<textPath href='#line'>Idea</textPath></text></svg>";
        auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
        auto dom = SkSVGDOM::Builder().setFontManager(ToolUtils::TestFontMgr()).make(*stream);
        REPORTER_ASSERT(reporter, dom);
        bitmap->allocN32Pixels(60, 40);
        bitmap->eraseColor(SK_ColorTRANSPARENT);
        SkCanvas canvas(*bitmap);
        dom->render(&canvas);
    };

    SkBitmap alphabetic;
    SkBitmap middle;
    render("", &alphabetic);
    render("dominant-baseline='middle'", &middle);

    int differentPixels = 0;
    for (int y = 0; y < alphabetic.height(); ++y) {
        for (int x = 0; x < alphabetic.width(); ++x) {
            differentPixels += alphabetic.getColor(x, y) != middle.getColor(x, y);
        }
    }
    REPORTER_ASSERT(reporter, differentPixels > 0);
}

DEF_TEST(Svg_TextDecorationCoversAdjustedSpacing, reporter) {
    static constexpr char kSVG[] =
            "<svg width='160' height='70'>"
            "<text x='5' y='45' font-size='40' text-decoration='underline' "
            "textLength='140' lengthAdjust='spacing'>ABC</text>"
            "</svg>";
    SkMemoryStream stream(kSVG, sizeof(kSVG) - 1);
    const auto dom = SkSVGDOM::Builder()
                             .setFontManager(ToolUtils::TestFontMgr())
                             .setTextShapingFactory(SkShapers::BestAvailable())
                             .make(stream);
    REPORTER_ASSERT(reporter, dom);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(160, 70);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    int longestDecoratedRun = 0;
    for (int y = 45; y < bitmap.height(); ++y) {
        int currentRun = 0;
        for (int x = 5; x < 150; ++x) {
            if (SkColorGetA(bitmap.getColor(x, y)) > 0) {
                longestDecoratedRun = std::max(longestDecoratedRun, ++currentRun);
            } else {
                currentRun = 0;
            }
        }
    }
    REPORTER_ASSERT(reporter, longestDecoratedRun > 120);
}
