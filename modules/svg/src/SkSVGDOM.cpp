/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "modules/svg/include/SkSVGDOM.h"

#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkString.h"
#include "include/private/base/SkAssert.h"
#include "include/private/base/SkTo.h"
#include "modules/skshaper/include/SkShaper_factory.h"
#include "modules/svg/include/SkSVGAttribute.h"
#include "modules/svg/include/SkSVGAttributeParser.h"
#include "modules/svg/include/SkSVGCircle.h"
#include "modules/svg/include/SkSVGClipPath.h"
#include "modules/svg/include/SkSVGDefs.h"
#include "modules/svg/include/SkSVGEllipse.h"
#include "modules/svg/include/SkSVGFeBlend.h"
#include "modules/svg/include/SkSVGFeColorMatrix.h"
#include "modules/svg/include/SkSVGFeComponentTransfer.h"
#include "modules/svg/include/SkSVGFeComposite.h"
#include "modules/svg/include/SkSVGFeDisplacementMap.h"
#include "modules/svg/include/SkSVGFeDropShadow.h"
#include "modules/svg/include/SkSVGFeFlood.h"
#include "modules/svg/include/SkSVGFeGaussianBlur.h"
#include "modules/svg/include/SkSVGFeImage.h"
#include "modules/svg/include/SkSVGFeLightSource.h"
#include "modules/svg/include/SkSVGFeLighting.h"
#include "modules/svg/include/SkSVGFeMerge.h"
#include "modules/svg/include/SkSVGFeMorphology.h"
#include "modules/svg/include/SkSVGFeOffset.h"
#include "modules/svg/include/SkSVGFeTurbulence.h"
#include "modules/svg/include/SkSVGFilter.h"
#include "modules/svg/include/SkSVGG.h"
#include "modules/svg/include/SkSVGImage.h"
#include "modules/svg/include/SkSVGLine.h"
#include "modules/svg/include/SkSVGLinearGradient.h"
#include "modules/svg/include/SkSVGMask.h"
#include "modules/svg/include/SkSVGMarker.h"
#include "modules/svg/include/SkSVGNode.h"
#include "modules/svg/include/SkSVGPath.h"
#include "modules/svg/include/SkSVGPattern.h"
#include "modules/svg/include/SkSVGPoly.h"
#include "modules/svg/include/SkSVGRadialGradient.h"
#include "modules/svg/include/SkSVGRect.h"
#include "modules/svg/include/SkSVGRenderContext.h"
#include "modules/svg/include/SkSVGSVG.h"
#include "modules/svg/include/SkSVGStop.h"
#include "modules/svg/include/SkSVGSymbol.h"
#include "modules/svg/include/SkSVGText.h"
#include "modules/svg/include/SkSVGTypes.h"
#include "modules/svg/include/SkSVGUse.h"
#include "modules/svg/include/SkSVGValue.h"
#include "src/base/SkTSearch.h"
#include "src/core/SkTraceEvent.h"
#include "src/xml/SkDOM.h"

#include <stdint.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

bool SetIRIAttribute(const sk_sp<SkSVGNode>& node, SkSVGAttribute attr,
                      const char* stringValue) {
    auto parseResult = SkSVGAttributeParser::parse<SkSVGIRI>(stringValue);
    if (!parseResult.isValid()) {
        return false;
    }

    node->setAttribute(attr, SkSVGStringValue(parseResult->iri()));
    return true;
}

bool SetStringAttribute(const sk_sp<SkSVGNode>& node, SkSVGAttribute attr,
                           const char* stringValue) {
    SkString str(stringValue, strlen(stringValue));
    SkSVGStringType strType = SkSVGStringType(str);
    node->setAttribute(attr, SkSVGStringValue(strType));
    return true;
}

bool SetTransformAttribute(const sk_sp<SkSVGNode>& node, SkSVGAttribute attr,
                           const char* stringValue) {
    auto parseResult = SkSVGAttributeParser::parse<SkSVGTransformType>(stringValue);
    if (!parseResult.isValid()) {
        return false;
    }

    node->setAttribute(attr, SkSVGTransformValue(*parseResult));
    return true;
}

bool SetLengthAttribute(const sk_sp<SkSVGNode>& node, SkSVGAttribute attr,
                        const char* stringValue) {
    auto parseResult = SkSVGAttributeParser::parse<SkSVGLength>(stringValue);
    if (!parseResult.isValid()) {
        return false;
    }

    node->setAttribute(attr, SkSVGLengthValue(*parseResult));
    return true;
}

bool SetViewBoxAttribute(const sk_sp<SkSVGNode>& node, SkSVGAttribute attr,
                         const char* stringValue) {
    SkSVGViewBoxType viewBox;
    SkSVGAttributeParser parser(stringValue);
    if (!parser.parseViewBox(&viewBox)) {
        return false;
    }

    node->setAttribute(attr, SkSVGViewBoxValue(viewBox));
    return true;
}

bool SetObjectBoundingBoxUnitsAttribute(const sk_sp<SkSVGNode>& node,
                                        SkSVGAttribute attr,
                                        const char* stringValue) {
    auto parseResult = SkSVGAttributeParser::parse<SkSVGObjectBoundingBoxUnits>(stringValue);
    if (!parseResult.isValid()) {
        return false;
    }

    node->setAttribute(attr, SkSVGObjectBoundingBoxUnitsValue(*parseResult));
    return true;
}

bool SetPreserveAspectRatioAttribute(const sk_sp<SkSVGNode>& node, SkSVGAttribute attr,
                                     const char* stringValue) {
    SkSVGPreserveAspectRatio par;
    SkSVGAttributeParser parser(stringValue);
    if (!parser.parsePreserveAspectRatio(&par)) {
        return false;
    }

    node->setAttribute(attr, SkSVGPreserveAspectRatioValue(par));
    return true;
}

SkString TrimmedString(const char* first, const char* last) {
    SkASSERT(first);
    SkASSERT(last);
    SkASSERT(first <= last);

    while (first <= last && *first <= ' ') { first++; }
    while (first <= last && *last  <= ' ') { last--; }

    SkASSERT(last - first + 1 >= 0);
    return SkString(first, SkTo<size_t>(last - first + 1));
}

// Breaks a "foo: bar; baz: ..." string into key:value pairs.
class StyleIterator {
public:
    StyleIterator(const char* str) : fPos(str) { }

    std::tuple<SkString, SkString> next() {
        SkString name, value;

        if (fPos) {
            const char* sep = this->nextSeparator();
            SkASSERT(*sep == ';' || *sep == '\0');

            const char* valueSep = strchr(fPos, ':');
            if (valueSep && valueSep < sep) {
                name  = TrimmedString(fPos, valueSep - 1);
                value = TrimmedString(valueSep + 1, sep - 1);
            }

            fPos = *sep ? sep + 1 : nullptr;
        }

        return std::make_tuple(name, value);
    }

private:
    const char* nextSeparator() const {
        const char* sep = fPos;
        while (*sep != ';' && *sep != '\0') {
            sep++;
        }
        return sep;
    }

    const char* fPos;
};

bool set_string_attribute(const sk_sp<SkSVGNode>& node, const char* name, const char* value);

bool SetStyleAttributes(const sk_sp<SkSVGNode>& node, SkSVGAttribute,
                        const char* stringValue) {

    SkString name, value;
    StyleIterator iter(stringValue);
    for (;;) {
        std::tie(name, value) = iter.next();
        if (name.isEmpty()) {
            break;
        }
        set_string_attribute(node, name.c_str(), value.c_str());
    }

    return true;
}

template<typename T>
struct SortedDictionaryEntry {
    const char* fKey;
    const T     fValue;
};

struct AttrParseInfo {
    SkSVGAttribute fAttr;
    bool (*fSetter)(const sk_sp<SkSVGNode>& node, SkSVGAttribute attr, const char* stringValue);
};

SortedDictionaryEntry<AttrParseInfo> gAttributeParseInfo[] = {
    { "cx"                 , { SkSVGAttribute::kCx               , SetLengthAttribute       }},
    { "cy"                 , { SkSVGAttribute::kCy               , SetLengthAttribute       }},
    { "filterUnits"        , { SkSVGAttribute::kFilterUnits      ,
                               SetObjectBoundingBoxUnitsAttribute }},
    // focal point x & y
    { "fx"                 , { SkSVGAttribute::kFx               , SetLengthAttribute       }},
    { "fy"                 , { SkSVGAttribute::kFy               , SetLengthAttribute       }},
    { "height"             , { SkSVGAttribute::kHeight           , SetLengthAttribute       }},
    { "href"               , { SkSVGAttribute::kHref             , SetIRIAttribute          }},
    { "preserveAspectRatio", { SkSVGAttribute::kPreserveAspectRatio,
                               SetPreserveAspectRatioAttribute }},
    { "r"                  , { SkSVGAttribute::kR                , SetLengthAttribute       }},
    { "rx"                 , { SkSVGAttribute::kRx               , SetLengthAttribute       }},
    { "ry"                 , { SkSVGAttribute::kRy               , SetLengthAttribute       }},
    { "style"              , { SkSVGAttribute::kUnknown          , SetStyleAttributes       }},
    { "text"               , { SkSVGAttribute::kText             , SetStringAttribute       }},
    { "transform"          , { SkSVGAttribute::kTransform        , SetTransformAttribute    }},
    { "viewBox"            , { SkSVGAttribute::kViewBox          , SetViewBoxAttribute      }},
    { "width"              , { SkSVGAttribute::kWidth            , SetLengthAttribute       }},
    { "x"                  , { SkSVGAttribute::kX                , SetLengthAttribute       }},
    { "x1"                 , { SkSVGAttribute::kX1               , SetLengthAttribute       }},
    { "x2"                 , { SkSVGAttribute::kX2               , SetLengthAttribute       }},
    { "xlink:href"         , { SkSVGAttribute::kHref             , SetIRIAttribute          }},
    { "y"                  , { SkSVGAttribute::kY                , SetLengthAttribute       }},
    { "y1"                 , { SkSVGAttribute::kY1               , SetLengthAttribute       }},
    { "y2"                 , { SkSVGAttribute::kY2               , SetLengthAttribute       }},
};

SortedDictionaryEntry<sk_sp<SkSVGNode>(*)()> gTagFactories[] = {
    { "a"                  , []() -> sk_sp<SkSVGNode> { return SkSVGG::Make();                   }},
    { "circle"             , []() -> sk_sp<SkSVGNode> { return SkSVGCircle::Make();              }},
    { "clipPath"           , []() -> sk_sp<SkSVGNode> { return SkSVGClipPath::Make();            }},
    { "defs"               , []() -> sk_sp<SkSVGNode> { return SkSVGDefs::Make();                }},
    { "ellipse"            , []() -> sk_sp<SkSVGNode> { return SkSVGEllipse::Make();             }},
    { "feBlend"            , []() -> sk_sp<SkSVGNode> { return SkSVGFeBlend::Make();             }},
    { "feColorMatrix"      , []() -> sk_sp<SkSVGNode> { return SkSVGFeColorMatrix::Make();       }},
    { "feComponentTransfer", []() -> sk_sp<SkSVGNode> { return SkSVGFeComponentTransfer::Make(); }},
    { "feComposite"        , []() -> sk_sp<SkSVGNode> { return SkSVGFeComposite::Make();         }},
    { "feDiffuseLighting"  , []() -> sk_sp<SkSVGNode> { return SkSVGFeDiffuseLighting::Make();   }},
    { "feDisplacementMap"  , []() -> sk_sp<SkSVGNode> { return SkSVGFeDisplacementMap::Make();   }},
    { "feDistantLight"     , []() -> sk_sp<SkSVGNode> { return SkSVGFeDistantLight::Make();      }},
    { "feDropShadow"       , []() -> sk_sp<SkSVGNode> { return SkSVGFeDropShadow::Make();        }},
    { "feFlood"            , []() -> sk_sp<SkSVGNode> { return SkSVGFeFlood::Make();             }},
    { "feFuncA"            , []() -> sk_sp<SkSVGNode> { return SkSVGFeFunc::MakeFuncA();         }},
    { "feFuncB"            , []() -> sk_sp<SkSVGNode> { return SkSVGFeFunc::MakeFuncB();         }},
    { "feFuncG"            , []() -> sk_sp<SkSVGNode> { return SkSVGFeFunc::MakeFuncG();         }},
    { "feFuncR"            , []() -> sk_sp<SkSVGNode> { return SkSVGFeFunc::MakeFuncR();         }},
    { "feGaussianBlur"     , []() -> sk_sp<SkSVGNode> { return SkSVGFeGaussianBlur::Make();      }},
    { "feImage"            , []() -> sk_sp<SkSVGNode> { return SkSVGFeImage::Make();             }},
    { "feMerge"            , []() -> sk_sp<SkSVGNode> { return SkSVGFeMerge::Make();             }},
    { "feMergeNode"        , []() -> sk_sp<SkSVGNode> { return SkSVGFeMergeNode::Make();         }},
    { "feMorphology"       , []() -> sk_sp<SkSVGNode> { return SkSVGFeMorphology::Make();        }},
    { "feOffset"           , []() -> sk_sp<SkSVGNode> { return SkSVGFeOffset::Make();            }},
    { "fePointLight"       , []() -> sk_sp<SkSVGNode> { return SkSVGFePointLight::Make();        }},
    { "feSpecularLighting" , []() -> sk_sp<SkSVGNode> { return SkSVGFeSpecularLighting::Make();  }},
    { "feSpotLight"        , []() -> sk_sp<SkSVGNode> { return SkSVGFeSpotLight::Make();         }},
    { "feTurbulence"       , []() -> sk_sp<SkSVGNode> { return SkSVGFeTurbulence::Make();        }},
    { "filter"             , []() -> sk_sp<SkSVGNode> { return SkSVGFilter::Make();              }},
    { "g"                  , []() -> sk_sp<SkSVGNode> { return SkSVGG::Make();                   }},
    { "image"              , []() -> sk_sp<SkSVGNode> { return SkSVGImage::Make();               }},
    { "line"               , []() -> sk_sp<SkSVGNode> { return SkSVGLine::Make();                }},
    { "linearGradient"     , []() -> sk_sp<SkSVGNode> { return SkSVGLinearGradient::Make();      }},
    { "marker"             , []() -> sk_sp<SkSVGNode> { return SkSVGMarker::Make();              }},
    { "mask"               , []() -> sk_sp<SkSVGNode> { return SkSVGMask::Make();                }},
    { "path"               , []() -> sk_sp<SkSVGNode> { return SkSVGPath::Make();                }},
    { "pattern"            , []() -> sk_sp<SkSVGNode> { return SkSVGPattern::Make();             }},
    { "polygon"            , []() -> sk_sp<SkSVGNode> { return SkSVGPoly::MakePolygon();         }},
    { "polyline"           , []() -> sk_sp<SkSVGNode> { return SkSVGPoly::MakePolyline();        }},
    { "radialGradient"     , []() -> sk_sp<SkSVGNode> { return SkSVGRadialGradient::Make();      }},
    { "rect"               , []() -> sk_sp<SkSVGNode> { return SkSVGRect::Make();                }},
    { "stop"               , []() -> sk_sp<SkSVGNode> { return SkSVGStop::Make();                }},
    { "symbol"             , []() -> sk_sp<SkSVGNode> { return SkSVGSymbol::Make();              }},
//    "svg" handled explicitly
    { "text"               , []() -> sk_sp<SkSVGNode> { return SkSVGText::Make();                }},
    { "textPath"           , []() -> sk_sp<SkSVGNode> { return SkSVGTextPath::Make();            }},
    { "tspan"              , []() -> sk_sp<SkSVGNode> { return SkSVGTSpan::Make();               }},
    { "use"                , []() -> sk_sp<SkSVGNode> { return SkSVGUse::Make();                 }},
};

struct ConstructionContext {
    enum class ColorScheme {
        kLight,
        kDark,
    };

    ConstructionContext(SkSVGIDMapper* mapper,
                        SkSVGDOM::Logger* logger,
                        std::vector<SkString>* localReferences)
        : fParent(nullptr)
        , fIDMapper(mapper)
        , fLogger(logger)
        , fLocalReferences(localReferences)
        , fColorScheme(ColorScheme::kLight) {}
    ConstructionContext(const ConstructionContext& other,
                        const sk_sp<SkSVGNode>& newParent,
                        ColorScheme colorScheme,
                        const std::unordered_map<std::string, std::string>& customProperties,
                        const SkSVGPresentationAttributes& inheritedPresentation)
        : fParent(newParent.get())
        , fIDMapper(other.fIDMapper)
        , fLogger(other.fLogger)
        , fLocalReferences(other.fLocalReferences)
        , fColorScheme(colorScheme)
        , fCustomProperties(customProperties)
        , fInheritedPresentation(inheritedPresentation) {}

    SkSVGNode*     fParent;
    SkSVGIDMapper* fIDMapper;
    SkSVGDOM::Logger* fLogger;
    std::vector<SkString>* fLocalReferences;
    ColorScheme fColorScheme;
    std::unordered_map<std::string, std::string> fCustomProperties;
    SkSVGPresentationAttributes fInheritedPresentation =
            SkSVGPresentationAttributes::MakeInitial();
};

SkSVGPresentationAttributes resolve_inherited_presentation(
        const SkSVGPresentationAttributes& parent,
        const SkSVGNode& node) {
    SkSVGPresentationAttributes result = parent;

#define APPLY_INHERITED_PRESENTATION(ATTR)              \
    do {                                                \
        if (node.get##ATTR().isValue()) {               \
            result.f##ATTR.set(*node.get##ATTR());      \
        }                                               \
    } while (false)

    APPLY_INHERITED_PRESENTATION(Fill);
    APPLY_INHERITED_PRESENTATION(FillOpacity);
    APPLY_INHERITED_PRESENTATION(FillRule);
    APPLY_INHERITED_PRESENTATION(FontFamily);
    APPLY_INHERITED_PRESENTATION(FontSize);
    APPLY_INHERITED_PRESENTATION(FontStyle);
    APPLY_INHERITED_PRESENTATION(FontWeight);
    APPLY_INHERITED_PRESENTATION(ClipRule);
    APPLY_INHERITED_PRESENTATION(Stroke);
    APPLY_INHERITED_PRESENTATION(StrokeDashOffset);
    APPLY_INHERITED_PRESENTATION(StrokeDashArray);
    APPLY_INHERITED_PRESENTATION(StrokeLineCap);
    APPLY_INHERITED_PRESENTATION(StrokeLineJoin);
    APPLY_INHERITED_PRESENTATION(StrokeMiterLimit);
    APPLY_INHERITED_PRESENTATION(StrokeOpacity);
    APPLY_INHERITED_PRESENTATION(StrokeWidth);
    APPLY_INHERITED_PRESENTATION(MarkerStart);
    APPLY_INHERITED_PRESENTATION(MarkerMid);
    APPLY_INHERITED_PRESENTATION(MarkerEnd);
    APPLY_INHERITED_PRESENTATION(TextAnchor);
    APPLY_INHERITED_PRESENTATION(TextDecoration);
    APPLY_INHERITED_PRESENTATION(Visibility);
    APPLY_INHERITED_PRESENTATION(Color);
    APPLY_INHERITED_PRESENTATION(ColorInterpolation);
    APPLY_INHERITED_PRESENTATION(ColorInterpolationFilters);

#undef APPLY_INHERITED_PRESENTATION

    return result;
}

bool parse_color_scheme(const char* value, ConstructionContext::ColorScheme* scheme) {
    // A computed style normally contains a single selected scheme. Also accept the CSS syntax
    // that lists supported schemes and use the first concrete preference deterministically.
    const char* current = value;
    while (*current) {
        while (*current <= ' ' && *current) {
            ++current;
        }
        const char* end = current;
        while (*end > ' ') {
            ++end;
        }
        const SkString token(current, end - current);
        if (!strcmp(token.c_str(), "dark")) {
            *scheme = ConstructionContext::ColorScheme::kDark;
            return true;
        }
        if (!strcmp(token.c_str(), "light")) {
            *scheme = ConstructionContext::ColorScheme::kLight;
            return true;
        }
        current = end;
    }
    if (!strcmp(value, "normal")) {
        *scheme = ConstructionContext::ColorScheme::kLight;
        return true;
    }
    return false;
}

bool resolve_light_dark(const char* value,
                        ConstructionContext::ColorScheme scheme,
                        SkString* resolved) {
    std::string result(value);
    constexpr char kFunction[] = "light-dark(";

    for (size_t functionStart = result.find(kFunction);
         functionStart != std::string::npos;
         functionStart = result.find(kFunction)) {
        const size_t argumentStart = functionStart + sizeof(kFunction) - 1;
        size_t comma = std::string::npos;
        size_t functionEnd = std::string::npos;
        int depth = 1;
        for (size_t i = argumentStart; i < result.size(); ++i) {
            if (result[i] == '(') {
                ++depth;
            } else if (result[i] == ')') {
                if (--depth == 0) {
                    functionEnd = i;
                    break;
                }
            } else if (result[i] == ',' && depth == 1 && comma == std::string::npos) {
                comma = i;
            }
        }
        if (comma == std::string::npos || functionEnd == std::string::npos) {
            return false;
        }

        size_t selectedStart = scheme == ConstructionContext::ColorScheme::kDark
                                       ? comma + 1
                                       : argumentStart;
        size_t selectedEnd = scheme == ConstructionContext::ColorScheme::kDark
                                     ? functionEnd
                                     : comma;
        while (selectedStart < selectedEnd && result[selectedStart] <= ' ') {
            ++selectedStart;
        }
        while (selectedEnd > selectedStart && result[selectedEnd - 1] <= ' ') {
            --selectedEnd;
        }
        if (selectedStart == selectedEnd) {
            return false;
        }

        result.replace(functionStart,
                       functionEnd - functionStart + 1,
                       result.substr(selectedStart, selectedEnd - selectedStart));
    }

    resolved->set(result.c_str(), result.size());
    return true;
}

bool resolve_css_variables(
        const char* value,
        const std::unordered_map<std::string, std::string>& customProperties,
        SkString* resolved) {
    std::string result(value);
    constexpr char kFunction[] = "var(";
    constexpr int kMaxSubstitutions = 32;

    for (int substitution = 0; substitution < kMaxSubstitutions; ++substitution) {
        const size_t functionStart = result.find(kFunction);
        if (functionStart == std::string::npos) {
            resolved->set(result.c_str(), result.size());
            return true;
        }

        const size_t argumentStart = functionStart + sizeof(kFunction) - 1;
        size_t comma = std::string::npos;
        size_t functionEnd = std::string::npos;
        int depth = 1;
        for (size_t i = argumentStart; i < result.size(); ++i) {
            if (result[i] == '(') {
                ++depth;
            } else if (result[i] == ')') {
                if (--depth == 0) {
                    functionEnd = i;
                    break;
                }
            } else if (result[i] == ',' && depth == 1 && comma == std::string::npos) {
                comma = i;
            }
        }
        if (functionEnd == std::string::npos) {
            return false;
        }

        size_t nameStart = argumentStart;
        size_t nameEnd = comma == std::string::npos ? functionEnd : comma;
        while (nameStart < nameEnd && result[nameStart] <= ' ') {
            ++nameStart;
        }
        while (nameEnd > nameStart && result[nameEnd - 1] <= ' ') {
            --nameEnd;
        }
        const std::string name = result.substr(nameStart, nameEnd - nameStart);
        if (name.size() < 3 || name.compare(0, 2, "--") != 0) {
            return false;
        }

        std::string replacement;
        if (const auto property = customProperties.find(name);
            property != customProperties.end()) {
            replacement = property->second;
        } else {
            if (comma == std::string::npos) {
                return false;
            }
            size_t fallbackStart = comma + 1;
            size_t fallbackEnd = functionEnd;
            while (fallbackStart < fallbackEnd && result[fallbackStart] <= ' ') {
                ++fallbackStart;
            }
            while (fallbackEnd > fallbackStart && result[fallbackEnd - 1] <= ' ') {
                --fallbackEnd;
            }
            if (fallbackStart == fallbackEnd) {
                return false;
            }
            replacement = result.substr(fallbackStart, fallbackEnd - fallbackStart);
        }

        result.replace(functionStart, functionEnd - functionStart + 1, replacement);
    }

    // More than 32 substitutions indicates a custom-property cycle or unreasonable nesting.
    return false;
}

bool resolve_css_value(
        const char* value,
        ConstructionContext::ColorScheme colorScheme,
        const std::unordered_map<std::string, std::string>& customProperties,
        SkString* resolved) {
    SkString variablesResolved;
    return resolve_css_variables(value, customProperties, &variablesResolved) &&
           resolve_light_dark(variablesResolved.c_str(), colorScheme, resolved);
}

bool is_ignorable_attribute(const char* name, const char* value) {
    // Some browser normalizers serialize an unspecified inherited weight as an empty attribute.
    // Treat it as absent, which preserves normal SVG inheritance behavior.
    if (!strcmp(name, "font-weight") && value[0] == '\0') {
        return true;
    }
    return !strcmp(name, "class") || !strcmp(name, "focusable") ||
           !strcmp(name, "pointer-events") || !strcmp(name, "role") ||
           !strcmp(name, "target") || !strcmp(name, "version") ||
           !strcmp(name, "xml:space") ||
           !strncmp(name, "xmlns", 5) || !strncmp(name, "aria-", 5) ||
           !strncmp(name, "data-", 5);
}

bool is_ignorable_element(const char* name) {
    return !strcmp(name, "desc") || !strcmp(name, "metadata") || !strcmp(name, "title");
}

bool contains_drawio_capability_warning_text(const SkDOM& dom, const SkDOM::Node* node) {
    constexpr char kWarning[] = "Text is not SVG - cannot display";
    for (auto* child = dom.getFirstChild(node); child; child = dom.getNextSibling(child)) {
        if (dom.getType(child) == SkDOM::kText_Type) {
            const char* start = dom.getName(child);
            const char* end = start + strlen(start);
            while (start < end && *start <= ' ') {
                ++start;
            }
            while (end > start && end[-1] <= ' ') {
                --end;
            }
            if (static_cast<size_t>(end - start) == sizeof(kWarning) - 1 &&
                !strncmp(start, kWarning, sizeof(kWarning) - 1)) {
                return true;
            }
        } else if (contains_drawio_capability_warning_text(dom, child)) {
            return true;
        }
    }
    return false;
}

bool is_drawio_capability_warning(const SkDOM& dom, const SkDOM::Node* node) {
    if (strcmp(dom.getName(node), "a")) {
        return false;
    }

    const char* href = dom.findAttr(node, "href");
    if (!href) {
        href = dom.findAttr(node, "xlink:href");
    }
    return (href && strstr(href, "/doc/faq/svg-export-text-problems")) ||
           contains_drawio_capability_warning_text(dom, node);
}

SkTLazy<SkSVGColorType> parse_background_color(const char* name, const char* value) {
    SkTLazy<SkSVGColorType> result;

    if (!strcmp(name, "background-color")) {
        const auto color = SkSVGAttributeParser::parse<SkSVGColorType>(value);
        if (color.isValid()) {
            result.set(*color);
        }
        return result;
    }

    // Only solid backgrounds can be represented by SkSVGSVG::fBackgroundColor. Reject any
    // top-level CSS function other than a supported color function; otherwise a color nested in
    // a gradient would be mistaken for the viewport background color below.
    for (const char* current = value; *current; ++current) {
        if (*current != '(') {
            continue;
        }
        const char* functionStart = current;
        while (functionStart > value &&
               ((functionStart[-1] >= 'a' && functionStart[-1] <= 'z') ||
                functionStart[-1] == '-')) {
            --functionStart;
        }
        const SkString function(functionStart, current - functionStart);
        if (strcmp(function.c_str(), "rgb") && strcmp(function.c_str(), "rgba") &&
            strcmp(function.c_str(), "hsl") && strcmp(function.c_str(), "hsla")) {
            return result;
        }

        int depth = 1;
        while (depth > 0) {
            ++current;
            if (!*current) {
                return result;
            }
            depth += *current == '(' ? 1 : *current == ')' ? -1 : 0;
        }
    }

    if (!strcmp(value, "none") || !strcmp(value, "transparent")) {
        result.set(SK_ColorTRANSPARENT);
        return result;
    }

    // Browser-computed background shorthands place the resolved color at the end.
    for (const char* function : {"rgba(", "rgb(", "hsla(", "hsl("}) {
        if (const char* start = strstr(value, function)) {
            if (const char* end = strchr(start, ')')) {
                const SkString colorText(start, end - start + 1);
                const auto color = SkSVGAttributeParser::parse<SkSVGColorType>(colorText.c_str());
                if (color.isValid()) {
                    result.set(*color);
                }
                return result;
            }
        }
    }
    if (strstr(value, "transparent")) {
        result.set(SK_ColorTRANSPARENT);
    }
    return result;
}

bool parse_css_number(const std::string& token, float* number) {
    if (token.empty()) {
        return false;
    }
    char* end = nullptr;
    *number = std::strtof(token.c_str(), &end);
    return end == token.c_str() + token.size() && std::isfinite(*number);
}

bool parse_css_time(const std::string& token, float* time) {
    size_t suffixLength = 0;
    if (token.size() > 2 && token.compare(token.size() - 2, 2, "ms") == 0) {
        suffixLength = 2;
    } else if (token.size() > 1 && token.back() == 's') {
        suffixLength = 1;
    }
    return suffixLength != 0 &&
           parse_css_number(token.substr(0, token.size() - suffixLength), time);
}

bool is_css_function(const std::string& token, const char* name) {
    const size_t nameLength = strlen(name);
    return token.size() > nameLength + 1 && token.compare(0, nameLength, name) == 0 &&
           token[nameLength] == '(' && token.back() == ')';
}

bool is_css_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

bool animation_shorthand_is_static(const char* value) {
    std::vector<std::vector<std::string>> animations(1);
    const char* tokenStart = nullptr;
    char quote = '\0';
    bool escaped = false;
    int depth = 0;

    for (const char* current = value;; ++current) {
        const char c = *current;
        if (quote) {
            if (escaped) {
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
            } else if (c == quote) {
                quote = '\0';
            } else if (c == '\0') {
                return false;
            }
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == '(') {
            ++depth;
        } else if (c == ')') {
            if (--depth < 0) {
                return false;
            }
        }

        const bool separator = c == '\0' ||
                               (!quote && depth == 0 && (c == ',' || is_css_whitespace(c)));
        if (separator) {
            if (tokenStart) {
                animations.back().emplace_back(tokenStart, current - tokenStart);
                tokenStart = nullptr;
            }
            if (c == ',') {
                if (animations.back().empty()) {
                    return false;
                }
                animations.emplace_back();
            } else if (c == '\0') {
                break;
            }
        } else if (!tokenStart) {
            tokenStart = current;
        }
    }

    if (quote || depth != 0 || animations.back().empty()) {
        return false;
    }

    for (const auto& animation : animations) {
        int timeCount = 0;
        bool hasEasing = false;
        bool hasIterationCount = false;
        bool hasDirection = false;
        bool hasFillMode = false;
        bool hasPlayState = false;
        bool hasName = false;
        std::string name = "none";

        for (const auto& token : animation) {
            float numericValue;
            if (timeCount < 2 && parse_css_time(token, &numericValue) &&
                (timeCount != 0 || numericValue >= 0)) {
                ++timeCount;
                continue;
            }
            if (!hasEasing &&
                (token == "linear" || token == "ease" || token == "ease-in" ||
                 token == "ease-out" || token == "ease-in-out" || token == "step-start" ||
                 token == "step-end" || is_css_function(token, "linear") ||
                 is_css_function(token, "cubic-bezier") || is_css_function(token, "steps"))) {
                hasEasing = true;
                continue;
            }
            if (!hasIterationCount &&
                (token == "infinite" ||
                 (parse_css_number(token, &numericValue) && numericValue >= 0))) {
                hasIterationCount = true;
                continue;
            }
            if (!hasDirection &&
                (token == "normal" || token == "reverse" || token == "alternate" ||
                 token == "alternate-reverse")) {
                hasDirection = true;
                continue;
            }
            if (!hasFillMode &&
                (token == "none" || token == "forwards" || token == "backwards" ||
                 token == "both")) {
                hasFillMode = true;
                continue;
            }
            if (!hasPlayState && (token == "running" || token == "paused")) {
                hasPlayState = true;
                continue;
            }
            if (hasName) {
                return false;
            }
            hasName = true;
            name = token;
        }

        if (hasName && name != "none") {
            return false;
        }
    }
    return true;
}

bool handle_compatible_computed_style(const sk_sp<SkSVGNode>& node,
                                      const char* name,
                                      const char* value) {
    if (!strcmp(name, "color-scheme")) {
        ConstructionContext::ColorScheme ignored;
        return parse_color_scheme(value, &ignored);
    }

    // Custom properties are collected before ordinary declarations are parsed so var() references
    // can use them regardless of declaration order. Reaching this branch is harmless for callers
    // that pass a custom property through the generic compatibility check directly.
    if (!strncmp(name, "--", 2)) {
        return true;
    }

    if (!strcmp(name, "background") || !strcmp(name, "background-color")) {
        const auto color = parse_background_color(name, value);
        if (!color.isValid()) {
            return false;
        }
        // CSS backgrounds have no paint box on SVG graphics elements. For an SVG viewport,
        // preserve the computed background as vector paint behind its children.
        if (node->tag() == SkSVGTag::kSvg) {
            static_cast<SkSVGSVG*>(node.get())->setBackgroundColor(*color);
        }
        return true;
    }

    if (!strcmp(name, "line-height")) {
        return !strcmp(value, "normal");
    }

    if (!strcmp(name, "vertical-align")) {
        return !strcmp(value, "baseline");
    }

    if (!strcmp(name, "white-space")) {
        const char* xmlSpace = !strcmp(value, "pre") ? "preserve"
                             : !strcmp(value, "normal") ? "default"
                                                         : nullptr;
        if (!xmlSpace) {
            return false;
        }
        // Computed styles are flattened onto each node. Text containers already implement the
        // equivalent SVG 1.1 xml:space behavior; non-text nodes have no text of their own.
        switch (node->tag()) {
            case SkSVGTag::kText:
            case SkSVGTag::kTextPath:
            case SkSVGTag::kTSpan:
                return set_string_attribute(node, "xml:space", xmlSpace);
            default:
                return true;
        }
    }

    // CSSOM normalization often copies the full browser computed style onto every SVG node.
    // These box-layout, interaction, and animation properties do not alter Skia's static SVG
    // geometry or paint. Keep diagnostics strict for unknown presentation properties.
    static constexpr const char* kIgnoredProperties[] = {
            "border", "border-radius", "cursor", "margin", "max-height", "max-width",
            "min-height", "min-width", "padding", "pointer-events", "position", "text-align",
            "z-index",
    };
    for (const char* property : kIgnoredProperties) {
        if (!strcmp(name, property)) {
            return true;
        }
    }

    // Static output is unaffected when the computed animation name is "none".
    if (!strcmp(name, "animation-name")) {
        return !strcmp(value, "none");
    }
    if (!strcmp(name, "animation")) {
        return animation_shorthand_is_static(value);
    }

    // CSS layout can compute auto dimensions without replacing explicit SVG geometry attributes.
    if ((!strcmp(name, "height") || !strcmp(name, "width")) && !strcmp(value, "auto")) {
        return true;
    }

    // Non-viewport graphics elements have no CSS overflow box. SVG viewport elements consume this
    // property in SkSVGSVG::parseAndSetAttribute before reaching this compatibility fallback.
    if (!strcmp(name, "overflow")) {
        return !strcmp(value, "visible") || !strcmp(value, "hidden") ||
               !strcmp(value, "clip") || !strcmp(value, "auto");
    }

    return false;
}

const char* url_value_start(const char* value, char* quote) {
    while (*value <= ' ' && *value != '\0') {
        ++value;
    }
    *quote = *value == '\'' || *value == '"' ? *value++ : '\0';
    while (*value <= ' ' && *value != '\0') {
        ++value;
    }
    return value;
}

void collect_local_references(const char* elementName,
                              const char* name,
                              const char* value,
                              std::vector<SkString>* references) {
    if (!references) {
        return;
    }
    if (strcmp(elementName, "a") &&
        (!strcmp(name, "href") || !strcmp(name, "xlink:href"))) {
        char quote;
        const char* referenceStart = url_value_start(value, &quote);
        if (*referenceStart == '#') {
            const char* referenceEnd = quote ? strchr(referenceStart, quote)
                                             : referenceStart + strlen(referenceStart);
            if (referenceEnd) {
                while (!quote && referenceEnd > referenceStart && referenceEnd[-1] <= ' ') {
                    --referenceEnd;
                }
                references->emplace_back(referenceStart + 1,
                                         referenceEnd - referenceStart - 1);
            }
        }
    }
    for (const char* current = value; (current = strstr(current, "url("));) {
        current += 4;
        char quote;
        current = url_value_start(current, &quote);
        const char* end = strchr(current, quote ? quote : ')');
        if (!end) {
            break;
        }
        if (*current == '#') {
            const char* referenceEnd = end;
            while (!quote && referenceEnd > current && referenceEnd[-1] <= ' ') {
                --referenceEnd;
            }
            references->emplace_back(current + 1, referenceEnd - current - 1);
        }
        current = end + 1;
    }
}

bool has_external_reference(const char* elementName, const char* name, const char* value) {
    if (!strcmp(name, "href") || !strcmp(name, "xlink:href")) {
        // Hyperlinks are not fetched while rendering: <a> is represented as a visual group.
        if (!strcmp(elementName, "a")) {
            return false;
        }
        char quote;
        const char* referenceStart = url_value_start(value, &quote);
        if (*referenceStart == '#') {
            return false;
        }
        // Data URI decoding is supported only by image elements. Other href consumers resolve
        // nodes from the local SVG DOM and would silently omit a data URI target.
        const bool isImageElement = !strcmp(elementName, "image") ||
                                    !strcmp(elementName, "feImage");
        return strncmp(referenceStart, "data:", 5) != 0 || !isImageElement;
    }

    for (const char* current = value; (current = strstr(current, "url("));) {
        current += 4;
        char quote;
        current = url_value_start(current, &quote);
        // url() values name local paint/filter/clip nodes. Data URIs are not decoded in this
        // syntax, including on image elements (which consume a direct href instead).
        if (*current != '#') {
            return true;
        }
    }
    return false;
}

bool can_reference_svg_resource(const char* name) {
    return !strcmp(name, "href") || !strcmp(name, "xlink:href") ||
           !strcmp(name, "clip-path") || !strcmp(name, "fill") ||
           !strcmp(name, "filter") || !strcmp(name, "marker-end") ||
           !strcmp(name, "marker-mid") || !strcmp(name, "marker-start") ||
           !strcmp(name, "mask") || !strcmp(name, "stroke");
}

void inspect_resource_reference(const char* elementName,
                                const char* name,
                                const char* value,
                                SkSVGDOM::Logger* logger,
                                std::vector<SkString>* localReferences) {
    if (!can_reference_svg_resource(name)) {
        return;
    }
    collect_local_references(elementName, name, value, localReferences);
    if (logger && has_external_reference(elementName, name, value)) {
        SkString message;
        message.printf("External SVG resource reference in '%s=\"%s\"' is not allowed.",
                       name, value);
        logger->log(SkSVGDOM::Logger::Level::kError, message.c_str());
    }
}

bool set_string_attribute(const sk_sp<SkSVGNode>& node, const char* name, const char* value) {
    if (node->parseAndSetAttribute(name, value)) {
        // Handled by new code path
        return true;
    }

    const int attrIndex = SkStrSearch(&gAttributeParseInfo[0].fKey,
                                      SkTo<int>(std::size(gAttributeParseInfo)),
                                      name, sizeof(gAttributeParseInfo[0]));
    if (attrIndex < 0) {
#if defined(SK_VERBOSE_SVG_PARSING)
        SkDebugf("unhandled attribute: %s\n", name);
#endif
        return false;
    }

    SkASSERT(SkTo<size_t>(attrIndex) < std::size(gAttributeParseInfo));
    const auto& attrInfo = gAttributeParseInfo[attrIndex].fValue;
    if (!attrInfo.fSetter(node, attrInfo.fAttr, value)) {
#if defined(SK_VERBOSE_SVG_PARSING)
        SkDebugf("could not parse attribute: '%s=\"%s\"'\n", name, value);
#endif
        return false;
    }

    return true;
}

ConstructionContext::ColorScheme parse_node_attributes(
        const SkDOM& xmlDom,
        const SkDOM::Node* xmlNode,
        const sk_sp<SkSVGNode>& svgNode,
        SkSVGIDMapper* mapper,
        SkSVGDOM::Logger* logger,
        std::vector<SkString>* localReferences,
        ConstructionContext::ColorScheme inheritedColorScheme,
        const std::unordered_map<std::string, std::string>& inheritedCustomProperties,
        std::unordered_map<std::string, std::string>* customProperties) {
    auto colorScheme = inheritedColorScheme;
    *customProperties = inheritedCustomProperties;

    // color-scheme is inherited and light-dark() can occur earlier in serialization order, so
    // determine the local scheme before parsing any declarations on this node.
    const char* name, *value;
    SkDOM::AttrIter schemeAttrIter(xmlDom, xmlNode);
    while ((name = schemeAttrIter.next(&value))) {
        if (!strcmp(name, "color-scheme")) {
            parse_color_scheme(value, &colorScheme);
        } else if (!strcmp(name, "style")) {
            SkString propertyName, propertyValue;
            StyleIterator iter(value);
            for (;;) {
                std::tie(propertyName, propertyValue) = iter.next();
                if (propertyName.isEmpty()) {
                    break;
                }
                if (!strcmp(propertyName.c_str(), "color-scheme")) {
                    parse_color_scheme(propertyValue.c_str(), &colorScheme);
                } else if (propertyName.startsWith("--")) {
                    (*customProperties)[propertyName.c_str()] = propertyValue.c_str();
                }
            }
        }
    }

    SkDOM::AttrIter attrIter(xmlDom, xmlNode);
    const char* elementName = xmlDom.getName(xmlNode);
    while ((name = attrIter.next(&value))) {
        // We're handling id attributes out of band for now.
        if (!strcmp(name, "id")) {
            mapper->set(SkString(value), svgNode);
            continue;
        }
        if (!strcmp(name, "style")) {
            SkString propertyName, propertyValue;
            StyleIterator iter(value);
            for (;;) {
                std::tie(propertyName, propertyValue) = iter.next();
                if (propertyName.isEmpty()) {
                    break;
                }
                if (propertyName.startsWith("--")) {
                    // Collected in the prepass above so declaration order does not affect var().
                    continue;
                }
                SkString resolvedValue;
                const bool resolved = resolve_css_value(propertyValue.c_str(),
                                                        colorScheme,
                                                        *customProperties,
                                                        &resolvedValue);
                if (resolved) {
                    inspect_resource_reference(elementName,
                                               propertyName.c_str(),
                                               resolvedValue.c_str(),
                                               logger,
                                               localReferences);
                }
                if ((!resolved ||
                     (!set_string_attribute(svgNode,
                                            propertyName.c_str(),
                                            resolvedValue.c_str()) &&
                    !handle_compatible_computed_style(svgNode,
                                                      propertyName.c_str(),
                                                      resolvedValue.c_str()))) && logger) {
                        SkString message;
                        message.printf("Unsupported or invalid inline SVG style '%s: %s'.",
                                       propertyName.c_str(), propertyValue.c_str());
                        logger->log(SkSVGDOM::Logger::Level::kWarning, message.c_str());
                }
            }
            continue;
        }
        SkString resolvedValue;
        const bool resolved = resolve_css_value(value,
                                                colorScheme,
                                                *customProperties,
                                                &resolvedValue);
        if (resolved) {
            inspect_resource_reference(elementName,
                                       name,
                                       resolvedValue.c_str(),
                                       logger,
                                       localReferences);
        }
        if ((!resolved || !set_string_attribute(svgNode, name, resolvedValue.c_str())) &&
            !is_ignorable_attribute(name, value)) {
            if (logger) {
                SkString message;
                message.printf("Unsupported or invalid SVG attribute '%s=\"%s\"'.",
                               name, value);
                logger->log(SkSVGDOM::Logger::Level::kWarning, message.c_str());
            }
        }
    }
    return colorScheme;
}

sk_sp<SkSVGNode> construct_svg_node(const SkDOM& dom, const ConstructionContext& ctx,
                                    const SkDOM::Node* xmlNode) {
    const char* elem = dom.getName(xmlNode);
    const SkDOM::Type elemType = dom.getType(xmlNode);

    if (elemType == SkDOM::kText_Type) {
        // Text literals require special handling.
        SkASSERT(dom.countChildren(xmlNode) == 0);
        auto txt = SkSVGTextLiteral::Make();
        txt->setText(SkString(dom.getName(xmlNode)));
        ctx.fParent->appendChild(std::move(txt));

        return nullptr;
    }

    SkASSERT(elemType == SkDOM::kElement_Type);

    // Draw.io appends a renderer-capability fallback after diagrams that use foreignObject text.
    // Browsers suppress this branch through <switch>, but callers that normalize the switch for
    // CanvasKit can leave the fallback anchor behind. It is export metadata, not diagram content.
    if (is_drawio_capability_warning(dom, xmlNode)) {
        return nullptr;
    }

    auto make_node = [](const ConstructionContext& ctx, const char* elem) -> sk_sp<SkSVGNode> {
        if (strcmp(elem, "svg") == 0) {
            // Outermost SVG element must be tagged as such.
            return SkSVGSVG::Make(ctx.fParent ? SkSVGSVG::Type::kInner
                                              : SkSVGSVG::Type::kRoot);
        }

        const int tagIndex = SkStrSearch(&gTagFactories[0].fKey,
                                         SkTo<int>(std::size(gTagFactories)),
                                         elem, sizeof(gTagFactories[0]));
        if (tagIndex < 0) {
#if defined(SK_VERBOSE_SVG_PARSING)
            SkDebugf("unhandled element: <%s>\n", elem);
#endif
            if (!is_ignorable_element(elem)) {
                if (ctx.fLogger) {
                    SkString message;
                    message.printf("Unsupported SVG element <%s>.", elem);
                    ctx.fLogger->log(SkSVGDOM::Logger::Level::kWarning, message.c_str());
                }
            }
            return nullptr;
        }
        SkASSERT(SkTo<size_t>(tagIndex) < std::size(gTagFactories));

        return gTagFactories[tagIndex].fValue();
    };

    auto node = make_node(ctx, elem);
    if (!node) {
        return nullptr;
    }

    std::unordered_map<std::string, std::string> customProperties;
    const auto colorScheme = parse_node_attributes(dom,
                                                   xmlNode,
                                                   node,
                                                   ctx.fIDMapper,
                                                   ctx.fLogger,
                                                   ctx.fLocalReferences,
                                                   ctx.fColorScheme,
                                                   ctx.fCustomProperties,
                                                   &customProperties);

    if (node->tag() == SkSVGTag::kMarker) {
        static_cast<SkSVGMarker*>(node.get())
                ->setInheritedPresentationAttributes(ctx.fInheritedPresentation);
    }
    const auto inheritedPresentation =
            resolve_inherited_presentation(ctx.fInheritedPresentation, *node);
    ConstructionContext localCtx(
            ctx, node, colorScheme, customProperties, inheritedPresentation);
    for (auto* child = dom.getFirstChild(xmlNode, nullptr); child;
         child = dom.getNextSibling(child)) {
        sk_sp<SkSVGNode> childNode = construct_svg_node(dom, localCtx, child);
        if (childNode) {
            node->appendChild(std::move(childNode));
        }
    }

    return node;
}

} // anonymous namespace

SkSVGDOM::Builder& SkSVGDOM::Builder::setFontManager(sk_sp<SkFontMgr> fmgr) {
    fFontMgr = std::move(fmgr);
    return *this;
}

SkSVGDOM::Builder& SkSVGDOM::Builder::setResourceProvider(sk_sp<skresources::ResourceProvider> rp) {
    fResourceProvider = std::move(rp);
    return *this;
}

SkSVGDOM::Builder& SkSVGDOM::Builder::setTextShapingFactory(sk_sp<SkShapers::Factory> f) {
    fTextShapingFactory = f;
    return *this;
}

SkSVGDOM::Builder& SkSVGDOM::Builder::setLogger(sk_sp<Logger> logger) {
    fLogger = std::move(logger);
    return *this;
}

sk_sp<SkSVGDOM> SkSVGDOM::Builder::make(SkStream& str) const {
    TRACE_EVENT0("skia", TRACE_FUNC);
    SkDOM xmlDom;
    if (!xmlDom.build(str)) {
        if (fLogger) {
            fLogger->log(Logger::Level::kError, "Failed to parse SVG XML.");
        }
        return nullptr;
    }

    SkSVGIDMapper mapper;
    std::vector<SkString> localReferences;
    ConstructionContext ctx(&mapper, fLogger.get(), &localReferences);

    auto root = construct_svg_node(xmlDom, ctx, xmlDom.getRootNode());
    if (!root || root->tag() != SkSVGTag::kSvg) {
        if (fLogger) {
            fLogger->log(Logger::Level::kError, "SVG document has no valid root <svg> element.");
        }
        return nullptr;
    }

    if (fLogger) {
        for (const auto& reference : localReferences) {
            if (!mapper.find(reference)) {
                SkString message;
                message.printf("Unresolved local SVG reference '#%s'.", reference.c_str());
                fLogger->log(Logger::Level::kWarning, message.c_str());
            }
        }
    }

    class NullResourceProvider final : public skresources::ResourceProvider {
        sk_sp<SkData> load(const char[], const char[]) const override { return nullptr; }
    };

    auto resource_provider = fResourceProvider ? fResourceProvider
                                               : sk_make_sp<NullResourceProvider>();

    auto factory = fTextShapingFactory ? fTextShapingFactory : SkShapers::Primitive::Factory();

    return sk_sp<SkSVGDOM>(new SkSVGDOM(sk_sp<SkSVGSVG>(static_cast<SkSVGSVG*>(root.release())),
                                        std::move(fFontMgr),
                                        std::move(resource_provider),
                                        std::move(mapper),
                                        std::move(factory)));
}

SkSVGDOM::SkSVGDOM(sk_sp<SkSVGSVG> root,
                   sk_sp<SkFontMgr> fmgr,
                   sk_sp<skresources::ResourceProvider> rp,
                   SkSVGIDMapper&& mapper,
                   sk_sp<SkShapers::Factory> fact)
        : fRoot(std::move(root))
        , fFontMgr(std::move(fmgr))
        , fTextShapingFactory(std::move(fact))
        , fResourceProvider(std::move(rp))
        , fIDMapper(std::move(mapper))
        , fContainerSize(fRoot->intrinsicSize(SkSVGLengthContext(SkSize::Make(0, 0)))) {
    SkASSERT(fResourceProvider);
    SkASSERT(fTextShapingFactory);
}

sk_sp<SkSVGDOM> SkSVGDOM::MakeFromStream(SkStream& str) { return Builder().make(str); }

void SkSVGDOM::render(SkCanvas* canvas) const {
    TRACE_EVENT0("skia", TRACE_FUNC);
    if (fRoot) {
        SkSVGLengthContext       lctx(fContainerSize);
        SkSVGPresentationContext pctx;
        fRoot->render(SkSVGRenderContext(canvas,
                                         fFontMgr,
                                         fResourceProvider,
                                         fIDMapper,
                                         lctx,
                                         pctx,
                                         {nullptr, nullptr},
                                         fTextShapingFactory));
    }
}

void SkSVGDOM::renderNode(SkCanvas* canvas, SkSVGPresentationContext& pctx, const char* id) const {
    TRACE_EVENT0("skia", TRACE_FUNC);

    if (fRoot) {
        SkSVGLengthContext lctx(fContainerSize);
        fRoot->renderNode(SkSVGRenderContext(canvas,
                                             fFontMgr,
                                             fResourceProvider,
                                             fIDMapper,
                                             lctx,
                                             pctx,
                                             {nullptr, nullptr},
                                             fTextShapingFactory),
                          SkSVGIRI(SkSVGIRI::Type::kLocal, SkSVGStringType(id)));
    }
}

const SkSize& SkSVGDOM::containerSize() const {
    return fContainerSize;
}

void SkSVGDOM::setContainerSize(const SkSize& containerSize) {
    // TODO: inval
    fContainerSize = containerSize;
}

sk_sp<SkSVGNode>* SkSVGDOM::findNodeById(const char* id) {
    SkString idStr(id);
    return this->fIDMapper.find(idStr);
}

// TODO(fuego): move this to SkSVGNode or its own CU.
bool SkSVGNode::setAttribute(const char* attributeName, const char* attributeValue) {
    return set_string_attribute(sk_ref_sp(this), attributeName, attributeValue);
}
