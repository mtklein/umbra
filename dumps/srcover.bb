  v0   = imm_32          0x0
  v1   = imm_32          0x3f800000
  v2   = load_32         p0
  v3   = shr_u32_imm     v2 24
  v4   = f32_from_i32    v3
  v5   = mul_f32_imm     v4 0x3b808081
  v6   = sub_f32         v1 v5
  v7   = shr_u32_imm     v2 8
  v8   = and_imm         v7 0xff
  v9   = f32_from_i32    v8
  v10  = mul_f32_imm     v9 0x3b808081
  v11  = shr_u32_imm     v2 16
  v12  = and_imm         v11 0xff
  v13  = f32_from_i32    v12
  v14  = mul_f32_imm     v13 0x3b808081
  v15  = and_imm         v2 0xff
  v16  = f32_from_i32    v15
  v17  = mul_f32_imm     v16 0x3b808081
  v18  = load_16         p1
  v19  = f32_from_f16    v18
  v20  = fma_f32         v19 v6 v17
  v21  = f16_from_f32    v20
      store_16        p1 v21
  v23  = load_16         p2
  v24  = f32_from_f16    v23
  v25  = fma_f32         v24 v6 v10
  v26  = f16_from_f32    v25
      store_16        p2 v26
  v28  = load_16         p3
  v29  = f32_from_f16    v28
  v30  = fma_f32         v29 v6 v14
  v31  = f16_from_f32    v30
      store_16        p3 v31
  v33  = load_16         p4
  v34  = f32_from_f16    v33
  v35  = fma_f32         v34 v6 v5
  v36  = f16_from_f32    v35
      store_16        p4 v36
