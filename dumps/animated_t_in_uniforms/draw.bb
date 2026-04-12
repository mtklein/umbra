  v0   = imm_32          0x0
  v1   = uniform_32      p0 byte0
  v2   = uniform_32      p0 byte4
  v3   = uniform_32      p0 byte8
  v4   = uniform_32      p0 byte12
  v5   = f16_from_f32    v1
  v6   = f16_from_f32    v2
  v7   = f16_from_f32    v3
  v8   = f16_from_f32    v4
      store_16x4      p1 v5 v6 v7 v8
