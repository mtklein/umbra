  v0   = imm_32          0x0
  v1   = imm_32          0xff
  v2   = imm_32          0x3b808081
  v3   = imm_32          0x3f800000
  v4   = load_32         p0
  v5   = and_32          v4 v1
  v6   = and_imm         v4 0xff
  v7   = join            v5 v6
  v8   = f32_from_i32    v7
  v9   = load_16         p1
  v10  = widen_f16       v9
  v11  = shr_u32_imm     v4 24
  v12  = f32_from_i32    v11
  v13  = fms_f32         v2 v12 v3
  v14  = mul_f32         v10 v13
  v15  = fma_f32         v2 v8 v14
  v16  = narrow_f32      v15
      store_16        p1 v16
  v18  = shr_u32_imm     v4 8
  v19  = and_32          v1 v18
  v20  = and_imm         v18 0xff
  v21  = join            v19 v20
  v22  = f32_from_i32    v21
  v23  = load_16         p2
  v24  = widen_f16       v23
  v25  = mul_f32         v24 v13
  v26  = fma_f32         v2 v22 v25
  v27  = narrow_f32      v26
      store_16        p2 v27
  v29  = shr_u32_imm     v4 16
  v30  = and_32          v1 v29
  v31  = and_imm         v29 0xff
  v32  = join            v30 v31
  v33  = f32_from_i32    v32
  v34  = load_16         p3
  v35  = widen_f16       v34
  v36  = mul_f32         v35 v13
  v37  = fma_f32         v2 v33 v36
  v38  = narrow_f32      v37
      store_16        p3 v38
  v40  = load_16         p4
  v41  = widen_f16       v40
  v42  = mul_f32         v41 v13
  v43  = fma_f32         v2 v12 v42
  v44  = narrow_f32      v43
      store_16        p4 v44
