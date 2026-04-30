const std = @import("std");

const src_files = [_][]const u8{
    "src/asm_arm64.c",
    "src/asm_x86.c",
    "src/builder.c",
    "src/dispatch_overlap.c",
    "src/draw.c",
    "src/fingerprint.c",
    "src/flat_ir.c",
    "src/gpu_buf_cache.c",
    "src/hash.c",
    "src/interpreter.c",
    "src/interval.c",
    "src/jit.c",
    "src/jit_arm64.c",
    "src/jit_x86.c",
    "src/metal.c",
    "src/op.c",
    "src/ra.c",
    "src/spirv.c",
    "src/uniform_ring.c",
    "src/vulkan.c",
    "src/wgpu.c",
};

const test_files = [_][]const u8{
    "tests/asm_arm64_test.c",
    "tests/asm_x86_test.c",
    "tests/dispatch_overlap_test.c",
    "tests/draw_test.c",
    "tests/fingerprint_test.c",
    "tests/flat_ir_test.c",
    "tests/golden_test.c",
    "tests/gpu_buf_cache_test.c",
    "tests/hash_test.c",
    "tests/interval_test.c",
    "tests/jit_loop_invariant_test.c",
    "tests/ra_test.c",
    "tests/test.c",
    "tests/uniform_ring_test.c",
};

const slide_files = [_][]const u8{
    "slides/blend.c",
    "slides/color.c",
    "slides/coverage.c",
    "slides/gradient.c",
    "slides/overview.c",
    "slides/sdf.c",
    "slides/slides.c",
    "slides/slug.c",
};

const clang_warn = [_][]const u8{
    "-Weverything",
    "-Wno-declaration-after-statement",
    "-Wno-disabled-macro-expansion",
    "-Wno-implicit-void-ptr-cast",
    "-Wno-nrvo",
    "-Wno-poison-system-directories",
    "-Wno-pre-c11-compat",
    "-Wno-switch-default",
    "-Wno-unsafe-buffer-usage",
};

const gcc_warn = [_][]const u8{
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wno-unknown-pragmas",
};

const sysroot = "/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk";
const llvm_bin = "/opt/homebrew/opt/llvm/bin";

fn fileFlags(path: []const u8) []const []const u8 {
    if (std.mem.eql(u8, path, "src/vulkan.c")) {
        return &.{ "-isystem", "/opt/homebrew/Cellar/molten-vk/1.4.1/libexec/include" };
    }
    if (std.mem.eql(u8, path, "src/wgpu.c")) {
        return &.{ "-isystem", "/opt/homebrew/include" };
    }
    if (std.mem.eql(u8, path, "tests/golden_test.c")) {
        return &.{"-Wno-cast-align"};
    }
    if (std.mem.eql(u8, path, "tools/stb_truetype.c") or
        std.mem.eql(u8, path, "tools/stb_image_write.c"))
    {
        return &.{ "-Wno-everything", "-fno-sanitize=all" };
    }
    if (std.mem.eql(u8, path, "tools/demo.c")) {
        return &.{ "-I/opt/homebrew/include", "-Wno-cast-align" };
    }
    return &.{};
}

const Config = struct {
    name: []const u8,
    cc: []const []const u8,
    warn: []const []const u8 = &clang_warn,
    profinfo: bool = true,
    extra_compile: []const []const u8 = &.{},
    ldflags: []const []const u8 = &.{},
    test_ldflags: []const []const u8 = &.{},
    has_demo: bool = false,
    demo_ldflags: []const []const u8 = &.{},
    codesign: bool = true,
    exec_prefix: []const []const u8 = &.{},
    has_dump: bool = false,
    test_timeout_secs: u32 = 60,
};

const test_shards = 10;
const dump_shards = 10;

fn baseFlags(b: *std.Build, cfg: Config) []const []const u8 {
    var list = std.ArrayList([]const u8).initCapacity(b.allocator, 32) catch @panic("OOM");
    list.appendSlice(b.allocator, &.{ "-g", "-O1", "-std=c11", "-Werror" }) catch @panic("OOM");
    if (cfg.profinfo) list.append(b.allocator, "-fdebug-info-for-profiling") catch @panic("OOM");
    list.appendSlice(b.allocator, cfg.warn) catch @panic("OOM");
    list.appendSlice(b.allocator, cfg.extra_compile) catch @panic("OOM");
    return list.items;
}

fn objBasename(b: *std.Build, src: []const u8) []const u8 {
    const stem = std.fs.path.stem(src);
    const dir = std.fs.path.dirname(src) orelse "";
    const dir_us = std.mem.replaceOwned(u8, b.allocator, dir, "/", "_") catch @panic("OOM");
    return b.fmt("{s}_{s}.o", .{ dir_us, stem });
}

fn compile(
    b: *std.Build,
    cfg: Config,
    base: []const []const u8,
    src: []const u8,
) std.Build.LazyPath {
    const cc = b.addSystemCommand(cfg.cc);
    cc.addArgs(base);
    cc.addArgs(fileFlags(src));
    cc.addArg("-c");
    cc.addFileArg(b.path(src));
    cc.addArg("-o");
    const obj = cc.addOutputFileArg(objBasename(b, src));
    cc.addArg("-MD");
    cc.addArg("-MF");
    _ = cc.addDepFileOutputArg(b.fmt("{s}.d", .{objBasename(b, src)}));
    return obj;
}

fn linkAndSign(
    b: *std.Build,
    cfg: Config,
    objs: []const std.Build.LazyPath,
    ldflags: []const []const u8,
    bin_name: []const u8,
) std.Build.LazyPath {
    const ent = b.pathFromRoot("build/entitlements.plist");

    const script = if (cfg.codesign) b.fmt(
        \\set -e
        \\out="$1"; shift
        \\"$@" -o "$out"
        \\codesign -s - -f --entitlements '{s}' "$out" 2>/dev/null
        \\dsymutil "$out" 2>/dev/null || true
    , .{ent}) else
        \\set -e
        \\out="$1"; shift
        \\"$@" -o "$out"
    ;

    const cmd = b.addSystemCommand(&.{ "sh", "-c", script, "umbra-link" });
    const out = cmd.addOutputFileArg(bin_name);
    cmd.addArgs(cfg.cc);
    for (objs) |o| cmd.addFileArg(o);
    cmd.addArgs(ldflags);
    return out;
}

fn compileSources(
    b: *std.Build,
    cfg: Config,
    base: []const []const u8,
    files: []const []const u8,
) []std.Build.LazyPath {
    const out = b.allocator.alloc(std.Build.LazyPath, files.len) catch @panic("OOM");
    for (files, 0..) |f, i| out[i] = compile(b, cfg, base, f);
    return out;
}

fn concatLazy(
    b: *std.Build,
    parts: []const []const std.Build.LazyPath,
) []std.Build.LazyPath {
    var total: usize = 0;
    for (parts) |p| total += p.len;
    const out = b.allocator.alloc(std.Build.LazyPath, total) catch @panic("OOM");
    var i: usize = 0;
    for (parts) |p| {
        for (p) |x| {
            out[i] = x;
            i += 1;
        }
    }
    return out;
}

const Outputs = struct {
    test_bin: std.Build.LazyPath,
    bench_bin: std.Build.LazyPath,
    dump_bin: std.Build.LazyPath,
    demo_bin: ?std.Build.LazyPath,
};

fn buildConfig(b: *std.Build, cfg: Config) Outputs {
    const base = baseFlags(b, cfg);

    const src_objs = compileSources(b, cfg, base, &src_files);
    const test_objs = compileSources(b, cfg, base, &test_files);
    const slide_objs = compileSources(b, cfg, base, &slide_files);
    const stb_truetype = compile(b, cfg, base, "tools/stb_truetype.c");
    const stb_image_write = compile(b, cfg, base, "tools/stb_image_write.c");
    const test_main = compile(b, cfg, base, "tools/test.c");
    const bench_main = compile(b, cfg, base, "tools/bench.c");
    const dump_main = compile(b, cfg, base, "tools/dump.c");
    const wgpu_intercept = compile(b, cfg, base, "tools/wgpu_msl_intercept.c");

    const test_objs_all = concatLazy(b, &.{
        &.{test_main}, test_objs, src_objs, &.{stb_truetype}, slide_objs,
    });
    const bench_objs_all = concatLazy(b, &.{
        &.{bench_main}, src_objs, &.{stb_truetype}, slide_objs,
    });
    const dump_objs_all = concatLazy(b, &.{
        &.{dump_main}, src_objs, &.{stb_truetype}, &.{stb_image_write}, &.{wgpu_intercept}, slide_objs,
    });

    const test_bin = linkAndSign(b, cfg, test_objs_all, cfg.test_ldflags, "test");
    const bench_bin = linkAndSign(b, cfg, bench_objs_all, cfg.ldflags, "bench");
    const dump_bin = linkAndSign(b, cfg, dump_objs_all, cfg.ldflags, "dump");

    var demo_bin: ?std.Build.LazyPath = null;
    if (cfg.has_demo) {
        const demo_main = compile(b, cfg, base, "tools/demo.c");
        const work_group = compile(b, cfg, base, "tools/work_group.c");
        const demo_objs = concatLazy(b, &.{
            &.{demo_main}, src_objs, &.{stb_truetype}, &.{work_group}, &.{stb_image_write}, slide_objs,
        });
        demo_bin = linkAndSign(b, cfg, demo_objs, cfg.demo_ldflags, "demo");
    }

    return .{
        .test_bin = test_bin,
        .bench_bin = bench_bin,
        .dump_bin = dump_bin,
        .demo_bin = demo_bin,
    };
}

fn runShard(
    b: *std.Build,
    cfg: Config,
    bin: std.Build.LazyPath,
    shard: u32,
    total: u32,
    log_basename: []const u8,
) *std.Build.Step {
    const log_name = b.fmt("{s}_{d}.log", .{ log_basename, shard });

    const run = b.addSystemCommand(&.{ "perl", "-e", b.fmt("alarm {d}; exec @ARGV", .{cfg.test_timeout_secs}) });
    for (cfg.exec_prefix) |p| run.addArg(p);
    run.addFileArg(bin);
    run.addArgs(&.{
        "--shards", b.fmt("{d}", .{total}),
        "--shard",  b.fmt("{d}", .{shard}),
    });

    const stdout = run.captureStdOut(.{ .basename = log_name });
    const inst = b.addInstallFileWithDir(stdout, .{ .custom = cfg.name }, log_name);
    return &inst.step;
}

fn registerConfig(b: *std.Build, cfg: Config, out: Outputs) void {
    const cfg_step = b.step(cfg.name, b.fmt("Build {s} artifacts", .{cfg.name}));

    const test_inst = b.addInstallFileWithDir(out.test_bin, .{ .custom = cfg.name }, "test");
    const bench_inst = b.addInstallFileWithDir(out.bench_bin, .{ .custom = cfg.name }, "bench");
    const dump_inst = b.addInstallFileWithDir(out.dump_bin, .{ .custom = cfg.name }, "dump");
    cfg_step.dependOn(&test_inst.step);
    cfg_step.dependOn(&bench_inst.step);
    cfg_step.dependOn(&dump_inst.step);
    if (out.demo_bin) |d| {
        const demo_inst = b.addInstallFileWithDir(d, .{ .custom = cfg.name }, "demo");
        cfg_step.dependOn(&demo_inst.step);

        const demo_step = b.step(b.fmt("{s}-demo", .{cfg.name}), b.fmt("Build {s} demo", .{cfg.name}));
        demo_step.dependOn(&demo_inst.step);
    }
    b.getInstallStep().dependOn(cfg_step);

    const test_step = b.step(b.fmt("{s}-test", .{cfg.name}), b.fmt("Run {s} tests", .{cfg.name}));
    var s: u32 = 0;
    while (s < test_shards) : (s += 1) {
        const log = runShard(b, cfg, out.test_bin, s, test_shards, "test");
        test_step.dependOn(log);
    }

    if (cfg.has_dump) {
        const dump_step = b.step(b.fmt("{s}-dump", .{cfg.name}), b.fmt("Run {s} dumps", .{cfg.name}));
        var d: u32 = 0;
        while (d < dump_shards) : (d += 1) {
            const log = runShard(b, cfg, out.dump_bin, d, dump_shards, "dump");
            dump_step.dependOn(log);
        }
    }
}

fn join(
    b: *std.Build,
    parts: []const []const []const u8,
) []const []const u8 {
    var total: usize = 0;
    for (parts) |p| total += p.len;
    const out = b.allocator.alloc([]const u8, total) catch @panic("OOM");
    var i: usize = 0;
    for (parts) |p| {
        for (p) |x| {
            out[i] = x;
            i += 1;
        }
    }
    return out;
}

pub fn build(b: *std.Build) void {
    const clang = [_][]const u8{ "/usr/bin/clang", "-fcolor-diagnostics" };
    const llvm_clang_path = b.pathJoin(&.{ llvm_bin, "clang" });

    const mac_ldflags = [_][]const u8{
        "-framework",          "Metal",
        "-framework",          "Foundation",
        "-L/opt/homebrew/lib", "-lMoltenVK",
        "-lwgpu_native",
    };
    const mac_demo_ldflags = [_][]const u8{
        "-framework",          "Metal",
        "-framework",          "Foundation",
        "-L/opt/homebrew/lib", "-lSDL3",
        "-lMoltenVK",          "-lwgpu_native",
    };

    const host_cfg = Config{
        .name = "host",
        .cc = &clang,
        .ldflags = &mac_ldflags,
        .test_ldflags = join(b, &.{ &mac_ldflags, &.{"-lm"} }),
        .has_demo = true,
        .demo_ldflags = &mac_demo_ldflags,
    };
    registerConfig(b, host_cfg, buildConfig(b, host_cfg));

    const sanitize = [_][]const u8{
        "-fsanitize=address,integer,undefined",
        "-fno-sanitize-recover=all",
    };
    const coverage = [_][]const u8{
        "-fprofile-instr-generate",
        "-fcoverage-mapping",
    };
    const xsan_extra_compile = join(b, &.{ &sanitize, &coverage });
    const xsan_link = join(b, &.{ xsan_extra_compile, &mac_ldflags });
    const xsan_demo_link = join(b, &.{ xsan_extra_compile, &mac_demo_ldflags });

    const xsan_cfg = Config{
        .name = "xsan",
        .cc = &clang,
        .extra_compile = xsan_extra_compile,
        .ldflags = xsan_link,
        .test_ldflags = join(b, &.{ xsan_link, &.{"-lm"} }),
        .has_demo = true,
        .demo_ldflags = xsan_demo_link,
        .exec_prefix = &.{
            "env",
            "UBSAN_OPTIONS=print_stacktrace=1",
            "LLVM_PROFILE_FILE=zig-out/xsan/%p.profraw",
        },
        .has_dump = true,
    };
    registerConfig(b, xsan_cfg, buildConfig(b, xsan_cfg));

    const tsan_compile = [_][]const u8{"-fsanitize=thread"};
    const tsan_link = join(b, &.{ &tsan_compile, &mac_ldflags });

    const tsan_cfg = Config{
        .name = "tsan",
        .cc = &clang,
        .extra_compile = &tsan_compile,
        .ldflags = tsan_link,
        .test_ldflags = join(b, &.{ tsan_link, &.{"-lm"} }),
        .exec_prefix = &.{
            "env",
            "TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1",
        },
    };
    registerConfig(b, tsan_cfg, buildConfig(b, tsan_cfg));

    const x86_compile = [_][]const u8{
        "-momit-leaf-frame-pointer",
        "-target",
        "x86_64-apple-macos13",
        "-isysroot",
        sysroot,
        "-march=x86-64-v3",
        "-mno-vzeroupper",
    };
    const x86_ldflags = [_][]const u8{
        "-target",    "x86_64-apple-macos13",
        "-isysroot",  sysroot,
        "-framework", "Metal",
        "-framework", "Foundation",
    };
    const x86_cfg = Config{
        .name = "x86",
        .cc = &clang,
        .extra_compile = &x86_compile,
        .ldflags = &x86_ldflags,
        .test_ldflags = join(b, &.{ &x86_ldflags, &.{"-lm"} }),
        .exec_prefix = &.{ "arch", "-x86_64" },
    };
    registerConfig(b, x86_cfg, buildConfig(b, x86_cfg));

    const x86_xsan_compile = join(b, &.{ &x86_compile, &sanitize, &coverage });
    const x86_xsan_link_base = [_][]const u8{
        "-target",    "x86_64-apple-macos13",
        "-isysroot",  sysroot,
        "-framework", "Metal",
        "-framework", "Foundation",
        "-Wl,-w",
    };
    const x86_xsan_link = join(b, &.{ &sanitize, &coverage, &x86_xsan_link_base });
    const x86_xsan_cfg = Config{
        .name = "x86_xsan",
        .cc = &clang,
        .extra_compile = x86_xsan_compile,
        .ldflags = x86_xsan_link,
        .test_ldflags = join(b, &.{ x86_xsan_link, &.{"-lm"} }),
        .exec_prefix = &.{
            "env",
            "UBSAN_OPTIONS=print_stacktrace=1",
            "LLVM_PROFILE_FILE=zig-out/x86_xsan/%p.profraw",
            "arch",
            "-x86_64",
        },
        .has_dump = true,
    };
    registerConfig(b, x86_xsan_cfg, buildConfig(b, x86_xsan_cfg));

    const wasm_cc = [_][]const u8{
        llvm_clang_path,             "-fcolor-diagnostics",
        "-target",                   "wasm32-wasip1",
        "-msimd128",                 "-B",
        "/opt/homebrew/opt/lld/bin", "-D_WASI_EMULATED_MMAN",
    };
    const wasm_ldflags = [_][]const u8{"-lwasi-emulated-mman"};
    const wasm_cfg = Config{
        .name = "wasm",
        .cc = &wasm_cc,
        .ldflags = &wasm_ldflags,
        .test_ldflags = &wasm_ldflags,
        .codesign = false,
        .exec_prefix = &.{ "env", "WASMTIME_BACKTRACE_DETAILS=1", "wasmtime" },
    };
    registerConfig(b, wasm_cfg, buildConfig(b, wasm_cfg));

    const gcc_cc = [_][]const u8{"/opt/homebrew/bin/gcc-15"};
    const gcc_ldflags = [_][]const u8{
        "-lm",                 "-lobjc",
        "-framework",          "Metal",
        "-framework",          "Foundation",
        "-L/opt/homebrew/lib", "-lMoltenVK",
        "-lwgpu_native",
    };
    const gcc_cfg = Config{
        .name = "gcc",
        .cc = &gcc_cc,
        .warn = &gcc_warn,
        .profinfo = false,
        .ldflags = &gcc_ldflags,
        .test_ldflags = &gcc_ldflags,
    };
    registerConfig(b, gcc_cfg, buildConfig(b, gcc_cfg));

    addCoverage(b);
    addTest(b);
}

fn addTest(b: *std.Build) void {
    const test_step = b.step("test", "Build all configs, run all tests/dumps, and generate coverage report");
    const cfg_names = [_][]const u8{ "host", "xsan", "tsan", "x86", "x86_xsan", "wasm", "gcc" };
    for (cfg_names) |n| {
        const t = b.top_level_steps.get(b.fmt("{s}-test", .{n})).?;
        test_step.dependOn(&t.step);
    }
    const coverage_step = b.top_level_steps.get("coverage").?;
    const report_step = b.top_level_steps.get("report").?;
    test_step.dependOn(&coverage_step.step);
    test_step.dependOn(&report_step.step);
}

fn addCoverage(b: *std.Build) void {
    const xsan_step = b.top_level_steps.get("xsan").?;
    const x86_step = b.top_level_steps.get("x86_xsan").?;
    const xsan_test_step = b.top_level_steps.get("xsan-test").?;
    const xsan_dump_step = b.top_level_steps.get("xsan-dump").?;
    const x86_test_step = b.top_level_steps.get("x86_xsan-test").?;
    const x86_dump_step = b.top_level_steps.get("x86_xsan-dump").?;

    const xsan_dir = b.getInstallPath(.{ .custom = "xsan" }, "");
    const x86_dir = b.getInstallPath(.{ .custom = "x86_xsan" }, "");

    const profmerge_script = b.fmt(
        \\set -e
        \\out="$1"; shift
        \\{s}/llvm-profdata merge -sparse {s}/*.profraw {s}/*.profraw -o "$out"
    , .{ llvm_bin, xsan_dir, x86_dir });
    const profmerge = b.addSystemCommand(&.{ "sh", "-c", profmerge_script, "umbra-profmerge" });
    const profdata = profmerge.addOutputFileArg("coverage.profdata");
    profmerge.step.dependOn(&xsan_step.step);
    profmerge.step.dependOn(&x86_step.step);
    profmerge.step.dependOn(&xsan_test_step.step);
    profmerge.step.dependOn(&xsan_dump_step.step);
    profmerge.step.dependOn(&x86_test_step.step);
    profmerge.step.dependOn(&x86_dump_step.step);

    const objects_args = [_][]const u8{
        b.fmt("{s}/test", .{xsan_dir}),
        b.fmt("-object={s}/dump", .{xsan_dir}),
        b.fmt("-object={s}/test", .{x86_dir}),
        b.fmt("-object={s}/dump", .{x86_dir}),
    };

    const report_script = b.fmt(
        \\set -e
        \\profdata="$1"; out="$2"; shift 2
        \\{s}/llvm-cov report --show-branch-summary --instr-profile="$profdata" "$@" src/ include/ >"$out"
    , .{llvm_bin});
    const report = b.addSystemCommand(&.{ "sh", "-c", report_script, "umbra-covreport" });
    report.addFileArg(profdata);
    const report_out = report.addOutputFileArg("coverage.txt");
    report.addArgs(&objects_args);
    report.step.dependOn(&xsan_step.step);
    report.step.dependOn(&x86_step.step);

    const report_inst = b.addInstallFileWithDir(report_out, .{ .custom = "xsan" }, "coverage.txt");
    const profdata_inst = b.addInstallFileWithDir(profdata, .{ .custom = "xsan" }, "coverage.profdata");

    const coverage_step = b.step("coverage", "Build merged coverage report (xsan+x86_xsan)");
    coverage_step.dependOn(&report_inst.step);
    coverage_step.dependOn(&profdata_inst.step);

    const html_dir = b.getInstallPath(.{ .custom = "xsan" }, "report");
    const show_script = b.fmt(
        \\set -e
        \\profdata="$1"; out="$2"; dir="$3"; shift 3
        \\rm -rf "$dir"
        \\{s}/llvm-cov show --show-branches=count --instr-profile="$profdata" "$@" --format=html --output-dir="$dir" src/ include/ 2>/dev/null
        \\echo "Coverage report: file://$dir/index.html" >"$out"
    , .{llvm_bin});
    const show = b.addSystemCommand(&.{ "sh", "-c", show_script, "umbra-covshow" });
    show.addFileArg(profdata);
    const show_out = show.addOutputFileArg("report.txt");
    show.addArg(html_dir);
    show.addArgs(&objects_args);
    show.has_side_effects = true;
    show.step.dependOn(&xsan_step.step);
    show.step.dependOn(&x86_step.step);

    const show_inst = b.addInstallFileWithDir(show_out, .{ .custom = "xsan" }, "report.txt");
    const report_html_step = b.step("report", "Build coverage HTML report");
    report_html_step.dependOn(&show_inst.step);
}
