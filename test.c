#include "len.h"
#include "test.h"
#include "umbra.h"
#include <stdint.h>

static void test_mul_f32(void) {
    struct umbra_inst const inst[] = {
        [0] = {.op=op_lane},
        [1] = {op_load_32, .ptr=0, .x=0},
        [2] = {op_load_32, .ptr=1, .x=0},
        [3] = {op_mul_f32, .x=1, .y=2},
        [4] = {op_store_32, .ptr=2, .x=0, .y=3},
    };
    struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));

    float x[] = {1,2,3,4,5},
          y[] = {6,7,8,9,0},
          z[len(x)] = {0};
    umbra_interpreter_run(p,len(z), (void*[]){x,y,z});

    equiv(z[0],  6) here;
    equiv(z[1], 14) here;
    equiv(z[2], 24) here;
    equiv(z[3], 36) here;
    equiv(z[4],  0) here;

    umbra_interpreter_free(p);
}

static void test_f32_ops(void) {
    // add
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_add_f32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 11) here;
        equiv(z[1], 22) here;
        equiv(z[2], 33) here;
        umbra_interpreter_free(p);
    }
    // sub
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_sub_f32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0],  9) here;
        equiv(z[1], 18) here;
        equiv(z[2], 27) here;
        umbra_interpreter_free(p);
    }
    // div
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_div_f32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 5) here;
        equiv(z[1], 5) here;
        equiv(z[2], 6) here;
        umbra_interpreter_free(p);
    }
}

static void test_i32_ops(void) {
    // add
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_add_i32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 11) here;
        (z[1] == 22) here;
        (z[2] == 33) here;
        umbra_interpreter_free(p);
    }
    // sub
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_sub_i32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] ==  9) here;
        (z[1] == 18) here;
        (z[2] == 27) here;
        umbra_interpreter_free(p);
    }
    // mul
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_mul_i32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 10) here;
        (z[1] == 18) here;
        (z[2] == 28) here;
        umbra_interpreter_free(p);
    }
    // shl
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_shl_i32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {1,3,7}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] ==  2) here;
        (z[1] == 12) here;
        (z[2] == 56) here;
        umbra_interpreter_free(p);
    }
    // shr (logical, unsigned)
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_shr_u32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {-1, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == (int)(0xFFFFFFFFu >> 1)) here;  // logical: top bit cleared
        (z[1] == 4) here;
        (z[2] == 8) here;
        umbra_interpreter_free(p);
    }
    // sra (arithmetic, signed)
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_shr_s32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {-8, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -4) here;  // arithmetic: sign bit preserved
        (z[1] ==  4) here;
        (z[2] ==  8) here;
        umbra_interpreter_free(p);
    }
    // and, or, xor
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_and_32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {0xFF, 0x0F}, y[] = {0x0F, 0xFF}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0x0F) here;
        (z[1] == 0x0F) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_or_32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {0xF0, 0x0F}, y[] = {0x0F, 0xF0}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0xFF) here;
        (z[1] == 0xFF) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_xor_32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {0xFF, 0xFF}, y[] = {0x0F, 0xFF}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0xF0) here;
        (z[1] == 0x00) here;
        umbra_interpreter_free(p);
    }
    // sel: (cond & a) | (~cond & b)
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},  // cond
            {op_load_32, .ptr=1, .x=0},  // a
            {op_load_32, .ptr=2, .x=0},  // b
            {op_sel_32, .x=1, .y=2, .z=3},
            {op_store_32, .ptr=3, .x=0, .y=4},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int cond[] = {-1, 0, -1},  // -1 = all bits set, 0 = no bits
              a[] = {10, 20, 30},
              b[] = {40, 50, 60},
              z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){cond, a, b, z});
        (z[0] == 10) here;  // cond=-1: pick a
        (z[1] == 50) here;  // cond=0:  pick b
        (z[2] == 30) here;  // cond=-1: pick a
        umbra_interpreter_free(p);
    }
}

static void test_f16_ops(void) {
    // mul
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_mul_f16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 10) here;
        equiv((float)z[1], 18) here;
        equiv((float)z[2], 28) here;
        umbra_interpreter_free(p);
    }
    // add
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_add_f16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 11) here;
        equiv((float)z[1], 22) here;
        equiv((float)z[2], 33) here;
        umbra_interpreter_free(p);
    }
    // sub
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_sub_f16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0],  9) here;
        equiv((float)z[1], 18) here;
        equiv((float)z[2], 27) here;
        umbra_interpreter_free(p);
    }
    // div
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_div_f16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 5) here;
        equiv((float)z[1], 5) here;
        equiv((float)z[2], 6) here;
        umbra_interpreter_free(p);
    }
}

static void test_i16_ops(void) {
    // add
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_add_i16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 11) here;
        (z[1] == 22) here;
        (z[2] == 33) here;
        umbra_interpreter_free(p);
    }
    // sub
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_sub_i16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] ==  9) here;
        (z[1] == 18) here;
        (z[2] == 27) here;
        umbra_interpreter_free(p);
    }
    // mul
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_mul_i16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 10) here;
        (z[1] == 18) here;
        (z[2] == 28) here;
        umbra_interpreter_free(p);
    }
    // shl, shr, sra
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_shl_i16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {1,3}, y[] = {4,2}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 16) here;
        (z[1] == 12) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_shr_u16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {-1, 64}, y[] = {1, 3}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == (short)(0xFFFFu >> 1)) here;  // logical: top bit cleared
        (z[1] == 8) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_shr_s16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {-8, 64}, y[] = {1, 3}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == -4) here;  // arithmetic: sign preserved
        (z[1] ==  8) here;
        umbra_interpreter_free(p);
    }
    // and, or, xor
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_and_16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {0xFF}, y[] = {0x0F}, z[1] = {0};
        umbra_interpreter_run(p, 1, (void*[]){x,y,z});
        (z[0] == 0x0F) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_or_16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {0xF0}, y[] = {0x0F}, z[1] = {0};
        umbra_interpreter_run(p, 1, (void*[]){x,y,z});
        (z[0] == 0xFF) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_xor_16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {0xFF}, y[] = {0x0F}, z[1] = {0};
        umbra_interpreter_run(p, 1, (void*[]){x,y,z});
        (z[0] == 0xF0) here;
        umbra_interpreter_free(p);
    }
    // sel
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_load_16, .ptr=2, .x=0},
            {op_sel_16, .x=1, .y=2, .z=3},
            {op_store_16, .ptr=3, .x=0, .y=4},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short cond[] = {-1, 0},
                 a[] = {10, 20},
                 b[] = {30, 40},
                 z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){cond, a, b, z});
        (z[0] == 10) here;
        (z[1] == 40) here;
        umbra_interpreter_free(p);
    }
}

static void test_cmp_i32(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_eq_i32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_ne_i32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {1,2}, y[] = {1,9}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] ==  0) here;
        (z[1] == -1) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_lt_s32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_le_s32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] == -1) here;
        (z[2] ==  0) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_gt_s32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_ge_s32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] == -1) here;
        (z[2] ==  0) here;
        umbra_interpreter_free(p);
    }
    // unsigned
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_lt_u32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {1, -1}, y[] = {2, 1}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        umbra_interpreter_free(p);
    }
    // sel with comparison mask
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_lt_s32, .x=1, .y=2},
            {op_sel_32, .x=3, .y=1, .z=2},  // pick min(x,y)
            {op_store_32, .ptr=2, .x=0, .y=4},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == 2) here;
        (z[1] == 1) here;
        (z[2] == 3) here;
        umbra_interpreter_free(p);
    }
}

static void test_cmp_i16(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_eq_i16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_lt_s16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_interpreter_free(p);
    }
    // sel with comparison mask
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_lt_s16, .x=1, .y=2},
            {op_sel_16, .x=3, .y=1, .z=2},
            {op_store_16, .ptr=2, .x=0, .y=4},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short x[] = {5,1}, y[] = {2,4}, z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        (z[0] == 2) here;
        (z[1] == 1) here;
        umbra_interpreter_free(p);
    }
}

static void test_cmp_f32(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_eq_f32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {1,2,3}, y[] = {1,9,3};
        int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_lt_f32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {1,5,3}, y[] = {2,5,1};
        int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_interpreter_free(p);
    }
    // sel with f32 comparison: pick min
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_lt_f32, .x=1, .y=2},
            {op_sel_32, .x=3, .y=1, .z=2},
            {op_store_32, .ptr=2, .x=0, .y=4},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 2) here;
        equiv(z[1], 1) here;
        equiv(z[2], 3) here;
        umbra_interpreter_free(p);
    }
}

static void test_cmp_f16(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_eq_f16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {1,2,3}, y[] = {1,9,3};
        short z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_lt_f16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {1,5,3}, y[] = {2,5,1};
        short z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_interpreter_free(p);
    }
    // sel with f16 comparison: pick min
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_lt_f16, .x=1, .y=2},
            {op_sel_16, .x=3, .y=1, .z=2},
            {op_store_16, .ptr=2, .x=0, .y=4},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {5,1}, y[] = {2,4};
        __fp16 z[2] = {0};
        umbra_interpreter_run(p, 2, (void*[]){x,y,z});
        equiv((float)z[0], 2) here;
        equiv((float)z[1], 1) here;
        umbra_interpreter_free(p);
    }
}

static void test_imm(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_imm_32, .immi=42},
            {op_store_32, .ptr=0, .x=0, .y=1},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){z});
        (z[0] == 42) here;
        (z[1] == 42) here;
        (z[2] == 42) here;
        umbra_interpreter_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_imm_16, .immi=7},
            {op_store_16, .ptr=0, .x=0, .y=1},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        short z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){z});
        (z[0] == 7) here;
        (z[1] == 7) here;
        (z[2] == 7) here;
        umbra_interpreter_free(p);
    }
}

static void test_fma_f32(void) {
    struct umbra_inst const inst[] = {
        {.op=op_lane},
        {op_load_32, .ptr=0, .x=0},
        {op_load_32, .ptr=1, .x=0},
        {op_load_32, .ptr=2, .x=0},
        {op_fma_f32, .x=1, .y=2, .z=3},
        {op_store_32, .ptr=3, .x=0, .y=4},
    };
    struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
    float x[] = {2,3}, y[] = {4,5}, w[] = {10,20}, z[2] = {0};
    umbra_interpreter_run(p, 2, (void*[]){x,y,w,z});
    equiv(z[0], 18) here;  // 2*4+10
    equiv(z[1], 35) here;  // 3*5+20
    umbra_interpreter_free(p);
}

static void test_fma_f16(void) {
    struct umbra_inst const inst[] = {
        {.op=op_lane},
        {op_load_16, .ptr=0, .x=0},
        {op_load_16, .ptr=1, .x=0},
        {op_load_16, .ptr=2, .x=0},
        {op_fma_f16, .x=1, .y=2, .z=3},
        {op_store_16, .ptr=3, .x=0, .y=4},
    };
    struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
    __fp16 x[] = {2,3}, y[] = {4,5}, w[] = {10,20}, z[2] = {0};
    umbra_interpreter_run(p, 2, (void*[]){x,y,w,z});
    equiv((float)z[0], 18) here;
    equiv((float)z[1], 35) here;
    umbra_interpreter_free(p);
}

static void test_min_max_sqrt_f32(void) {
    // min
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_min_f32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 2) here;
        equiv(z[1], 1) here;
        equiv(z[2], 3) here;
        umbra_interpreter_free(p);
    }
    // max
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_max_f32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 5) here;
        equiv(z[1], 4) here;
        equiv(z[2], 3) here;
        umbra_interpreter_free(p);
    }
    // sqrt
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_sqrt_f32, .x=1},
            {op_store_32, .ptr=1, .x=0, .y=2},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {4,9,16}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,z});
        equiv(z[0], 2) here;
        equiv(z[1], 3) here;
        equiv(z[2], 4) here;
        umbra_interpreter_free(p);
    }
}

static void test_min_max_sqrt_f16(void) {
    // min
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_min_f16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 2) here;
        equiv((float)z[1], 1) here;
        equiv((float)z[2], 3) here;
        umbra_interpreter_free(p);
    }
    // max
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_load_16, .ptr=1, .x=0},
            {op_max_f16, .x=1, .y=2},
            {op_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 5) here;
        equiv((float)z[1], 4) here;
        equiv((float)z[2], 3) here;
        umbra_interpreter_free(p);
    }
    // sqrt
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_16, .ptr=0, .x=0},
            {op_sqrt_f16, .x=1},
            {op_store_16, .ptr=1, .x=0, .y=2},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        __fp16 x[] = {4,9,16}, z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,z});
        equiv((float)z[0], 2) here;
        equiv((float)z[1], 3) here;
        equiv((float)z[2], 4) here;
        umbra_interpreter_free(p);
    }
}

// Test with n > K to exercise both vector bulk and scalar tail paths.
static void test_large_n(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_load_32, .ptr=1, .x=0},
            {op_add_f32, .x=1, .y=2},
            {op_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[11], y[11], z[11];
        for (int i = 0; i < 11; i++) {
            x[i] = (float)i;
            y[i] = (float)(10 - i);
        }
        umbra_interpreter_run(p, 11, (void*[]){x,y,z});
        for (int i = 0; i < 11; i++) {
            equiv(z[i], 10) here;
        }
        umbra_interpreter_free(p);
    }
}

static void test_uni_via_load(void) {
    // load(ptr, imm(0)) broadcasts element 0
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_imm_32, .immi=0},
            {op_load_32, .ptr=0, .x=1},
            {op_store_32, .ptr=1, .x=0, .y=2},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int src[] = {42, 99, 7};
        int dst[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){src, dst});
        (dst[0] == 42) here;
        (dst[1] == 42) here;
        (dst[2] == 42) here;
        umbra_interpreter_free(p);
    }
    // load(ptr, imm(2)) broadcasts element 2
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_imm_32, .immi=2},
            {op_load_32, .ptr=0, .x=1},
            {op_store_32, .ptr=1, .x=0, .y=2},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int src[] = {42, 99, 7};
        int dst[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){src, dst});
        (dst[0] == 7) here;
        (dst[1] == 7) here;
        (dst[2] == 7) here;
        umbra_interpreter_free(p);
    }
}

static void test_scatter(void) {
    // Store to reversed indices
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},   // load src contiguously
            {op_load_32, .ptr=1, .x=0},   // load idx contiguously
            {op_store_32, .ptr=2, .x=2, .y=1},  // scatter: idx=reg2, data=reg1
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int src[] = {10, 20, 30};
        int idx[] = {2, 1, 0};
        int dst[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){src, idx, dst});
        (dst[0] == 30) here;
        (dst[1] == 20) here;
        (dst[2] == 10) here;
        umbra_interpreter_free(p);
    }
}

static void test_canonicalize(void) {
    struct umbra_inst inst[] = {
        {.op=op_lane},
        {op_load_32, .ptr=0, .x=0},
        {op_load_32, .ptr=1, .x=0},
        {op_add_i32, .x=2, .y=1},
        {op_store_32, .ptr=2, .x=0, .y=3},
    };
    int const n = umbra_optimize(inst, len(inst));
    (n == 5) here;
    // After canonicalization, the add should have x <= y.
    _Bool found = 0;
    for (int i = 0; i < n; i++) {
        if (inst[i].op == op_add_i32) {
            (inst[i].x <= inst[i].y) here;
            found = 1;
        }
    }
    found here;
}

static void test_gvn(void) {
    struct umbra_inst inst[] = {
        {.op=op_lane},
        {.op=op_imm_32, .immi=42},
        {.op=op_imm_32, .immi=42},  // dup of [1]
        {.op=op_imm_32, .immi=99},
        {op_add_i32, .x=1, .y=3},   // uses 42 and 99
        {op_store_32, .ptr=0, .x=0, .y=4},
    };
    int const before = len(inst);
    int const after = umbra_optimize(inst, before);
    (after < before) here;  // dups removed, constants folded
}

static void test_fma_fusion(void) {
    struct umbra_inst inst[] = {
        {.op=op_lane},
        {op_load_32, .ptr=0, .x=0},
        {op_load_32, .ptr=1, .x=0},
        {op_load_32, .ptr=2, .x=0},
        {op_mul_f32, .x=1, .y=2},
        {op_add_f32, .x=4, .y=3},
        {op_store_32, .ptr=3, .x=0, .y=5},
    };
    int const n = umbra_optimize(inst, len(inst));

    // Find the fma.
    _Bool found_fma = 0;
    for (int i = 0; i < n; i++) {
        if (inst[i].op == op_fma_f32) { found_fma = 1; }
    }
    found_fma here;

    // Verify it computes correctly: x*y+z.
    struct umbra_interpreter *p = umbra_interpreter(inst, n);
    float x[] = {2,3}, y[] = {4,5}, w[] = {10,20}, z[2] = {0};
    umbra_interpreter_run(p, 2, (void*[]){x,y,w,z});
    equiv(z[0], 18) here;
    equiv(z[1], 35) here;
    umbra_interpreter_free(p);
}

static void test_dce(void) {
    struct umbra_inst inst[] = {
        {.op=op_lane},
        {op_load_32, .ptr=0, .x=0},
        {op_load_32, .ptr=1, .x=0},
        {op_mul_f32, .x=1, .y=2},   // unused
        {op_add_f32, .x=1, .y=2},
        {op_store_32, .ptr=2, .x=0, .y=4},
    };
    int const before = len(inst);
    int const after = umbra_optimize(inst, before);
    (after < before) here;  // mul and its dead-only deps should be removed

    // Verify it still computes x+y correctly.
    struct umbra_interpreter *p = umbra_interpreter(inst, after);
    float x[] = {1,2}, y[] = {3,4}, z[2] = {0};
    umbra_interpreter_run(p, 2, (void*[]){x,y,z});
    equiv(z[0], 4) here;
    equiv(z[1], 6) here;
    umbra_interpreter_free(p);
}

static void test_convert(void) {
    // f32_from_i32
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_f32_from_i32, .x=1},
            {op_store_32, .ptr=1, .x=0, .y=2},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        int x[] = {1, 255, -3};
        float z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,z});
        equiv(z[0], 1) here;
        equiv(z[1], 255) here;
        equiv(z[2], -3) here;
        umbra_interpreter_free(p);
    }
    // i32_from_f32
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_i32_from_f32, .x=1},
            {op_store_32, .ptr=1, .x=0, .y=2},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {1.9f, 255.0f, -3.7f};
        int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,z});
        (z[0] == 1) here;
        (z[1] == 255) here;
        (z[2] == -3) here;
        umbra_interpreter_free(p);
    }
    // f16_from_f32 and f32_from_f16 roundtrip
    {
        struct umbra_inst const inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_f16_from_f32, .x=1},
            {op_f32_from_f16, .x=2},
            {op_store_32, .ptr=1, .x=0, .y=3},
        };
        struct umbra_interpreter *p = umbra_interpreter(inst, len(inst));
        float x[] = {1.0f, 0.5f, 100.0f};
        float z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){x,z});
        equiv(z[0], 1.0f) here;
        equiv(z[1], 0.5f) here;
        equiv(z[2], 100.0f) here;
        umbra_interpreter_free(p);
    }
}

static void test_constprop(void) {
    // add_i32(imm(3), imm(5)) → 8
    {
        struct umbra_inst inst[] = {
            {.op=op_lane},
            {op_imm_32, .immi=3},
            {op_imm_32, .immi=5},
            {op_add_i32, .x=1, .y=2},
            {op_store_32, .ptr=0, .x=0, .y=3},
        };
        int const n = umbra_optimize(inst, len(inst));
        struct umbra_interpreter *p = umbra_interpreter(inst, n);
        int z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){z});
        (z[0] == 8) here;
        (z[1] == 8) here;
        (z[2] == 8) here;
        umbra_interpreter_free(p);
    }
    // mul_f32(imm(2.0), imm(3.0)) → 6.0
    {
        struct umbra_inst inst[] = {
            {.op=op_lane},
            {op_imm_32, .immf=2.0f},
            {op_imm_32, .immf=3.0f},
            {op_mul_f32, .x=1, .y=2},
            {op_store_32, .ptr=0, .x=0, .y=3},
        };
        int const n = umbra_optimize(inst, len(inst));
        struct umbra_interpreter *p = umbra_interpreter(inst, n);
        float z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){z});
        equiv(z[0], 6) here;
        equiv(z[1], 6) here;
        equiv(z[2], 6) here;
        umbra_interpreter_free(p);
    }
    // f32_from_i32(imm(42)) → 42.0
    {
        struct umbra_inst inst[] = {
            {.op=op_lane},
            {op_imm_32, .immi=42},
            {op_f32_from_i32, .x=1},
            {op_store_32, .ptr=0, .x=0, .y=2},
        };
        int const n = umbra_optimize(inst, len(inst));
        struct umbra_interpreter *p = umbra_interpreter(inst, n);
        float z[3] = {0};
        umbra_interpreter_run(p, 3, (void*[]){z});
        equiv(z[0], 42) here;
        equiv(z[1], 42) here;
        equiv(z[2], 42) here;
        umbra_interpreter_free(p);
    }
}

static void test_hoisting(void) {
    // Mix of imm + lane + load + arith + store.
    // After optimization, uniforms should be hoisted before varyings.
    struct umbra_inst inst[] = {
        {.op=op_lane},
        {op_imm_32, .immf=2.0f},
        {op_load_32, .ptr=0, .x=0},
        {op_mul_f32, .x=2, .y=1},
        {op_store_32, .ptr=1, .x=0, .y=3},
    };
    int const n = umbra_optimize(inst, len(inst));
    (n == 5) here;  // instruction count unchanged

    // Verify the imm is hoisted before the lane.
    (inst[0].op == op_imm_32) here;

    // Verify correctness.
    struct umbra_interpreter *p = umbra_interpreter(inst, n);
    float x[] = {1,2,3,4,5,6,7,8,9,10,11}, z[11] = {0};
    umbra_interpreter_run(p, 11, (void*[]){x,z});
    for (int i = 0; i < 11; i++) {
        equiv(z[i], (float)x[i] * 2.0f) here;
    }
    umbra_interpreter_free(p);
}

static void test_strength_reduction(void) {
    // add_i32(load(x), imm(0)) → result == x
    {
        struct umbra_inst inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_imm_32, .immi=0},
            {op_add_i32, .x=1, .y=2},
            {op_store_32, .ptr=1, .x=0, .y=3},
        };
        int const n = umbra_optimize(inst, len(inst));
        struct umbra_interpreter *p = umbra_interpreter(inst, n);
        int32_t x[] = {1,2,3,4,5}, z[5] = {0};
        umbra_interpreter_run(p, 5, (void*[]){x,z});
        for (int i = 0; i < 5; i++) { (z[i] == x[i]) here; }
        umbra_interpreter_free(p);
    }
    // mul_i32(imm(0), load(x)) → result == 0
    {
        struct umbra_inst inst[] = {
            {.op=op_lane},
            {op_imm_32, .immi=0},
            {op_load_32, .ptr=0, .x=0},
            {op_mul_i32, .x=1, .y=2},
            {op_store_32, .ptr=1, .x=0, .y=3},
        };
        int const n = umbra_optimize(inst, len(inst));
        struct umbra_interpreter *p = umbra_interpreter(inst, n);
        int32_t x[] = {1,2,3,4,5}, z[5] = {0};
        umbra_interpreter_run(p, 5, (void*[]){x,z});
        for (int i = 0; i < 5; i++) { (z[i] == 0) here; }
        umbra_interpreter_free(p);
    }
    // mul_f32(load(x), imm(1.0)) → result == x
    {
        struct umbra_inst inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_imm_32, .immf=1.0f},
            {op_mul_f32, .x=1, .y=2},
            {op_store_32, .ptr=1, .x=0, .y=3},
        };
        int const n = umbra_optimize(inst, len(inst));
        struct umbra_interpreter *p = umbra_interpreter(inst, n);
        float x[] = {1,2,3,4,5}, z[5] = {0};
        umbra_interpreter_run(p, 5, (void*[]){x,z});
        for (int i = 0; i < 5; i++) { equiv(z[i], x[i]) here; }
        umbra_interpreter_free(p);
    }
    // shl_i32(load(x), imm(0)) → result == x
    {
        struct umbra_inst inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_imm_32, .immi=0},
            {op_shl_i32, .x=1, .y=2},
            {op_store_32, .ptr=1, .x=0, .y=3},
        };
        int const n = umbra_optimize(inst, len(inst));
        struct umbra_interpreter *p = umbra_interpreter(inst, n);
        int32_t x[] = {1,2,3,4,5}, z[5] = {0};
        umbra_interpreter_run(p, 5, (void*[]){x,z});
        for (int i = 0; i < 5; i++) { (z[i] == x[i]) here; }
        umbra_interpreter_free(p);
    }
    // mul_i32(imm(8), load(x)) → result == x*8, check shl present
    {
        struct umbra_inst inst[] = {
            {.op=op_lane},
            {op_imm_32, .immi=8},
            {op_load_32, .ptr=0, .x=0},
            {op_mul_i32, .x=1, .y=2},
            {op_store_32, .ptr=1, .x=0, .y=3},
        };
        int const n = umbra_optimize(inst, len(inst));
        _Bool found_shl = 0;
        for (int j = 0; j < n; j++) {
            if (inst[j].op == op_shl_i32) { found_shl = 1; }
        }
        found_shl here;
        struct umbra_interpreter *p = umbra_interpreter(inst, n);
        int32_t x[] = {1,2,3,4,5}, z[5] = {0};
        umbra_interpreter_run(p, 5, (void*[]){x,z});
        for (int i = 0; i < 5; i++) { (z[i] == x[i] * 8) here; }
        umbra_interpreter_free(p);
    }
    // sub_i32(load(x), load(x)) → result == 0
    {
        struct umbra_inst inst[] = {
            {.op=op_lane},
            {op_load_32, .ptr=0, .x=0},
            {op_sub_i32, .x=1, .y=1},
            {op_store_32, .ptr=1, .x=0, .y=2},
        };
        int const n = umbra_optimize(inst, len(inst));
        struct umbra_interpreter *p = umbra_interpreter(inst, n);
        int32_t x[] = {1,2,3,4,5}, z[5] = {0};
        umbra_interpreter_run(p, 5, (void*[]){x,z});
        for (int i = 0; i < 5; i++) { (z[i] == 0) here; }
        umbra_interpreter_free(p);
    }
}

static void test_optimize_srcover(void) {
    // Build a naive SrcOver blend: 8888 src -> f32 dst (simplified).
    // ptr[0]=src(u32 packed RGBA), ptr[1..4]=dst R,G,B,A (fp16), ptr[5..8]=out R,G,B,A (f32).
    // Written naively with duplicate constants per channel.
    int n = 0;
    struct umbra_inst inst[64];
    __builtin_memset(inst, 0, sizeof inst);

    int const lane_  = n; inst[n++] = (struct umbra_inst){.op=op_lane};
    int const src_   = n; inst[n++] = (struct umbra_inst){op_load_32, .ptr=0, .x=lane_};

    // Extract R (bits 0..7): mask with 0xFF
    int const maskr  = n; inst[n++] = (struct umbra_inst){op_imm_32, .immi=0xFF};
    int const ri32   = n; inst[n++] = (struct umbra_inst){op_and_32, .x=src_, .y=maskr};
    int const rf32   = n; inst[n++] = (struct umbra_inst){op_f32_from_i32, .x=ri32};
    int const inv255r= n; inst[n++] = (struct umbra_inst){op_imm_32, .immf=1.0f/255.0f};
    int const srf32  = n; inst[n++] = (struct umbra_inst){op_mul_f32, .x=rf32, .y=inv255r};

    // Extract G (bits 8..15)
    int const sh8    = n; inst[n++] = (struct umbra_inst){op_imm_32, .immi=8};
    int const gshr   = n; inst[n++] = (struct umbra_inst){op_shr_u32, .x=src_, .y=sh8};
    int const maskg  = n; inst[n++] = (struct umbra_inst){op_imm_32, .immi=0xFF};
    int const gi32   = n; inst[n++] = (struct umbra_inst){op_and_32, .x=gshr, .y=maskg};
    int const gf32   = n; inst[n++] = (struct umbra_inst){op_f32_from_i32, .x=gi32};
    int const inv255g= n; inst[n++] = (struct umbra_inst){op_imm_32, .immf=1.0f/255.0f};
    int const sgf32  = n; inst[n++] = (struct umbra_inst){op_mul_f32, .x=gf32, .y=inv255g};

    // Extract B (bits 16..23)
    int const sh16   = n; inst[n++] = (struct umbra_inst){op_imm_32, .immi=16};
    int const bshr   = n; inst[n++] = (struct umbra_inst){op_shr_u32, .x=src_, .y=sh16};
    int const maskb  = n; inst[n++] = (struct umbra_inst){op_imm_32, .immi=0xFF};
    int const bi32   = n; inst[n++] = (struct umbra_inst){op_and_32, .x=bshr, .y=maskb};
    int const bf32   = n; inst[n++] = (struct umbra_inst){op_f32_from_i32, .x=bi32};
    int const inv255b= n; inst[n++] = (struct umbra_inst){op_imm_32, .immf=1.0f/255.0f};
    int const sbf32  = n; inst[n++] = (struct umbra_inst){op_mul_f32, .x=bf32, .y=inv255b};

    // Extract A (bits 24..31)
    int const sh24   = n; inst[n++] = (struct umbra_inst){op_imm_32, .immi=24};
    int const ashr   = n; inst[n++] = (struct umbra_inst){op_shr_u32, .x=src_, .y=sh24};
    int const maska  = n; inst[n++] = (struct umbra_inst){op_imm_32, .immi=0xFF};
    int const ai32   = n; inst[n++] = (struct umbra_inst){op_and_32, .x=ashr, .y=maska};
    int const af32   = n; inst[n++] = (struct umbra_inst){op_f32_from_i32, .x=ai32};
    int const inv255a= n; inst[n++] = (struct umbra_inst){op_imm_32, .immf=1.0f/255.0f};
    int const saf32  = n; inst[n++] = (struct umbra_inst){op_mul_f32, .x=af32, .y=inv255a};

    // Load dst fp16, convert to f32
    int const dr16   = n; inst[n++] = (struct umbra_inst){op_load_16, .ptr=1, .x=lane_};
    int const drf32  = n; inst[n++] = (struct umbra_inst){op_f32_from_f16, .x=dr16};
    int const dg16   = n; inst[n++] = (struct umbra_inst){op_load_16, .ptr=2, .x=lane_};
    int const dgf32  = n; inst[n++] = (struct umbra_inst){op_f32_from_f16, .x=dg16};
    int const db16   = n; inst[n++] = (struct umbra_inst){op_load_16, .ptr=3, .x=lane_};
    int const dbf32  = n; inst[n++] = (struct umbra_inst){op_f32_from_f16, .x=db16};
    int const da16   = n; inst[n++] = (struct umbra_inst){op_load_16, .ptr=4, .x=lane_};
    int const daf32  = n; inst[n++] = (struct umbra_inst){op_f32_from_f16, .x=da16};

    // Blend: out = src + dst * (1 - sa). Each channel gets its own "one" and "inv_a".
    // R
    int const oner   = n; inst[n++] = (struct umbra_inst){op_imm_32, .immf=1.0f};
    int const invar  = n; inst[n++] = (struct umbra_inst){op_sub_f32, .x=oner, .y=saf32};
    int const drmul  = n; inst[n++] = (struct umbra_inst){op_mul_f32, .x=drf32, .y=invar};
    int const rout   = n; inst[n++] = (struct umbra_inst){op_add_f32, .x=srf32, .y=drmul};
    // G
    int const oneg   = n; inst[n++] = (struct umbra_inst){op_imm_32, .immf=1.0f};
    int const invag  = n; inst[n++] = (struct umbra_inst){op_sub_f32, .x=oneg, .y=saf32};
    int const dgmul  = n; inst[n++] = (struct umbra_inst){op_mul_f32, .x=dgf32, .y=invag};
    int const gout   = n; inst[n++] = (struct umbra_inst){op_add_f32, .x=sgf32, .y=dgmul};
    // B
    int const oneb   = n; inst[n++] = (struct umbra_inst){op_imm_32, .immf=1.0f};
    int const invab  = n; inst[n++] = (struct umbra_inst){op_sub_f32, .x=oneb, .y=saf32};
    int const dbmul  = n; inst[n++] = (struct umbra_inst){op_mul_f32, .x=dbf32, .y=invab};
    int const bout   = n; inst[n++] = (struct umbra_inst){op_add_f32, .x=sbf32, .y=dbmul};
    // A
    int const onea   = n; inst[n++] = (struct umbra_inst){op_imm_32, .immf=1.0f};
    int const invaa  = n; inst[n++] = (struct umbra_inst){op_sub_f32, .x=onea, .y=saf32};
    int const damul  = n; inst[n++] = (struct umbra_inst){op_mul_f32, .x=daf32, .y=invaa};
    int const aout   = n; inst[n++] = (struct umbra_inst){op_add_f32, .x=saf32, .y=damul};

    // Store f32 outputs.
    inst[n++] = (struct umbra_inst){op_store_32, .ptr=5, .x=lane_, .y=rout};
    inst[n++] = (struct umbra_inst){op_store_32, .ptr=6, .x=lane_, .y=gout};
    inst[n++] = (struct umbra_inst){op_store_32, .ptr=7, .x=lane_, .y=bout};
    inst[n++] = (struct umbra_inst){op_store_32, .ptr=8, .x=lane_, .y=aout};

    int const before = n;
    int const after = umbra_optimize(inst, before);
    (after < before) here;

    // Verify: src=RGBA(128,64,32,255), alpha=1 → out = src.
    uint32_t src_px[] = {0xFF204080u, 0xFF204080u, 0xFF204080u};
    __fp16 dst_r[] = {0.5, 0.5, 0.5};
    __fp16 dst_g[] = {0.5, 0.5, 0.5};
    __fp16 dst_b[] = {0.5, 0.5, 0.5};
    __fp16 dst_a[] = {0.5, 0.5, 0.5};
    float out_r[3]={0}, out_g[3]={0}, out_b[3]={0}, out_a[3]={0};

    struct umbra_interpreter *p = umbra_interpreter(inst, after);
    umbra_interpreter_run(p, 3, (void*[]){src_px, dst_r, dst_g, dst_b, dst_a,
                                      out_r, out_g, out_b, out_a});

    float const tol = 0.01f;
    (out_r[0] > 0.50f - tol && out_r[0] < 0.50f + tol) here;
    (out_g[0] > 0.25f - tol && out_g[0] < 0.25f + tol) here;
    (out_b[0] > 0.12f - tol && out_b[0] < 0.13f + tol) here;
    umbra_interpreter_free(p);
}

// ── new builder API tests ────────────────────────────────────────────

static void test_bb_basic(void) {
    // Simple f32 multiply through builder API.
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 ix  = umbra_lane(bb);
    umbra_v32 x   = umbra_load_32(bb, (umbra_ptr){0}, ix);
    umbra_v32 y   = umbra_load_32(bb, (umbra_ptr){1}, ix);
    umbra_v32 z   = umbra_mul_f32(bb, x, y);
    umbra_store_32(bb, (umbra_ptr){2}, ix, z);

    float a[] = {1,2,3,4,5}, b[] = {6,7,8,9,0}, c[5] = {0};
    umbra_basic_block_exec(bb, 5, (void*[]){a,b,c});
    equiv(c[0],  6) here;
    equiv(c[1], 14) here;
    equiv(c[2], 24) here;
    equiv(c[3], 36) here;
    equiv(c[4],  0) here;
    umbra_basic_block_free(bb);
}

static void test_bb_dedup(void) {
    // Two identical imm_32(42) should produce the same handle.
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 a = umbra_imm_32(bb, 42);
    umbra_v32 b = umbra_imm_32(bb, 42);
    (a.id == b.id) here;

    // Different values should differ.
    umbra_v32 c = umbra_imm_32(bb, 99);
    (a.id != c.id) here;

    // Identical ops on identical operands should dedup.
    umbra_v32 ix = umbra_lane(bb);
    umbra_v32 x  = umbra_load_32(bb, (umbra_ptr){0}, ix);
    umbra_v32 s1 = umbra_add_i32(bb, x, a);
    umbra_v32 s2 = umbra_add_i32(bb, x, a);
    (s1.id == s2.id) here;

    umbra_basic_block_free(bb);
}

static void test_bb_constprop(void) {
    // add_i32(imm(3), imm(5)) should fold to imm(8).
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 ix = umbra_lane(bb);
    umbra_v32 a  = umbra_imm_32(bb, 3);
    umbra_v32 b  = umbra_imm_32(bb, 5);
    umbra_v32 s  = umbra_add_i32(bb, a, b);
    umbra_store_32(bb, (umbra_ptr){0}, ix, s);

    int z[3] = {0};
    umbra_basic_block_exec(bb, 3, (void*[]){z});
    (z[0] == 8) here;
    (z[1] == 8) here;
    (z[2] == 8) here;
    umbra_basic_block_free(bb);
}

static void test_bb_strength_reduction(void) {
    // add(x, imm(0)) → x
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb);
        umbra_v32 x  = umbra_load_32(bb, (umbra_ptr){0}, ix);
        umbra_v32 z  = umbra_imm_32(bb, 0);
        umbra_v32 s  = umbra_add_i32(bb, x, z);
        (s.id == x.id) here;  // should be identity
        umbra_basic_block_free(bb);
    }
    // mul(x, imm(1)) → x
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb);
        umbra_v32 x  = umbra_load_32(bb, (umbra_ptr){0}, ix);
        umbra_v32 one = umbra_imm_32(bb, 1);
        umbra_v32 s  = umbra_mul_i32(bb, x, one);
        (s.id == x.id) here;
        umbra_basic_block_free(bb);
    }
    // mul(x, imm(8)) → shl(x, 3), verify result
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix  = umbra_lane(bb);
        umbra_v32 x   = umbra_load_32(bb, (umbra_ptr){0}, ix);
        umbra_v32 eight = umbra_imm_32(bb, 8);
        umbra_v32 s   = umbra_mul_i32(bb, x, eight);
        umbra_store_32(bb, (umbra_ptr){1}, ix, s);

        int32_t a[] = {1,2,3,4,5}, b[5] = {0};
        umbra_basic_block_exec(bb, 5, (void*[]){a, b});
        for (int i = 0; i < 5; i++) { (b[i] == a[i] * 8) here; }
        umbra_basic_block_free(bb);
    }
    // sub(x, x) → imm(0)
    {
        struct umbra_basic_block *bb = umbra_basic_block();
        umbra_v32 ix = umbra_lane(bb);
        umbra_v32 x  = umbra_load_32(bb, (umbra_ptr){0}, ix);
        umbra_v32 s  = umbra_sub_i32(bb, x, x);
        umbra_store_32(bb, (umbra_ptr){1}, ix, s);

        int32_t a[] = {1,2,3}, b[3] = {0};
        umbra_basic_block_exec(bb, 3, (void*[]){a, b});
        for (int i = 0; i < 3; i++) { (b[i] == 0) here; }
        umbra_basic_block_free(bb);
    }
}

static void test_bb_fma(void) {
    // add(mul(a,b), c) → fma(a,b,c)
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 ix = umbra_lane(bb);
    umbra_v32 x  = umbra_load_32(bb, (umbra_ptr){0}, ix);
    umbra_v32 y  = umbra_load_32(bb, (umbra_ptr){1}, ix);
    umbra_v32 w  = umbra_load_32(bb, (umbra_ptr){2}, ix);
    umbra_v32 m  = umbra_mul_f32(bb, x, y);
    umbra_v32 s  = umbra_add_f32(bb, m, w);
    umbra_store_32(bb, (umbra_ptr){3}, ix, s);

    float a[] = {2,3}, b[] = {4,5}, c[] = {10,20}, z[2] = {0};
    umbra_basic_block_exec(bb, 2, (void*[]){a,b,c,z});
    equiv(z[0], 18) here;
    equiv(z[1], 35) here;
    umbra_basic_block_free(bb);
}

static void test_bb_srcover(void) {
    struct umbra_basic_block *bb = umbra_basic_block();
    umbra_v32 ix = umbra_lane(bb);

    umbra_v32 src = umbra_load_32(bb, (umbra_ptr){0}, ix);

    // Extract RGBA channels from packed u32.
    umbra_v32 mask8  = umbra_imm_32(bb, 0xFF);
    umbra_v32 inv255 = umbra_imm_32(bb, 0x3B808081u); // 1.0f/255.0f

    umbra_v32 ri = umbra_and_32(bb, src, mask8);
    umbra_v32 rf = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ri), inv255);
    umbra_v16 sr = umbra_f16_from_f32(bb, rf);

    umbra_v32 sh8 = umbra_imm_32(bb, 8);
    umbra_v32 gi  = umbra_and_32(bb, umbra_shr_u32(bb, src, sh8), mask8);
    umbra_v32 gf  = umbra_mul_f32(bb, umbra_f32_from_i32(bb, gi), inv255);
    umbra_v16 sg  = umbra_f16_from_f32(bb, gf);

    umbra_v32 sh16 = umbra_imm_32(bb, 16);
    umbra_v32 bi   = umbra_and_32(bb, umbra_shr_u32(bb, src, sh16), mask8);
    umbra_v32 bf   = umbra_mul_f32(bb, umbra_f32_from_i32(bb, bi), inv255);
    umbra_v16 sb   = umbra_f16_from_f32(bb, bf);

    umbra_v32 sh24 = umbra_imm_32(bb, 24);
    umbra_v32 ai   = umbra_and_32(bb, umbra_shr_u32(bb, src, sh24), mask8);
    umbra_v32 af   = umbra_mul_f32(bb, umbra_f32_from_i32(bb, ai), inv255);
    umbra_v16 sa   = umbra_f16_from_f32(bb, af);

    // Load dst fp16 channels.
    umbra_v16 dr = umbra_load_16(bb, (umbra_ptr){1}, ix);
    umbra_v16 dg = umbra_load_16(bb, (umbra_ptr){2}, ix);
    umbra_v16 db = umbra_load_16(bb, (umbra_ptr){3}, ix);
    umbra_v16 da = umbra_load_16(bb, (umbra_ptr){4}, ix);

    // Blend: out = src + dst * (1 - sa)
    umbra_v16 one = umbra_imm_16(bb, 0x3C00);
    umbra_v16 inv_a = umbra_sub_f16(bb, one, sa);

    umbra_v16 rout = umbra_add_f16(bb, sr, umbra_mul_f16(bb, dr, inv_a));
    umbra_v16 gout = umbra_add_f16(bb, sg, umbra_mul_f16(bb, dg, inv_a));
    umbra_v16 bout = umbra_add_f16(bb, sb, umbra_mul_f16(bb, db, inv_a));
    umbra_v16 aout = umbra_add_f16(bb, sa, umbra_mul_f16(bb, da, inv_a));

    umbra_store_16(bb, (umbra_ptr){1}, ix, rout);
    umbra_store_16(bb, (umbra_ptr){2}, ix, gout);
    umbra_store_16(bb, (umbra_ptr){3}, ix, bout);
    umbra_store_16(bb, (umbra_ptr){4}, ix, aout);

    // Run it.
    uint32_t src_px[] = {0x80402010u, 0x80402010u, 0x80402010u};
    __fp16 dst_r[] = {0.5, 0.5, 0.5};
    __fp16 dst_g[] = {0.5, 0.5, 0.5};
    __fp16 dst_b[] = {0.5, 0.5, 0.5};
    __fp16 dst_a[] = {0.5, 0.5, 0.5};

    umbra_basic_block_exec(bb, 3, (void*[]){src_px, dst_r, dst_g, dst_b, dst_a});

    float const tol = 0.02f;
    // R channel: src_r = 0x10/255 ≈ 0.063, inv_a = 1 - 0x80/255 ≈ 0.498
    // out_r = 0.063 + 0.5*0.498 ≈ 0.312
    ((float)dst_r[0] > 0.28f - tol && (float)dst_r[0] < 0.34f + tol) here;

    umbra_basic_block_free(bb);
}

int main(void) {
    test_mul_f32();
    test_f32_ops();
    test_i32_ops();
    test_f16_ops();
    test_i16_ops();
    test_cmp_i32();
    test_cmp_i16();
    test_cmp_f32();
    test_cmp_f16();
    test_imm();
    test_fma_f32();
    test_fma_f16();
    test_min_max_sqrt_f32();
    test_min_max_sqrt_f16();
    test_large_n();
    test_uni_via_load();
    test_scatter();
    test_convert();
    test_canonicalize();
    test_gvn();
    test_fma_fusion();
    test_dce();
    test_constprop();
    test_hoisting();
    test_strength_reduction();
    test_optimize_srcover();
    test_bb_basic();
    test_bb_dedup();
    test_bb_constprop();
    test_bb_strength_reduction();
    test_bb_fma();
    test_bb_srcover();
    return 0;
}
