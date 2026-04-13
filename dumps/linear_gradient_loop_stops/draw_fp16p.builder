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
  v16  = imm_32          0x3f800000
  v17  = min_f32         v15 v16
  v18  = min_f32_imm     v15 0x3f800000 (a.k.a. v16)
  v19  = uniform_32      p0 [3]
  v20  = i32_from_f32    v19
  v21  = imm_32          0x1
  v22  = sub_i32         v20 v21
  v23  = sub_i32_imm     v20 0x1 (a.k.a. v21)
  v24  = add_i32         v20 v20
  v25  = add_i32         v20 v24
  v26  = loop_begin      v23
  v27  = load_var        var4
  v28  = add_i32         v21 v27
  v29  = add_i32_imm     v27 0x1 (a.k.a. v21)
  v30  = gather_uniform_32 p-2147483642 v27
  v31  = gather_uniform_32 p-2147483642 v29
  v32  = le_f32          v30 v18
  v33  = le_f32          v18 v31
  v34  = and_32          v32 v33
  v35  = if_begin        v34
  v36  = sub_f32         v18 v30
  v37  = sub_f32         v31 v30
  v38  = div_f32         v36 v37
  v39  = gather_uniform_32 p-2147483643 v27
  v40  = gather_uniform_32 p-2147483643 v29
  v41  = add_i32         v20 v27
  v42  = gather_uniform_32 p-2147483643 v41
  v43  = add_i32         v20 v29
  v44  = gather_uniform_32 p-2147483643 v43
  v45  = add_i32         v24 v27
  v46  = gather_uniform_32 p-2147483643 v45
  v47  = add_i32         v24 v29
  v48  = gather_uniform_32 p-2147483643 v47
  v49  = add_i32         v25 v27
  v50  = gather_uniform_32 p-2147483643 v49
  v51  = add_i32         v25 v29
  v52  = gather_uniform_32 p-2147483643 v51
  v53  = sub_f32         v40 v39
  v54  = mul_f32         v38 v53
  v55  = fma_f32         v38 v53 v39
      store_var       var0 v55
  v57  = sub_f32         v44 v42
  v58  = mul_f32         v38 v57
  v59  = fma_f32         v38 v57 v42
      store_var       var1 v59
  v61  = sub_f32         v48 v46
  v62  = mul_f32         v38 v61
  v63  = fma_f32         v38 v61 v46
      store_var       var2 v63
  v65  = sub_f32         v52 v50
  v66  = mul_f32         v38 v65
  v67  = fma_f32         v38 v65 v50
      store_var       var3 v67
      if_end
  v70  = load_var        var4
  v71  = add_i32         v21 v70
  v72  = add_i32_imm     v70 0x1 (a.k.a. v21)
      store_var       var4 v72
      loop_end
  v75  = load_var        var0
  v76  = load_var        var1
  v77  = load_var        var2
  v78  = load_var        var3
  v79  = f16_from_f32    v75
  v80  = f16_from_f32    v76
  v81  = f16_from_f32    v77
  v82  = f16_from_f32    v78
      store_16x4_planar p1 v79 v80 v81 v82
