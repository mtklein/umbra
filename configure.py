#!/usr/bin/env python3
"""Generate the ninja build files.

Runs implicitly via the `configure` rule in build.ninja whenever this script
is newer than any generated file; invoke directly to regenerate without
building.  The generated files are checked in alongside this script — a
fresh clone builds without needing to run configure.py.
"""

import glob
import os

# Ensure subsequent relative paths and globs resolve from the project root,
# not wherever the user ran `python3 configure.py` from.
os.chdir(os.path.dirname(os.path.abspath(__file__)))


SRC    = sorted(glob.glob('src/*.c'))
TESTS  = sorted(glob.glob('tests/*.c'))
SLIDES = sorted(glob.glob('slides/*.c'))

# Tools are listed individually because each one is an entry point for a
# distinct link target (or, in the case of stb_* and work_group, an extra
# .o pulled into one or more targets).

CFLAGS = {
    'src/vulkan.c':            '-isystem /opt/homebrew/Cellar/molten-vk/1.4.1/libexec/include',
    'src/wgpu.c':              '-isystem /opt/homebrew/include',
    'tests/golden_test.c':     '-Wno-cast-align',
    'tools/stb_truetype.c':    '-Wno-everything -fno-sanitize=all',
    'tools/stb_image_write.c': '-Wno-everything -fno-sanitize=all',
    'tools/demo.c':            '-I/opt/homebrew/include -Wno-cast-align',
}


TEST_SHARDS = 3


def obj(src):
    """src/foo.c → $out/src/foo.o"""
    return '$out/' + src[:src.rfind('.')] + '.o'


def compile_line(src):
    """Emit a `build $out/X.o: compile X.c` block, with optional cflags."""
    line = f'build {obj(src)}: compile {src}\n'
    if src in CFLAGS:
        line += f'    cflags = {CFLAGS[src]}\n'
    return line


def link_objs(entry, *extras):
    """Return space-joined .o paths for a link target.

    Order: entry tool .o → tests (if any) → src → stb_truetype → extras → slides.
    """
    objs = [obj(entry)]
    if entry == 'tools/test.c':
        objs += [obj(s) for s in TESTS]
    objs += [obj(s) for s in SRC]
    objs += [obj('tools/stb_truetype.c')]
    objs += [obj(s) for s in extras]
    objs += [obj(s) for s in SLIDES]
    return ' '.join(objs)


BUILD_NINJA = r"""builddir = out

sysroot = /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
llvm    = /opt/homebrew/opt/llvm/bin
clang   = $llvm/clang -fcolor-diagnostics
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

rule link
    command = $cc $in $ldflags -o $out && codesign -s - -f --entitlements build/entitlements.plist $out 2>/dev/null


rule run
    command = perl -e 'alarm 30; exec @ARGV' $exec ./$in $args > $out

rule profmerge
    command = $llvm/llvm-profdata merge -sparse $$(for d in $dir; do echo $$d/*.profraw; done) -o $out

rule cov_report
    command = $llvm/llvm-cov report --show-branch-summary --instr-profile=$in $objects src/ include/ > $out 2>/dev/null

rule cov_show
    command = rm -rf $dir && $llvm/llvm-cov show --show-branches=count --instr-profile=$in $objects --format=html --output-dir=$dir src/ include/ 2>/dev/null && echo "Coverage report: file://$$(pwd)/$dir/index.html" > $out

subninja build/host.ninja
subninja build/xsan.ninja
subninja build/wasm.ninja
subninja build/x86.ninja
subninja build/x86_xsan.ninja
subninja build/gcc.ninja

rule configure
    command = python3 configure.py
    description = configure
    generator = 1

build build.ninja $
      build/project.ninja $
      build/host.ninja $
      build/xsan.ninja $
      build/wasm.ninja $
      build/x86.ninja $
      build/x86_xsan.ninja $
      build/gcc.ninja: configure | configure.py
"""


# Each entry is `(name, count)`: `tools/dump.c` emits `count` pipelines for
# slide `name` into `dumps/{name}/{0..count-1}/{arm64|ir|builder|metal.msl|
# vulkan.spirv|vulkan.msl}.txt` (xsan) and `dumps/{name}/{i}/avx2.txt`
# (x86_xsan).  Order here drives the order in the generated `build ... |`
# output lists.

DUMPS = [
    ('srcover',                         1),
    ('blend_src',                       1),
    ('blend_srcover',                   1),
    ('blend_dstover',                   1),
    ('blend_multiply',                  1),
    ('blend_null',                      1),
    ('color_swatches',                  1),
    ('coverage_8_bit_bitmap',           1),
    ('coverage_sdf_bitmap',             1),
    ('coverage_8_bit_bitmap_matrix',    1),
    ('coverage_null',                   1),
    ('gradient_linear_two_stop',        1),
    ('gradient_radial_two_stop',        1),
    ('gradient_linear',                 1),
    ('gradient_radial',                 1),
    ('gradient_linear_lut',             1),
    ('gradient_radial_lut',             1),
    ('gradient_linear_evenly_spaced',   1),
    ('gradient_radial_evenly_spaced',   1),
    ('sdf_circle',                      1),
    ('sdf_union',                       1),
    ('sdf_intersection',                1),
    ('sdf_difference',                  1),
    ('sdf_ring',                        1),
    ('sdf_rounded_rect',                1),
    ('sdf_capsule',                     1),
    ('sdf_halfplane',                   1),
    ('sdf_text',                        1),
    ('sdf_n_gon',                       1),
    ('slug_two_pass',                   2),
    ('slug_one_pass',                   1),
    ('overview',                        1),
]


def dump_paths(suffixes):
    """Yield 'dumps/{name}/{i}/{suffix}' for every (name, i, suffix) triple."""
    for name, count in DUMPS:
        for i in range(count):
            for suf in suffixes:
                yield f'dumps/{name}/{i}/{suf}'


def dump_outputs_block(suffixes):
    """Emit the `build $out/dump.log | $\n     path $\n     ...: run $out/dump`
    block from the current DUMPS registry, in dump.c order."""
    paths = list(dump_paths(suffixes))
    lines = ['build $out/dump.log | $\n']
    for p in paths[:-1]:
        lines.append(f'      {p} $\n')
    lines.append(f'      {paths[-1]}: run $out/dump\n')
    return ''.join(lines)


XSAN_DUMP_SUFFIXES    = ['arm64.txt', 'ir.txt', 'builder.txt',
                         'metal.msl', 'vulkan.spirv', 'vulkan.msl']
X86_DUMP_SUFFIXES     = ['avx2.txt']


def render_project_ninja():
    parts = []
    # stb tool sources (with their cflag overrides).
    for s in ['tools/stb_truetype.c', 'tools/stb_image_write.c']:
        parts.append(compile_line(s))
    parts.append('\n')
    # Main source, tests, tools, slides.
    for s in SRC:
        parts.append(compile_line(s))
    for s in TESTS:
        parts.append(compile_line(s))
    for s in ['tools/test.c', 'tools/bench.c', 'tools/dump.c']:
        parts.append(compile_line(s))
    for s in SLIDES:
        parts.append(compile_line(s))
    parts.append('\n')
    # Link targets.
    parts.append(f'build $out/test:  link {link_objs("tools/test.c")}\n')
    parts.append( '    ldflags = $ldflags -lm\n')
    parts.append(f'build $out/bench: link {link_objs("tools/bench.c")}\n')
    parts.append(f'build $out/dump:  link {link_objs("tools/dump.c", "tools/stb_image_write.c")}\n')
    parts.append('\n')
    for s in range(TEST_SHARDS):
        parts.append(f'build $out/test_{s}.log: run $out/test\n')
        parts.append(f'    args = --shards {TEST_SHARDS} --shard {s}\n')
    return ''.join(parts)


PROJECT_NINJA = render_project_ninja()


# Demo target — host and xsan emit an identical block, so factor it out.
DEMO_LDFLAGS ='-framework Metal -framework Foundation -L/opt/homebrew/lib -lSDL3 -lMoltenVK -lwgpu_native'


def render_demo_block():
    return (compile_line('tools/demo.c')
            + compile_line('tools/work_group.c')
            + f'build $out/demo: link {link_objs("tools/demo.c", "tools/work_group.c", "tools/stb_image_write.c")}\n'
            + f'    ldflags = {DEMO_LDFLAGS}\n')


DEMO_BLOCK = render_demo_block()


HOST_NINJA = f"""out     = $builddir/host
cc      = $clang
ldflags = -framework Metal -framework Foundation -L/opt/homebrew/lib -lMoltenVK -lwgpu_native

include build/project.ninja

{DEMO_BLOCK}"""



WASM_NINJA = r"""out  = $builddir/wasm
cc   = $clang -target wasm32-wasip1 -msimd128 -B /opt/homebrew/opt/lld/bin -D_WASI_EMULATED_MMAN
exec = env WASMTIME_BACKTRACE_DETAILS=1 wasmtime
ldflags = -lwasi-emulated-mman
include build/project.ninja
"""


X86_NINJA = r"""out     = $builddir/x86
cc      = $clang -momit-leaf-frame-pointer -target x86_64-apple-macos13 -isysroot $sysroot $
                 -march=x86-64-v3 -mno-vzeroupper
ldflags = -framework Metal -framework Foundation
include build/project.ninja
"""


GCC_NINJA = r"""out     = $builddir/gcc
cc      = /opt/homebrew/bin/gcc-15
warn    = -Wall -Wextra -Wpedantic -Wno-unknown-pragmas
ldflags = -lm -lobjc -framework Metal -framework Foundation -L/opt/homebrew/lib -lMoltenVK -lwgpu_native
include build/project.ninja
"""



XSAN_NINJA_PREFIX = f"""cov = -fprofile-instr-generate -fcoverage-mapping

out     = $builddir/xsan
cc      = $clang -fsanitize=address,integer,undefined -fno-sanitize-recover=all $cov
exec    = env UBSAN_OPTIONS=print_stacktrace=1 LLVM_PROFILE_FILE=$builddir/xsan/%p.profraw
ldflags = -framework Metal -framework Foundation -L/opt/homebrew/lib -lMoltenVK -lwgpu_native $cov
include build/project.ninja

{DEMO_BLOCK}
"""
def render_xsan_suffix():
    test_deps = ''
    for s in range(TEST_SHARDS):
        test_deps += f'      $out/test_{s}.log $\n'
        test_deps += f'      $x86/test_{s}.log $\n'
    return f"""
x86 = $builddir/x86_xsan

build $out/coverage.profdata: profmerge | $
{test_deps}      $out/dump.log $
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

XSAN_NINJA_SUFFIX = render_xsan_suffix()


X86_XSAN_NINJA_PREFIX = r"""cov = -fprofile-instr-generate -fcoverage-mapping

out     = $builddir/x86_xsan
cc      = $clang -momit-leaf-frame-pointer -target x86_64-apple-macos13 -isysroot $sysroot $
                 -march=x86-64-v3 -mno-vzeroupper $
                 -fsanitize=address,integer,undefined -fno-sanitize-recover=all $cov
exec    = env UBSAN_OPTIONS=print_stacktrace=1 LLVM_PROFILE_FILE=$builddir/x86_xsan/%p.profraw $
              arch -x86_64
ldflags = -framework Metal -framework Foundation $cov -Wl,-w
include build/project.ninja

"""

XSAN_NINJA      = (XSAN_NINJA_PREFIX
                   + dump_outputs_block(XSAN_DUMP_SUFFIXES)
                   + XSAN_NINJA_SUFFIX)
X86_XSAN_NINJA  = (X86_XSAN_NINJA_PREFIX
                   + dump_outputs_block(X86_DUMP_SUFFIXES))


FILES = {
    'build.ninja':               BUILD_NINJA,
    'build/project.ninja':       PROJECT_NINJA,
    'build/host.ninja':          HOST_NINJA,
    'build/xsan.ninja':          XSAN_NINJA,
    'build/wasm.ninja':          WASM_NINJA,
    'build/x86.ninja':            X86_NINJA,
    'build/x86_xsan.ninja':      X86_XSAN_NINJA,
    'build/gcc.ninja':           GCC_NINJA,
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
    for path, content in FILES.items():
        write(path, content)


if __name__ == '__main__':
    main()
