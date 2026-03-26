  v0   = imm_32          0x0
  v1   = imm_32          0xff
  v2   = imm_32          0x3b808081
  v3   = imm_32          0x3f800000
  v4   = load_32         p0
  v5   = shr_u32_imm     v4 24
  v6   = f32_from_i32    v5
  v7   = mul_f32_imm     v6 0x3b808081 (a.k.a. v2)
  v8   = sub_f32         v3 v7
  v9   = shr_u32_imm     v4 8
  v10  = and_32_imm      v9 0xff (a.k.a. v1)
  v11  = f32_from_i32    v10
  v12  = mul_f32_imm     v11 0x3b808081 (a.k.a. v2)
  v13  = shr_u32_imm     v4 16
  v14  = and_32_imm      v13 0xff (a.k.a. v1)
  v15  = f32_from_i32    v14
  v16  = mul_f32_imm     v15 0x3b808081 (a.k.a. v2)
  v17  = and_32_imm      v4 0xff (a.k.a. v1)
  v18  = f32_from_i32    v17
  v19  = mul_f32_imm     v18 0x3b808081 (a.k.a. v2)
  v20  = load_16         p1
  v21  = f32_from_f16    v20
  v22  = fma_f32         v21 v8 v19
  v23  = f16_from_f32    v22
      store_16        p1 v23
  v25  = load_16         p2
  v26  = f32_from_f16    v25
  v27  = fma_f32         v26 v8 v12
  v28  = f16_from_f32    v27
      store_16        p2 v28
  v30  = load_16         p3
  v31  = f32_from_f16    v30
  v32  = fma_f32         v31 v8 v16
  v33  = f16_from_f32    v32
      store_16        p3 v33
  v35  = load_16         p4
  v36  = f32_from_f16    v35
  v37  = fma_f32         v36 v8 v7
  v38  = f16_from_f32    v37
      store_16        p4 v38
