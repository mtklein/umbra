  v0   = imm_32          0x0
  v1   = lane           
  v2   = load_8x4        p0
  v3   = load_8x4        v2 ch1
  v4   = load_8x4        v2 ch2
  v5   = load_8x4        v2 ch3
  v6   = imm_half        0x1c04
  v7   = half_from_i16   v2
  v8   = mul_half        v6 v7
  v9   = half_from_i16   v3
  v10  = mul_half        v6 v9
  v11  = half_from_i16   v4
  v12  = mul_half        v6 v11
  v13  = half_from_i16   v5
  v14  = mul_half        v6 v13
  v15  = load_half       p1
  v16  = load_half       p2
  v17  = load_half       p3
  v18  = load_half       p4
  v19  = imm_half        0x3c00
  v20  = fms_half        v6 v13 v19
  v21  = mul_half        v15 v20
  v22  = fma_half        v6 v7 v21
  v23  = mul_half        v16 v20
  v24  = fma_half        v6 v9 v23
  v25  = mul_half        v17 v20
  v26  = fma_half        v6 v11 v25
  v27  = mul_half        v18 v20
  v28  = fma_half        v6 v13 v27
      store_half      p1 v22
      store_half      p2 v24
      store_half      p3 v26
      store_half      p4 v28
