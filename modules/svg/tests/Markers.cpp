/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkRect.h"
#include "include/core/SkStream.h"
#include "include/utils/SkNoDrawCanvas.h"
#include "modules/skshaper/utils/FactoryHelpers.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "modules/svg/include/SkSVGIDMapper.h"
#include "modules/svg/include/SkSVGRect.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "modules/svg/include/SkSVGSymbol.h"
#include "tests/Test.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace {

class TestLogger final : public SkSVGDOM::Logger {
public:
    void log(Level level, const char[]) override {
        level == Level::kError ? ++fErrors : ++fWarnings;
    }

    int fWarnings = 0;
    int fErrors = 0;
};

sk_sp<SkSVGDOM> parse_svg(const std::string& svg, sk_sp<SkSVGDOM::Logger> logger = nullptr) {
    auto stream = SkMemoryStream::MakeDirect(svg.data(), svg.size());
    return SkSVGDOM::Builder().setLogger(std::move(logger)).make(*stream);
}

bool has_color_near(const SkBitmap& bitmap, int cx, int cy, SkColor expected) {
    const auto close = [](unsigned a, unsigned b) {
        return std::abs(static_cast<int>(a) - static_cast<int>(b)) < 32;
    };
    for (int y = std::max(0, cy - 6); y <= std::min(bitmap.height() - 1, cy + 6); ++y) {
        for (int x = std::max(0, cx - 6); x <= std::min(bitmap.width() - 1, cx + 6); ++x) {
            const SkColor actual = bitmap.getColor(x, y);
            if (SkColorGetA(actual) > 0 &&
                close(SkColorGetR(actual), SkColorGetR(expected)) &&
                close(SkColorGetG(actual), SkColorGetG(expected)) &&
                close(SkColorGetB(actual), SkColorGetB(expected))) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

DEF_TEST(Svg_Markers_RenderAndContextPaint, reporter) {
    const std::string svg = R"SVG(
      <svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
        <defs>
          <marker id="arrow" markerWidth="10" markerHeight="10" refX="10" refY="5"
                  markerUnits="userSpaceOnUse" orient="auto" viewBox="0 0 10 10">
            <path d="M0 0 L10 5 L0 10 z" fill="context-stroke"/>
          </marker>
        </defs>
        <path d="M10 50 C35 10 55 90 80 50" fill="none" stroke="#0055ff"
              stroke-width="3" marker-end="url(#arrow)"/>
      </svg>)SVG";

    auto logger = sk_make_sp<TestLogger>();
    auto dom = parse_svg(svg, logger);
    REPORTER_ASSERT(reporter, dom);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(100, 100);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    bool foundBlueMarkerPixel = false;
    for (int y = 43; y <= 57 && !foundBlueMarkerPixel; ++y) {
        for (int x = 70; x <= 80; ++x) {
            const SkColor color = bitmap.getColor(x, y);
            if (SkColorGetB(color) > 150 && SkColorGetA(color) > 0) {
                foundBlueMarkerPixel = true;
                break;
            }
        }
    }
    REPORTER_ASSERT(reporter, foundBlueMarkerPixel);
}

DEF_TEST(Svg_Markers_ContextPaintPreservesGradientCoordinates, reporter) {
    const std::string svg = R"SVG(
      <svg width="100" height="30">
        <defs>
          <linearGradient id="gradient" gradientUnits="userSpaceOnUse"
                          x1="0" y1="0" x2="100" y2="0">
            <stop offset="0" stop-color="#ff0000"/>
            <stop offset="1" stop-color="#0000ff"/>
          </linearGradient>
          <marker id="marker" markerWidth="10" markerHeight="10" refX="5" refY="5"
                  markerUnits="userSpaceOnUse" viewBox="0 0 10 10">
            <rect width="10" height="10" fill="context-fill"/>
          </marker>
        </defs>
        <path d="M10 15H80" fill="url(#gradient)" stroke="black"
              marker-end="url(#marker)"/>
      </svg>)SVG";

    auto dom = parse_svg(svg);
    REPORTER_ASSERT(reporter, dom);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(100, 30);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    // The marker samples the context element's user-space gradient at x=80. If the shader is
    // incorrectly restarted in marker coordinates, this pixel is red instead of blue.
    const SkColor markerCenter = bitmap.getColor(80, 15);
    REPORTER_ASSERT(reporter, SkColorGetA(markerCenter) > 200);
    REPORTER_ASSERT(reporter, SkColorGetB(markerCenter) > 150);
    REPORTER_ASSERT(reporter, SkColorGetB(markerCenter) > 2 * SkColorGetR(markerCenter));
}

DEF_TEST(Svg_Markers_PercentageReferenceUsesViewBox, reporter) {
    const std::string svg = R"SVG(
      <svg width="60" height="30">
        <defs>
          <marker id="marker" markerWidth="20" markerHeight="20" refX="50%" refY="50%"
                  markerUnits="userSpaceOnUse" viewBox="0 0 10 10">
            <rect width="10" height="10" fill="#ff0000"/>
          </marker>
        </defs>
        <path d="M5 15H30" fill="none" stroke="black" marker-end="url(#marker)"/>
      </svg>)SVG";

    auto dom = parse_svg(svg);
    REPORTER_ASSERT(reporter, dom);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(60, 30);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    // Fifty percent of the 10-unit viewBox maps to 10 pixels in the 20-pixel marker viewport,
    // centering the marker on the path endpoint.
    REPORTER_ASSERT(reporter, SkColorGetR(bitmap.getColor(38, 6)) > 200);
    REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(12, 6)) == 0);
}

DEF_TEST(Svg_MarkerAndSymbolHonorVisibleOverflow, reporter) {
    const std::string svg = R"SVG(
      <svg width="90" height="30">
        <defs>
          <marker id="marker" markerWidth="10" markerHeight="10" refX="5" refY="5"
                  markerUnits="userSpaceOnUse" style="overflow:visible">
            <rect x="-5" width="20" height="10" fill="#ff0000"/>
          </marker>
          <symbol id="visible" style="overflow:visible">
            <rect x="-5" width="20" height="10" fill="#0000ff"/>
          </symbol>
          <symbol id="clipped">
            <rect x="-5" width="20" height="10" fill="#00ff00"/>
          </symbol>
        </defs>
        <path d="M5 15H20" fill="none" stroke="black" marker-end="url(#marker)"/>
        <use href="#visible" x="40" width="10" height="10"/>
        <use href="#clipped" x="70" width="10" height="10"/>
      </svg>)SVG";

    auto logger = sk_make_sp<TestLogger>();
    auto dom = parse_svg(svg, logger);
    REPORTER_ASSERT(reporter, dom);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(90, 30);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    REPORTER_ASSERT(reporter, SkColorGetR(bitmap.getColor(11, 12)) > 200);
    REPORTER_ASSERT(reporter, SkColorGetB(bitmap.getColor(36, 5)) > 200);
    REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(66, 5)) == 0);
}

DEF_TEST(Svg_Markers_RecursiveContextPaintTerminates, reporter) {
    const std::string svg = R"SVG(
      <svg width="20" height="20">
        <defs>
          <marker id="marker" markerWidth="4" markerHeight="4"
                  markerUnits="userSpaceOnUse">
            <rect width="4" height="4" fill="context-fill"/>
          </marker>
        </defs>
        <path d="M2 10 L16 10" fill="context-fill" marker-end="url(#marker)"/>
      </svg>)SVG";

    auto dom = parse_svg(svg);
    REPORTER_ASSERT(reporter, dom);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(20, 20);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    // The referencing path has no context element, so its context-fill and the marker's
    // recursively referenced context-fill both resolve to no paint.
    for (int y = 0; y < bitmap.height(); ++y) {
        for (int x = 0; x < bitmap.width(); ++x) {
            REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(x, y)) == 0);
        }
    }
}

DEF_TEST(Svg_Use_EstablishesContextPaints, reporter) {
    const std::string svg = R"SVG(
      <svg width="30" height="10">
        <defs>
          <symbol id="symbol" viewBox="0 0 10 10">
            <rect width="10" height="10" fill="context-fill"/>
          </symbol>
          <g id="group">
            <rect width="10" height="10" fill="context-stroke"/>
          </g>
        </defs>
        <use href="#symbol" width="10" height="10" fill="#ff0000"/>
        <use href="#group" x="20" stroke="#0000ff"/>
      </svg>)SVG";

    auto dom = parse_svg(svg);
    REPORTER_ASSERT(reporter, dom);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(30, 10);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    REPORTER_ASSERT(reporter, has_color_near(bitmap, 5, 5, SK_ColorRED));
    REPORTER_ASSERT(reporter, has_color_near(bitmap, 25, 5, SK_ColorBLUE));
}

DEF_TEST(Svg_Use_OpacityAppliesToReferencedSubtree, reporter) {
    const std::string svg = R"SVG(
      <svg width="20" height="10">
        <defs>
          <symbol id="symbol" viewBox="0 0 10 10">
            <rect width="10" height="10" fill="#ff0000"/>
          </symbol>
        </defs>
        <use href="#symbol" width="10" height="10" opacity="0.25"/>
      </svg>)SVG";

    auto dom = parse_svg(svg);
    REPORTER_ASSERT(reporter, dom);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(20, 10);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    const SkColor center = bitmap.getColor(5, 5);
    REPORTER_ASSERT(reporter, SkColorGetR(center) > 200);
    REPORTER_ASSERT(reporter, SkColorGetA(center) > 50 && SkColorGetA(center) < 80);
}

DEF_TEST(Svg_SymbolObjectBoundingBoxIncludesOverflow, reporter) {
    auto symbol = SkSVGSymbol::Make();
    symbol->setViewBox(SkRect::MakeWH(10, 10));
    auto rect = SkSVGRect::Make();
    rect->setX(SkSVGLength(-10));
    rect->setWidth(SkSVGLength(30));
    rect->setHeight(SkSVGLength(10));
    symbol->appendChild(std::move(rect));

    SkNoDrawCanvas canvas(100, 50);
    const SkSVGIDMapper mapper;
    const SkSVGLengthContext lengthContext({100, 50});
    SkSVGPresentationContext presentationContext;
    presentationContext.fInherited = SkSVGPresentationAttributes::MakeInitial();
    sk_sp<skresources::ResourceProvider> resourceProvider;
    const SkSVGRenderContext renderContext(&canvas,
                                           SkFontMgr::RefEmpty(),
                                           resourceProvider,
                                           mapper,
                                           lengthContext,
                                           presentationContext,
                                           {nullptr, nullptr},
                                           SkShapers::BestAvailable());

    // preserveAspectRatio="xMidYMid meet" maps [-10, 20] to [-25, 125]. The viewport clips
    // rendering to [0, 100], but the object bounding box must retain the overflowing geometry.
    const SkRect bounds = symbol->objectBoundingBox(renderContext, 100, 50);
    REPORTER_ASSERT(reporter, bounds == SkRect::MakeLTRB(-25, 0, 125, 50));
}

DEF_TEST(Svg_MarkerEmptyViewBoxDoesNotRender, reporter) {
    const std::string svg = R"SVG(
      <svg width="30" height="20">
        <defs>
          <marker id="marker" markerUnits="userSpaceOnUse" markerWidth="10" markerHeight="10"
                  viewBox="0 0 0 10">
            <rect width="10" height="10" fill="red"/>
          </marker>
        </defs>
        <path d="M5 5 L20 5" fill="none" stroke="none" marker-start="url(#marker)"/>
      </svg>)SVG";

    auto dom = parse_svg(svg);
    REPORTER_ASSERT(reporter, dom);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(30, 20);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    for (int y = 0; y < bitmap.height(); ++y) {
        for (int x = 0; x < bitmap.width(); ++x) {
            REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(x, y)) == 0);
        }
    }
}

DEF_TEST(Svg_Use_TrimsLocalReferenceWhitespace, reporter) {
    const std::string svg = R"SVG(
      <svg width="20" height="10">
        <defs>
          <rect id="leading" width="10" height="10" fill="#ff0000"/>
          <rect id="trailing" width="10" height="10" fill="#0000ff"/>
        </defs>
        <use href=" #leading"/>
        <use href="#trailing " x="10"/>
      </svg>)SVG";

    auto logger = sk_make_sp<TestLogger>();
    auto dom = parse_svg(svg, logger);
    REPORTER_ASSERT(reporter, dom);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    REPORTER_ASSERT(reporter, logger->fErrors == 0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(20, 10);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    REPORTER_ASSERT(reporter, has_color_near(bitmap, 5, 5, SK_ColorRED));
    REPORTER_ASSERT(reporter, has_color_near(bitmap, 15, 5, SK_ColorBLUE));
}

DEF_TEST(Svg_Diagnostics_UnsupportedAndUnresolved, reporter) {
    const std::string svg = R"SVG(
      <svg width="10" height="10">
        <style>.x { fill: red; }</style>
        <path d="M0 0L10 10" marker-end="url(#missing)"/>
      </svg>)SVG";
    auto logger = sk_make_sp<TestLogger>();
    auto dom = parse_svg(svg, logger);
    REPORTER_ASSERT(reporter, dom);
    REPORTER_ASSERT(reporter, logger->fWarnings == 2);

    auto malformedLogger = sk_make_sp<TestLogger>();
    REPORTER_ASSERT(reporter, !parse_svg("<svg><path></svg>", malformedLogger));
    REPORTER_ASSERT(reporter, malformedLogger->fErrors == 1);

    auto invalidStyleLogger = sk_make_sp<TestLogger>();
    REPORTER_ASSERT(reporter,
                    parse_svg("<svg><path style='fill: red; unsupported: value'/></svg>",
                              invalidStyleLogger));
    REPORTER_ASSERT(reporter, invalidStyleLogger->fWarnings == 1);

    auto externalLogger = sk_make_sp<TestLogger>();
    REPORTER_ASSERT(reporter,
                    parse_svg("<svg><use href='https://example.com/shape.svg#id'/></svg>",
                              externalLogger));
    REPORTER_ASSERT(reporter, externalLogger->fErrors == 1);

    auto dataUseLogger = sk_make_sp<TestLogger>();
    REPORTER_ASSERT(reporter,
                    parse_svg("<svg><use href='data:image/svg+xml,%3Csvg/%3E'/></svg>",
                              dataUseLogger));
    REPORTER_ASSERT(reporter, dataUseLogger->fErrors == 1);

    auto safeReferencesLogger = sk_make_sp<TestLogger>();
    auto safeReferencesDOM = parse_svg(
            R"SVG(<svg width="10" height="10">
                     <defs><clipPath id="clip"><rect width="5" height="5"/></clipPath></defs>
                     <a href="https://example.com/details">
                       <rect width="10" height="10" style="clip-path:url('#clip')"/>
                     </a>
                   </svg>)SVG",
            safeReferencesLogger);
    REPORTER_ASSERT(reporter, safeReferencesDOM);
    REPORTER_ASSERT(reporter, safeReferencesLogger->fWarnings == 0);
    REPORTER_ASSERT(reporter, safeReferencesLogger->fErrors == 0);

    auto spacedReferenceLogger = sk_make_sp<TestLogger>();
    auto spacedReferenceDOM = parse_svg(
            R"SVG(<svg width="10" height="10">
                     <defs><rect id="shape" width="10" height="10"/></defs>
                     <use href=" #shape"/>
                   </svg>)SVG",
            spacedReferenceLogger);
    REPORTER_ASSERT(reporter, spacedReferenceDOM);
    REPORTER_ASSERT(reporter, spacedReferenceLogger->fWarnings == 0);
    REPORTER_ASSERT(reporter, spacedReferenceLogger->fErrors == 0);

    auto metadataLogger = sk_make_sp<TestLogger>();
    auto metadataDOM = parse_svg(
            R"SVG(<svg width="10" height="10"
                       aria-label="See url(https://example.com/help)"
                       data-note="url(#not-an-svg-reference)">
                     <rect width="10" height="10"/>
                   </svg>)SVG",
            metadataLogger);
    REPORTER_ASSERT(reporter, metadataDOM);
    REPORTER_ASSERT(reporter, metadataLogger->fWarnings == 0);
    REPORTER_ASSERT(reporter, metadataLogger->fErrors == 0);

    SkBitmap safeReferencesBitmap;
    safeReferencesBitmap.allocN32Pixels(10, 10);
    safeReferencesBitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas safeReferencesCanvas(safeReferencesBitmap);
    safeReferencesDOM->render(&safeReferencesCanvas);
    REPORTER_ASSERT(reporter, SkColorGetA(safeReferencesBitmap.getColor(2, 2)) > 0);
    REPORTER_ASSERT(reporter, SkColorGetA(safeReferencesBitmap.getColor(8, 8)) == 0);
}

DEF_TEST(Svg_Markers_StartMidEndInheritanceAndOrientation, reporter) {
    const std::string svg = R"SVG(
      <svg width="200" height="80">
        <defs>
          <marker id="start" markerWidth="12" markerHeight="12" refX="6" refY="6"
                  markerUnits="userSpaceOnUse" orient="auto-start-reverse" viewBox="0 0 12 12">
            <path d="M0 6 L12 0 L12 12 z" fill="#ff0000"/>
          </marker>
          <marker id="mid" markerWidth="12" markerHeight="12" refX="6" refY="6"
                  markerUnits="userSpaceOnUse" orient="auto">
            <circle cx="6" cy="6" r="5" fill="#00ff00"/>
          </marker>
          <marker id="end" markerWidth="12" markerHeight="12" refX="6" refY="6"
                  markerUnits="userSpaceOnUse" orient="90">
            <rect x="2" y="2" width="8" height="8" fill="#0000ff"/>
          </marker>
        </defs>
        <g marker-start="url(#start)" marker-mid="url(#mid)" marker-end="url(#end)">
          <polyline points="20,60 100,20 180,60" fill="none" stroke="black"/>
        </g>
      </svg>)SVG";

    auto logger = sk_make_sp<TestLogger>();
    auto dom = parse_svg(svg, logger);
    REPORTER_ASSERT(reporter, dom);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(200, 80);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);
    REPORTER_ASSERT(reporter, has_color_near(bitmap, 20, 60, SK_ColorRED));
    REPORTER_ASSERT(reporter, has_color_near(bitmap, 100, 20, SK_ColorGREEN));
    REPORTER_ASSERT(reporter, has_color_near(bitmap, 180, 60, SK_ColorBLUE));
}

DEF_TEST(Svg_Markers_ShapeKindsClosedDegenerateAndMultipleContours, reporter) {
    const std::string svg = R"SVG(
      <svg width="160" height="100">
        <defs>
          <marker id="dot" markerWidth="8" markerHeight="8" refX="4" refY="4"
                  markerUnits="strokeWidth" orient="auto" viewBox="0 0 8 8">
            <circle cx="4" cy="4" r="3" fill="#ff00ff"/>
          </marker>
        </defs>
        <line x1="10" y1="15" x2="45" y2="15" stroke="black" stroke-width="2"
              marker-end="url(#dot)"/>
        <polygon points="60,10 90,10 75,30" fill="none" stroke="black"
                 marker-end="url(#dot)"/>
        <path d="M10 55 L10 55 L45 55 Z M65 70 C80 35 100 90 120 55"
              fill="none" stroke="black" marker-end="url(#dot)"/>
        <path d="M140 80 L140 80" fill="none" stroke="black"
              marker-start="url(#dot)" marker-end="url(#dot)"/>
      </svg>)SVG";

    auto logger = sk_make_sp<TestLogger>();
    auto dom = parse_svg(svg, logger);
    REPORTER_ASSERT(reporter, dom);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(160, 100);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);
    REPORTER_ASSERT(reporter, has_color_near(bitmap, 45, 15, SK_ColorMAGENTA));
    REPORTER_ASSERT(reporter, has_color_near(bitmap, 60, 10, SK_ColorMAGENTA));
    REPORTER_ASSERT(reporter, has_color_near(bitmap, 120, 55, SK_ColorMAGENTA));
    REPORTER_ASSERT(reporter, has_color_near(bitmap, 140, 80, SK_ColorMAGENTA));
}

DEF_TEST(Svg_Markers_DoNotInheritReferencingShapePresentation, reporter) {
    const std::string svg = R"SVG(
      <svg width="100" height="60">
        <defs>
          <marker id="arrow" markerWidth="10" markerHeight="10" refX="0" refY="5"
                  markerUnits="userSpaceOnUse" orient="auto" viewBox="0 0 10 10">
            <path d="M0 0 L10 5 L0 10 z"/>
          </marker>
        </defs>
        <path d="M10 30 L60 30" fill="none" stroke="#ff0000" marker-end="url(#arrow)"/>
      </svg>)SVG";

    auto logger = sk_make_sp<TestLogger>();
    auto dom = parse_svg(svg, logger);
    REPORTER_ASSERT(reporter, dom);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(100, 60);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    REPORTER_ASSERT(reporter, has_color_near(bitmap, 65, 30, SK_ColorBLACK));
}

DEF_TEST(Svg_Markers_InheritDefinitionAncestorPresentation, reporter) {
    const std::string svg = R"SVG(
      <svg width="30" height="20">
        <defs fill="#0000ff">
          <marker id="square" markerWidth="6" markerHeight="6" refX="0" refY="0"
                  markerUnits="userSpaceOnUse">
            <rect width="6" height="6"/>
          </marker>
        </defs>
        <path d="M10 10 L20 10" fill="none" stroke="#ff0000"
              marker-start="url(#square)"/>
      </svg>)SVG";

    auto logger = sk_make_sp<TestLogger>();
    auto dom = parse_svg(svg, logger);
    REPORTER_ASSERT(reporter, dom);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);

    SkBitmap bitmap;
    bitmap.allocN32Pixels(30, 20);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    REPORTER_ASSERT(reporter, has_color_near(bitmap, 12, 12, SK_ColorBLUE));
}

DEF_TEST(Svg_Markers_ViewBoxTransformClipOpacityAndContextFill, reporter) {
    const std::string svg = R"SVG(
      <svg width="120" height="80">
        <defs>
          <marker id="clipped" markerWidth="20" markerHeight="20" refX="5" refY="5"
                  markerUnits="userSpaceOnUse" orient="auto" viewBox="0 0 10 10"
                  preserveAspectRatio="xMidYMid meet">
            <rect x="-10" y="-10" width="30" height="30" fill="context-fill"/>
          </marker>
        </defs>
        <g transform="translate(10 5)" fill="#ff0000" opacity="0.5"
           marker-end="url(#clipped)">
          <line x1="10" y1="30" x2="60" y2="30" stroke="black"/>
        </g>
      </svg>)SVG";

    auto logger = sk_make_sp<TestLogger>();
    auto dom = parse_svg(svg, logger);
    REPORTER_ASSERT(reporter, dom);
    REPORTER_ASSERT(reporter, logger->fWarnings == 0);
    SkBitmap bitmap;
    bitmap.allocN32Pixels(120, 80);
    bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(bitmap);
    dom->render(&canvas);

    const SkColor markerColor = bitmap.getColor(70, 30);
    REPORTER_ASSERT(reporter, SkColorGetR(markerColor) > 200);
    REPORTER_ASSERT(reporter, SkColorGetA(markerColor) > 90 && SkColorGetA(markerColor) < 170);
    REPORTER_ASSERT(reporter, SkColorGetA(bitmap.getColor(45, 25)) == 0);
}
