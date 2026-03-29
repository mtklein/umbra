  v0   = imm_32          0x0
  v1   = uniform_32      p0[0]
  v2   = uniform_32      p0[1]
  v3   = uniform_32      p0[2]
  v4   = uniform_32      p0[3]
  v5   = imm_32          0x3f800000
  v6   = sub_f32         v5 v4
  v7   = load_color      p1
  v8   = fma_f32         v7 v6 v4
  v9   = fma_f32         v7 v6 v1
  v10  = fma_f32         v7 v6 v3
  v11  = fma_f32         v7 v6 v2
      store_color     p1 v11
