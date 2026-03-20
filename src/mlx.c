#if defined(__APPLE__) && defined(__aarch64__)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#include <mlx/c/mlx.h>
#pragma clang diagnostic pop

#include "program.h"
#include "bb.h"
#include <stdlib.h>

struct store_desc {
    int inst, ptr_idx, off_pre, elem_sz;
    int val_out, idx_out, is_scatter, :32;
};

struct umbra_mlx {
    struct umbra_basic_block *bb;
    int    *deref_slot;
    int    *input_map;
    int    *input_inst;
    struct store_desc *stores;
    mlx_closure compiled;
    int     nptr, n_deref, n_inputs, :32;
    int     n_stores, n_outputs;
};

static mlx_array vw(mlx_array a, mlx_dtype dt,
                     mlx_stream s) {
    mlx_array r = mlx_array_new();
    if (mlx_array_dtype(a) == dt) {
        mlx_array_set(&r, a);
    } else {
        mlx_view(&r, a, dt, s);
    }
    return r;
}

static void bool_to_mask(mlx_array *res,
                          mlx_array b,
                          mlx_stream s) {
    mlx_array tmp = mlx_array_new();
    mlx_astype(&tmp, b, MLX_INT32, s);
    *res = mlx_array_new();
    mlx_negative(res, tmp, s);
    mlx_array_free(tmp);
}

static int closure_fn(mlx_vector_array *outputs,
                      const mlx_vector_array inputs,
                      void *payload) {
    struct umbra_mlx *m = payload;
    struct umbra_basic_block const *bb = m->bb;
    mlx_stream s = mlx_default_gpu_stream_new();

    int nval = bb->insts;
    mlx_array *v = calloc((size_t)nval, sizeof *v);

    for (int i = 0; i < nval; i++) {
        struct bb_inst const *ip = &bb->inst[i];

        if (m->input_map[i] >= 0
         && ip->op != op_gather_32
         && ip->op != op_gather_16) {
            v[i] = mlx_array_new();
            mlx_vector_array_get(
                &v[i], inputs, (size_t)m->input_map[i]);
            continue;
        }

        if (is_store(ip->op)) { continue; }

        switch (ip->op) {

        case op_imm_32:
            v[i] = mlx_array_new_int(ip->imm);
            break;

        case op_gather_32: {
            mlx_array src = mlx_array_new();
            mlx_vector_array_get(
                &src, inputs, (size_t)m->input_map[i]);
            mlx_array idx = vw(v[ip->x], MLX_INT32, s);
            int sn = mlx_array_dim(src, 0);
            mlx_array lo = mlx_array_new_int(0);
            mlx_array hi = mlx_array_new_int(
                sn > 0 ? sn - 1 : 0);
            mlx_array ci = mlx_array_new();
            mlx_clip(&ci, idx, lo, hi, s);
            v[i] = mlx_array_new();
            mlx_take(&v[i], src, ci, s);
            mlx_array_free(src);
            mlx_array_free(idx);
            mlx_array_free(lo);
            mlx_array_free(hi);
            mlx_array_free(ci);
        } break;
        case op_gather_16: {
            mlx_array src = mlx_array_new();
            mlx_vector_array_get(
                &src, inputs, (size_t)m->input_map[i]);
            mlx_array idx = vw(v[ip->x], MLX_INT32, s);
            int sn = mlx_array_dim(src, 0);
            mlx_array lo = mlx_array_new_int(0);
            mlx_array hi = mlx_array_new_int(
                sn > 0 ? sn - 1 : 0);
            mlx_array ci = mlx_array_new();
            mlx_clip(&ci, idx, lo, hi, s);
            v[i] = mlx_array_new();
            mlx_take(&v[i], src, ci, s);
            mlx_array_free(src);
            mlx_array_free(idx);
            mlx_array_free(lo);
            mlx_array_free(hi);
            mlx_array_free(ci);
        } break;

        case op_add_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array b = vw(v[ip->y], MLX_FLOAT32, s);
            v[i] = mlx_array_new();
            mlx_add(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_sub_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array b = vw(v[ip->y], MLX_FLOAT32, s);
            v[i] = mlx_array_new();
            mlx_subtract(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_mul_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array b = vw(v[ip->y], MLX_FLOAT32, s);
            v[i] = mlx_array_new();
            mlx_multiply(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_div_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array b = vw(v[ip->y], MLX_FLOAT32, s);
            v[i] = mlx_array_new();
            mlx_divide(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_min_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array b = vw(v[ip->y], MLX_FLOAT32, s);
            v[i] = mlx_array_new();
            mlx_minimum(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_max_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array b = vw(v[ip->y], MLX_FLOAT32, s);
            v[i] = mlx_array_new();
            mlx_maximum(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_sqrt_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            v[i] = mlx_array_new();
            mlx_sqrt(&v[i], a, s);
            mlx_array_free(a);
        } break;
        case op_fma_f32: {
            mlx_array x = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array y = vw(v[ip->y], MLX_FLOAT32, s);
            mlx_array z = vw(v[ip->z], MLX_FLOAT32, s);
            mlx_array xy = mlx_array_new();
            mlx_multiply(&xy, x, y, s);
            v[i] = mlx_array_new();
            mlx_add(&v[i], z, xy, s);
            mlx_array_free(xy);
            mlx_array_free(x);
            mlx_array_free(y);
            mlx_array_free(z);
        } break;
        case op_fms_f32: {
            mlx_array x = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array y = vw(v[ip->y], MLX_FLOAT32, s);
            mlx_array z = vw(v[ip->z], MLX_FLOAT32, s);
            mlx_array xy = mlx_array_new();
            mlx_multiply(&xy, x, y, s);
            v[i] = mlx_array_new();
            mlx_subtract(&v[i], z, xy, s);
            mlx_array_free(xy);
            mlx_array_free(x);
            mlx_array_free(y);
            mlx_array_free(z);
        } break;

        case op_add_i32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            v[i] = mlx_array_new();
            mlx_add(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_sub_i32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            v[i] = mlx_array_new();
            mlx_subtract(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_mul_i32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            v[i] = mlx_array_new();
            mlx_multiply(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;

        case op_shl_i32: {
            mlx_array a = vw(v[ip->x], MLX_UINT32, s);
            mlx_array b = vw(v[ip->y], MLX_UINT32, s);
            v[i] = mlx_array_new();
            mlx_left_shift(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_shr_u32: {
            mlx_array a = vw(v[ip->x], MLX_UINT32, s);
            mlx_array b = vw(v[ip->y], MLX_UINT32, s);
            v[i] = mlx_array_new();
            mlx_right_shift(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_shr_s32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            v[i] = mlx_array_new();
            mlx_right_shift(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;

        case op_shl_imm: {
            mlx_array a = vw(v[ip->x], MLX_UINT32, s);
            mlx_array sh = mlx_array_new_int(ip->imm);
            mlx_array su = mlx_array_new();
            mlx_astype(&su, sh, MLX_UINT32, s);
            v[i] = mlx_array_new();
            mlx_left_shift(&v[i], a, su, s);
            mlx_array_free(a);
            mlx_array_free(sh);
            mlx_array_free(su);
        } break;
        case op_shr_u32_imm: {
            mlx_array a = vw(v[ip->x], MLX_UINT32, s);
            mlx_array sh = mlx_array_new_int(ip->imm);
            mlx_array su = mlx_array_new();
            mlx_astype(&su, sh, MLX_UINT32, s);
            v[i] = mlx_array_new();
            mlx_right_shift(&v[i], a, su, s);
            mlx_array_free(a);
            mlx_array_free(sh);
            mlx_array_free(su);
        } break;
        case op_shr_s32_imm: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array sh = mlx_array_new_int(ip->imm);
            v[i] = mlx_array_new();
            mlx_right_shift(&v[i], a, sh, s);
            mlx_array_free(a);
            mlx_array_free(sh);
        } break;

        case op_and_32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            v[i] = mlx_array_new();
            mlx_bitwise_and(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_or_32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            v[i] = mlx_array_new();
            mlx_bitwise_or(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_xor_32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            v[i] = mlx_array_new();
            mlx_bitwise_xor(&v[i], a, b, s);
            mlx_array_free(a); mlx_array_free(b);
        } break;

        case op_sel_32: {
            mlx_array c = vw(v[ip->x], MLX_INT32, s);
            mlx_array t = vw(v[ip->y], MLX_INT32, s);
            mlx_array f = vw(v[ip->z], MLX_INT32, s);
            mlx_array ct = mlx_array_new();
            mlx_bitwise_and(&ct, c, t, s);
            mlx_array nc = mlx_array_new();
            mlx_bitwise_invert(&nc, c, s);
            mlx_array nf = mlx_array_new();
            mlx_bitwise_and(&nf, nc, f, s);
            v[i] = mlx_array_new();
            mlx_bitwise_or(&v[i], ct, nf, s);
            mlx_array_free(ct);
            mlx_array_free(nc);
            mlx_array_free(nf);
            mlx_array_free(c);
            mlx_array_free(t);
            mlx_array_free(f);
        } break;

        case op_f32_from_i32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            v[i] = mlx_array_new();
            mlx_astype(&v[i], a, MLX_FLOAT32, s);
            mlx_array_free(a);
        } break;
        case op_i32_from_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            v[i] = mlx_array_new();
            mlx_astype(&v[i], a, MLX_INT32, s);
            mlx_array_free(a);
        } break;

        case op_eq_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array b = vw(v[ip->y], MLX_FLOAT32, s);
            mlx_array c = mlx_array_new();
            mlx_equal(&c, a, b, s);
            bool_to_mask(&v[i], c, s);
            mlx_array_free(c);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_lt_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array b = vw(v[ip->y], MLX_FLOAT32, s);
            mlx_array c = mlx_array_new();
            mlx_less(&c, a, b, s);
            bool_to_mask(&v[i], c, s);
            mlx_array_free(c);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_le_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array b = vw(v[ip->y], MLX_FLOAT32, s);
            mlx_array c = mlx_array_new();
            mlx_less_equal(&c, a, b, s);
            bool_to_mask(&v[i], c, s);
            mlx_array_free(c);
            mlx_array_free(a); mlx_array_free(b);
        } break;

        case op_eq_i32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            mlx_array c = mlx_array_new();
            mlx_equal(&c, a, b, s);
            bool_to_mask(&v[i], c, s);
            mlx_array_free(c);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_lt_s32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            mlx_array c = mlx_array_new();
            mlx_less(&c, a, b, s);
            bool_to_mask(&v[i], c, s);
            mlx_array_free(c);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_le_s32: {
            mlx_array a = vw(v[ip->x], MLX_INT32, s);
            mlx_array b = vw(v[ip->y], MLX_INT32, s);
            mlx_array c = mlx_array_new();
            mlx_less_equal(&c, a, b, s);
            bool_to_mask(&v[i], c, s);
            mlx_array_free(c);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_lt_u32: {
            mlx_array a = vw(v[ip->x], MLX_UINT32, s);
            mlx_array b = vw(v[ip->y], MLX_UINT32, s);
            mlx_array c = mlx_array_new();
            mlx_less(&c, a, b, s);
            bool_to_mask(&v[i], c, s);
            mlx_array_free(c);
            mlx_array_free(a); mlx_array_free(b);
        } break;
        case op_le_u32: {
            mlx_array a = vw(v[ip->x], MLX_UINT32, s);
            mlx_array b = vw(v[ip->y], MLX_UINT32, s);
            mlx_array c = mlx_array_new();
            mlx_less_equal(&c, a, b, s);
            bool_to_mask(&v[i], c, s);
            mlx_array_free(c);
            mlx_array_free(a); mlx_array_free(b);
        } break;

        case op_widen_s16:
            v[i] = mlx_array_new();
            mlx_astype(&v[i], v[ip->x], MLX_INT32, s);
            break;
        case op_widen_u16: {
            mlx_array u = mlx_array_new();
            mlx_view(&u, v[ip->x], MLX_UINT16, s);
            v[i] = mlx_array_new();
            mlx_astype(&v[i], u, MLX_UINT32, s);
            mlx_array_free(u);
        } break;
        case op_narrow_16:
            v[i] = mlx_array_new();
            mlx_astype(&v[i], v[ip->x], MLX_INT16, s);
            break;
        case op_widen_f16: {
            mlx_array f16 = mlx_array_new();
            mlx_view(&f16, v[ip->x], MLX_FLOAT16, s);
            v[i] = mlx_array_new();
            mlx_astype(&v[i], f16, MLX_FLOAT32, s);
            mlx_array_free(f16);
        } break;
        case op_narrow_f32: {
            mlx_array a = vw(v[ip->x], MLX_FLOAT32, s);
            mlx_array f16 = mlx_array_new();
            mlx_astype(&f16, a, MLX_FLOAT16, s);
            v[i] = mlx_array_new();
            mlx_view(&v[i], f16, MLX_INT16, s);
            mlx_array_free(f16);
            mlx_array_free(a);
        } break;

        case op_iota:
        case op_uni_32: case op_uni_16:
        case op_load_32: case op_load_16:
        case op_deref_ptr:
        case op_store_32: case op_store_16:
        case op_scatter_32: case op_scatter_16:
        case op_join:
        case op_pack:
        case op_and_imm:
        case op_add_f32_imm: case op_sub_f32_imm:
        case op_mul_f32_imm: case op_div_f32_imm:
        case op_min_f32_imm: case op_max_f32_imm:
        case op_add_i32_imm: case op_sub_i32_imm:
        case op_mul_i32_imm:
        case op_or_32_imm: case op_xor_32_imm:
        case op_eq_f32_imm: case op_lt_f32_imm:
        case op_le_f32_imm:
        case op_eq_i32_imm: case op_lt_s32_imm:
        case op_le_s32_imm:
            break;

        }
    }

    *outputs = mlx_vector_array_new();
    for (int si = 0; si < m->n_stores; si++) {
        struct store_desc *sd = &m->stores[si];
        struct bb_inst const *ip = &bb->inst[sd->inst];
        if (sd->is_scatter) {
            mlx_vector_array_append_value(
                *outputs, v[ip->y]);
            mlx_vector_array_append_value(
                *outputs, v[ip->x]);
        } else if (sd->elem_sz == 2) {
            mlx_array n16 = mlx_array_new();
            mlx_astype(&n16, v[ip->y], MLX_INT16, s);
            mlx_vector_array_append_value(*outputs, n16);
            mlx_array_free(n16);
        } else {
            mlx_array i32 = vw(v[ip->y], MLX_INT32, s);
            mlx_vector_array_append_value(*outputs, i32);
            mlx_array_free(i32);
        }
    }

    for (int i = 0; i < nval; i++) {
        mlx_array_free(v[i]);
    }
    free(v);
    mlx_stream_free(s);
    return 0;
}

static int rptr(struct umbra_mlx const *m,
                struct bb_inst const *ip) {
    return ip->ptr < 0
         ? m->deref_slot[~ip->ptr]
         : ip->ptr;
}

struct umbra_mlx* umbra_mlx(
    struct umbra_basic_block const *bb)
{
    struct umbra_basic_block *resolved =
        umbra_resolve_joins(bb, NULL);

    int max_ptr = -1, n_deref = 0;
    for (int i = 0; i < resolved->insts; i++) {
        struct bb_inst const *ip = &resolved->inst[i];
        if (has_ptr(ip->op)
         && ip->ptr >= 0
         && ip->ptr > max_ptr) {
            max_ptr = ip->ptr;
        }
        if (ip->op == op_deref_ptr) { n_deref++; }
    }

    int nptr = max_ptr + 1;
    int *deref_slot = calloc(
        (size_t)resolved->insts, sizeof *deref_slot);
    {
        int di = 0;
        for (int i = 0; i < resolved->insts; i++) {
            if (resolved->inst[i].op == op_deref_ptr) {
                deref_slot[i] = nptr + di++;
            }
        }
    }

    int nval = resolved->insts;
    int *input_map = malloc((size_t)nval * sizeof *input_map);
    int *input_inst = malloc((size_t)nval * sizeof *input_inst);
    for (int i = 0; i < nval; i++) { input_map[i] = -1; }

    int n_inputs = 0;
    for (int i = 0; i < resolved->preamble; i++) {
        if (resolved->inst[i].op == op_deref_ptr) {
            continue;
        }
        input_map[i] = n_inputs;
        input_inst[n_inputs] = i;
        n_inputs++;
    }
    for (int i = resolved->preamble; i < nval; i++) {
        enum op op = resolved->inst[i].op;
        if (op == op_iota
         || op == op_load_32 || op == op_load_16
         || op == op_gather_32 || op == op_gather_16) {
            input_map[i] = n_inputs;
            input_inst[n_inputs] = i;
            n_inputs++;
        }
    }

    struct store_desc *stores = NULL;
    int n_stores = 0, n_outputs = 0;
    for (int i = resolved->preamble; i < nval; i++) {
        if (!is_store(resolved->inst[i].op)) { continue; }
        stores = realloc(stores,
            (size_t)(n_stores + 1) * sizeof *stores);
        struct bb_inst const *ip = &resolved->inst[i];
        _Bool scatter = (ip->op == op_scatter_32
                      || ip->op == op_scatter_16);
        int esz = (ip->op == op_store_16
                || ip->op == op_scatter_16) ? 2 : 4;
        stores[n_stores] = (struct store_desc){
            .inst = i,
            .ptr_idx = -1,
            .off_pre = ip->x ? ip->x : -1,
            .elem_sz = esz,
            .is_scatter = scatter,
            .val_out = n_outputs,
            .idx_out = scatter ? n_outputs + 1 : -1,
        };
        n_outputs += scatter ? 2 : 1;
        n_stores++;
    }

    struct umbra_mlx *m = malloc(sizeof *m);
    *m = (struct umbra_mlx){
        .bb         = resolved,
        .deref_slot = deref_slot,
        .input_map  = input_map,
        .input_inst = input_inst,
        .stores     = stores,
        .nptr       = nptr,
        .n_deref    = n_deref,
        .n_inputs   = n_inputs,
        .n_stores   = n_stores,
        .n_outputs  = n_outputs,
    };

    mlx_closure raw = mlx_closure_new_func_payload(
        closure_fn, m, NULL);
    m->compiled = mlx_closure_new();
    mlx_compile(&m->compiled, raw, false);
    mlx_closure_free(raw);

    return m;
}

void umbra_mlx_run(struct umbra_mlx *m,
                    int n, umbra_buf buf[])
{
    struct umbra_basic_block const *bb = m->bb;
    mlx_stream s = mlx_default_gpu_stream_new();

    int total_ptrs = m->nptr + m->n_deref;
    void **ptr = calloc((size_t)total_ptrs, sizeof *ptr);
    long  *psz = calloc((size_t)total_ptrs, sizeof *psz);
    for (int i = 0; i < m->nptr; i++) {
        ptr[i] = buf[i].ptr;
        psz[i] = buf[i].sz < 0 ? -buf[i].sz : buf[i].sz;
    }

    int32_t *pre = calloc(
        (size_t)bb->preamble, sizeof *pre);
    for (int i = 0; i < bb->preamble; i++) {
        struct bb_inst const *ip = &bb->inst[i];
        int P = rptr(m, ip);
        switch (ip->op) {
        case op_imm_32:
            pre[i] = ip->imm;
            break;
        case op_deref_ptr: {
            void *base = ptr[ip->ptr];
            void *derived; long dsz;
            __builtin_memcpy(&derived,
                (char*)base + ip->imm, sizeof derived);
            __builtin_memcpy(&dsz,
                (char*)base + ip->imm + 8, sizeof dsz);
            ptr[m->deref_slot[i]] = derived;
            psz[m->deref_slot[i]] = dsz;
        } break;
        case op_uni_32: {
            if (ip->x) {
                __builtin_memcpy(&pre[i],
                    (int32_t*)ptr[P] + pre[ip->x], 4);
            } else {
                __builtin_memcpy(&pre[i],
                    (int32_t*)ptr[P] + ip->imm, 4);
            }
        } break;
        case op_uni_16: {
            uint16_t val;
            if (ip->x) {
                __builtin_memcpy(&val,
                    (uint16_t*)ptr[P] + pre[ip->x], 2);
            } else {
                __builtin_memcpy(&val,
                    (uint16_t*)ptr[P] + ip->imm, 2);
            }
            pre[i] = (int32_t)(uint32_t)val;
        } break;

        case op_iota:
        case op_load_32: case op_load_16:
        case op_gather_32: case op_gather_16:
        case op_store_32: case op_store_16:
        case op_scatter_32: case op_scatter_16:
            break;

        case op_add_f32: case op_sub_f32:
        case op_mul_f32: case op_div_f32:
        case op_min_f32: case op_max_f32:
        case op_sqrt_f32:
        case op_fma_f32: case op_fms_f32:
        case op_add_i32: case op_sub_i32:
        case op_mul_i32:
        case op_shl_i32: case op_shr_u32:
        case op_shr_s32:
        case op_and_32: case op_or_32:
        case op_xor_32: case op_sel_32:
        case op_f32_from_i32: case op_i32_from_f32:
        case op_eq_f32: case op_lt_f32:
        case op_le_f32:
        case op_eq_i32: case op_lt_s32:
        case op_le_s32:
        case op_lt_u32: case op_le_u32:
        case op_widen_s16: case op_widen_u16:
        case op_narrow_16:
        case op_widen_f16: case op_narrow_f32:
        case op_shl_imm: case op_shr_u32_imm:
        case op_shr_s32_imm:
        case op_join: case op_pack: case op_and_imm:
        case op_add_f32_imm: case op_sub_f32_imm:
        case op_mul_f32_imm: case op_div_f32_imm:
        case op_min_f32_imm: case op_max_f32_imm:
        case op_add_i32_imm: case op_sub_i32_imm:
        case op_mul_i32_imm:
        case op_or_32_imm: case op_xor_32_imm:
        case op_eq_f32_imm: case op_lt_f32_imm:
        case op_le_f32_imm:
        case op_eq_i32_imm: case op_lt_s32_imm:
        case op_le_s32_imm:
            pre[i] = umbra_const_eval(
                ip->op, pre[ip->x],
                pre[ip->y], pre[ip->z]);
            break;
        }
    }

    mlx_array *inp = calloc(
        (size_t)m->n_inputs, sizeof *inp);
    for (int k = 0; k < m->n_inputs; k++) {
        int i = m->input_inst[k];
        struct bb_inst const *ip = &bb->inst[i];
        if (i < bb->preamble) {
            inp[k] = mlx_array_new_int(pre[i]);
        } else {
            int P = rptr(m, ip);
            switch (ip->op) {
            case op_iota:
                inp[k] = mlx_array_new();
                mlx_arange(
                    &inp[k], 0, n, 1, MLX_INT32, s);
                break;
            case op_load_32: {
                int off = ip->x ? pre[ip->x] : 0;
                inp[k] = mlx_array_new_data(
                    (int32_t*)ptr[P] + off,
                    &n, 1, MLX_INT32);
            } break;
            case op_load_16: {
                int off = ip->x ? pre[ip->x] : 0;
                inp[k] = mlx_array_new_data(
                    (int16_t*)ptr[P] + off,
                    &n, 1, MLX_INT16);
            } break;
            case op_gather_32: {
                int el = (int)(psz[P] / 4);
                if (el <= 0) { el = 1; }
                inp[k] = mlx_array_new_data(
                    ptr[P], &el, 1, MLX_INT32);
            } break;
            case op_gather_16: {
                int el = (int)(psz[P] / 2);
                if (el <= 0) { el = 1; }
                inp[k] = mlx_array_new_data(
                    ptr[P], &el, 1, MLX_INT16);
            } break;

            case op_imm_32: case op_uni_32:
            case op_uni_16: case op_deref_ptr:
            case op_store_32: case op_store_16:
            case op_scatter_32: case op_scatter_16:
            case op_add_f32: case op_sub_f32:
            case op_mul_f32: case op_div_f32:
            case op_min_f32: case op_max_f32:
            case op_sqrt_f32:
            case op_fma_f32: case op_fms_f32:
            case op_add_i32: case op_sub_i32:
            case op_mul_i32:
            case op_shl_i32: case op_shr_u32:
            case op_shr_s32:
            case op_and_32: case op_or_32:
            case op_xor_32: case op_sel_32:
            case op_f32_from_i32: case op_i32_from_f32:
            case op_eq_f32: case op_lt_f32:
            case op_le_f32:
            case op_eq_i32: case op_lt_s32:
            case op_le_s32:
            case op_lt_u32: case op_le_u32:
            case op_widen_s16: case op_widen_u16:
            case op_narrow_16:
            case op_widen_f16: case op_narrow_f32:
            case op_shl_imm: case op_shr_u32_imm:
            case op_shr_s32_imm:
            case op_join: case op_pack:
            case op_and_imm:
            case op_add_f32_imm: case op_sub_f32_imm:
            case op_mul_f32_imm: case op_div_f32_imm:
            case op_min_f32_imm: case op_max_f32_imm:
            case op_add_i32_imm: case op_sub_i32_imm:
            case op_mul_i32_imm:
            case op_or_32_imm: case op_xor_32_imm:
            case op_eq_f32_imm: case op_lt_f32_imm:
            case op_le_f32_imm:
            case op_eq_i32_imm: case op_lt_s32_imm:
            case op_le_s32_imm:
                break;
            }
        }
    }

    mlx_vector_array inputs =
        mlx_vector_array_new_data(inp, (size_t)m->n_inputs);
    mlx_vector_array outputs = mlx_vector_array_new();
    mlx_closure_apply(&outputs, m->compiled, inputs);

    for (int si = 0; si < m->n_stores; si++) {
        struct store_desc *sd = &m->stores[si];
        int P = rptr(m, &bb->inst[sd->inst]);

        if (sd->is_scatter) {
            mlx_array val = mlx_array_new();
            mlx_array idx = mlx_array_new();
            mlx_vector_array_get(
                &val, outputs, (size_t)sd->val_out);
            mlx_vector_array_get(
                &idx, outputs, (size_t)sd->idx_out);
            mlx_array_eval(val);
            mlx_array_eval(idx);
            int32_t const *ix =
                mlx_array_data_int32(idx);
            int cnt = (int)mlx_array_size(val);
            int hi = (sd->elem_sz == 2)
                ? (int)(psz[P] / 2) - 1
                : (int)(psz[P] / 4) - 1;
            if (hi < 0) { hi = 0; }
            if (sd->elem_sz == 2) {
                int16_t const *d =
                    mlx_array_data_int16(val);
                int16_t *dst = (int16_t*)ptr[P];
                for (int j = 0; j < cnt; j++) {
                    int k = ix[j];
                    if (k < 0) { k = 0; }
                    if (k > hi) { k = hi; }
                    dst[k] = d[j];
                }
            } else {
                int32_t const *d =
                    mlx_array_data_int32(val);
                int32_t *dst = (int32_t*)ptr[P];
                for (int j = 0; j < cnt; j++) {
                    int k = ix[j];
                    if (k < 0) { k = 0; }
                    if (k > hi) { k = hi; }
                    dst[k] = d[j];
                }
            }
            mlx_array_free(val);
            mlx_array_free(idx);
        } else {
            int off = (sd->off_pre >= 0)
                    ? pre[sd->off_pre] : 0;
            mlx_array val = mlx_array_new();
            mlx_vector_array_get(
                &val, outputs, (size_t)sd->val_out);
            mlx_array_eval(val);
            if (sd->elem_sz == 2) {
                __builtin_memcpy(
                    (int16_t*)ptr[P] + off,
                    mlx_array_data_int16(val),
                    (size_t)n * 2);
            } else {
                __builtin_memcpy(
                    (int32_t*)ptr[P] + off,
                    mlx_array_data_int32(val),
                    (size_t)n * 4);
            }
            mlx_array_free(val);
        }
    }

    for (int k = 0; k < m->n_inputs; k++) {
        mlx_array_free(inp[k]);
    }
    free(inp);
    mlx_vector_array_free(inputs);
    mlx_vector_array_free(outputs);
    free(pre);
    free(ptr);
    free(psz);
    mlx_stream_free(s);
}

void umbra_mlx_free(struct umbra_mlx *m) {
    if (!m) { return; }
    mlx_closure_free(m->compiled);
    umbra_basic_block_free(m->bb);
    free(m->deref_slot);
    free(m->input_map);
    free(m->input_inst);
    free(m->stores);
    free(m);
}

#else

#include "program.h"
#include "bb.h"

struct umbra_mlx { int dummy; };

struct umbra_mlx* umbra_mlx(
    struct umbra_basic_block const *bb)
{
    (void)bb;
    return 0;
}
void umbra_mlx_run(struct umbra_mlx *m,
                    int n, umbra_buf buf[])
{
    (void)m; (void)n; (void)buf;
}
void umbra_mlx_free(struct umbra_mlx *m) {
    (void)m;
}

#endif
