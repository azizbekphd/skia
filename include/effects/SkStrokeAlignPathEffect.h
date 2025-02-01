#ifndef SkStrokeAlignPathEffect_DEFINED
#define SkStrokeAlignPathEffect_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/core/SkScalar.h"
#include "include/core/SkTypes.h"

class SkPathEffect;

/** \class SkCornerPathEffect

    SkCornerPathEffect is a subclass of SkPathEffect that can turn sharp corners
    into various treatments (e.g. rounded corners)
*/
class SK_API SkStrokeAlignPathEffect {
public:
    /** radius must be > 0 to have an effect. It specifies the distance from each corner
        that should be "rounded".
    */
    static sk_sp<SkPathEffect> Make(uint8_t align, SkScalar radius);

    static void RegisterFlattenables();
};

#endif
