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
  v15  = load_16         p1
  v16  = htof            v15
  v17  = load_16         p2
  v18  = htof            v17
  v19  = load_16         p3
  v20  = htof            v19
  v21  = load_16         p4
  v22  = htof            v21
  v23  = imm_32          0x3f800000
  v24  = fms_f32         v6 v13 v23
  v25  = mul_f32         v16 v24
  v26  = fma_f32         v6 v7 v25
  v27  = mul_f32         v18 v24
  v28  = fma_f32         v6 v9 v27
  v29  = mul_f32         v20 v24
  v30  = fma_f32         v6 v11 v29
  v31  = mul_f32         v22 v24
  v32  = fma_f32         v6 v13 v31
  v33  = ftoh            v26
      store_16        p1 v33
  v35  = ftoh            v28
      store_16        p2 v35
  v37  = ftoh            v30
      store_16        p3 v37
  v39  = ftoh            v32
      store_16        p4 v39
