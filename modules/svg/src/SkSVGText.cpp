/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "modules/svg/include/SkSVGText.h"

#include "include/core/SkCanvas.h"
#include "include/core/SkContourMeasure.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkFontStyle.h"
#include "include/core/SkFontTypes.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRect.h"
#include "include/core/SkRSXform.h"
#include "include/core/SkScalar.h"
#include "include/core/SkString.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTypeface.h"
#include "include/core/SkTypes.h"
#include "include/private/base/SkTArray.h"
#include "include/private/base/SkTemplates.h"
#include "include/private/base/SkTo.h"
#include "modules/skshaper/include/SkShaper.h"
#include "modules/skunicode/include/SkUnicode.h"
#include "modules/svg/include/SkSVGAttribute.h"
#include "modules/svg/include/SkSVGAttributeParser.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "modules/svg/src/SkSVGTextPriv.h"
#include "src/base/SkTLazy.h"
#include "src/base/SkUTF.h"
#include "src/core/SkTextBlobPriv.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <tuple>
#include <utility>

using namespace skia_private;

namespace {

static SkFont ResolveFont(const SkSVGRenderContext& ctx) {
    auto weight = [](const SkSVGFontWeight& w) {
        switch (w.type()) {
            case SkSVGFontWeight::Type::k100:     return SkFontStyle::kThin_Weight;
            case SkSVGFontWeight::Type::k200:     return SkFontStyle::kExtraLight_Weight;
            case SkSVGFontWeight::Type::k300:     return SkFontStyle::kLight_Weight;
            case SkSVGFontWeight::Type::k400:     return SkFontStyle::kNormal_Weight;
            case SkSVGFontWeight::Type::k500:     return SkFontStyle::kMedium_Weight;
            case SkSVGFontWeight::Type::k600:     return SkFontStyle::kSemiBold_Weight;
            case SkSVGFontWeight::Type::k700:     return SkFontStyle::kBold_Weight;
            case SkSVGFontWeight::Type::k800:     return SkFontStyle::kExtraBold_Weight;
            case SkSVGFontWeight::Type::k900:     return SkFontStyle::kBlack_Weight;
            case SkSVGFontWeight::Type::kNormal:  return SkFontStyle::kNormal_Weight;
            case SkSVGFontWeight::Type::kBold:    return SkFontStyle::kBold_Weight;
            case SkSVGFontWeight::Type::kBolder:  return SkFontStyle::kExtraBold_Weight;
            case SkSVGFontWeight::Type::kLighter: return SkFontStyle::kLight_Weight;
            case SkSVGFontWeight::Type::kInherit: {
                SkASSERT(false);
                return SkFontStyle::kNormal_Weight;
            }
        }
        SkUNREACHABLE;
    };

    auto slant = [](const SkSVGFontStyle& s) {
        switch (s.type()) {
            case SkSVGFontStyle::Type::kNormal:  return SkFontStyle::kUpright_Slant;
            case SkSVGFontStyle::Type::kItalic:  return SkFontStyle::kItalic_Slant;
            case SkSVGFontStyle::Type::kOblique: return SkFontStyle::kOblique_Slant;
            case SkSVGFontStyle::Type::kInherit: {
                SkASSERT(false);
                return SkFontStyle::kUpright_Slant;
            }
        }
        SkUNREACHABLE;
    };

    const auto& family = ctx.presentationContext().fInherited.fFontFamily->family();
    const SkFontStyle style(weight(*ctx.presentationContext().fInherited.fFontWeight),
                            SkFontStyle::kNormal_Width,
                            slant(*ctx.presentationContext().fInherited.fFontStyle));

    const auto size =
            ctx.lengthContext().resolve(ctx.presentationContext().fInherited.fFontSize->size(),
                                        SkSVGLengthContext::LengthType::kVertical);

    // TODO: we likely want matchFamilyStyle here, but switching away from legacyMakeTypeface
    // changes all the results when using the default fontmgr.
    auto tf = ctx.fontMgr()->legacyMakeTypeface(family.c_str(), style);
    if (!tf) {
        tf = ctx.fontMgr()->legacyMakeTypeface(nullptr, style);
    }
    // Font-stripped clients can still parse and render non-text SVG content. SkFont substitutes an
    // empty typeface when no font is available, so do not turn an omitted optional font into a
    // debug-only process abort.
    SkFont font(std::move(tf), size);
    font.setHinting(SkFontHinting::kNone);
    font.setSubpixel(true);
    font.setLinearMetrics(true);
    font.setBaselineSnap(false);
    font.setEdging(SkFont::Edging::kAntiAlias);

    return font;
}

static std::vector<float> ResolveLengths(const SkSVGRenderContext& ctx,
                                         const std::vector<SkSVGLength>& lengths,
                                         SkSVGLengthContext::LengthType lt) {
    std::vector<float> resolved;
    resolved.reserve(lengths.size());

    SkTLazy<SkFont> font;
    for (const auto& l : lengths) {
        switch (l.unit()) {
            case SkSVGLength::Unit::kEMS:
                if (!font.isValid()) {
                    font.set(ResolveFont(ctx));
                }
                resolved.push_back(l.value() * font->getSize());
                break;
            case SkSVGLength::Unit::kEXS: {
                if (!font.isValid()) {
                    font.set(ResolveFont(ctx));
                }
                SkFontMetrics metrics;
                font->getMetrics(&metrics);
                const SkScalar xHeight = metrics.fXHeight != 0
                                               ? std::abs(metrics.fXHeight)
                                               : font->getSize() * 0.5f;
                resolved.push_back(l.value() * xHeight);
                break;
            }
            default:
                resolved.push_back(ctx.lengthContext().resolve(l, lt));
                break;
        }
    }

    return resolved;
}

static float ComputeAlignmentFactor(const SkSVGPresentationContext& pctx) {
    switch (pctx.fInherited.fTextAnchor->type()) {
    case SkSVGTextAnchor::Type::kStart : return  0.0f;
    case SkSVGTextAnchor::Type::kMiddle: return -0.5f;
    case SkSVGTextAnchor::Type::kEnd   : return -1.0f;
    case SkSVGTextAnchor::Type::kInherit:
        SkASSERT(false);
        return 0.0f;
    }
    SkUNREACHABLE;
}

static float ResolveBaselineOffset(const SkSVGRenderContext& ctx,
                                   SkSVGDominantBaseline dominant,
                                   SkSVGAlignmentBaseline alignment,
                                   float inheritedOffset) {
    if (dominant == SkSVGDominantBaseline::kAuto &&
        alignment == SkSVGAlignmentBaseline::kAuto) {
        return inheritedOffset;
    }

    const SkFont font = ResolveFont(ctx);
    SkFontMetrics metrics;
    font.getMetrics(&metrics);
    const SkScalar xHeight = metrics.fXHeight != 0
                                     ? std::abs(metrics.fXHeight)
                                     : font.getSize() * 0.5f;

    auto dominantOffset = [&]() {
        switch (dominant) {
            case SkSVGDominantBaseline::kAuto:
            case SkSVGDominantBaseline::kAlphabetic:   return 0.0f;
            case SkSVGDominantBaseline::kMiddle:       return xHeight * 0.5f;
            case SkSVGDominantBaseline::kCentral:      return -(metrics.fAscent +
                                                                 metrics.fDescent) * 0.5f;
            case SkSVGDominantBaseline::kHanging:      return -metrics.fAscent * 0.8f;
            case SkSVGDominantBaseline::kMathematical: return font.getSize() * 0.5f;
        }
        SkUNREACHABLE;
    };

    switch (alignment) {
        case SkSVGAlignmentBaseline::kAuto:         return dominantOffset();
        case SkSVGAlignmentBaseline::kBaseline:
        case SkSVGAlignmentBaseline::kAlphabetic:   return 0;
        case SkSVGAlignmentBaseline::kMiddle:       return xHeight * 0.5f;
        case SkSVGAlignmentBaseline::kCentral:      return -(metrics.fAscent +
                                                              metrics.fDescent) * 0.5f;
        case SkSVGAlignmentBaseline::kHanging:      return -metrics.fAscent * 0.8f;
        case SkSVGAlignmentBaseline::kMathematical: return font.getSize() * 0.5f;
    }
    SkUNREACHABLE;
}

static SkPath BuildTextDecorationPath(const sk_sp<SkTextBlob>& blob,
                                      const SkSVGTextDecoration& decoration) {
    SkPathBuilder pathBuilder;
    if (!blob) {
        return pathBuilder.detach();
    }

    for (SkTextBlobRunIterator it(blob.get()); !it.done(); it.next()) {
        SkFontMetrics metrics;
        it.font().getMetrics(&metrics);

        SkScalar thickness;
        if (!metrics.hasUnderlineThickness(&thickness) || thickness <= 0) {
            thickness = std::max(it.font().getSize() / 16, 0.5f);
        }

        SkScalar underlinePosition;
        if (!metrics.hasUnderlinePosition(&underlinePosition)) {
            underlinePosition = it.font().getSize() / 10;
        }

        SkScalar strikeoutPosition;
        if (!metrics.hasStrikeoutPosition(&strikeoutPosition)) {
            strikeoutPosition = -it.font().getSize() / 3;
        }

        std::vector<SkScalar> glyphWidths(it.glyphCount());
        it.font().getWidths({it.glyphs(), it.glyphCount()}, glyphWidths);

        SkASSERT(it.positioning() == SkTextBlobRunIterator::kRSXform_Positioning);
        for (uint32_t i = 0; i < it.glyphCount(); ++i) {
            if (glyphWidths[i] <= 0) {
                continue;
            }

            SkMatrix glyphMatrix;
            glyphMatrix.setRSXform(it.xforms()[i]);
            SkScalar decorationWidth = glyphWidths[i];
            // Decorations include spacing between aligned glyphs, including textLength expansion.
            if (i + 1 < it.glyphCount()) {
                const SkRSXform& current = it.xforms()[i];
                const SkRSXform& next = it.xforms()[i + 1];
                const SkScalar scaleSquared = current.fSCos * current.fSCos +
                                              current.fSSin * current.fSSin;
                if (scaleSquared > 0 &&
                    SkScalarNearlyEqual(current.fSCos, next.fSCos) &&
                    SkScalarNearlyEqual(current.fSSin, next.fSSin)) {
                    const SkScalar dx = next.fTx - current.fTx;
                    const SkScalar dy = next.fTy - current.fTy;
                    const SkScalar normalOffset = dx * current.fSSin - dy * current.fSCos;
                    if (SkScalarNearlyZero(normalOffset)) {
                        const SkScalar inlineAdvance =
                                (dx * current.fSCos + dy * current.fSSin) / scaleSquared;
                        decorationWidth = std::max(decorationWidth, inlineAdvance);
                    }
                }
            }
            const auto appendRect = [&](SkScalar top) {
                // Add the contour directly so a multi-rectangle path is not left tagged with the
                // first rectangle's convexity.
                SkPoint corners[] = {
                        {0, top},
                        {decorationWidth, top},
                        {decorationWidth, top + thickness},
                        {0, top + thickness},
                };
                glyphMatrix.mapPoints(corners);
                pathBuilder.moveTo(corners[0]);
                pathBuilder.lineTo(corners[1]);
                pathBuilder.lineTo(corners[2]);
                pathBuilder.lineTo(corners[3]);
                pathBuilder.close();
            };

            if (decoration.has(SkSVGTextDecoration::kUnderline)) {
                appendRect(underlinePosition);
            }
            if (decoration.has(SkSVGTextDecoration::kOverline)) {
                appendRect(metrics.fAscent);
            }
            if (decoration.has(SkSVGTextDecoration::kLineThrough)) {
                appendRect(strikeoutPosition - thickness);
            }
        }
    }

    return pathBuilder.detach();
}

} // namespace

SkSVGTextContext::ScopedPosResolver::ScopedPosResolver(const SkSVGTextContainer& txt,
                                                       const SkSVGRenderContext& ctx,
                                                       SkSVGTextContext* tctx,
                                                       size_t charIndexOffset)
    : fTextContext(tctx)
    , fParent(tctx->fPosResolver)
    , fCharIndexOffset(charIndexOffset)
    , fX(ResolveLengths(ctx, txt.getX(), SkSVGLengthContext::LengthType::kHorizontal))
    , fY(ResolveLengths(ctx, txt.getY(), SkSVGLengthContext::LengthType::kVertical))
    , fDx(ResolveLengths(ctx, txt.getDx(), SkSVGLengthContext::LengthType::kHorizontal))
    , fDy(ResolveLengths(ctx, txt.getDy(), SkSVGLengthContext::LengthType::kVertical))
    , fRotate(txt.getRotate())
{
    fTextContext->fPosResolver = this;
}

SkSVGTextContext::ScopedPosResolver::ScopedPosResolver(const SkSVGTextContainer& txt,
                                                       const SkSVGRenderContext& ctx,
                                                       SkSVGTextContext* tctx)
    : ScopedPosResolver(txt, ctx, tctx, tctx->fCurrentCharIndex) {}

SkSVGTextContext::ScopedPosResolver::~ScopedPosResolver() {
    fTextContext->fPosResolver = fParent;
}

SkSVGTextContext::ScopedTextLayout::ScopedTextLayout(const SkSVGTextContainer& text,
                                                     const SkSVGRenderContext& ctx,
                                                     SkSVGTextContext* tctx)
    : fTextContext(tctx)
    , fRenderContext(ctx)
    , fPreviousBaselineOffset(tctx->fBaselineOffset)
    , fPreviousTextLength(tctx->fTextLength)
    , fHasTextLength(text.getTextLength().isValid()) {
    fTextContext->fBaselineOffset = ResolveBaselineOffset(ctx,
                                                          text.getDominantBaseline(),
                                                          text.getAlignmentBaseline(),
                                                          fPreviousBaselineOffset);
    if (fHasTextLength) {
        fTextContext->flushChunk(ctx);
        fTextContext->fTextLength = ResolveLengths(
                ctx,
                {*text.getTextLength()},
                SkSVGLengthContext::LengthType::kHorizontal)[0];
    }
}

SkSVGTextContext::ScopedTextLayout::~ScopedTextLayout() {
    if (fHasTextLength) {
        fTextContext->flushChunk(fRenderContext);
        fTextContext->fTextLength = fPreviousTextLength;
    }
    fTextContext->fBaselineOffset = fPreviousBaselineOffset;
}

SkSVGTextContext::PosAttrs SkSVGTextContext::ScopedPosResolver::resolve(size_t charIndex) const {
    PosAttrs attrs;

    if (charIndex < fLastPosIndex) {
        SkASSERT(charIndex >= fCharIndexOffset);
        const auto localCharIndex = charIndex - fCharIndexOffset;

        const auto hasAllLocal = localCharIndex < fX.size() &&
                                 localCharIndex < fY.size() &&
                                 localCharIndex < fDx.size() &&
                                 localCharIndex < fDy.size() &&
                                 localCharIndex < fRotate.size();
        if (!hasAllLocal && fParent) {
            attrs = fParent->resolve(charIndex);
        }

        if (localCharIndex < fX.size()) {
            attrs[PosAttrs::kX] = fX[localCharIndex];
        }
        if (localCharIndex < fY.size()) {
            attrs[PosAttrs::kY] = fY[localCharIndex];
        }
        if (localCharIndex < fDx.size()) {
            attrs[PosAttrs::kDx] = fDx[localCharIndex];
        }
        if (localCharIndex < fDy.size()) {
            attrs[PosAttrs::kDy] = fDy[localCharIndex];
        }

        // Rotation semantics are interestingly different [1]:
        //
        //   - values are not cumulative
        //   - if explicit values are present at any level in the ancestor chain, those take
        //     precedence (closest ancestor)
        //   - last specified value applies to all remaining chars (closest ancestor)
        //   - these rules apply at node scope (not chunk scope)
        //
        // This means we need to discriminate between explicit rotation (rotate value provided for
        // current char) and implicit rotation (ancestor has some values - but not for the requested
        // char - we use the last specified value).
        //
        // [1] https://www.w3.org/TR/SVG11/text.html#TSpanElementRotateAttribute
        if (!fRotate.empty()) {
            if (localCharIndex < fRotate.size()) {
                // Explicit rotation value overrides anything in the ancestor chain.
                attrs[PosAttrs::kRotate] = fRotate[localCharIndex];
                attrs.setImplicitRotate(false);
            } else if (!attrs.has(PosAttrs::kRotate) || attrs.isImplicitRotate()){
                // Local implicit rotation (last specified value) overrides ancestor implicit
                // rotation.
                attrs[PosAttrs::kRotate] = fRotate.back();
                attrs.setImplicitRotate(true);
            }
        }

        if (!attrs.hasAny()) {
            // Once we stop producing explicit position data, there is no reason to
            // continue trying for higher indices.  We can suppress future lookups.
            fLastPosIndex = charIndex;
        }
    }

    return attrs;
}

void SkSVGTextContext::ShapeBuffer::append(SkUnichar ch, PositionAdjustment pos) {
    // relative pos adjustments are cumulative
    if (!fUtf8PosAdjust.empty()) {
        pos.offset += fUtf8PosAdjust.back().offset;
    }

    char utf8_buf[SkUTF::kMaxBytesInUTF8Sequence];
    const auto utf8_len = SkToInt(SkUTF::ToUTF8(ch, utf8_buf));
    fUtf8         .push_back_n(utf8_len, utf8_buf);
    fUtf8PosAdjust.push_back_n(utf8_len, pos);
}

void SkSVGTextContext::shapePendingBuffer(const SkSVGRenderContext& ctx, const SkFont& font) {
    const char* utf8 = fShapeBuffer.fUtf8.data();
    size_t utf8Bytes = fShapeBuffer.fUtf8.size();

    // SkShaper clusters can start at individual combining characters. textLength spacing is
    // defined between typographic characters, so map every UTF-8 byte back to its containing
    // Unicode grapheme before the shaper callbacks consume this buffer.
    fShapeGraphemeMap.resize(utf8Bytes);
    for (size_t i = 0; i < utf8Bytes; ++i) {
        fShapeGraphemeMap[i] = SkTo<uint32_t>(i);
    }
    if (SkUnicode* unicode = ctx.unicode()) {
        skia_private::TArray<SkUnicode::CodeUnitFlags, true> flags;
        if (unicode->computeCodeUnitFlags(fShapeBuffer.fUtf8.data(),
                                          SkToInt(utf8Bytes),
                                          /*replaceTabs=*/false,
                                          &flags)) {
            uint32_t graphemeStart = 0;
            for (size_t i = 0; i < utf8Bytes; ++i) {
                if ((flags[SkToInt(i)] & SkUnicode::kGraphemeStart) ==
                    SkUnicode::kGraphemeStart) {
                    graphemeStart = SkTo<uint32_t>(i);
                }
                fShapeGraphemeMap[i] = graphemeStart;
            }
        }
    }

    std::unique_ptr<SkShaper::FontRunIterator> font_runs =
            SkShaper::MakeFontMgrRunIterator(utf8, utf8Bytes, font, ctx.fontMgr());
    if (!font_runs) {
        return;
    }
    if (!fForcePrimitiveShaping) {
        // Try to use the passed in shaping callbacks to shape, for example, using harfbuzz and ICU.
        const uint8_t defaultLTR = 0;
        std::unique_ptr<SkShaper::BiDiRunIterator> bidi =
                ctx.makeBidiRunIterator(utf8, utf8Bytes, defaultLTR);
        std::unique_ptr<SkShaper::LanguageRunIterator> language =
                SkShaper::MakeStdLanguageRunIterator(utf8, utf8Bytes);
        std::unique_ptr<SkShaper::ScriptRunIterator> script = ctx.makeScriptRunIterator(utf8, utf8Bytes);

        if (bidi && script && language) {
            fShaper->shape(utf8,
                           utf8Bytes,
                           *font_runs,
                           *bidi,
                           *script,
                           *language,
                           nullptr,
                           0,
                           SK_ScalarMax,
                           this);
            ++fShapeBatch;
            fShapeBuffer.reset();
            return;
        }  // If any of the callbacks fail, we'll fallback to the primitive shaping.
    }

    // bidi, script, and lang are all unused so we can construct them with empty data.
    SkShaper::TrivialBiDiRunIterator trivial_bidi{0, 0};
    SkShaper::TrivialScriptRunIterator trivial_script{0, 0};
    SkShaper::TrivialLanguageRunIterator trivial_lang{nullptr, 0};
    fShaper->shape(utf8,
                   utf8Bytes,
                   *font_runs,
                   trivial_bidi,
                   trivial_script,
                   trivial_lang,
                   nullptr,
                   0,
                   SK_ScalarMax,
                   this);
    ++fShapeBatch;
    fShapeBuffer.reset();
}

SkSVGTextContext::SkSVGTextContext(const SkSVGRenderContext& ctx,
                                   const ShapedTextCallback& cb,
                                   const SkSVGTextPath* tpath,
                                   float inheritedBaselineOffset)
        : fRenderContext(ctx)
        , fCallback(cb)
        , fShaper(ctx.makeShaper())
        , fChunkAlignmentFactor(ComputeAlignmentFactor(ctx.presentationContext()))
        , fBaselineOffset(inheritedBaselineOffset) {
    // If the shaper callback returns null, fallback to the primitive shaper and
    // signal that we should not use the other callbacks in shapePendingBuffer
    if (!fShaper) {
        fShaper = SkShapers::Primitive::PrimitiveText();
        fForcePrimitiveShaping = true;
    }
    if (tpath) {
        fPathData = std::make_unique<PathData>(ctx, *tpath);

        // https://www.w3.org/TR/SVG11/text.html#TextPathElementStartOffsetAttribute
        auto resolve_offset = [this](const SkSVGLength& offset) {
            if (offset.unit() != SkSVGLength::Unit::kPercentage) {
                // "If a <length> other than a percentage is given, then the ‘startOffset’
                // represents a distance along the path measured in the current user coordinate
                // system."
                return fRenderContext.lengthContext()
                                     .resolve(offset, SkSVGLengthContext::LengthType::kHorizontal);
            }

            // "If a percentage is given, then the ‘startOffset’ represents a percentage distance
            // along the entire path."
            return offset.value() * fPathData->length() / 100;
        };

        // startOffset acts as an initial absolute position
        fChunkPos.fX = resolve_offset(tpath->getStartOffset());
    }
}

SkSVGTextContext::~SkSVGTextContext() {
    this->flushChunk(fRenderContext);
}

void SkSVGTextContext::shapeFragment(const SkString& txt, const SkSVGRenderContext& ctx,
                                     SkSVGXmlSpace xs) {
    // https://www.w3.org/TR/SVG11/text.html#WhiteSpace
    // https://www.w3.org/TR/2008/REC-xml-20081126/#NT-S
    auto filterWSDefault = [this](SkUnichar ch) -> SkUnichar {
        // Remove all newline chars.
        if (ch == '\n') {
            return -1;
        }

        // Convert tab chars to space.
        if (ch == '\t') {
            ch = ' ';
        }

        // Consolidate contiguous space chars and strip leading spaces (fPrevCharSpace
        // starts off as true).
        if (fPrevCharSpace && ch == ' ') {
            return -1;
        }

        // TODO: Strip trailing WS?  Doing this across chunks would require another buffering
        //   layer.  In general, trailing WS should have no rendering side effects. Skipping
        //   for now.
        return ch;
    };
    auto filterWSPreserve = [](SkUnichar ch) -> SkUnichar {
        // Convert newline and tab chars to space.
        if (ch == '\n' || ch == '\t') {
            ch = ' ';
        }
        return ch;
    };

    // Stash paints for access from SkShaper callbacks.
    fCurrentFill   = ctx.fillPaint();
    fCurrentStroke = ctx.strokePaint();
    fCurrentTextDecoration = *ctx.presentationContext().fInherited.fTextDecoration;

    const auto font = ResolveFont(ctx);
    fShapeBuffer.reserve(txt.size());

    const char* ch_ptr = txt.c_str();
    const char* ch_end = ch_ptr + txt.size();

    while (ch_ptr < ch_end) {
        auto ch = SkUTF::NextUTF8(&ch_ptr, ch_end);
        ch = (xs == SkSVGXmlSpace::kDefault)
                ? filterWSDefault(ch)
                : filterWSPreserve(ch);

        if (ch < 0) {
            // invalid utf or char filtered out
            continue;
        }

        SkASSERT(fPosResolver);
        const auto pos = fPosResolver->resolve(fCurrentCharIndex++);

        // Absolute position adjustments define a new chunk.
        // (https://www.w3.org/TR/SVG11/text.html#TextLayoutIntroduction)
        if (pos.has(PosAttrs::kX) || pos.has(PosAttrs::kY)) {
            this->shapePendingBuffer(ctx, font);
            this->flushChunk(ctx);

            // New chunk position.
            if (pos.has(PosAttrs::kX)) {
                fChunkPos.fX = pos[PosAttrs::kX];
            }
            if (pos.has(PosAttrs::kY)) {
                fChunkPos.fY = pos[PosAttrs::kY];
            }
        }

        fShapeBuffer.append(ch, {
            {
                pos.has(PosAttrs::kDx) ? pos[PosAttrs::kDx] : 0,
                pos.has(PosAttrs::kDy) ? pos[PosAttrs::kDy] : 0,
            },
            pos.has(PosAttrs::kRotate) ? SkDegreesToRadians(pos[PosAttrs::kRotate]) : 0,
            fBaselineOffset,
        });

        fPrevCharSpace = (ch == ' ');
    }

    this->shapePendingBuffer(ctx, font);

    // Note: at this point we have shaped and buffered RunRecs for the current fragment.
    // The active text chunk continues until an explicit or implicit flush.
}

SkSVGTextContext::PathData::PathData(const SkSVGRenderContext& ctx, const SkSVGTextPath& tpath)
{
    const auto ref = ctx.findNodeById(tpath.getHref());
    if (!ref) {
        return;
    }

    SkContourMeasureIter cmi(ref->asPath(ctx), false);
    while (sk_sp<SkContourMeasure> contour = cmi.next()) {
        fLength += contour->length();
        fContours.push_back(std::move(contour));
    }
}

SkMatrix SkSVGTextContext::PathData::getMatrixAt(float offset) const {
    if (offset >= 0) {
        for (const auto& contour : fContours) {
            const auto contour_len = contour->length();
            if (offset < contour_len) {
                SkMatrix m;
                return contour->getMatrix(offset, &m) ? m : SkMatrix::I();
            }
            offset -= contour_len;
        }
    }

    // Quick & dirty way to "skip" rendering of glyphs off path.
    return SkMatrix::Translate(std::numeric_limits<float>::infinity(),
                               std::numeric_limits<float>::infinity());
}

SkRSXform SkSVGTextContext::computeGlyphXform(SkGlyphID glyph, const SkFont& font,
                                              const SkPoint& glyph_pos,
                                              const PositionAdjustment& pos_adjust) const {
    // text-anchor aligns a horizontal text chunk along its inline (x) axis. Relative vertical
    // positioning contributes to fChunkAdvance for subsequent chunks, but must not be folded into
    // the anchor correction. Doing so moves centered/end-aligned text vertically by a fraction of
    // its dy value (notably Mermaid's y="-0.1em" dy="1.1em" labels).
    const SkVector anchor_adjust = {fChunkAdvance.fX * fChunkAlignmentFactor, 0};
    SkPoint pos = fChunkPos + glyph_pos + pos_adjust.offset + anchor_adjust +
                  SkVector{0, pos_adjust.baselineOffset};
    if (!fPathData) {
        return SkRSXform::MakeFromRadians(/*scale=*/ 1, pos_adjust.rotation, pos.fX, pos.fY, 0, 0);
    }

    // We're in a textPath scope, reposition the glyph on path.
    // (https://www.w3.org/TR/SVG11/text.html#TextpathLayoutRules)

    // Path positioning is based on the glyph center (horizontal component).
    float glyph_width = font.getWidth(glyph);
    auto path_offset = pos.fX + glyph_width * .5f;

    // In addition to the path matrix, the final glyph matrix also includes:
    //
    //   -- vertical position adjustment "dy" ("dx" is factored into path_offset)
    //   -- glyph origin adjustment (undoing the glyph center offset above)
    //   -- explicit rotation adjustment (composing with the path glyph rotation)
    const auto m = fPathData->getMatrixAt(path_offset) *
            SkMatrix::Translate(-glyph_width * .5f,
                                pos_adjust.offset.fY + pos_adjust.baselineOffset) *
            SkMatrix::RotateRad(pos_adjust.rotation);

    return SkRSXform::Make(m.getScaleX(), m.getSkewY(), m.getTranslateX(), m.getTranslateY());
}

void SkSVGTextContext::flushChunk(const SkSVGRenderContext& ctx) {
    size_t clusterCount = 0;
    size_t previousBatch = 0;
    uint32_t previousCluster = 0;
    bool hasPreviousCluster = false;
    for (const auto& run : fRuns) {
        for (size_t i = 0; i < run.glyphCount; ++i) {
            if (!hasPreviousCluster || run.shapeBatch != previousBatch ||
                run.clusters[i] != previousCluster) {
                ++clusterCount;
                previousBatch = run.shapeBatch;
                previousCluster = run.clusters[i];
                hasPreviousCluster = true;
            }
        }
    }
    SkScalar spacingAdjustment = 0;
    if (fTextLength >= 0 && clusterCount > 1) {
        spacingAdjustment = (fTextLength - fChunkAdvance.fX) / (clusterCount - 1);
        fChunkAdvance.fX = fTextLength;
    }

    SkTextBlobBuilder blobBuilder;

    size_t clusterIndex = 0;
    hasPreviousCluster = false;
    for (const auto& run : fRuns) {
        const auto& buf = blobBuilder.allocRunRSXform(run.font, SkToInt(run.glyphCount));
        std::copy(run.glyphs.get(), run.glyphs.get() + run.glyphCount, buf.glyphs);
        for (size_t i = 0; i < run.glyphCount; ++i) {
            if (hasPreviousCluster &&
                (run.shapeBatch != previousBatch || run.clusters[i] != previousCluster)) {
                ++clusterIndex;
            }
            previousBatch = run.shapeBatch;
            previousCluster = run.clusters[i];
            hasPreviousCluster = true;
            SkPoint adjustedPosition = run.glyphPos[i];
            adjustedPosition.fX += clusterIndex * spacingAdjustment;
            buf.xforms()[i] = this->computeGlyphXform(run.glyphs[i],
                                                      run.font,
                                                      adjustedPosition,
                                                      run.glyhPosAdjust[i]);
        }

        fCallback(ctx,
                  blobBuilder.make(),
                  run.fillPaint.get(),
                  run.strokePaint.get(),
                  run.textDecoration);
    }

    fChunkPos += fChunkAdvance;
    fChunkAdvance = {0,0};
    fChunkAlignmentFactor = ComputeAlignmentFactor(ctx.presentationContext());

    fRuns.clear();
}

SkShaper::RunHandler::Buffer SkSVGTextContext::runBuffer(const RunInfo& ri) {
    SkASSERT(ri.glyphCount);

    fRuns.push_back({
        ri.fFont,
        fCurrentFill.isValid()   ? std::make_unique<SkPaint>(*fCurrentFill)   : nullptr,
        fCurrentStroke.isValid() ? std::make_unique<SkPaint>(*fCurrentStroke) : nullptr,
        fCurrentTextDecoration,
        std::make_unique<SkGlyphID[]         >(ri.glyphCount),
        std::make_unique<SkPoint[]           >(ri.glyphCount),
        std::make_unique<uint32_t[]          >(ri.glyphCount),
        std::make_unique<PositionAdjustment[]>(ri.glyphCount),
        fShapeBatch,
        ri.glyphCount,
        ri.fAdvance,
    });

    return {
        fRuns.back().glyphs.get(),
        fRuns.back().glyphPos.get(),
        nullptr,
        fRuns.back().clusters.get(),
        fChunkAdvance,
    };
}

void SkSVGTextContext::commitRunBuffer(const RunInfo& ri) {
    const auto& current_run = fRuns.back();

    // stash position adjustments
    for (size_t i = 0; i < ri.glyphCount; ++i) {
        const auto utf8_index = current_run.clusters[i];
        current_run.glyhPosAdjust[i] = fShapeBuffer.fUtf8PosAdjust[SkToInt(utf8_index)];
        if (utf8_index < fShapeGraphemeMap.size()) {
            current_run.clusters[i] = fShapeGraphemeMap[utf8_index];
        }
    }

    fChunkAdvance += ri.fAdvance;
}

void SkSVGTextContext::commitLine() {
    if (!fShapeBuffer.fUtf8PosAdjust.empty()) {
        // Offset adjustments are cumulative - only advance the current chunk with the last value.
        fChunkAdvance += fShapeBuffer.fUtf8PosAdjust.back().offset;
    }
}

void SkSVGTextFragment::renderText(const SkSVGRenderContext& ctx, SkSVGTextContext* tctx,
                                   SkSVGXmlSpace xs) const {
    // N.B.: unlike regular elements, text fragments do not establish a new OBB scope -- they
    // always defer to the root <text> element for OBB resolution.
    SkSVGRenderContext localContext(ctx);

    if (this->onPrepareToRender(&localContext)) {
        this->onShapeText(localContext, tctx, xs);
    }
}

SkPath SkSVGTextFragment::onAsPath(const SkSVGRenderContext&) const {
    // TODO
    return SkPath();
}

void SkSVGTextContainer::appendChild(sk_sp<SkSVGNode> child) {
    // Only allow text content child nodes.
    switch (child->tag()) {
    case SkSVGTag::kTextLiteral:
    case SkSVGTag::kTextPath:
    case SkSVGTag::kTSpan:
        fChildren.push_back(
            sk_sp<SkSVGTextFragment>(static_cast<SkSVGTextFragment*>(child.release())));
        break;
    default:
        break;
    }
}

void SkSVGTextContainer::onShapeText(const SkSVGRenderContext& ctx, SkSVGTextContext* tctx,
                                     SkSVGXmlSpace) const {
    SkASSERT(tctx);

    const SkSVGTextContext::ScopedPosResolver resolver(*this, ctx, tctx);
    const SkSVGTextContext::ScopedTextLayout layout(*this, ctx, tctx);

    for (const auto& frag : fChildren) {
        // Containers always override xml:space with the local value.
        frag->renderText(ctx, tctx, this->getXmlSpace());
    }
}

// https://www.w3.org/TR/SVG11/text.html#WhiteSpace
template <>
bool SkSVGAttributeParser::parse(SkSVGXmlSpace* xs) {
    static constexpr std::tuple<const char*, SkSVGXmlSpace> gXmlSpaceMap[] = {
            {"default" , SkSVGXmlSpace::kDefault },
            {"preserve", SkSVGXmlSpace::kPreserve},
    };

    return this->parseEnumMap(gXmlSpaceMap, xs) && this->parseEOSToken();
}

template <>
bool SkSVGAttributeParser::parse(SkSVGLengthAdjust* adjust) {
    if (this->parseExpectedStringToken("spacing")) {
        *adjust = SkSVGLengthAdjust::kSpacing;
        return this->parseEOSToken();
    }
    return false;
}

template <>
bool SkSVGAttributeParser::parse(SkSVGDominantBaseline* baseline) {
    static constexpr std::tuple<const char*, SkSVGDominantBaseline> kValues[] = {
            {"auto", SkSVGDominantBaseline::kAuto},
            {"alphabetic", SkSVGDominantBaseline::kAlphabetic},
            {"middle", SkSVGDominantBaseline::kMiddle},
            {"central", SkSVGDominantBaseline::kCentral},
            {"hanging", SkSVGDominantBaseline::kHanging},
            {"mathematical", SkSVGDominantBaseline::kMathematical},
    };
    return this->parseEnumMap(kValues, baseline) && this->parseEOSToken();
}

template <>
bool SkSVGAttributeParser::parse(SkSVGAlignmentBaseline* baseline) {
    static constexpr std::tuple<const char*, SkSVGAlignmentBaseline> kValues[] = {
            {"auto", SkSVGAlignmentBaseline::kAuto},
            {"baseline", SkSVGAlignmentBaseline::kBaseline},
            {"alphabetic", SkSVGAlignmentBaseline::kAlphabetic},
            {"middle", SkSVGAlignmentBaseline::kMiddle},
            {"central", SkSVGAlignmentBaseline::kCentral},
            {"hanging", SkSVGAlignmentBaseline::kHanging},
            {"mathematical", SkSVGAlignmentBaseline::kMathematical},
    };
    return this->parseEnumMap(kValues, baseline) && this->parseEOSToken();
}

bool SkSVGTextContainer::parseAndSetAttribute(const char* name, const char* value) {
    return INHERITED::parseAndSetAttribute(name, value) ||
           this->setX(SkSVGAttributeParser::parse<std::vector<SkSVGLength>>("x", name, value)) ||
           this->setY(SkSVGAttributeParser::parse<std::vector<SkSVGLength>>("y", name, value)) ||
           this->setDx(SkSVGAttributeParser::parse<std::vector<SkSVGLength>>("dx", name, value)) ||
           this->setDy(SkSVGAttributeParser::parse<std::vector<SkSVGLength>>("dy", name, value)) ||
           this->setRotate(SkSVGAttributeParser::parse<std::vector<SkSVGNumberType>>("rotate",
                                                                                     name,
                                                                                     value)) ||
           this->setTextLength(SkSVGAttributeParser::parse<SkSVGLength>("textLength",
                                                                        name,
                                                                        value)) ||
           this->setLengthAdjust(SkSVGAttributeParser::parse<SkSVGLengthAdjust>("lengthAdjust",
                                                                                name,
                                                                                value)) ||
           this->setDominantBaseline(
                   SkSVGAttributeParser::parse<SkSVGDominantBaseline>("dominant-baseline",
                                                                      name,
                                                                      value)) ||
           this->setAlignmentBaseline(
                   SkSVGAttributeParser::parse<SkSVGAlignmentBaseline>("alignment-baseline",
                                                                       name,
                                                                       value)) ||
           this->setXmlSpace(SkSVGAttributeParser::parse<SkSVGXmlSpace>("xml:space", name, value));
}

void SkSVGTextLiteral::onShapeText(const SkSVGRenderContext& ctx, SkSVGTextContext* tctx,
                                   SkSVGXmlSpace xs) const {
    SkASSERT(tctx);

    tctx->shapeFragment(this->getText(), ctx, xs);
}

void SkSVGText::onRender(const SkSVGRenderContext& ctx) const {
    const SkSVGTextContext::ShapedTextCallback render_text = [](const SkSVGRenderContext& ctx,
                                                                const sk_sp<SkTextBlob>& blob,
                                                                const SkPaint* fill,
                                                                const SkPaint* stroke,
                                                                const SkSVGTextDecoration&
                                                                        decoration) {
        if (fill) {
            ctx.canvas()->drawTextBlob(blob, 0, 0, *fill);
        }
        if (stroke) {
            ctx.canvas()->drawTextBlob(blob, 0, 0, *stroke);
        }
        const SkPaint* decorationSource = fill ? fill : stroke;
        if (decorationSource) {
            SkPaint decorationPaint(*decorationSource);
            decorationPaint.setStyle(SkPaint::kFill_Style);
            decorationPaint.setPathEffect(nullptr);
            ctx.canvas()->drawPath(BuildTextDecorationPath(blob, decoration), decorationPaint);
        }
    };

    // Root <text> nodes establish a text layout context.
    SkSVGTextContext tctx(ctx, render_text);

    this->onShapeText(ctx, &tctx, this->getXmlSpace());
}

SkRect SkSVGText::onTransformableObjectBoundingBox(const SkSVGRenderContext& ctx) const {
    SkRect bounds = SkRect::MakeEmpty();

    const SkSVGTextContext::ShapedTextCallback compute_bounds =
        [&bounds](const SkSVGRenderContext& ctx, const sk_sp<SkTextBlob>& blob, const SkPaint*,
                  const SkPaint*, const SkSVGTextDecoration& decoration) {
            if (!blob) {
                return;
            }

            bounds.join(BuildTextDecorationPath(blob, decoration).getBounds());

            AutoSTArray<64, SkRect> glyphBounds;

            for (SkTextBlobRunIterator it(blob.get()); !it.done(); it.next()) {
                const auto nglyphs = it.glyphCount();
                glyphBounds.reset(SkToInt(nglyphs));
                it.font().getBounds({it.glyphs(), nglyphs}, {glyphBounds.get(), nglyphs}, nullptr);

                SkASSERT(it.positioning() == SkTextBlobRunIterator::kRSXform_Positioning);
                SkMatrix m;
                for (uint32_t i = 0; i < it.glyphCount(); ++i) {
                    m.setRSXform(it.xforms()[i]);
                    bounds.join(m.mapRect(glyphBounds[i]));
                }
            }
        };

    {
        SkSVGTextContext tctx(ctx, compute_bounds);
        this->onShapeText(ctx, &tctx, this->getXmlSpace());
    }

    return bounds;
}

SkPath SkSVGText::onAsPath(const SkSVGRenderContext& ctx) const {
    SkPathBuilder builder;

    const SkSVGTextContext::ShapedTextCallback as_path =
        [&builder](const SkSVGRenderContext& ctx, const sk_sp<SkTextBlob>& blob, const SkPaint*,
                   const SkPaint*, const SkSVGTextDecoration& decoration) {
            if (!blob) {
                return;
            }

            builder.addPath(BuildTextDecorationPath(blob, decoration));

            for (SkTextBlobRunIterator it(blob.get()); !it.done(); it.next()) {
                struct GetPathsCtx {
                    SkPathBuilder&   builder;
                    const SkRSXform* xform;
                } get_paths_ctx {builder, it.xforms()};

                it.font().getPaths({it.glyphs(), it.glyphCount()}, [](const SkPath* path,
                                                                      const SkMatrix& matrix,
                                                                      void* raw_ctx) {
                    auto* get_paths_ctx = static_cast<GetPathsCtx*>(raw_ctx);
                    const auto& glyph_rsx = *get_paths_ctx->xform++;

                    if (!path) {
                        return;
                    }

                    SkMatrix glyph_matrix;
                    glyph_matrix.setRSXform(glyph_rsx);
                    glyph_matrix.preConcat(matrix);

                    get_paths_ctx->builder.addPath(path->makeTransform(glyph_matrix));
                }, &get_paths_ctx);
            }
        };

    {
        SkSVGTextContext tctx(ctx, as_path);
        this->onShapeText(ctx, &tctx, this->getXmlSpace());
    }

    auto path = builder.detach();
    this->mapToParent(&path);

    return path;
}

void SkSVGTextPath::onShapeText(const SkSVGRenderContext& ctx, SkSVGTextContext* parent_tctx,
                                 SkSVGXmlSpace xs) const {
    SkASSERT(parent_tctx);

    // textPath nodes establish a new text layout context.
    SkSVGTextContext tctx(ctx,
                          parent_tctx->getCallback(),
                          this,
                          parent_tctx->baselineOffset());

    this->INHERITED::onShapeText(ctx, &tctx, xs);
}

bool SkSVGTextPath::parseAndSetAttribute(const char* name, const char* value) {
    return INHERITED::parseAndSetAttribute(name, value) ||
        this->setHref(SkSVGAttributeParser::parse<SkSVGIRI>("href", name, value)) ||
        this->setHref(SkSVGAttributeParser::parse<SkSVGIRI>("xlink:href", name, value)) ||
        this->setStartOffset(SkSVGAttributeParser::parse<SkSVGLength>("startOffset", name, value));
}
