/*
 * Copyright 2021 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkStream.h"
#include "include/utils/SkNoDrawCanvas.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "modules/svg/include/SkSVGNode.h"
#include "tests/Test.h"
#include "tools/fonts/FontToolUtils.h"

namespace {

class FilterTestLogger final : public SkSVGDOM::Logger {
public:
    void log(Level level, const char[]) override {
        level == Level::kError ? ++fErrors : ++fWarnings;
    }

    int fWarnings = 0;
    int fErrors = 0;
};

}  // namespace

DEF_TEST(Svg_Filters_NonePaintInputs, r) {
    const std::string svgText = R"EOF(
    <svg width="500" height="500" xmlns="http://www.w3.org/2000/svg"
         xmlns:xlink="http://www.w3.org/1999/xlink">
        <defs>
            <filter id="f" x="0" y="0" width="1" height="1">
                <feComposite operator="arithmetic" in="FillPaint" in2="StrokePaint"
                             k1="0" k2="10" k3="20" k4="0"/>
            </filter>
        </defs>
        <rect fill="none" stroke="none" filter="url(#f)" x="10" y="10" width="100" height="1,0"/>
    </svg>
    )EOF";

    auto str = SkMemoryStream::MakeDirect(svgText.c_str(), svgText.size());
    auto svg_dom = SkSVGDOM::Builder().make(*str);
    SkNoDrawCanvas canvas(500, 500);
    svg_dom->render(&canvas);
}

DEF_TEST(Svg_Filters_DropShadowHSLAndComputedStyle, reporter) {
    const std::string svgText = R"SVG(
      <svg width="100" height="70" xmlns="http://www.w3.org/2000/svg"
           style="display: block; max-width: 877.139px; margin: 0px; padding: 0px;
                  background: none; position: static; pointer-events: auto; overflow: hidden;
                  animation: 0s ease 0s 1 normal none running none">
        <defs>
          <linearGradient id="gradient">
            <stop offset="0" stop-color="hsl(78.1578947368, 18.4615384615%, 64.5098039216%)"/>
            <stop offset="1" stop-color="hsl(98.961038961, 60%, 74.9019607843%)"/>
          </linearGradient>
          <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
            <feDropShadow dx="4" dy="4" stdDeviation="2"
                          flood-color="hsl(0, 0%, 0%)" flood-opacity="0.6"/>
          </filter>
        </defs>
        <rect x="20" y="15" width="50" height="30" rx="3"
              fill="url(#gradient)" filter="url(#shadow)"/>
      </svg>)SVG";

    auto logger = sk_make_sp<FilterTestLogger>();
    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().setLogger(logger).make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(100, 70);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(35, 25)) > 200);
    REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(73, 48)) > 0);
}

DEF_TEST(Svg_Filters_DropShadowMultipliesFloodAlpha, reporter) {
    const std::string svgText = R"SVG(
      <svg width="30" height="10">
        <defs>
          <filter id="shadow" x="0" y="0" width="30" height="10"
                  filterUnits="userSpaceOnUse">
            <feDropShadow dx="15" dy="0" stdDeviation="0"
                          flood-color="rgba(255, 0, 0, 0.5)" flood-opacity="0.5"/>
          </filter>
        </defs>
        <rect width="10" height="10" fill="black" filter="url(#shadow)"/>
      </svg>)SVG";

    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(30, 10);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    const SkColor shadow = bitmap.getColor(17, 5);
    REPORTER_ASSERT(reporter, SkColorGetR(shadow) > 200);
    REPORTER_ASSERT(reporter, SkColorGetA(shadow) > 50 && SkColorGetA(shadow) < 80);
}

DEF_TEST(Svg_Filters_DropShadowHasContinuousFeather, reporter) {
    const std::string svgText = R"SVG(
      <svg width="80" height="40">
        <path d="M 10 5 L 10 35" fill="none" stroke="red" stroke-width="1"
              style="filter: drop-shadow(rgba(0, 0, 0, 0.8) 30px 0 3px)"/>
      </svg>)SVG";

    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(80, 40);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    // A coarse translated-copy blur leaves transparent gaps between copies of a thin line. The
    // feather should instead be continuous and become lighter as it moves away from the source.
    for (int x = 34; x <= 46; ++x) {
        REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(x, 20)) > 0);
    }
    const U8CPU center = SkColorGetA(bitmap.getColor(40, 20));
    const U8CPU near = SkColorGetA(bitmap.getColor(42, 20));
    const U8CPU middle = SkColorGetA(bitmap.getColor(44, 20));
    const U8CPU far = SkColorGetA(bitmap.getColor(46, 20));
    REPORTER_ASSERT(reporter, center > near);
    REPORTER_ASSERT(reporter, near > middle);
    REPORTER_ASSERT(reporter, middle > far);
    REPORTER_ASSERT(reporter, near * 2 < center);
}

DEF_TEST(Svg_Filters_DropShadowOffsetUsesLocalCoordinates, reporter) {
    const std::string svgText = R"SVG(
      <svg width="50" height="20">
        <rect width="10" height="10" fill="red" transform="scale(2)"
              style="filter: drop-shadow(black 10px 0 0)"/>
      </svg>)SVG";

    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(50, 20);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    REPORTER_ASSERT(reporter, bitmap.getColor(35, 10) == SK_ColorBLACK);
    REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(45, 10)) == 0);
}

DEF_TEST(Svg_Filters_DropShadowPreservesShaderAlpha, reporter) {
    const std::string svgText = R"SVG(
      <svg width="60" height="10">
        <defs>
          <linearGradient id="alpha">
            <stop offset="0" stop-color="black" stop-opacity="0"/>
            <stop offset="1" stop-color="black"/>
          </linearGradient>
        </defs>
        <rect width="20" height="10" fill="url(#alpha)"
              style="filter: drop-shadow(black 25px 0 0)"/>
      </svg>)SVG";

    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(60, 10);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    const U8CPU transparentEndAlpha = SkColorGetA(bitmap.getColor(26, 5));
    const U8CPU opaqueEndAlpha = SkColorGetA(bitmap.getColor(43, 5));
    REPORTER_ASSERT(reporter, transparentEndAlpha < 64);
    REPORTER_ASSERT(reporter, opaqueEndAlpha > 192);
}

DEF_TEST(Svg_Filters_DropShadowPreservesEmptyFill, reporter) {
    const std::string svgText = R"SVG(
      <svg width="40" height="20">
        <path d="M5 10 L15 10" fill="red" stroke="none"
              style="filter: drop-shadow(rgba(0, 0, 0, 0.8) 15px 0 3px)"/>
      </svg>)SVG";

    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(40, 20);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    for (int y = 0; y < bitmap.height(); ++y) {
        for (int x = 0; x < bitmap.width(); ++x) {
            REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(x, y)) == 0);
        }
    }
}

DEF_TEST(Svg_Filters_DropShadowCompositesSourceBeforeOpacity, reporter) {
    const std::string svgText = R"SVG(
      <svg width="50" height="20">
        <g style="filter: drop-shadow(rgba(0, 0, 0, 0.5) 25px 0 0)">
          <rect x="0" y="2" width="10" height="10"/>
          <rect x="5" y="2" width="10" height="10"/>
        </g>
      </svg>)SVG";

    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(50, 20);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    const U8CPU singleShapeAlpha = SkColorGetA(bitmap.getColor(27, 6));
    const U8CPU overlappingShapesAlpha = SkColorGetA(bitmap.getColor(32, 6));
    REPORTER_ASSERT(reporter, singleShapeAlpha > 100 && singleShapeAlpha < 150);
    REPORTER_ASSERT(reporter, overlappingShapesAlpha == singleShapeAlpha);
}

DEF_TEST(Svg_OverflowClipsNestedSVGViewport, reporter) {
    const std::string svgText = R"SVG(
      <svg width="30" height="10">
        <svg width="10" height="10" style="overflow: hidden">
          <rect width="20" height="10"/>
        </svg>
      </svg>)SVG";

    auto logger = sk_make_sp<FilterTestLogger>();
    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().setLogger(logger).make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(30, 10);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(5, 5)) > 0);
    REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(15, 5)) == 0);
}

DEF_TEST(Svg_BrowserNormalizedCompatibilityValues, reporter) {
    const std::string svgText = R"SVG(
      <svg width="20" height="10"
           style="background: none 0% 0% / auto repeat scroll padding-box border-box
                              rgb(232,232,232)">
        <text x="1" y="8" fill="transparent" font-weight="" display="none"
              style="line-height: normal; white-space: pre"> A  B </text>
        <rect x="10" width="10" height="10" fill="#00000000" stroke="#00000000"/>
      </svg>)SVG";

    auto logger = sk_make_sp<FilterTestLogger>();
    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().setLogger(logger).make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(20, 10);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);
    REPORTER_ASSERT(reporter, bitmap.getColor(19, 9) == SkColorSetRGB(232, 232, 232));
}

DEF_TEST(Svg_GradientBackgroundIsUnsupported, reporter) {
    const std::string svgText = R"SVG(
      <svg width="10" height="10"
           style="background: linear-gradient(rgb(255, 0, 0), rgb(0, 0, 255))
                              0% 0% / auto repeat scroll padding-box border-box
                              rgb(0, 255, 0)"/>)SVG";

    auto logger = sk_make_sp<FilterTestLogger>();
    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().setLogger(logger).make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);
    REPORTER_ASSERT(reporter, logger->fWarnings == 1);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);
}

DEF_TEST(Svg_UnterminatedBackgroundFunctionIsUnsupported, reporter) {
    const std::string svgText = R"SVG(
      <svg width="10" height="10"
           style="background: none 0% 0% / auto repeat scroll padding-box border-box
                              rgb(0, 0, 0"/>)SVG";

    auto logger = sk_make_sp<FilterTestLogger>();
    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().setLogger(logger).make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);
    REPORTER_ASSERT(reporter, logger->fWarnings == 1);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);
}

DEF_TEST(Svg_ActiveAnimationShorthandIsUnsupported, reporter) {
    const std::string svgText = R"SVG(
      <svg width="10" height="10"
           style="animation: 1s linear 0s infinite normal none running spin">
        <rect width="10" height="10"/>
      </svg>)SVG";

    auto logger = sk_make_sp<FilterTestLogger>();
    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().setLogger(logger).make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);
    REPORTER_ASSERT(reporter, logger->fWarnings == 1);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);
}

DEF_TEST(Svg_ColorSchemeNormalRestoresDefault, reporter) {
    const std::string svgText = R"SVG(
      <svg width="10" height="10">
        <g style="color-scheme: dark">
          <g style="color-scheme: normal">
            <rect width="10" height="10" fill="light-dark(#ff0000, #0000ff)"/>
          </g>
        </g>
      </svg>)SVG";

    auto logger = sk_make_sp<FilterTestLogger>();
    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder().setLogger(logger).make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    const SkColor center = bitmap.getColor(5, 5);
    REPORTER_ASSERT(reporter, SkColorGetR(center) > 200);
    REPORTER_ASSERT(reporter, SkColorGetB(center) < 20);
}

DEF_TEST(Svg_AdaptiveColorsSymbolsAndTextCompatibility, reporter) {
    const std::string svgText = R"SVG(
      <svg width="140" height="70" style="color-scheme: dark;
           --ge-adaptive-bg: light-dark(#ffffff, #121212)">
        <defs>
          <symbol id="icon" viewBox="0 0 10 10" preserveAspectRatio="xMidYMid meet">
            <rect width="10" height="10" fill="#0000ff"/>
          </symbol>
        </defs>
        <rect x="10" y="10" width="20" height="20" pointer-events="stroke"
              style="fill: light-dark(rgb(175, 255, 175), rgb(0, 57, 0));
                     stroke: light-dark(rgb(0, 0, 0), rgb(255, 255, 255));
                     filter: drop-shadow(light-dark(rgba(61, 69, 116, 0.4),
                                                     rgba(168, 175, 216, 0.4))
                                                3px 3px 1.2px)"/>
        <g style="--shape-bg: var(--ge-adaptive-bg)">
          <circle cx="40" cy="20" r="8" fill="var(--shape-bg, #ffffff)"/>
        </g>
        <use href="#icon" x="50" y="10" width="20" height="20" pointer-events="all"/>
        <text x="80" y="25" textLength="50" lengthAdjust="spacing"
              dominant-baseline="middle" alignment-baseline="mathematical">AB</text>
        <a target="_blank">
          <text x="5" y="58" font-size="20" text-decoration="underline">ABC</text>
        </a>
      </svg>)SVG";

    auto logger = sk_make_sp<FilterTestLogger>();
    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder()
                          .setLogger(logger)
                          .setFontManager(ToolUtils::TestFontMgr())
                          .make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(140, 70);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    const SkColor darkFill = bitmap.getColor(20, 20);
    REPORTER_ASSERT(reporter, SkColorGetG(darkFill) > 40);
    REPORTER_ASSERT(reporter, SkColorGetR(darkFill) < 20);
    const SkColor adaptiveBackground = bitmap.getColor(40, 20);
    REPORTER_ASSERT(reporter, SkColorGetR(adaptiveBackground) == 0x12);
    REPORTER_ASSERT(reporter, SkColorGetG(adaptiveBackground) == 0x12);
    REPORTER_ASSERT(reporter, SkColorGetB(adaptiveBackground) == 0x12);
    REPORTER_ASSERT(reporter, SkColorGetB(bitmap.getColor(60, 20)) > 200);
    REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(33, 33)) > 0);

    int underlinePixels = 0;
    for (int y = 59; y < 66; ++y) {
        for (int x = 5; x < 45; ++x) {
            underlinePixels += SkColorGetA(bitmap.getColor(x, y)) > 0;
        }
    }
    REPORTER_ASSERT(reporter, underlinePixels > 5);
}

DEF_TEST(Svg_DrawioCapabilityWarningIsNotDiagramContent, reporter) {
    const std::string svgText = R"SVG(
      <svg width="220" height="32" xmlns:xlink="http://www.w3.org/1999/xlink">
        <a target="_blank">
          <text x="2" y="10" font-size="8">Ordinary linked text</text>
        </a>
        <a target="_blank" transform="translate(0,-5)"
           xlink:href="https://www.diagrams.net/doc/faq/svg-export-text-problems">
          <text x="2" y="31" font-size="10">Text is not SVG - cannot display</text>
        </a>
      </svg>)SVG";

    auto logger = sk_make_sp<FilterTestLogger>();
    auto stream = SkMemoryStream::MakeDirect(svgText.data(), svgText.size());
    auto svgDOM = SkSVGDOM::Builder()
                          .setLogger(logger)
                          .setFontManager(ToolUtils::TestFontMgr())
                          .make(*stream);
    REPORTER_ASSERT(reporter, svgDOM);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(220, 32);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    svgDOM->render(&canvas);

    int ordinaryTextPixels = 0;
    int warningTextPixels = 0;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 220; ++x) {
            ordinaryTextPixels += SkColorGetA(bitmap.getColor(x, y)) > 0;
        }
    }
    for (int y = 16; y < 32; ++y) {
        for (int x = 0; x < 220; ++x) {
            warningTextPixels += SkColorGetA(bitmap.getColor(x, y)) > 0;
        }
    }
    REPORTER_ASSERT(reporter, ordinaryTextPixels > 0);
    REPORTER_ASSERT(reporter, warningTextPixels == 0);
}
