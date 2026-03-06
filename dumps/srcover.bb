  v0   = imm_32          0x0
  v1   = lane           
  v2   = load_32         p0
  v3   = imm_half        0x1c04
  v4   = bytes           v2 0x1
  v5   = i16_from_i32    v4
  v6   = half_from_i16   v5
  v7   = mul_half        v3 v6
  v8   = bytes           v2 0x2
  v9   = i16_from_i32    v8
  v10  = half_from_i16   v9
  v11  = mul_half        v3 v10
  v12  = bytes           v2 0x3
  v13  = i16_from_i32    v12
  v14  = half_from_i16   v13
  v15  = mul_half        v3 v14
  v16  = bytes           v2 0x4
  v17  = i16_from_i32    v16
  v18  = half_from_i16   v17
  v19  = mul_half        v3 v18
  v20  = load_half       p1
  v21  = load_half       p2
  v22  = load_half       p3
  v23  = load_half       p4
  v24  = imm_half        0x3c00
  v25  = sub_half        v24 v19
  v26  = mul_half        v20 v25
  v27  = fma_half        v3 v6 v26
  v28  = mul_half        v21 v25
  v29  = fma_half        v3 v10 v28
  v30  = mul_half        v22 v25
  v31  = fma_half        v3 v14 v30
  v32  = mul_half        v23 v25
  v33  = fma_half        v3 v18 v32
      store_half      p1 v27
      store_half      p2 v29
      store_half      p3 v31
      store_half      p4 v33
