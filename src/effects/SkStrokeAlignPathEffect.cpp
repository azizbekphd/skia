#include "include/effects/SkStrokeAlignPathEffect.h"

#include "include/core/SkFlattenable.h"
#include "include/core/SkPath.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathEffect.h"
#include "include/core/SkPoint.h"
#include "include/core/SkScalar.h"
#include "src/core/SkPathEffectBase.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkWriteBuffer.h"

class SkMatrix;
class SkStrokeRec;
struct SkRect;

class SkStrokeAlignPathEffectImpl : public SkPathEffectBase {
public:
    explicit SkStrokeAlignPathEffectImpl(uint8_t align, SkScalar radius) : fAlign(align), fRadius(radius) {
        SkASSERT(align <= SkPaint::kLast_Align);
        SkASSERT(radius > 0);
    }

    bool onFilterPath(SkPath* dst, const SkPath& src, SkStrokeRec*, const SkRect*,
                      const SkMatrix&) const override {
        if (fAlign > SkPaint::kLast_Align || fRadius <= 0) {
            return false;
        }

        switch (fAlign) {
            case SkPaint::kMiddle_Align:
                *dst = src;
                break;
            case SkPaint::kInner_Align:
                src.shiftVertices(-fRadius, dst);
                break;
            case SkPaint::kOuter_Align:
                src.shiftVertices(fRadius, dst);
                break;
        }
        return true;
    }

    bool computeFastBounds(SkRect* bounds) const override {
        if (bounds) {
            if (fAlign == SkPaint::kInner_Align) {
                bounds->inset(fRadius, fRadius);
            } else if (fAlign == SkPaint::kOuter_Align) {
                bounds->outset(fRadius, fRadius);
            }
        }
        return true;
    }

    static sk_sp<SkFlattenable> CreateProc(SkReadBuffer& buffer) {
        return SkStrokeAlignPathEffect::Make(
                buffer.readUInt(),
                buffer.readScalar());
    }

    void flatten(SkWriteBuffer& buffer) const override {
        buffer.writeUInt(fAlign);
        buffer.writeScalar(fRadius);
    }

    Factory getFactory() const override { return CreateProc; }
    const char* getTypeName() const override { return "SkStrokeAlignPathEffect"; }

private:
    const uint8_t fAlign;
    const SkScalar fRadius;

    using INHERITED = SkPathEffectBase;
};

//////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkPathEffect> SkStrokeAlignPathEffect::Make(uint8_t align, SkScalar radius) {
    return (align <= SkPaint::kLast_Align) && SkIsFinite(radius) && (radius > 0) ?
            sk_sp<SkPathEffect>(new SkStrokeAlignPathEffectImpl(align, radius)) : nullptr;
}

void SkStrokeAlignPathEffect::RegisterFlattenables() {
    SkFlattenable::Register("SkStrokeAlignPathEffect", SkStrokeAlignPathEffectImpl::CreateProc);
}
