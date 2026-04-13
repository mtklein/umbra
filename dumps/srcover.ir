  v0   = imm_32          0x0
  v1   = imm_32          0xff
  v2   = imm_32          0x3b808081
  v3   = imm_32          0x3f800000
  v4   = load_32         p0
  v5   = and_32_imm      v4 0xff (a.k.a. v1)
  v6   = f32_from_i32    v5
  v7   = mul_f32_imm     v6 0x3b808081 (a.k.a. v2)
  v8   = shr_u32_imm     v4 24
  v9   = f32_from_i32    v8
  v10  = mul_f32_imm     v9 0x3b808081 (a.k.a. v2)
  v11  = sub_f32         v3 v10
  v12  = shr_u32_imm     v4 8
  v13  = and_32_imm      v12 0xff (a.k.a. v1)
  v14  = f32_from_i32    v13
  v15  = mul_f32_imm     v14 0x3b808081 (a.k.a. v2)
  v16  = shr_u32_imm     v4 16
  v17  = and_32_imm      v16 0xff (a.k.a. v1)
  v18  = f32_from_i32    v17
  v19  = mul_f32_imm     v18 0x3b808081 (a.k.a. v2)
  v20  = load_16         p1
  v21  = f32_from_f16    v20
  v22  = fma_f32         v21 v11 v7
  v23  = f16_from_f32    v22
      store_16        p1 v23
  v25  = load_16         p2
  v26  = f32_from_f16    v25
  v27  = fma_f32         v26 v11 v15
  v28  = f16_from_f32    v27
      store_16        p2 v28
  v30  = load_16         p3
  v31  = f32_from_f16    v30
  v32  = fma_f32         v31 v11 v19
  v33  = f16_from_f32    v32
      store_16        p3 v33
  v35  = load_16         p4
  v36  = f32_from_f16    v35
  v37  = fma_f32         v36 v11 v10
  v38  = f16_from_f32    v37
      store_16        p4 v38
