#pragma once
#include "flat_ir.h"
#include <stdio.h>

struct jit_backend;
void jit_acquire_code_buf(struct jit_backend*, void **mem, size_t *size, size_t min_size);
void jit_release_code_buf(struct jit_backend*, void  *mem, size_t  size);

struct jit_label {
    char const *name;
    int         byte_off;
    int         pad;
};

struct jit_program {
    struct umbra_program base;
    void  *code;
    size_t code_size;
    void (*entry)(int, int, int, int, struct umbra_buf*);
    int code_bytes;   // bytes of code before the constant pool
    int bindings;
    struct buffer_binding *binding;
    int labels, :32;
    struct jit_label label[16];
};
struct jit_program* jit_program(struct jit_backend*, struct umbra_flat_ir const*);
void   jit_program_run (struct jit_program*, int,int,int,int, struct umbra_buf[]);
void   jit_program_dump(struct jit_program const*, FILE*);

// Stream llvm-objdump -d output for `obj_path` to `out`, prepending
// `# <label>:` lines at matching byte offsets and stripping the leading
// address column.  Assumes the .o has a single __text section starting at
// offset 0.  Returns 1 on success.
_Bool jit_dump_with_labels(FILE *out, char const *obj_path,
                           struct jit_label const *label, int labels);
