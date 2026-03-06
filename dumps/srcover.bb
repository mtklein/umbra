  v0   = imm_32          0x0
  v1   = lane           
  v2   = load_32         p0
  v3   = imm_half        0x1c04
  v4   = imm_16          0xff
  v5   = i16_from_i32    v2
  v6   = and_16          v4 v5
  v7   = half_from_i16   v6
  v8   = mul_half        v3 v7
  v9   = imm_32          0x8
  v10  = shr_u32_imm     v2 8
  v11  = i16_from_i32    v10
  v12  = and_16          v4 v11
  v13  = half_from_i16   v12
  v14  = mul_half        v3 v13
  v15  = imm_32          0x10
  v16  = shr_u32_imm     v2 16
  v17  = i16_from_i32    v16
  v18  = and_16          v4 v17
  v19  = half_from_i16   v18
  v20  = mul_half        v3 v19
  v21  = imm_32          0x18
  v22  = shr_u32_imm     v2 24
  v23  = i16_from_i32    v22
  v24  = half_from_i16   v23
  v25  = mul_half        v3 v24
  v26  = load_half       p1
  v27  = load_half       p2
  v28  = load_half       p3
  v29  = load_half       p4
  v30  = imm_half        0x3c00
  v31  = sub_half        v30 v25
  v32  = mul_half        v26 v31
  v33  = fma_half        v3 v7 v32
  v34  = mul_half        v27 v31
  v35  = fma_half        v3 v13 v34
  v36  = mul_half        v28 v31
  v37  = fma_half        v3 v19 v36
  v38  = mul_half        v29 v31
  v39  = fma_half        v3 v24 v38
      store_half      p1 v33
      store_half      p2 v35
      store_half      p3 v37
      store_half      p4 v39
