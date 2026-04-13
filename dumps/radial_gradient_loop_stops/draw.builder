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
  v10  = sub_f32         v3 v7
  v11  = sub_f32         v4 v8
  v12  = mul_f32         v10 v10
  v13  = mul_f32         v11 v11
  v14  = fma_f32         v10 v10 v13
  v15  = sqrt_f32        v14
  v16  = mul_f32         v9 v15
  v17  = max_f32         v0 v16
  v18  = max_f32_imm     v16 0x0
  v19  = imm_32          0x3f800000
  v20  = min_f32         v18 v19
  v21  = min_f32_imm     v18 0x3f800000 (a.k.a. v19)
  v22  = uniform_32      p0 [3]
  v23  = i32_from_f32    v22
  v24  = imm_32          0x1
  v25  = sub_i32         v23 v24
  v26  = sub_i32_imm     v23 0x1 (a.k.a. v24)
  v27  = add_i32         v23 v23
  v28  = add_i32         v23 v27
  v29  = loop_begin      v26
  v30  = load_var        var4
  v31  = add_i32         v24 v30
  v32  = add_i32_imm     v30 0x1 (a.k.a. v24)
  v33  = gather_uniform_32 p-2147483642 v30
  v34  = gather_uniform_32 p-2147483642 v32
  v35  = le_f32          v33 v21
  v36  = le_f32          v21 v34
  v37  = and_32          v35 v36
  v38  = if_begin        v37
  v39  = sub_f32         v21 v33
  v40  = sub_f32         v34 v33
  v41  = div_f32         v39 v40
  v42  = gather_uniform_32 p-2147483643 v30
  v43  = gather_uniform_32 p-2147483643 v32
  v44  = add_i32         v23 v30
  v45  = gather_uniform_32 p-2147483643 v44
  v46  = add_i32         v23 v32
  v47  = gather_uniform_32 p-2147483643 v46
  v48  = add_i32         v27 v30
  v49  = gather_uniform_32 p-2147483643 v48
  v50  = add_i32         v27 v32
  v51  = gather_uniform_32 p-2147483643 v50
  v52  = add_i32         v28 v30
  v53  = gather_uniform_32 p-2147483643 v52
  v54  = add_i32         v28 v32
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
  v74  = add_i32         v24 v73
  v75  = add_i32_imm     v73 0x1 (a.k.a. v24)
      store_var       var4 v75
      loop_end
  v78  = load_var        var0
  v79  = load_var        var1
  v80  = load_var        var2
  v81  = load_var        var3
  v82  = f16_from_f32    v78
  v83  = f16_from_f32    v79
  v84  = f16_from_f32    v80
  v85  = f16_from_f32    v81
      store_16x4      p1 v82 v83 v84 v85
