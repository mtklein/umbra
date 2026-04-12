  v0   = imm_32          0x0
  v1   = deref_ptr       p0 byte16
  v2   = deref_ptr       p0 byte40
  v3   = uniform_32      p0 byte0
  v4   = uniform_32      p0 byte4
  v5   = uniform_32      p0 byte8
  v6   = imm_32          0x3f800000
  v7   = uniform_32      p0 byte12
  v8   = i32_from_f32    v7
  v9   = imm_32          0x1
  v10  = sub_i32_imm     v8 0x1 (a.k.a. v9)
  v11  = add_i32         v8 v8
  v12  = add_i32         v8 v11
  v13  = x              
  v14  = f32_from_i32    v13
  v15  = y              
  v16  = f32_from_i32    v15
  v17  = mul_f32         v16 v4
  v18  = fma_f32         v14 v3 v17
  v19  = add_f32         v5 v18
  v20  = max_f32_imm     v19 0x0
  v21  = min_f32_imm     v20 0x3f800000 (a.k.a. v6)
  v22  = loop_begin      v10
  v23  = load_var        var4
  v24  = add_i32_imm     v23 0x1 (a.k.a. v9)
  v25  = gather_uniform_32 p-2147483646 v24
  v26  = le_f32          v21 v25
  v27  = add_i32         v12 v24
  v28  = gather_uniform_32 p-2147483647 v27
  v29  = add_i32         v8 v24
  v30  = gather_uniform_32 p-2147483647 v29
  v31  = add_i32         v11 v24
  v32  = gather_uniform_32 p-2147483647 v31
  v33  = add_i32         v12 v23
  v34  = gather_uniform_32 p-2147483647 v33
  v35  = sub_f32         v28 v34
  v36  = gather_uniform_32 p-2147483646 v23
  v37  = le_f32          v36 v21
  v38  = and_32          v37 v26
  v39  = sub_f32         v25 v36
  v40  = sub_f32         v21 v36
  v41  = div_f32         v40 v39
  v42  = fma_f32         v41 v35 v34
      cond_store_var  var3 v38 v42
  v44  = add_i32         v8 v23
  v45  = gather_uniform_32 p-2147483647 v44
  v46  = sub_f32         v30 v45
  v47  = fma_f32         v41 v46 v45
      cond_store_var  var1 v38 v47
  v49  = add_i32         v11 v23
  v50  = gather_uniform_32 p-2147483647 v49
  v51  = sub_f32         v32 v50
  v52  = fma_f32         v41 v51 v50
      cond_store_var  var2 v38 v52
  v54  = gather_uniform_32 p-2147483647 v24
  v55  = gather_uniform_32 p-2147483647 v23
  v56  = sub_f32         v54 v55
  v57  = fma_f32         v41 v56 v55
      cond_store_var  var0 v38 v57
  v59  = load_var        var4
  v60  = add_i32_imm     v59 0x1 (a.k.a. v9)
      store_var       var4 v60
      loop_end
  v63  = load_var        var0
  v64  = f16_from_f32    v63
  v65  = load_var        var1
  v66  = f16_from_f32    v65
  v67  = load_var        var2
  v68  = f16_from_f32    v67
  v69  = load_var        var3
  v70  = f16_from_f32    v69
      store_16x4      p1 v64 v66 v68 v70
