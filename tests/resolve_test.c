#include "test.h"
#include "../include/umbra_draw.h"
#include "../src/flat_ir.h"
#include <stdint.h>

TEST(resolve_simple_no_joins) {
    struct umbra_buf dummy = {0};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst = umbra_bind_buf(b, &dummy);
    umbra_store_32(b, dst, umbra_imm_i32(b, 42));
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_flat_ir *r = flat_ir_resolve(ir, JOIN_PREFER_IMM);
    r->insts == ir->insts here;
    r->preamble == ir->preamble here;
    r->loop_begin == -1 here;
    r->loop_end == -1 here;

    for (int i = 0; i < r->insts; i++) {
        r->inst[i].op == ir->inst[i].op here;
        r->inst[i].x.id == ir->inst[i].x.id here;
        r->inst[i].y.id == ir->inst[i].y.id here;
    }

    umbra_flat_ir_free(r);
    umbra_flat_ir_free(ir);
}

TEST(resolve_with_loop) {
    int32_t uni[1] = {0};
    struct umbra_buf dummy = {0};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const u   = umbra_bind_uniforms(b, uni, 1);
    umbra_ptr const dst = umbra_bind_buf(b, &dummy);
    umbra_var32 acc = umbra_declare_var32(b, umbra_imm_i32(b, 0));
    umbra_val32 trip = umbra_uniform_32(b, u, 0);
    umbra_val32 j = umbra_loop(b, trip); {
        umbra_val32 prev = umbra_load_var32(b, acc);
        umbra_store_var32(b, acc, umbra_add_i32(b, prev, j));
    } umbra_end_loop(b);
    umbra_store_32(b, dst, umbra_load_var32(b, acc));
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_flat_ir *r = flat_ir_resolve(ir, JOIN_PREFER_IMM);
    r->loop_begin >= 0 here;
    r->loop_end > r->loop_begin here;
    r->preamble <= r->loop_begin here;

    r->inst[r->loop_begin].op == op_loop_begin here;
    r->inst[r->loop_end].op == op_loop_end here;

    for (int i = 0; i < r->insts; i++) {
        r->inst[i].x.id >= 0 here;
        r->inst[i].x.id < r->insts here;
        r->inst[i].y.id >= 0 here;
        r->inst[i].y.id < r->insts here;
    }

    umbra_flat_ir_free(r);
    umbra_flat_ir_free(ir);
}

TEST(resolve_eliminates_joins) {
    struct umbra_buf src = {0}, dst = {0};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const sp = umbra_bind_buf(b, &src),
                      dp = umbra_bind_buf(b, &dst);
    umbra_val32 v = umbra_gather_32(b, sp, umbra_x(b));
    umbra_val32 r = umbra_add_f32(b, v, umbra_imm_f32(b, 2.0f));
    umbra_store_32(b, dp, r);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    _Bool has_join = 0;
    for (int i = 0; i < ir->insts; i++) {
        if (ir->inst[i].op == op_join) { has_join = 1; }
    }
    has_join here;

    struct umbra_flat_ir *keep_x = flat_ir_resolve(ir, JOIN_KEEP_X);
    struct umbra_flat_ir *prefer = flat_ir_resolve(ir, JOIN_PREFER_IMM);

    for (int i = 0; i < keep_x->insts; i++) {
        keep_x->inst[i].op != op_join here;
    }
    for (int i = 0; i < prefer->insts; i++) {
        prefer->inst[i].op != op_join here;
    }

    umbra_flat_ir_free(keep_x);
    umbra_flat_ir_free(prefer);
    umbra_flat_ir_free(ir);
}

TEST(resolve_compaction_renumbers) {
    struct umbra_buf dummy = {0};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const dst = umbra_bind_buf(b, &dummy);
    umbra_val32 a = umbra_imm_i32(b, 10);
    umbra_val32 unused = umbra_imm_i32(b, 99);
    (void)unused;
    umbra_store_32(b, dst, a);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_flat_ir *r = flat_ir_resolve(ir, JOIN_KEEP_X);

    r->insts <= ir->insts here;

    for (int i = 0; i < r->insts; i++) {
        r->inst[i].x.id < r->insts here;
        r->inst[i].y.id < r->insts here;
        if (op_is_store(r->inst[i].op)) {
            r->inst[i].y.id < i here;
        }
    }

    umbra_flat_ir_free(r);
    umbra_flat_ir_free(ir);
}

TEST(resolve_preserves_channels) {
    struct umbra_buf src = {0}, dst = {0};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr sp = umbra_bind_buf(b, &src),
                dp = umbra_bind_buf(b, &dst);
    umbra_color_val32 c = umbra_fmt_fp16.load(b, sp);
    umbra_fmt_fp16.store(b, dp, c);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_flat_ir *r = flat_ir_resolve(ir, JOIN_PREFER_IMM);

    for (int i = 0; i < r->insts; i++) {
        r->inst[i].x.chan == ir->inst[i].x.chan here;
        r->inst[i].y.chan == ir->inst[i].y.chan here;
        r->inst[i].z.chan == ir->inst[i].z.chan here;
        r->inst[i].w.chan == ir->inst[i].w.chan here;
    }

    umbra_flat_ir_free(r);
    umbra_flat_ir_free(ir);
}

TEST(resolve_preserves_ptr) {
    struct umbra_buf src = {0}, dst = {0};
    struct umbra_builder *b = umbra_builder();
    umbra_ptr const sp = umbra_bind_buf(b, &src),
                      dp = umbra_bind_buf(b, &dst);
    umbra_val32 v = umbra_gather_32(b, sp, umbra_x(b));
    umbra_store_32(b, dp, v);
    struct umbra_flat_ir *ir = umbra_flat_ir(b);
    umbra_builder_free(b);

    struct umbra_flat_ir *r = flat_ir_resolve(ir, JOIN_PREFER_IMM);

    _Bool found_gather = 0;
    for (int i = 0; i < r->insts; i++) {
        if (r->inst[i].op == op_gather_32) {
            found_gather = 1;
        }
    }
    found_gather here;

    umbra_flat_ir_free(r);
    umbra_flat_ir_free(ir);
}
