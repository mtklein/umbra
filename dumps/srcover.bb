  v0   = imm_32          0x0
  v1   = lane           
  v2   = load_32         p0
  v3   = imm_32          0xff
  v4   = and_32          v2 v3
  v5   = imm_32          0x8
  v6   = shr_u32         v2 v5
  v7   = and_32          v3 v6
  v8   = imm_32          0x10
  v9   = shr_u32         v2 v8
  v10  = and_32          v3 v9
  v11  = imm_32          0x18
  v12  = shr_u32         v2 v11
  v13  = imm_32          0x3b808081
  v14  = f32_from_i32    v4
  v15  = mul_f32         v13 v14
  v16  = f32_from_i32    v7
  v17  = mul_f32         v13 v16
  v18  = f32_from_i32    v10
  v19  = mul_f32         v13 v18
  v20  = f32_from_i32    v12
  v21  = mul_f32         v13 v20
  v22  = load_16         p1
  v23  = widen_f16       v22
  v24  = load_16         p2
  v25  = widen_f16       v24
  v26  = load_16         p3
  v27  = widen_f16       v26
  v28  = load_16         p4
  v29  = widen_f16       v28
  v30  = imm_32          0x3f800000
  v31  = fms_f32         v13 v20 v30
  v32  = mul_f32         v23 v31
  v33  = fma_f32         v13 v14 v32
  v34  = mul_f32         v25 v31
  v35  = fma_f32         v13 v16 v34
  v36  = mul_f32         v27 v31
  v37  = fma_f32         v13 v18 v36
  v38  = mul_f32         v29 v31
  v39  = fma_f32         v13 v20 v38
  v40  = narrow_f32      v33
      store_16        p1 v40
  v42  = narrow_f32      v35
      store_16        p2 v42
  v44  = narrow_f32      v37
      store_16        p3 v44
  v46  = narrow_f32      v39
      store_16        p4 v46
