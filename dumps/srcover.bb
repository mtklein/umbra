  v0   = imm_32          0x0
  v1   = lane           
  v2   = load_32         p0
  v3   = imm_half        0x1c04
  v4   = imm_32          0xff00ff
  v5   = and_32          v2 v4
  v6   = imm_32          0x8
  v7   = shr_u32_imm     v2 8
  v8   = and_32          v4 v7
  v9   = i16_from_i32    v5
  v10  = half_from_i16   v9
  v11  = mul_half        v3 v10
  v12  = i16_from_i32    v8
  v13  = half_from_i16   v12
  v14  = mul_half        v3 v13
  v15  = imm_32          0x10
  v16  = shr_u32_imm     v5 16
  v17  = i16_from_i32    v16
  v18  = half_from_i16   v17
  v19  = mul_half        v3 v18
  v20  = shr_u32_imm     v8 16
  v21  = i16_from_i32    v20
  v22  = half_from_i16   v21
  v23  = mul_half        v3 v22
  v24  = load_half       p1
  v25  = load_half       p2
  v26  = load_half       p3
  v27  = load_half       p4
  v28  = imm_half        0x3c00
  v29  = sub_half        v28 v23
  v30  = mul_half        v24 v29
  v31  = fma_half        v3 v10 v30
  v32  = mul_half        v25 v29
  v33  = fma_half        v3 v13 v32
  v34  = mul_half        v26 v29
  v35  = fma_half        v3 v18 v34
  v36  = mul_half        v27 v29
  v37  = fma_half        v3 v22 v36
      store_half      p1 v31
      store_half      p2 v33
      store_half      p3 v35
      store_half      p4 v37
