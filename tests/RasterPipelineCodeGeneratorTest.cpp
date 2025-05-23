/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkColor.h"
#include "include/core/SkTypes.h"
#include "src/base/SkArenaAlloc.h"
#include "src/core/SkRasterPipeline.h"
#include "src/sksl/SkSLCompiler.h"
#include "src/sksl/SkSLProgramKind.h"
#include "src/sksl/SkSLProgramSettings.h"
#include "src/sksl/SkSLUtil.h"
#include "src/sksl/codegen/SkSLRasterPipelineBuilder.h"
#include "src/sksl/codegen/SkSLRasterPipelineCodeGenerator.h"
#include "src/sksl/ir/SkSLFunctionDeclaration.h"
#include "src/sksl/ir/SkSLProgram.h"
#include "src/sksl/tracing/SkSLDebugTracePriv.h"
#include "tests/Test.h"

#include <memory>
#include <optional>
#include <sstream>
#include <string>

//#define DUMP_PROGRAMS 1
#if defined(DUMP_PROGRAMS)
#include "src/core/SkStreamPriv.h"
#endif

static void test(skiatest::Reporter* r,
                 const char* src,
                 SkSpan<const float> uniforms,
                 SkColor4f startingColor,
                 std::optional<SkColor4f> expectedResult) {
    SkSL::Compiler compiler;
    SkSL::ProgramSettings settings;
    settings.fMaxVersionAllowed = SkSL::Version::k300;
    std::unique_ptr<SkSL::Program> program = compiler.convertProgram(
            SkSL::ProgramKind::kRuntimeColorFilter, std::string(src), settings);
    if (!program) {
        ERRORF(r, "Unexpected error compiling %s\n%s", src, compiler.errorText().c_str());
        return;
    }
    const SkSL::FunctionDeclaration* main = program->getFunction("main");
    if (!main) {
        ERRORF(r, "Program must have a 'main' function");
        return;
    }
    SkArenaAlloc alloc(/*firstHeapAllocation=*/1000);
    SkRasterPipeline pipeline(&alloc);
    pipeline.appendConstantColor(&alloc, startingColor);
    SkSL::DebugTracePriv debugTrace;
    std::unique_ptr<SkSL::RP::Program> rasterProg =
            SkSL::MakeRasterPipelineProgram(*program, *main->definition(), &debugTrace);
    if (!rasterProg && !expectedResult.has_value()) {
        // We didn't get a program, as expected. Test passes.
        return;
    }
    if (!rasterProg && expectedResult.has_value()) {
        ERRORF(r, "MakeRasterPipelineProgram failed");
        return;
    }
    if (rasterProg && !expectedResult.has_value()) {
        ERRORF(r, "MakeRasterPipelineProgram should have failed, but didn't");
        return;
    }

#if defined(DUMP_PROGRAMS)
    // Dump the program instructions via SkDebugf.
    SkDebugf("-----\n\n");
    SkDebugfStream stream;
    rasterProg->dump(&stream);
    SkDebugf("\n-----\n\n");
#endif

    // Append the SkSL program to the raster pipeline.
    rasterProg->appendStages(&pipeline, &alloc, /*callbacks=*/nullptr, uniforms);

    // Move the float values from RGBA into an 8888 memory buffer.
    uint32_t out[SkRasterPipelineContexts::kMaxStride_highp] = {};
    SkRasterPipelineContexts::MemoryCtx outCtx{/*pixels=*/out, /*stride=*/SkRasterPipelineContexts::kMaxStride_highp};
    pipeline.append(SkRasterPipelineOp::store_8888, &outCtx);
    pipeline.run(0, 0, 1, 1);

    // Make sure the first pixel (exclusively) of `out` matches RGBA.
    uint32_t expected = expectedResult->toBytes_RGBA();
    REPORTER_ASSERT(r, out[0] == expected,
                    "Got:%02X%02X%02X%02X Expected:%02X%02X%02X%02X",
                    (out[0] >> 24) & 0xFF,
                    (out[0] >> 16) & 0xFF,
                    (out[0] >> 8) & 0xFF,
                    out[0] & 0xFF,
                    (expected >> 24) & 0xFF,
                    (expected >> 16) & 0xFF,
                    (expected >> 8) & 0xFF,
                    expected & 0xFF);

    // Make sure the rest of the pixels are untouched.
    for (size_t i = 1; i < std::size(out); ++i) {
        REPORTER_ASSERT(r, out[i] == 0);
    }
}

DEF_TEST(SkSLRasterPipelineCodeGeneratorNestedTernaryTest, r) {
    // Add in your SkSL here.
    test(r,
         R"__SkSL__(
             half4 main(half4) {
                 half three = 3, one = 1, two = 2;
                 half result = (three > (one > two ? 2.0 : 5.0)) ? 1.0 : 0.499;
                 return half4(result);
             }
         )__SkSL__",
         /*uniforms=*/{},
         /*startingColor=*/SkColor4f{0.0, 0.0, 0.0, 0.0},
         /*expectedResult=*/SkColor4f{0.499f, 0.499f, 0.499f, 0.499f});
}

DEF_TEST(SkSLRasterPipelineCodeGeneratorArithmeticTest, r) {
    test(r,
         R"__SkSL__(
            half4 main(half4) {
                const half4 colorGreen = half4(0,1,0,1), colorRed = half4(1,0,0,1);

                half a = 3.0, b = 4.0, c = a + b - 2.0;
                if (a*a + b*b == c*c*c/5.0) {
                    int A = 3, B = 4, C = A + B - 2;
                    if (A*A + B*B == C*C*C/5) {
                        return colorGreen;
                    }
                }

                return colorRed;
            }
         )__SkSL__",
         /*uniforms=*/{},
         /*startingColor=*/SkColor4f{0.0, 0.0, 0.0, 0.0},
         /*expectedResult=*/SkColor4f{0.0f, 1.0f, 0.0f, 1.0f});
}

DEF_TEST(SkSLRasterPipelineCodeGeneratorCoercedTypeTest, r) {
    static constexpr float kUniforms[] = {0.0, 1.0, 0.0, 1.0,
                                          1.0, 0.0, 0.0, 1.0};
    test(r,
         R"__SkSL__(
             uniform half4 colorGreen;
             uniform float4 colorRed;
             half4 main(half4 color) {
                 return ((colorGreen + colorRed) == float4(1.0, 1.0, 0.0, 2.0)) ? colorGreen
                                                                                : colorGreen.gr01;
             }
         )__SkSL__",
         kUniforms,
         /*startingColor=*/SkColor4f{0.0, 0.0, 0.0, 0.0},
         /*expectedResult=*/SkColor4f{0.0, 1.0, 0.0, 1.0});
}

DEF_TEST(SkSLRasterPipelineCodeGeneratorIdentitySwizzle, r) {
    static constexpr float kUniforms[] = {0.0, 1.0, 0.0, 1.0,
                                          1.0, 0.0, 0.0, 1.0};
    test(r,
         R"__SkSL__(

uniform half4 colorGreen, colorRed;

const int SEVEN = 7, TEN = 10;
const half4x4 MATRIXFIVE = half4x4(5);

noinline bool verify_const_globals(int seven, int ten, half4x4 matrixFive) {
    return seven == 7 && ten == 10 && matrixFive == half4x4(5);
}

half4 main(float4) {
    return verify_const_globals(SEVEN, TEN, MATRIXFIVE) ? colorGreen : colorRed;
}

         )__SkSL__",
         kUniforms,
         /*startingColor=*/SkColor4f{0.5, 1.0, 0.0, 0.25},
         /*expectedResult=*/SkColor4f{0.0, 1.0, 0.0, 1.0});

}

DEF_TEST(SkSLRasterPipelineCodeGeneratorBitwiseNotTest, r) {
    static constexpr int32_t kUniforms[] = { 0,  12,  3456,  4567890,
                                            ~0, ~12, ~3456, ~4567890};
    test(r,
         R"__SkSL__(
            uniform int4 value, expected;
            const half4 colorGreen = half4(0,1,0,1), colorRed = half4(1,0,0,1);

            half4 main(vec4) {
                return (~value.x    == expected.x     &&
                        ~value.xy   == expected.xy    &&
                        ~value.xyz  == expected.xyz   &&
                        ~value.xyzw == expected.xyzw) ? colorGreen : colorRed;
            }
         )__SkSL__",
         SkSpan((const float*)kUniforms, std::size(kUniforms)),
         /*startingColor=*/SkColor4f{0.0, 0.0, 0.0, 0.0},
         /*expectedResult=*/SkColor4f{0.0, 1.0, 0.0, 1.0});
}

DEF_TEST(SkSLRasterPipelineCodeGeneratorComparisonIntrinsicTest, r) {
    test(r,
         R"__SkSL__(
            half4 main(vec4) {
                const half4 colorGreen = half4(0,1,0,1), colorRed = half4(1,0,0,1);
                half4 a = half4(1, 2, 0, 1),
                      b = half4(2, 2, 1, 0);
                int3  c = int3(1111, 3333, 5555),
                      d = int3(1111, 5555, 3333);
                uint2 e = uint2(1111111111u, 222),
                      f = uint2(3333333333u, 222);
                return (lessThan(a, b)         == bool4(true, false, true, false)  &&
                        lessThan(c, d)         == bool3(false, true, false)        &&
                        lessThan(e, f)         == bool2(true, false)               &&
                        greaterThan(a, b)      == bool4(false, false, false, true) &&
                        greaterThan(c, d)      == bool3(false, false, true)        &&
                        greaterThan(e, f)      == bool2(false, false)              &&
                        lessThanEqual(a, b)    == bool4(true, true, true, false)   &&
                        lessThanEqual(c, d)    == bool3(true, true, false)         &&
                        lessThanEqual(e, f)    == bool2(true, true)                &&
                        greaterThanEqual(a, b) == bool4(false, true, false, true)  &&
                        greaterThanEqual(c, d) == bool3(true, false, true)         &&
                        greaterThanEqual(e, f) == bool2(false, true)               &&
                        equal(a, b)            == bool4(false, true, false, false) &&
                        equal(c, d)            == bool3(true, false, false)        &&
                        equal(e, f)            == bool2(false, true)               &&
                        notEqual(a, b)         == bool4(true, false, true, true)   &&
                        notEqual(c, d)         == bool3(false, true, true)         &&
                        notEqual(e, f)         == bool2(true, false)               &&
                        max(a, b)              == half4(2, 2, 1, 1)                &&
                        max(c, d)              == int3(1111, 5555, 5555)           &&
                        max(e, f)              == uint2(3333333333u, 222)          &&
                        max(a, 1)              == half4(1, 2, 1, 1)                &&
                        max(c, 3333)           == int3(3333, 3333, 5555)           &&
                        max(e, 7777)           == uint2(1111111111u, 7777)         &&
                        min(a, b)              == half4(1, 2, 0, 0)                &&
                        min(c, d)              == int3(1111, 3333, 3333)           &&
                        min(e, f)              == uint2(1111111111u, 222)          &&
                        min(a, 1)              == half4(1, 1, 0, 1)                &&
                        min(c, 3333)           == int3(1111, 3333, 3333)           &&
                        min(e, 7777)           == uint2(7777, 222)) ? colorGreen : colorRed;
            }
         )__SkSL__",
         /*uniforms=*/{},
         /*startingColor=*/SkColor4f{0.0, 0.0, 0.0, 0.0},
         /*expectedResult=*/SkColor4f{0.0, 1.0, 0.0, 1.0});
}

DEF_TEST(SkSLRasterPipelineSlotOverflow_355465305, r) {
    constexpr int kStructMembers1 = 6200;
    constexpr int kStructMembers2 = 433;
    std::stringstream str;
    str << "struct M { float4x4 m";
    for (int i = 1; i < kStructMembers1; ++i) {
        str << ",m" << i;
    }
    str << ";};";
    str << "struct M2 { float4x4 m";
    for (int i = 1; i < kStructMembers2; ++i) {
        str << ",m" << i;
    }
    str << ";};";
    str << "M f() { M m; return m; }";
    constexpr int kConstMembers = 40;
    str << "struct T { float4x4 m0";
    for (int i = 1; i < kConstMembers; ++i) {
        str << ",m" << i;
    }
    str << ";};";
    str << "const T K = T(";
    for (int i = 0; i < kConstMembers; ++i) {
        if (i > 0) {
            str << ",";
        }
        str << "mat4x4(1337)";
    }
    str << ");";
    str << "half4 main(half4 color) {";
    str << "float4x4 a = M2(";
    for (int j = 0; j < kStructMembers2; ++j) {
        if (j > 0) {
            str << ",";
        }
        const int numAddOps = (j == kStructMembers1 - 1) ? 23 : 25;
        for (int i = 0; i < numAddOps; ++i) {
            if (i > 0) {
                str << "+";
            }
            str << "f().m";
        }
    }
    str << ").m;";
    str << "return half4(a[0]+(K.m0+K.m1+K.m2+K.m3)[0]);";
    str << "}";
    std::string src = str.str();

    SkSL::Compiler compiler;
    std::unique_ptr<SkSL::Program> program =
            compiler.convertProgram(SkSL::ProgramKind::kRuntimeColorFilter, src, {});
    if (!program) {
        ERRORF(r, "Unexpected error compiling %s\n%s", src.c_str(), compiler.errorText().c_str());
        return;
    }
    const SkSL::FunctionDeclaration* main = program->getFunction("main");
    if (!main) {
        ERRORF(r, "Program must have a 'main' function");
        return;
    }
    SkArenaAlloc alloc(1000);
    SkRasterPipeline pipeline(&alloc);
    pipeline.appendConstantColor(&alloc, SkColors::kWhite);
    std::unique_ptr<SkSL::RP::Program> rasterProg =
            SkSL::MakeRasterPipelineProgram(*program, *main->definition());
    // Ideally, this program would fail in the front-end, because of the number of slots needed
    // for expression evaluation. For now, it succeeds (but then fails in appendStages).
    if (!rasterProg) {
        ERRORF(r, "MakeRasterPipelineProgram failed");
        return;
    }

    // Append the SkSL program to the raster pipeline.
    bool success = rasterProg->appendStages(&pipeline, &alloc, /*callbacks=*/nullptr, {});
    REPORTER_ASSERT(r, !success, "appendStages should fail for very large program");
}
