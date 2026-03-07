  v0   = imm_32          0x0
  v1   = lane           
  v2   = load_8x4        p0
  v3   = load_8x4        v2 ch1
  v4   = load_8x4        v2 ch2
  v5   = load_8x4        v2 ch3
  v6   = imm_half        0x1c04
  v7   = i16_from_u8     v2
  v8   = half_from_i16   v7
  v9   = mul_half        v6 v8
  v10  = i16_from_u8     v3
  v11  = half_from_i16   v10
  v12  = mul_half        v6 v11
  v13  = i16_from_u8     v4
  v14  = half_from_i16   v13
  v15  = mul_half        v6 v14
  v16  = i16_from_u8     v5
  v17  = half_from_i16   v16
  v18  = mul_half        v6 v17
  v19  = load_half       p1
  v20  = load_half       p2
  v21  = load_half       p3
  v22  = load_half       p4
  v23  = imm_half        0x3c00
  v24  = sub_half        v23 v18
  v25  = mul_half        v19 v24
  v26  = fma_half        v6 v8 v25
  v27  = mul_half        v20 v24
  v28  = fma_half        v6 v11 v27
  v29  = mul_half        v21 v24
  v30  = fma_half        v6 v14 v29
  v31  = mul_half        v22 v24
  v32  = fma_half        v6 v17 v31
      store_half      p1 v26
      store_half      p2 v28
      store_half      p3 v30
      store_half      p4 v32
