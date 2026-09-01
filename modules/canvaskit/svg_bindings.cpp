/*
 * Copyright 2026 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/codec/SkCodec.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkPaint.h"
#include "include/core/SkScalar.h"
#include "include/core/SkSize.h"
#include "include/core/SkStream.h"
#include "include/core/SkString.h"
#include "include/core/SkTypeface.h"
#include "include/private/base/SkMutex.h"
#include "include/private/base/SkOnce.h"
#include "include/ports/SkFontMgr_data.h"
#include "include/utils/SkNoDrawCanvas.h"
#include "modules/skresources/include/SkResources.h"
#include "modules/skshaper/utils/FactoryHelpers.h"
#include "modules/svg/include/SkSVGDOM.h"
#include "src/core/SkTHash.h"

#include <emscripten/bind.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#if defined(SK_CODEC_DECODES_GIF)
#include "include/codec/SkGifDecoder.h"
#endif
#if defined(SK_CODEC_DECODES_JPEG)
#include "include/codec/SkJpegDecoder.h"
#endif
#if defined(SK_CODEC_DECODES_PNG)
#include "include/codec/SkPngDecoder.h"
#endif
#if defined(SK_CODEC_DECODES_WEBP)
#include "include/codec/SkWebpDecoder.h"
#endif

using namespace emscripten;

#if defined(CK_EMBED_FONT)
struct SkEmbeddedResource { const uint8_t* data; size_t size; };
struct SkEmbeddedResourceHeader { const SkEmbeddedResource* entries; int count; };
extern "C" const SkEmbeddedResourceHeader SK_EMBEDDED_FONTS;
#endif

namespace {

sk_sp<SkFontMgr> default_svg_font_manager() {
#if defined(CK_EMBED_FONT)
    static SkOnce once;
    static sk_sp<SkFontMgr> fontMgr;
    once([] {
        std::vector<sk_sp<SkData>> fonts;
        fonts.reserve(SK_EMBEDDED_FONTS.count);
        for (int i = 0; i < SK_EMBEDDED_FONTS.count; ++i) {
            const auto& font = SK_EMBEDDED_FONTS.entries[i];
            fonts.push_back(SkData::MakeWithoutCopy(font.data, font.size));
        }
        if (!fonts.empty()) {
            fontMgr = SkFontMgr_New_Custom_Data(fonts);
        }
    });
    if (fontMgr) {
        return fontMgr;
    }
#endif
    return SkFontMgr::RefEmpty();
}

void register_svg_codecs() {
    static SkOnce once;
    once([] {
#if defined(SK_CODEC_DECODES_PNG)
        SkCodecs::Register(SkPngDecoder::Decoder());
#endif
#if defined(SK_CODEC_DECODES_JPEG)
        SkCodecs::Register(SkJpegDecoder::Decoder());
#endif
#if defined(SK_CODEC_DECODES_GIF)
        SkCodecs::Register(SkGifDecoder::Decoder());
#endif
#if defined(SK_CODEC_DECODES_WEBP)
        SkCodecs::Register(SkWebpDecoder::Decoder());
#endif
    });
}

class SVGLogger final : public SkSVGDOM::Logger {
public:
    explicit SVGLogger(val logger) : fLogger(std::move(logger)) {}

    void log(Level level, const char message[]) override {
        fRenderable = false;
        if (fLogger.isNull() || fLogger.isUndefined()) {
            return;
        }
        const char* callbackName = level == Level::kError ? "onError" : "onWarning";
        const val callback = fLogger[callbackName];
        if (!callback.isUndefined() && !callback.isNull()) {
            fLogger.call<void>(callbackName, std::string(message));
        }
    }

    bool renderable() const { return fRenderable; }

private:
    val fLogger;
    bool fRenderable = true;
};

class TrackingResourceProvider final : public skresources::ResourceProvider {
public:
    TrackingResourceProvider(sk_sp<skresources::ResourceProvider> delegate,
                             sk_sp<SVGLogger> logger)
        : fDelegate(std::move(delegate)), fLogger(std::move(logger)) {}

    sk_sp<SkData> load(const char path[], const char name[]) const override {
        return fDelegate->load(path, name);
    }

    sk_sp<skresources::ImageAsset> loadImageAsset(const char path[],
                                                  const char name[],
                                                  const char id[]) const override {
        // SkSVGImage does not supply a resource id for data URIs. Cache those by their URI so
        // multiple embedded images do not alias the same empty-id entry.
        const SkString key(id && id[0] ? id : name ? name : "");
        SkAutoMutexExclusive lock(fImageMutex);
        if (const auto* cached = fImageCache.find(key)) {
            return *cached;
        }

        auto asset = fDelegate->loadImageAsset(path, name, id);
        if (!asset) {
            SkString message;
            message.printf("Unable to load SVG image resource '%s'.", name ? name : "");
            fLogger->log(SkSVGDOM::Logger::Level::kError, message.c_str());
        }
        fImageCache.set(key, asset);
        return asset;
    }

    sk_sp<SkTypeface> loadTypeface(const char name[], const char url[]) const override {
        auto typeface = fDelegate->loadTypeface(name, url);
        if (!typeface) {
            SkString message;
            message.printf("Unable to load SVG font resource '%s'.", url ? url : "");
            fLogger->log(SkSVGDOM::Logger::Level::kError, message.c_str());
        }
        return typeface;
    }

    sk_sp<SkData> loadFont(const char name[], const char url[]) const override {
        return fDelegate->loadFont(name, url);
    }

private:
    sk_sp<skresources::ResourceProvider> fDelegate;
    sk_sp<SVGLogger> fLogger;
    mutable SkMutex fImageMutex;
    mutable skia_private::THashMap<SkString, sk_sp<skresources::ImageAsset>> fImageCache;
};

class SVGValidationCanvas final : public SkNoDrawCanvas {
public:
    SVGValidationCanvas(int width, int height) : SkNoDrawCanvas(width, height) {}

    bool vectorSafe() const { return fVectorSafe; }

protected:
    SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override {
        if ((rec.fPaint && rec.fPaint->getImageFilter()) || rec.fBackdrop ||
            !rec.fFilters.empty()) {
            fVectorSafe = false;
        }
        return this->SkNoDrawCanvas::getSaveLayerStrategy(rec);
    }

private:
    bool fVectorSafe = true;
};

class SVGDOMWrapper final : public SkRefCnt {
public:
    static sk_sp<SVGDOMWrapper> Make(std::string svg,
                                     SkFontMgr* suppliedFontMgr,
                                     val jsLogger) {
        register_svg_codecs();

        auto logger = sk_make_sp<SVGLogger>(std::move(jsLogger));
        sk_sp<SkFontMgr> fontMgr = sk_ref_sp(suppliedFontMgr);
        if (!fontMgr) {
            fontMgr = default_svg_font_manager();
        }
        auto dataURIProvider = skresources::DataURIResourceProviderProxy::Make(
                nullptr, skresources::ImageDecodeStrategy::kPreDecode, fontMgr);
        auto trackingProvider = sk_make_sp<TrackingResourceProvider>(
                std::move(dataURIProvider), logger);

        SkMemoryStream stream(svg.data(), svg.size(), false);
        auto dom = SkSVGDOM::Builder()
                           .setFontManager(fontMgr)
                           .setResourceProvider(trackingProvider)
                           .setTextShapingFactory(SkShapers::BestAvailable())
                           .setLogger(logger)
                           .make(stream);
        if (!dom) {
            return nullptr;
        }
        return sk_sp<SVGDOMWrapper>(new SVGDOMWrapper(
                std::move(dom), std::move(trackingProvider), std::move(logger)));
    }

    void setContainerSize(float width, float height) {
        fDOM->setContainerSize(SkSize::Make(width, height));
        fValidated = false;
    }

    bool validate() {
        if (!fLogger->renderable()) {
            return false;
        }
        if (fValidated) {
            return true;
        }
        const SkSize size = fDOM->containerSize();
        const int width = std::max(1, SkScalarCeilToInt(size.width()));
        const int height = std::max(1, SkScalarCeilToInt(size.height()));
        SVGValidationCanvas canvas(width, height);
        fDOM->render(&canvas);
        if (!canvas.vectorSafe()) {
            fLogger->log(SkSVGDOM::Logger::Level::kWarning,
                         "SVG filter requires rasterizing vector content.");
            return false;
        }
        fValidated = fLogger->renderable();
        return fValidated;
    }

    bool render(SkCanvas* canvas) {
        if (!canvas || !this->validate()) {
            return false;
        }
        fDOM->render(canvas);
        return true;
    }

private:
    SVGDOMWrapper(sk_sp<SkSVGDOM> dom,
                  sk_sp<TrackingResourceProvider> resourceProvider,
                  sk_sp<SVGLogger> logger)
        : fDOM(std::move(dom))
        , fResourceProvider(std::move(resourceProvider))
        , fLogger(std::move(logger)) {}

    sk_sp<SkSVGDOM> fDOM;
    sk_sp<TrackingResourceProvider> fResourceProvider;
    sk_sp<SVGLogger> fLogger;
    bool fValidated = false;
};

}  // namespace

EMSCRIPTEN_BINDINGS(CanvasKitSVG) {
    class_<SVGDOMWrapper>("SVGDOM")
            .smart_ptr<sk_sp<SVGDOMWrapper>>("sk_sp<SVGDOM>")
            .class_function("_MakeFromString", &SVGDOMWrapper::Make, allow_raw_pointers())
            .function("setContainerSize", &SVGDOMWrapper::setContainerSize)
            .function("validate", &SVGDOMWrapper::validate)
            .function("render", &SVGDOMWrapper::render, allow_raw_pointers());
}
