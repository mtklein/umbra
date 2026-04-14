  v0   = imm_32          0x0
  v1   = x              
  v2   = y              
  v3   = f32_from_i32    v1
  v4   = f32_from_i32    v2
  v5   = deref_ptr       p0 [4]
  v6   = deref_ptr       p0 [8]
  v7   = uniform_32      p0 [0]
  v8   = uniform_32      p0 [1]
  v9   = uniform_32      p0 [2]
  v10  = mul_f32         v3 v7
  v11  = mul_f32         v4 v8
  v12  = fma_f32         v3 v7 v11
  v13  = add_f32         v9 v12
  v14  = max_f32         v0 v13
  v15  = max_f32_imm     v13 0x0
  v16  = join            v14 v15
  v17  = imm_32          0x3f800000
  v18  = min_f32         v16 v17
  v19  = min_f32_imm     v16 0x3f800000 (a.k.a. v17)
  v20  = join            v18 v19
  v21  = uniform_32      p0 [3]
  v22  = i32_from_f32    v21
  v23  = imm_32          0x1
  v24  = sub_i32         v22 v23
  v25  = sub_i32_imm     v22 0x1 (a.k.a. v23)
  v26  = add_i32         v22 v22
  v27  = add_i32         v22 v26
  v28  = loop_begin      v25
  v29  = load_var        var4
  v30  = add_i32         v23 v29
  v31  = add_i32_imm     v29 0x1 (a.k.a. v23)
  v32  = join            v30 v31
  v33  = gather_uniform_32 p-2147483642 v29
  v34  = gather_uniform_32 p-2147483642 v32
  v35  = le_f32          v33 v20
  v36  = le_f32          v20 v34
  v37  = and_32          v35 v36
  v38  = if_begin        v37
  v39  = sub_f32         v20 v33
  v40  = sub_f32         v34 v33
  v41  = div_f32         v39 v40
  v42  = gather_uniform_32 p-2147483643 v29
  v43  = gather_uniform_32 p-2147483643 v32
  v44  = add_i32         v22 v29
  v45  = gather_uniform_32 p-2147483643 v44
  v46  = add_i32         v22 v32
  v47  = gather_uniform_32 p-2147483643 v46
  v48  = add_i32         v26 v29
  v49  = gather_uniform_32 p-2147483643 v48
  v50  = add_i32         v26 v32
  v51  = gather_uniform_32 p-2147483643 v50
  v52  = add_i32         v27 v29
  v53  = gather_uniform_32 p-2147483643 v52
  v54  = add_i32         v27 v32
  v55  = gather_uniform_32 p-2147483643 v54
  v56  = sub_f32         v43 v42
  v57  = mul_f32         v41 v56
  v58  = fma_f32         v41 v56 v42
      store_var       var0 v58
  v60  = sub_f32         v47 v45
  v61  = mul_f32         v41 v60
  v62  = fma_f32         v41 v60 v45
      store_var       var1 v62
  v64  = sub_f32         v51 v49
  v65  = mul_f32         v41 v64
  v66  = fma_f32         v41 v64 v49
      store_var       var2 v66
  v68  = sub_f32         v55 v53
  v69  = mul_f32         v41 v68
  v70  = fma_f32         v41 v68 v53
      store_var       var3 v70
      if_end
  v73  = load_var        var4
  v74  = add_i32         v23 v73
  v75  = add_i32_imm     v73 0x1 (a.k.a. v23)
  v76  = join            v74 v75
      store_var       var4 v76
      loop_end
  v79  = load_var        var0
  v80  = load_var        var1
  v81  = load_var        var2
  v82  = load_var        var3
  v83  = f16_from_f32    v79
  v84  = f16_from_f32    v80
  v85  = f16_from_f32    v81
  v86  = f16_from_f32    v82
      store_16x4_planar p1 v83 v84 v85 v86
