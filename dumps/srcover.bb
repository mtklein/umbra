  v0   = imm_32          0x0
  v1   = lane           
  v2   = load_32         p0 v1
  v3   = imm_32          0xff
  v4   = imm_half        0x1c04
  v5   = and_32          v2 v3
  v6   = half_from_i32   v5
  v7   = mul_half        v4 v6
  v8   = imm_32          0x8
  v9   = shr_u32         v2 v8
  v10  = and_32          v3 v9
  v11  = half_from_i32   v10
  v12  = mul_half        v4 v11
  v13  = imm_32          0x10
  v14  = shr_u32         v2 v13
  v15  = and_32          v3 v14
  v16  = half_from_i32   v15
  v17  = mul_half        v4 v16
  v18  = imm_32          0x18
  v19  = shr_u32         v2 v18
  v20  = and_32          v3 v19
  v21  = half_from_i32   v20
  v22  = mul_half        v4 v21
  v23  = load_half       p1 v1
  v24  = load_half       p2 v1
  v25  = load_half       p3 v1
  v26  = load_half       p4 v1
  v27  = imm_half        0x3c00
  v28  = sub_half        v27 v22
  v29  = mul_half        v23 v28
  v30  = fma_half        v4 v6 v29
  v31  = mul_half        v24 v28
  v32  = fma_half        v4 v11 v31
  v33  = mul_half        v25 v28
  v34  = fma_half        v4 v16 v33
  v35  = mul_half        v26 v28
  v36  = fma_half        v4 v21 v35
      store_half p1 v1 v30
      store_half p2 v1 v32
      store_half p3 v1 v34
      store_half p4 v1 v36
