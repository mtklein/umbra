  v0   = imm_32          0x0
  v1   = lane           
  v2   = load_8x4        p0
  v3   = load_8x4        v2 ch1
  v4   = load_8x4        v2 ch2
  v5   = load_8x4        v2 ch3
  v6   = imm_32          0x3b808081
  v7   = f32_from_i32    v2
  v8   = mul_f32         v6 v7
  v9   = f32_from_i32    v3
  v10  = mul_f32         v6 v9
  v11  = f32_from_i32    v4
  v12  = mul_f32         v6 v11
  v13  = f32_from_i32    v5
  v14  = mul_f32         v6 v13
  v15  = load_f16        p1
  v16  = load_f16        p2
  v17  = load_f16        p3
  v18  = load_f16        p4
  v19  = imm_32          0x3f800000
  v20  = fms_f32         v6 v13 v19
  v21  = mul_f32         v15 v20
  v22  = fma_f32         v6 v7 v21
  v23  = mul_f32         v16 v20
  v24  = fma_f32         v6 v9 v23
  v25  = mul_f32         v17 v20
  v26  = fma_f32         v6 v11 v25
  v27  = mul_f32         v18 v20
  v28  = fma_f32         v6 v13 v27
      store_f16       p1 v22
      store_f16       p2 v24
      store_f16       p3 v26
      store_f16       p4 v28
