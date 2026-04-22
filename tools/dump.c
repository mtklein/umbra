#define _POSIX_C_SOURCE 200809L
#include "../include/umbra_draw.h"
#include "../slides/slide.h"
#include "../src/count.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include "../third_party/stb/stb_image_write.h"
#pragma clang diagnostic pop

static FILE* atomic_open(char const *path) {
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%s~", path);
    return fopen(tmp, "w");
}
static void atomic_close(FILE *f, char const *path) {
    fclose(f);
    char tmp[256];
    snprintf(tmp, sizeof tmp, "%s~", path);
    rename(tmp, path);
}
static void atomic_write_hdr(char const *path, int w, int h, int comp, float const *data) {
    char tmp[520];
    snprintf(tmp, sizeof tmp, "%s~", path);
    if (stbi_write_hdr(tmp, w, h, comp, data)) {
        rename(tmp, path);
    }
}

static struct umbra_builder* build_srcover(void) {
    static struct umbra_buf src    = {0},
                            dr_buf = {0}, dg_buf = {0}, db_buf = {0}, da_buf = {0};
    struct umbra_builder *b = umbra_builder();

    umbra_ptr const sp  = umbra_early_bind_buf(b, &src),
                    drp = umbra_early_bind_buf(b, &dr_buf),
                    dgp = umbra_early_bind_buf(b, &dg_buf),
                    dbp = umbra_early_bind_buf(b, &db_buf),
                    dap = umbra_early_bind_buf(b, &da_buf);

    umbra_val32 const px   = umbra_load_32(b, sp),
                      mask = umbra_imm_i32(b, 0xFF);
    umbra_val32 const rgba[4] = {
        umbra_and_32(b, px, mask),
        umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 8)), mask),
        umbra_and_32(b, umbra_shr_u32(b, px, umbra_imm_i32(b, 16)), mask),
        umbra_shr_u32(b, px, umbra_imm_i32(b, 24)),
    };

    umbra_val32 const inv255 = umbra_imm_f32(b, 1.0f / 255.0f),
                      one    = umbra_imm_f32(b, 1.0f);
    umbra_val32 const sr = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[0]), inv255),
                      sg = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[1]), inv255),
                      sb = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[2]), inv255),
                      sa = umbra_mul_f32(b, umbra_f32_from_i32(b, rgba[3]), inv255),
                      dr = umbra_f32_from_f16(b, umbra_load_16(b, drp)),
                      dg = umbra_f32_from_f16(b, umbra_load_16(b, dgp)),
                      db = umbra_f32_from_f16(b, umbra_load_16(b, dbp)),
                      da = umbra_f32_from_f16(b, umbra_load_16(b, dap));
    umbra_val32 const inv_a = umbra_sub_f32(b, one, sa),
                      rout  = umbra_add_f32(b, sr, umbra_mul_f32(b, dr, inv_a)),
                      gout  = umbra_add_f32(b, sg, umbra_mul_f32(b, dg, inv_a)),
                      bout  = umbra_add_f32(b, sb, umbra_mul_f32(b, db, inv_a)),
                      aout  = umbra_add_f32(b, sa, umbra_mul_f32(b, da, inv_a));
    umbra_store_16(b, drp, umbra_f16_from_f32(b, rout));
    umbra_store_16(b, dgp, umbra_f16_from_f32(b, gout));
    umbra_store_16(b, dbp, umbra_f16_from_f32(b, bout));
    umbra_store_16(b, dap, umbra_f16_from_f32(b, aout));
    return b;
}

#if defined(__aarch64__)
#define JIT_EXT "arm64"
#elif defined(__AVX2__)
#define JIT_EXT "avx2"
#endif

static char mvk_dump_dir[64];

static void clear_dir(char const *path) {
    DIR *d = opendir(path);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            if (e->d_name[0] != '.') {
                char fp[1280];
                snprintf(fp, sizeof fp, "%s/%s", path, e->d_name);
                unlink(fp);
            }
        }
        closedir(d);
    }
}

static void grab_mvk_msl(char const *dir) {
    DIR *d = opendir(mvk_dump_dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            size_t n = strlen(e->d_name);
            if (n > 6 && strcmp(e->d_name + n - 6, ".metal") == 0) {
                char src[1280], dst[256];
                snprintf(src, sizeof src, "%s/%s", mvk_dump_dir, e->d_name);
                snprintf(dst, sizeof dst, "%s/vulkan.msl", dir);
                FILE *in = fopen(src, "r");
                if (in) {
                    FILE *out = atomic_open(dst);
                    if (out) {
                        char buf[4096];
                        size_t r;
                        while ((r = fread(buf, 1, sizeof buf, in)) > 0) {
                            fwrite(buf, 1, r, out);
                        }
                        atomic_close(out, dst);
                    }
                    fclose(in);
                }
                break;
            }
        }
        closedir(d);
    }
}

struct dump_backends {
    struct umbra_backend *be[4];
    char const           *ext[4];
    int                   n;
    int                   vulkan_idx;
};

static struct dump_backends dump_backends_init(void) {
    struct dump_backends db = {.vulkan_idx = -1};
    int i = 0;
#ifdef JIT_EXT
    db.be[i] = umbra_backend_jit();    db.ext[i] = JIT_EXT ".txt"; i++;
#endif
    db.be[i] = umbra_backend_metal();  db.ext[i] = "metal.msl";    i++;
    db.be[i] = umbra_backend_vulkan(); db.ext[i] = "vulkan.spirv"; db.vulkan_idx = i; i++;
    db.be[i] = umbra_backend_wgpu();   db.ext[i] = "wgpu.spirv";   i++;
    db.n = i;
    return db;
}

static void dump_backends_free(struct dump_backends *db) {
    for (int i = 0; i < db->n; i++) {
        umbra_backend_free(db->be[i]);
    }
}

static void dump_builder(struct dump_backends *db,
                         char const *dir, struct umbra_builder *b);

static void dump_ir(struct dump_backends *db,
                    char const *dir, struct umbra_flat_ir *ir) {
    char p[256];
    {
        snprintf(p, sizeof p, "%s/ir.txt", dir);
        FILE *f = atomic_open(p);
        umbra_flat_ir_dump(ir, f);
        atomic_close(f, p);
    }

    for (int i = 0; i < db->n; i++) {
        if (db->be[i]) {
            if (i == db->vulkan_idx) {
                clear_dir(mvk_dump_dir);
            }

            struct umbra_program *prog = db->be[i]->compile(db->be[i], ir);
            if (prog) {
                snprintf(p, sizeof p, "%s/%s", dir, db->ext[i]);
                FILE *f = atomic_open(p);
                if (prog->dump) { prog->dump(prog, f); }
                atomic_close(f, p);
                umbra_program_free(prog);

                if (i == db->vulkan_idx) {
                    grab_mvk_msl(dir);
                }
            }
        }
    }
}

static void dump_builder(struct dump_backends *db,
                         char const *dir, struct umbra_builder *b) {
    char p[256];
    snprintf(p, sizeof p, "%s/builder.txt", dir);
    FILE *f = atomic_open(p);
    umbra_builder_dump(b, f);
    atomic_close(f, p);

    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);
    dump_ir(db, dir, ir);
    umbra_flat_ir_free(ir);
}

static void slugify(char const *title, char *out, size_t sz) {
    int n = snprintf(out, sz, "dumps/");
    for (int i = 0; title[i] && n < (int)sz - 1; i++) {
        char c = title[i];
        if (c >= 'A' && c <= 'Z') {
            out[n++] = (char)(c + 32);
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out[n++] = c;
        } else if (n > 0 && out[n - 1] != '_') {
            out[n++] = '_';
        }
    }
    while (n > 0 && out[n - 1] == '_') { n--; }
    out[n] = '\0';
}

enum { RW = 1024, RH = 768 };

static void fp16p_to_float(float *out, void const *pixbuf) {
    __fp16 const *src = pixbuf;
    int const ps = RW * RH;
    for (int i = 0; i < ps; i++) {
        out[4 * i + 0] = (float)src[i];
        out[4 * i + 1] = (float)src[i + ps];
        out[4 * i + 2] = (float)src[i + 2 * ps];
        out[4 * i + 3] = (float)src[i + 3 * ps];
    }
}

static void render_hdr_at(struct slide *s, struct slide_runtime *rt,
                          struct slide_bg *bg, struct umbra_backend *be,
                          double secs, char const *dir, char const *suffix) {
    struct umbra_fmt const fmt = umbra_fmt_fp16_planar;
    size_t const pixbuf_sz = (size_t)RW * RH * fmt.bpp * (size_t)fmt.planes;
    void *pixbuf = calloc(1, pixbuf_sz);

    struct umbra_buf const dst = {
        .ptr=pixbuf, .count=RW * RH * fmt.planes, .stride=RW,
    };
    if (rt) {
        slide_bg_draw(bg, s->bg, 0, 0, RW, RH, dst);
        slide_runtime_animate(s, secs);
        slide_runtime_draw(rt, dst, 0, 0, RW, RH);
    } else {
        s->draw(s, secs, 0, 0, RW, RH, dst);
    }
    be->flush(be);

    float *fdata = malloc((size_t)(RW * RH) * 4 * sizeof(float));
    fp16p_to_float(fdata, pixbuf);
    free(pixbuf);

    char const *base = strrchr(dir, '/');
    base = base ? base + 1 : dir;
    char p[512];
    snprintf(p, sizeof p, "%s/%s%s.hdr", dir, base, suffix);
    atomic_write_hdr(p, RW, RH, 4, fdata);
    free(fdata);
}

static void render_hdr(char const *dir, int slide_idx, struct umbra_backend *be) {
    struct slide *s = slide_get(slide_idx);
    struct umbra_fmt const fmt = umbra_fmt_fp16_planar;

    _Bool const leaf = s->build_draw || s->build_sdf_draw;
    struct slide_runtime *rt = NULL;
    struct slide_bg      *bg = NULL;
    if (leaf) {
        rt = slide_runtime(s, RW, RH, be, fmt, NULL);
        bg = slide_bg(be, fmt);
    } else {
        s->prepare(s, be, fmt);
    }

    render_hdr_at(s, rt, bg, be, 0.0, dir, "");
    if (s->animate) {
        render_hdr_at(s, rt, bg, be, 1.0, dir, "_1sec");
    }

    slide_bg_free(bg);
    slide_runtime_free(rt);
}

int main(int argc, char *argv[]) {
    int shards = 1, shard = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--shards") && i + 1 < argc) {
            shards = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--shard") && i + 1 < argc) {
            shard = atoi(argv[++i]);
        }
    }

    snprintf(mvk_dump_dir, sizeof mvk_dump_dir,
             "/tmp/umbra_mvk_dump.%d", shard);
    setenv("MVK_CONFIG_SHADER_DUMP_DIR", mvk_dump_dir, 1);
    mkdir(mvk_dump_dir, 0755);

    struct dump_backends db = dump_backends_init();
    int unit = 0;

    if (unit++ % shards == shard) {
        mkdir("dumps/srcover", 0755);
        mkdir("dumps/srcover/0", 0755);
        dump_builder(&db, "dumps/srcover/0", build_srcover());
    }

    slides_init(RW, RH);

    struct umbra_backend *be = umbra_backend_jit();
    if (!be) { be = umbra_backend_interp(); }

    for (int i = 0; i < slide_count(); i++) {
        struct slide *s = slide_get(i);
        char dir[128];
        slugify(s->title, dir, sizeof dir);
        mkdir(dir, 0755);

        int const slide_hdr_unit = unit;
        _Bool const has_draw = s->build_draw || s->build_sdf_draw;

        if (has_draw) {
            if (unit % shards == shard) {
                struct umbra_builder *draw =
                    slide_draw_builder(s, NULL, umbra_fmt_fp16, NULL);
                char sub[256];
                snprintf(sub, sizeof sub, "%s/0", dir);
                mkdir(sub, 0755);
                dump_builder(&db, sub, draw);
            }
            unit++;
        }
        if (s->build_sdf_draw) {
            if (unit % shards == shard) {
                struct umbra_builder *bb = umbra_builder();
                struct umbra_sdf_bounds_program *bounds =
                    umbra_sdf_bounds_program(bb, NULL, s->sdf_fn, s->sdf_ctx);
                char sub[256];
                snprintf(sub, sizeof sub, "%s/1", dir);
                mkdir(sub, 0755);
                dump_builder(&db, sub, bb);
                umbra_sdf_bounds_program_free(bounds);
            }
            unit++;
        }

        if (slide_hdr_unit % shards == shard) {
            render_hdr(dir, i, be);
        }
    }

    slides_cleanup();
    umbra_backend_free(be);
    dump_backends_free(&db);
    return 0;
}
