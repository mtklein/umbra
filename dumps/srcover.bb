  v0   = imm_32          0x0
  v1   = lane           
  v2   = load_32         p0
  v3   = imm_32          0xff
  v4   = imm_half        0x1c04
  v5   = and_32          v2 v3
  v6   = i16_from_i32    v5
  v7   = half_from_i16   v6
  v8   = mul_half        v4 v7
  v9   = imm_32          0x8
  v10  = shr_u32         v2 v9
  v11  = and_32          v3 v10
  v12  = i16_from_i32    v11
  v13  = half_from_i16   v12
  v14  = mul_half        v4 v13
  v15  = imm_32          0x10
  v16  = shr_u32         v2 v15
  v17  = and_32          v3 v16
  v18  = i16_from_i32    v17
  v19  = half_from_i16   v18
  v20  = mul_half        v4 v19
  v21  = imm_32          0x18
  v22  = shr_u32         v2 v21
  v23  = and_32          v3 v22
  v24  = i16_from_i32    v23
  v25  = half_from_i16   v24
  v26  = mul_half        v4 v25
  v27  = load_half       p1
  v28  = load_half       p2
  v29  = load_half       p3
  v30  = load_half       p4
  v31  = imm_half        0x3c00
  v32  = sub_half        v31 v26
  v33  = mul_half        v27 v32
  v34  = fma_half        v4 v7 v33
  v35  = mul_half        v28 v32
  v36  = fma_half        v4 v13 v35
  v37  = mul_half        v29 v32
  v38  = fma_half        v4 v19 v37
  v39  = mul_half        v30 v32
  v40  = fma_half        v4 v25 v39
      store_half      p1 v34
      store_half      p2 v36
      store_half      p3 v38
      store_half      p4 v40
