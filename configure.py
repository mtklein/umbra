#!/usr/bin/env python3
"""Generate the ninja build files.

Runs implicitly via the `configure` rule in build.ninja whenever this script
is newer than any generated file; invoke directly to regenerate without
building.  The generated files are checked in alongside this script — a
fresh clone builds without needing to run configure.py.

First pass: emits byte-identical output to the hand-maintained ninja files
so `git diff` is the regression oracle for subsequent refactors.
"""

import os


BUILD_NINJA = r"""builddir = out

sysroot = /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
clang   = /opt/homebrew/opt/llvm/bin/clang -fcolor-diagnostics
warn    = -Weverything $
          -Wno-declaration-after-statement $
          -Wno-disabled-macro-expansion $
          -Wno-implicit-void-ptr-cast $
          -Wno-nrvo $
          -Wno-pre-c11-compat $
          -Wno-switch-default $
          -Wno-unsafe-buffer-usage

rule compile
    command = $cc -g -O1 -std=c11 -Werror $warn $cflags -MD -MF $out.d -c $in -o $out
    depfile = $out.d
    deps    = gcc

rule compile_m
    command = $cc -g -O1 -fobjc-arc -Werror $warn -MD -MF $out.d -c $in -o $out
    depfile = $out.d
    deps    = gcc

rule link
    command = $cc $in $ldflags -o $out && codesign -s - -f --entitlements build/entitlements.plist $out 2>/dev/null


rule run
    command = perl -e 'alarm 30; exec @ARGV' $exec ./$in $args > $out

llvm = /opt/homebrew/opt/llvm/bin

rule profmerge
    command = $llvm/llvm-profdata merge -sparse $$(for d in $dir; do echo $$d/*.profraw; done) -o $out

rule cov_report
    command = $llvm/llvm-cov report --show-branch-summary --instr-profile=$in $objects src/ include/ > $out 2>/dev/null

rule cov_show
    command = rm -rf $dir && $llvm/llvm-cov show --show-branches=count --instr-profile=$in $objects --format=html --output-dir=$dir src/ include/ 2>/dev/null && echo "Coverage report: file://$$(pwd)/$dir/index.html" > $out

subninja build/host.ninja
subninja build/lto.ninja
subninja build/xsan.ninja
subninja build/wasm.ninja
subninja build/x86_64.ninja
subninja build/x86_64h.ninja
subninja build/x86_64h_xsan.ninja
subninja build/gcc.ninja
subninja build/size.ninja

rule configure
    command = python3 configure.py
    description = configure
    generator = 1

build build.ninja $
      build/project.ninja $
      build/host.ninja $
      build/lto.ninja $
      build/xsan.ninja $
      build/wasm.ninja $
      build/x86_64.ninja $
      build/x86_64h.ninja $
      build/x86_64h_xsan.ninja $
      build/gcc.ninja $
      build/size.ninja: configure | configure.py
"""


PROJECT_NINJA = r"""build $out/stb_truetype.o:       compile tools/stb_truetype.c
    cflags = -Wno-everything -fno-sanitize=all
build $out/stb_image_write.o:   compile tools/stb_image_write.c
    cflags = -Wno-everything -fno-sanitize=all

build $out/src/uniforms.o:       compile   src/uniforms.c
build $out/src/uniform_ring.o:   compile   src/uniform_ring.c
build $out/src/fingerprint.o:    compile   src/fingerprint.c
build $out/src/dispatch_overlap.o: compile src/dispatch_overlap.c
build $out/src/gpu_buf_cache.o: compile   src/gpu_buf_cache.c
build $out/src/hash.o:           compile   src/hash.c
build $out/src/op.o:             compile   src/op.c
build $out/src/builder.o:        compile   src/builder.c
build $out/src/flat_ir.o:    compile   src/flat_ir.c
build $out/src/interval.o:       compile   src/interval.c
build $out/src/ra.o:             compile   src/ra.c
build $out/src/interpreter.o:  compile   src/interpreter.c
build $out/src/jit.o:            compile   src/jit.c
build $out/src/jit_arm64.o:      compile   src/jit_arm64.c
build $out/src/jit_x86.o:        compile   src/jit_x86.c
build $out/src/asm_arm64.o:      compile   src/asm_arm64.c
build $out/src/asm_x86.o:        compile   src/asm_x86.c
build $out/src/metal.o:          compile_m src/metal.m
build $out/src/spirv.o:          compile   src/spirv.c
build $out/src/vulkan.o:         compile   src/vulkan.c
    cflags = -isystem /opt/homebrew/Cellar/molten-vk/1.4.1/libexec/include
build $out/src/wgpu.o:           compile   src/wgpu.c
    cflags = -isystem /opt/homebrew/include
build $out/src/draw.o:           compile   src/draw.c
build $out/tests/test.o:         compile   tests/test.c
build $out/tests/bb_test.o:      compile   tests/bb_test.c
build $out/tests/fingerprint_test.o:    compile tests/fingerprint_test.c
build $out/tests/gpu_buf_cache_test.o: compile tests/gpu_buf_cache_test.c
build $out/tests/hash_test.o:    compile   tests/hash_test.c
build $out/tests/uniform_ring_test.o: compile tests/uniform_ring_test.c
build $out/tests/ra_test.o:      compile   tests/ra_test.c
build $out/tests/draw_test.o:    compile   tests/draw_test.c
build $out/tests/golden_test.o:  compile   tests/golden_test.c
    cflags = -Wno-cast-align
build $out/tools/test.o:         compile   tools/test.c
build $out/tools/bench.o:        compile   tools/bench.c
build $out/tools/dump.o:         compile   tools/dump.c
build $out/slides/slides.o:      compile   slides/slides.c
build $out/slides/solid.o:       compile   slides/solid.c
build $out/slides/text.o:        compile   slides/text.c
build $out/slides/persp.o:       compile   slides/persp.c
build $out/slides/gradient.o:    compile   slides/gradient.c
build $out/slides/slug.o:        compile   slides/slug.c
build $out/slides/anim.o:        compile   slides/anim.c
build $out/slides/overview.o:    compile   slides/overview.c
build $out/slides/swatch.o:     compile   slides/swatch.c
build $out/slides/circle.o:     compile   slides/circle.c
build $out/tests/dispatch_overlap_test.o: compile tests/dispatch_overlap_test.c
build $out/tests/asm_arm64_test.o: compile tests/asm_arm64_test.c
build $out/tests/asm_x86_test.o:   compile tests/asm_x86_test.c
build $out/tests/resolve_test.o:   compile tests/resolve_test.c
build $out/tests/interval_test.o:  compile tests/interval_test.c

build $out/test:      link $out/tools/test.o $out/tests/test.o $out/tests/bb_test.o $out/tests/fingerprint_test.o $out/tests/gpu_buf_cache_test.o $out/tests/dispatch_overlap_test.o $out/tests/hash_test.o $out/tests/uniform_ring_test.o $out/tests/ra_test.o $out/tests/draw_test.o $out/tests/asm_arm64_test.o $out/tests/asm_x86_test.o $out/tests/golden_test.o $out/tests/resolve_test.o $out/tests/interval_test.o $out/src/builder.o $out/src/op.o $out/src/flat_ir.o $out/src/interval.o $out/src/fingerprint.o $out/src/dispatch_overlap.o $out/src/gpu_buf_cache.o $out/src/hash.o $out/src/uniforms.o $out/src/uniform_ring.o $out/src/ra.o $out/src/interpreter.o $out/src/jit.o $out/src/jit_arm64.o $out/src/jit_x86.o $out/src/asm_arm64.o $out/src/asm_x86.o $out/src/metal.o $out/src/spirv.o $out/src/vulkan.o $out/src/wgpu.o $out/src/draw.o $out/stb_truetype.o $out/slides/slides.o $out/slides/solid.o $out/slides/text.o $out/slides/persp.o $out/slides/gradient.o $out/slides/slug.o $out/slides/anim.o $out/slides/overview.o $out/slides/swatch.o $out/slides/circle.o
    ldflags = $ldflags -lm
build $out/bench:     link $out/tools/bench.o           $out/src/builder.o $out/src/op.o $out/src/flat_ir.o $out/src/interval.o $out/src/fingerprint.o $out/src/dispatch_overlap.o $out/src/gpu_buf_cache.o $out/src/hash.o $out/src/uniforms.o $out/src/uniform_ring.o $out/src/ra.o $out/src/interpreter.o $out/src/jit.o $out/src/jit_arm64.o $out/src/jit_x86.o $out/src/asm_arm64.o $out/src/asm_x86.o $out/src/metal.o $out/src/spirv.o $out/src/vulkan.o $out/src/wgpu.o $out/src/draw.o $out/stb_truetype.o $out/slides/slides.o $out/slides/solid.o $out/slides/text.o $out/slides/persp.o $out/slides/gradient.o $out/slides/slug.o $out/slides/anim.o $out/slides/overview.o $out/slides/swatch.o $out/slides/circle.o
build $out/dump:      link $out/tools/dump.o            $out/src/builder.o $out/src/op.o $out/src/flat_ir.o $out/src/interval.o $out/src/fingerprint.o $out/src/dispatch_overlap.o $out/src/gpu_buf_cache.o $out/src/hash.o $out/src/uniforms.o $out/src/uniform_ring.o $out/src/ra.o $out/src/interpreter.o $out/src/jit.o $out/src/jit_arm64.o $out/src/jit_x86.o $out/src/asm_arm64.o $out/src/asm_x86.o $out/src/metal.o $out/src/spirv.o $out/src/vulkan.o $out/src/wgpu.o $out/src/draw.o $out/stb_truetype.o $out/stb_image_write.o $out/slides/slides.o $out/slides/solid.o $out/slides/text.o $out/slides/persp.o $out/slides/gradient.o $out/slides/slug.o $out/slides/anim.o $out/slides/overview.o $out/slides/swatch.o $out/slides/circle.o

build $out/test.log:             run  $out/test
"""


HOST_NINJA = r"""out     = $builddir/host
cc      = $clang
ldflags = -framework Metal -framework Foundation -L/opt/homebrew/lib -lMoltenVK -lwgpu_native

include build/project.ninja


build $out/tools/demo.o: compile tools/demo.c
    cflags = -I/opt/homebrew/include -Wno-cast-align
build $out/tools/work_group.o: compile tools/work_group.c
build $out/demo:   link $out/tools/demo.o $out/tools/work_group.o $out/src/builder.o $out/src/flat_ir.o $out/src/op.o $out/src/fingerprint.o $out/src/dispatch_overlap.o $out/src/gpu_buf_cache.o $out/src/hash.o $out/src/uniforms.o $out/src/uniform_ring.o $out/src/ra.o $out/src/interpreter.o $out/src/jit.o $out/src/jit_arm64.o $out/src/jit_x86.o $out/src/asm_arm64.o $out/src/asm_x86.o $out/src/metal.o $out/src/spirv.o $out/src/vulkan.o $out/src/wgpu.o $out/src/draw.o $out/stb_truetype.o $out/stb_image_write.o $out/slides/slides.o $out/slides/solid.o $out/slides/text.o $out/slides/persp.o $out/slides/gradient.o $out/slides/slug.o $out/slides/anim.o $out/slides/overview.o $out/slides/swatch.o $out/slides/circle.o
    ldflags = -framework Metal -framework Foundation -L/opt/homebrew/lib -lSDL3 -lMoltenVK -lwgpu_native


"""


LTO_NINJA = r"""out     = $builddir/lto
cc      = $clang -flto
ldflags = -framework Metal -framework Foundation -L/opt/homebrew/lib -lMoltenVK -lwgpu_native
include build/project.ninja
"""


WASM_NINJA = r"""out  = $builddir/wasm
cc   = $clang -target wasm32-wasip1 -msimd128 -B /opt/homebrew/opt/lld/bin -D_WASI_EMULATED_MMAN
exec = env WASMTIME_BACKTRACE_DETAILS=1 wasmtime
ldflags = -lwasi-emulated-mman
include build/project.ninja
"""


X86_64_NINJA = r"""out     = $builddir/x86_64
cc      = $clang -momit-leaf-frame-pointer -target x86_64-apple-macos13 -isysroot $sysroot
ldflags = -framework Metal -framework Foundation
include build/project.ninja
"""


X86_64H_NINJA = r"""out     = $builddir/x86_64h
cc      = $clang -momit-leaf-frame-pointer -target x86_64-apple-macos13 -isysroot $sysroot $
                 -march=x86-64-v3 -mno-vzeroupper
ldflags = -framework Metal -framework Foundation
include build/project.ninja

"""


GCC_NINJA = r"""out     = $builddir/gcc
cc      = /opt/homebrew/bin/gcc-15
warn    = -Wall -Wextra -Wpedantic -Wno-unknown-pragmas
ldflags = -lm -L/opt/homebrew/lib -lMoltenVK -lwgpu_native

rule compile_m
    command = $cc -g -O1 -std=c11 -Werror $warn -MD -MF $out.d -c src/metal_stub.c -o $out
    depfile = $out.d
    deps    = gcc

include build/project.ninja
"""


SIZE_NINJA = r"""out     = $builddir/size
cc      = $clang -Oz -g0 -fno-asynchronous-unwind-tables -fno-unwind-tables -ffunction-sections -fdata-sections
ldflags = -framework Metal -framework Foundation -L/opt/homebrew/lib -lMoltenVK -lwgpu_native -dead_strip -Wl,-x,-S

rule compile
    command = $cc -std=c11 -Werror $warn $cflags -MD -MF $out.d -c $in -o $out
    depfile = $out.d
    deps    = gcc

rule compile_m
    command = $cc -fobjc-arc -Werror $warn -MD -MF $out.d -c $in -o $out
    depfile = $out.d
    deps    = gcc

include build/project.ninja
"""


# xsan's dump.log output list and x86_64h_xsan's avx2 variant are the big
# bespoke pieces — left in exact form for now.
XSAN_NINJA = r"""cov = -fprofile-instr-generate -fcoverage-mapping

out     = $builddir/xsan
cc      = $clang -fsanitize=address,integer,undefined -fno-sanitize-recover=all $cov
exec    = env UBSAN_OPTIONS=print_stacktrace=1 LLVM_PROFILE_FILE=$builddir/xsan/%p.profraw
ldflags = -framework Metal -framework Foundation -L/opt/homebrew/lib -lMoltenVK -lwgpu_native $cov
include build/project.ninja

build $out/tools/demo.o: compile tools/demo.c
    cflags = -I/opt/homebrew/include -Wno-cast-align
build $out/tools/work_group.o: compile tools/work_group.c
build $out/demo:   link $out/tools/demo.o $out/tools/work_group.o $out/src/builder.o $out/src/flat_ir.o $out/src/op.o $out/src/fingerprint.o $out/src/dispatch_overlap.o $out/src/gpu_buf_cache.o $out/src/hash.o $out/src/uniforms.o $out/src/uniform_ring.o $out/src/ra.o $out/src/interpreter.o $out/src/jit.o $out/src/jit_arm64.o $out/src/jit_x86.o $out/src/asm_arm64.o $out/src/asm_x86.o $out/src/metal.o $out/src/spirv.o $out/src/vulkan.o $out/src/wgpu.o $out/src/draw.o $out/stb_truetype.o $out/stb_image_write.o $out/slides/slides.o $out/slides/solid.o $out/slides/text.o $out/slides/persp.o $out/slides/gradient.o $out/slides/slug.o $out/slides/anim.o $out/slides/overview.o $out/slides/swatch.o $out/slides/circle.o
    ldflags = -framework Metal -framework Foundation -L/opt/homebrew/lib -lSDL3 -lMoltenVK -lwgpu_native

build $out/dump.log | $
      dumps/srcover/0/arm64.txt $
      dumps/srcover/0/ir.txt $
      dumps/srcover/0/builder.txt $
      dumps/srcover/0/metal.msl $
      dumps/srcover/0/vulkan.spirv $
      dumps/srcover/0/vulkan.msl $
      dumps/solid_fill_src/0/arm64.txt $
      dumps/solid_fill_src/0/ir.txt $
      dumps/solid_fill_src/0/builder.txt $
      dumps/solid_fill_src/0/metal.msl $
      dumps/solid_fill_src/0/vulkan.spirv $
      dumps/solid_fill_src/0/vulkan.msl $
      dumps/source_over_srcover/0/arm64.txt $
      dumps/source_over_srcover/0/ir.txt $
      dumps/source_over_srcover/0/builder.txt $
      dumps/source_over_srcover/0/metal.msl $
      dumps/source_over_srcover/0/vulkan.spirv $
      dumps/source_over_srcover/0/vulkan.msl $
      dumps/destination_over_dstover/0/arm64.txt $
      dumps/destination_over_dstover/0/ir.txt $
      dumps/destination_over_dstover/0/builder.txt $
      dumps/destination_over_dstover/0/metal.msl $
      dumps/destination_over_dstover/0/vulkan.spirv $
      dumps/destination_over_dstover/0/vulkan.msl $
      dumps/multiply_blend/0/arm64.txt $
      dumps/multiply_blend/0/ir.txt $
      dumps/multiply_blend/0/builder.txt $
      dumps/multiply_blend/0/metal.msl $
      dumps/multiply_blend/0/vulkan.spirv $
      dumps/multiply_blend/0/vulkan.msl $
      dumps/full_coverage_no_rect_clip/0/arm64.txt $
      dumps/full_coverage_no_rect_clip/0/ir.txt $
      dumps/full_coverage_no_rect_clip/0/builder.txt $
      dumps/full_coverage_no_rect_clip/0/metal.msl $
      dumps/full_coverage_no_rect_clip/0/vulkan.spirv $
      dumps/full_coverage_no_rect_clip/0/vulkan.msl $
      dumps/no_blend_direct_paint/0/arm64.txt $
      dumps/no_blend_direct_paint/0/ir.txt $
      dumps/no_blend_direct_paint/0/builder.txt $
      dumps/no_blend_direct_paint/0/metal.msl $
      dumps/no_blend_direct_paint/0/vulkan.spirv $
      dumps/no_blend_direct_paint/0/vulkan.msl $
      dumps/text_8_bit_aa/0/arm64.txt $
      dumps/text_8_bit_aa/0/ir.txt $
      dumps/text_8_bit_aa/0/builder.txt $
      dumps/text_8_bit_aa/0/metal.msl $
      dumps/text_8_bit_aa/0/vulkan.spirv $
      dumps/text_8_bit_aa/0/vulkan.msl $
      dumps/text_sdf/0/arm64.txt $
      dumps/text_sdf/0/ir.txt $
      dumps/text_sdf/0/builder.txt $
      dumps/text_sdf/0/metal.msl $
      dumps/text_sdf/0/vulkan.spirv $
      dumps/text_sdf/0/vulkan.msl $
      dumps/perspective_text/0/arm64.txt $
      dumps/perspective_text/0/ir.txt $
      dumps/perspective_text/0/builder.txt $
      dumps/perspective_text/0/metal.msl $
      dumps/perspective_text/0/vulkan.spirv $
      dumps/perspective_text/0/vulkan.msl $
      dumps/linear_gradient_2_stop/0/arm64.txt $
      dumps/linear_gradient_2_stop/0/ir.txt $
      dumps/linear_gradient_2_stop/0/builder.txt $
      dumps/linear_gradient_2_stop/0/metal.msl $
      dumps/linear_gradient_2_stop/0/vulkan.spirv $
      dumps/linear_gradient_2_stop/0/vulkan.msl $
      dumps/radial_gradient_2_stop/0/arm64.txt $
      dumps/radial_gradient_2_stop/0/ir.txt $
      dumps/radial_gradient_2_stop/0/builder.txt $
      dumps/radial_gradient_2_stop/0/metal.msl $
      dumps/radial_gradient_2_stop/0/vulkan.spirv $
      dumps/radial_gradient_2_stop/0/vulkan.msl $
      dumps/linear_gradient_wide_gamut/0/arm64.txt $
      dumps/linear_gradient_wide_gamut/0/ir.txt $
      dumps/linear_gradient_wide_gamut/0/builder.txt $
      dumps/linear_gradient_wide_gamut/0/metal.msl $
      dumps/linear_gradient_wide_gamut/0/vulkan.spirv $
      dumps/linear_gradient_wide_gamut/0/vulkan.msl $
      dumps/radial_gradient_wide_gamut/0/arm64.txt $
      dumps/radial_gradient_wide_gamut/0/ir.txt $
      dumps/radial_gradient_wide_gamut/0/builder.txt $
      dumps/radial_gradient_wide_gamut/0/metal.msl $
      dumps/radial_gradient_wide_gamut/0/vulkan.spirv $
      dumps/radial_gradient_wide_gamut/0/vulkan.msl $
      dumps/linear_gradient_loop_stops/0/arm64.txt $
      dumps/linear_gradient_loop_stops/0/ir.txt $
      dumps/linear_gradient_loop_stops/0/builder.txt $
      dumps/linear_gradient_loop_stops/0/metal.msl $
      dumps/linear_gradient_loop_stops/0/vulkan.spirv $
      dumps/linear_gradient_loop_stops/0/vulkan.msl $
      dumps/radial_gradient_loop_stops/0/arm64.txt $
      dumps/radial_gradient_loop_stops/0/ir.txt $
      dumps/radial_gradient_loop_stops/0/builder.txt $
      dumps/radial_gradient_loop_stops/0/metal.msl $
      dumps/radial_gradient_loop_stops/0/vulkan.spirv $
      dumps/radial_gradient_loop_stops/0/vulkan.msl $
      dumps/slug_two_pass/0/arm64.txt $
      dumps/slug_two_pass/0/ir.txt $
      dumps/slug_two_pass/0/builder.txt $
      dumps/slug_two_pass/0/metal.msl $
      dumps/slug_two_pass/0/vulkan.spirv $
      dumps/slug_two_pass/0/vulkan.msl $
      dumps/slug_two_pass/1/arm64.txt $
      dumps/slug_two_pass/1/ir.txt $
      dumps/slug_two_pass/1/builder.txt $
      dumps/slug_two_pass/1/metal.msl $
      dumps/slug_two_pass/1/vulkan.spirv $
      dumps/slug_two_pass/1/vulkan.msl $
      dumps/slug_one_pass/0/arm64.txt $
      dumps/slug_one_pass/0/ir.txt $
      dumps/slug_one_pass/0/builder.txt $
      dumps/slug_one_pass/0/metal.msl $
      dumps/slug_one_pass/0/vulkan.spirv $
      dumps/slug_one_pass/0/vulkan.msl $
      dumps/animated_t_in_uniforms/0/arm64.txt $
      dumps/animated_t_in_uniforms/0/ir.txt $
      dumps/animated_t_in_uniforms/0/builder.txt $
      dumps/animated_t_in_uniforms/0/metal.msl $
      dumps/animated_t_in_uniforms/0/vulkan.spirv $
      dumps/animated_t_in_uniforms/0/vulkan.msl $
      dumps/color_swatches/0/arm64.txt $
      dumps/color_swatches/0/ir.txt $
      dumps/color_swatches/0/builder.txt $
      dumps/color_swatches/0/metal.msl $
      dumps/color_swatches/0/vulkan.spirv $
      dumps/color_swatches/0/vulkan.msl $
      dumps/circle_coverage_interval_ready/0/arm64.txt $
      dumps/circle_coverage_interval_ready/0/ir.txt $
      dumps/circle_coverage_interval_ready/0/builder.txt $
      dumps/circle_coverage_interval_ready/0/metal.msl $
      dumps/circle_coverage_interval_ready/0/vulkan.spirv $
      dumps/circle_coverage_interval_ready/0/vulkan.msl $
      dumps/overview/0/arm64.txt $
      dumps/overview/0/ir.txt $
      dumps/overview/0/builder.txt $
      dumps/overview/0/metal.msl $
      dumps/overview/0/vulkan.spirv $
      dumps/overview/0/vulkan.msl: run $out/dump

x86 = $builddir/x86_64h_xsan

build $out/coverage.profdata: profmerge | $
      $out/test.log $
      $out/dump.log $
      $x86/test.log $
      $x86/dump.log
    dir = $out $x86

build $out/coverage.txt: cov_report $out/coverage.profdata | $
      $out/test $
      $out/dump $
      $x86/test $
      $x86/dump
    objects = $out/test $
              -object=$out/dump $
              -object=$x86/test $
              -object=$x86/dump

build $out/report.txt: cov_show $out/coverage.profdata | $
      $out/test $
      $out/dump $
      $x86/test $
      $x86/dump
    objects = $out/test $
              -object=$out/dump $
              -object=$x86/test $
              -object=$x86/dump
    dir = $out/report
"""


X86_64H_XSAN_NINJA = r"""cov = -fprofile-instr-generate -fcoverage-mapping

out     = $builddir/x86_64h_xsan
cc      = $clang -momit-leaf-frame-pointer -target x86_64-apple-macos13 -isysroot $sysroot $
                 -march=x86-64-v3 -mno-vzeroupper $
                 -fsanitize=address,integer,undefined -fno-sanitize-recover=all $cov
exec    = env UBSAN_OPTIONS=print_stacktrace=1 LLVM_PROFILE_FILE=$builddir/x86_64h_xsan/%p.profraw $
              arch -x86_64
ldflags = -framework Metal -framework Foundation $cov -Wl,-w
include build/project.ninja

build $out/dump.log | $
      dumps/srcover/0/avx2.txt $
      dumps/solid_fill_src/0/avx2.txt $
      dumps/source_over_srcover/0/avx2.txt $
      dumps/destination_over_dstover/0/avx2.txt $
      dumps/multiply_blend/0/avx2.txt $
      dumps/full_coverage_no_rect_clip/0/avx2.txt $
      dumps/no_blend_direct_paint/0/avx2.txt $
      dumps/text_8_bit_aa/0/avx2.txt $
      dumps/text_sdf/0/avx2.txt $
      dumps/perspective_text/0/avx2.txt $
      dumps/linear_gradient_2_stop/0/avx2.txt $
      dumps/radial_gradient_2_stop/0/avx2.txt $
      dumps/linear_gradient_wide_gamut/0/avx2.txt $
      dumps/radial_gradient_wide_gamut/0/avx2.txt $
      dumps/linear_gradient_loop_stops/0/avx2.txt $
      dumps/radial_gradient_loop_stops/0/avx2.txt $
      dumps/slug_two_pass/0/avx2.txt $
      dumps/slug_two_pass/1/avx2.txt $
      dumps/slug_one_pass/0/avx2.txt $
      dumps/animated_t_in_uniforms/0/avx2.txt $
      dumps/color_swatches/0/avx2.txt $
      dumps/overview/0/avx2.txt $
      dumps/circle_coverage_interval_ready/0/avx2.txt: run $out/dump
"""


FILES = {
    'build.ninja':               BUILD_NINJA,
    'build/project.ninja':       PROJECT_NINJA,
    'build/host.ninja':          HOST_NINJA,
    'build/lto.ninja':           LTO_NINJA,
    'build/xsan.ninja':          XSAN_NINJA,
    'build/wasm.ninja':          WASM_NINJA,
    'build/x86_64.ninja':        X86_64_NINJA,
    'build/x86_64h.ninja':       X86_64H_NINJA,
    'build/x86_64h_xsan.ninja':  X86_64H_XSAN_NINJA,
    'build/gcc.ninja':           GCC_NINJA,
    'build/size.ninja':          SIZE_NINJA,
}


def write(path, content):
    # Always rewrite — ninja's self-regen loop needs the output mtimes to
    # advance past configure.py's, even when the content didn't change.
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, 'w') as f:
        f.write(content)


def main():
    root = os.path.dirname(os.path.abspath(__file__))
    os.chdir(root)
    for path, content in FILES.items():
        write(path, content)


if __name__ == '__main__':
    main()
