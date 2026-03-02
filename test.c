#include "len.h"
#include "test.h"
#include "umbra.h"

static void test_mul_f32(void) {
    struct umbra_inst const inst[] = {
        [0] = {.op=umbra_lane},
        [1] = {umbra_load_32, .ptr=0, .x=0},
        [2] = {umbra_load_32, .ptr=1, .x=0},
        [3] = {umbra_mul_f32, .x=1, .y=2},
        [4] = {umbra_store_32, .ptr=2, .x=0, .y=3},
    };
    struct umbra_program *p = umbra_program(inst, len(inst));

    float x[] = {1,2,3,4,5},
          y[] = {6,7,8,9,0},
          z[len(x)] = {0};
    umbra_program_run(p,len(z), (void*[]){x,y,z});

    equiv(z[0],  6) here;
    equiv(z[1], 14) here;
    equiv(z[2], 24) here;
    equiv(z[3], 36) here;
    equiv(z[4],  0) here;

    umbra_program_free(p);
}

static void test_f32_ops(void) {
    // add
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_add_f32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        float x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 11) here;
        equiv(z[1], 22) here;
        equiv(z[2], 33) here;
        umbra_program_free(p);
    }
    // sub
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_sub_f32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        float x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        equiv(z[0],  9) here;
        equiv(z[1], 18) here;
        equiv(z[2], 27) here;
        umbra_program_free(p);
    }
    // div
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_div_f32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        float x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 5) here;
        equiv(z[1], 5) here;
        equiv(z[2], 6) here;
        umbra_program_free(p);
    }
}

static void test_i32_ops(void) {
    // add
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_add_i32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == 11) here;
        (z[1] == 22) here;
        (z[2] == 33) here;
        umbra_program_free(p);
    }
    // sub
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_sub_i32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] ==  9) here;
        (z[1] == 18) here;
        (z[2] == 27) here;
        umbra_program_free(p);
    }
    // mul
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_mul_i32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == 10) here;
        (z[1] == 18) here;
        (z[2] == 28) here;
        umbra_program_free(p);
    }
    // shl
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_shl_i32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {1,3,7}, y[] = {1,2,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] ==  2) here;
        (z[1] == 12) here;
        (z[2] == 56) here;
        umbra_program_free(p);
    }
    // shr (logical, unsigned)
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_shr_u32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {-1, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == (int)(0xFFFFFFFFu >> 1)) here;  // logical: top bit cleared
        (z[1] == 4) here;
        (z[2] == 8) here;
        umbra_program_free(p);
    }
    // sra (arithmetic, signed)
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_shr_s32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {-8, 8, 64}, y[] = {1, 1, 3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -4) here;  // arithmetic: sign bit preserved
        (z[1] ==  4) here;
        (z[2] ==  8) here;
        umbra_program_free(p);
    }
    // and, or, xor
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_and_i32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {0xFF, 0x0F}, y[] = {0x0F, 0xFF}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0x0F) here;
        (z[1] == 0x0F) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_or_i32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {0xF0, 0x0F}, y[] = {0x0F, 0xF0}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0xFF) here;
        (z[1] == 0xFF) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_xor_i32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {0xFF, 0xFF}, y[] = {0x0F, 0xFF}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        (z[0] == 0xF0) here;
        (z[1] == 0x00) here;
        umbra_program_free(p);
    }
    // sel: (cond & a) | (~cond & b)
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},  // cond
            {umbra_load_32, .ptr=1, .x=0},  // a
            {umbra_load_32, .ptr=2, .x=0},  // b
            {umbra_sel_i32, .x=1, .y=2, .z=3},
            {umbra_store_32, .ptr=3, .x=0, .y=4},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int cond[] = {-1, 0, -1},  // -1 = all bits set, 0 = no bits
              a[] = {10, 20, 30},
              b[] = {40, 50, 60},
              z[3] = {0};
        umbra_program_run(p, 3, (void*[]){cond, a, b, z});
        (z[0] == 10) here;  // cond=-1: pick a
        (z[1] == 50) here;  // cond=0:  pick b
        (z[2] == 30) here;  // cond=-1: pick a
        umbra_program_free(p);
    }
}

static void test_f16_ops(void) {
    // mul
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_mul_f16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        __fp16 x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 10) here;
        equiv((float)z[1], 18) here;
        equiv((float)z[2], 28) here;
        umbra_program_free(p);
    }
    // add
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_add_f16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        __fp16 x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 11) here;
        equiv((float)z[1], 22) here;
        equiv((float)z[2], 33) here;
        umbra_program_free(p);
    }
    // sub
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_sub_f16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        __fp16 x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0],  9) here;
        equiv((float)z[1], 18) here;
        equiv((float)z[2], 27) here;
        umbra_program_free(p);
    }
    // div
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_div_f16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        __fp16 x[] = {10,20,30}, y[] = {2,4,5}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        equiv((float)z[0], 5) here;
        equiv((float)z[1], 5) here;
        equiv((float)z[2], 6) here;
        umbra_program_free(p);
    }
}

static void test_i16_ops(void) {
    // add
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_add_i16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {1,2,3}, y[] = {10,20,30}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == 11) here;
        (z[1] == 22) here;
        (z[2] == 33) here;
        umbra_program_free(p);
    }
    // sub
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_sub_i16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {10,20,30}, y[] = {1,2,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] ==  9) here;
        (z[1] == 18) here;
        (z[2] == 27) here;
        umbra_program_free(p);
    }
    // mul
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_mul_i16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {2,3,4}, y[] = {5,6,7}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == 10) here;
        (z[1] == 18) here;
        (z[2] == 28) here;
        umbra_program_free(p);
    }
    // shl, shr, sra
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_shl_i16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {1,3}, y[] = {4,2}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        (z[0] == 16) here;
        (z[1] == 12) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_shr_u16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {-1, 64}, y[] = {1, 3}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        (z[0] == (short)(0xFFFFu >> 1)) here;  // logical: top bit cleared
        (z[1] == 8) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_shr_s16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {-8, 64}, y[] = {1, 3}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        (z[0] == -4) here;  // arithmetic: sign preserved
        (z[1] ==  8) here;
        umbra_program_free(p);
    }
    // and, or, xor
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_and_i16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {0xFF}, y[] = {0x0F}, z[1] = {0};
        umbra_program_run(p, 1, (void*[]){x,y,z});
        (z[0] == 0x0F) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_or_i16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {0xF0}, y[] = {0x0F}, z[1] = {0};
        umbra_program_run(p, 1, (void*[]){x,y,z});
        (z[0] == 0xFF) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_xor_i16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {0xFF}, y[] = {0x0F}, z[1] = {0};
        umbra_program_run(p, 1, (void*[]){x,y,z});
        (z[0] == 0xF0) here;
        umbra_program_free(p);
    }
    // sel
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_load_16, .ptr=2, .x=0},
            {umbra_sel_i16, .x=1, .y=2, .z=3},
            {umbra_store_16, .ptr=3, .x=0, .y=4},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short cond[] = {-1, 0},
                 a[] = {10, 20},
                 b[] = {30, 40},
                 z[2] = {0};
        umbra_program_run(p, 2, (void*[]){cond, a, b, z});
        (z[0] == 10) here;
        (z[1] == 40) here;
        umbra_program_free(p);
    }
}

static void test_cmp_i32(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_eq_i32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_ne_i32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {1,2}, y[] = {1,9}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        (z[0] ==  0) here;
        (z[1] == -1) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_lt_s32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_le_s32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] == -1) here;
        (z[2] ==  0) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_gt_s32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_ge_s32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {3,5,1}, y[] = {2,5,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] == -1) here;
        (z[2] ==  0) here;
        umbra_program_free(p);
    }
    // unsigned
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_lt_u32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {1, -1}, y[] = {2, 1}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        umbra_program_free(p);
    }
    // sel with comparison mask
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_lt_s32, .x=1, .y=2},
            {umbra_sel_i32, .x=3, .y=1, .z=2},  // pick min(x,y)
            {umbra_store_32, .ptr=2, .x=0, .y=4},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == 2) here;
        (z[1] == 1) here;
        (z[2] == 3) here;
        umbra_program_free(p);
    }
}

static void test_cmp_i16(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_eq_i16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {1,2,3}, y[] = {1,9,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_lt_s16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {1,5,3}, y[] = {2,5,1}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_program_free(p);
    }
    // sel with comparison mask
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_lt_s16, .x=1, .y=2},
            {umbra_sel_i16, .x=3, .y=1, .z=2},
            {umbra_store_16, .ptr=2, .x=0, .y=4},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short x[] = {5,1}, y[] = {2,4}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        (z[0] == 2) here;
        (z[1] == 1) here;
        umbra_program_free(p);
    }
}

static void test_cmp_f32(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_eq_f32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        float x[] = {1,2,3}, y[] = {1,9,3};
        int z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_lt_f32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        float x[] = {1,5,3}, y[] = {2,5,1};
        int z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_program_free(p);
    }
    // sel with f32 comparison: pick min
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_lt_f32, .x=1, .y=2},
            {umbra_sel_i32, .x=3, .y=1, .z=2},
            {umbra_store_32, .ptr=2, .x=0, .y=4},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        float x[] = {5,1,3}, y[] = {2,4,3}, z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        equiv(z[0], 2) here;
        equiv(z[1], 1) here;
        equiv(z[2], 3) here;
        umbra_program_free(p);
    }
}

static void test_cmp_f16(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_eq_f16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        __fp16 x[] = {1,2,3}, y[] = {1,9,3};
        short z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] == -1) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_lt_f16, .x=1, .y=2},
            {umbra_store_16, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        __fp16 x[] = {1,5,3}, y[] = {2,5,1};
        short z[3] = {0};
        umbra_program_run(p, 3, (void*[]){x,y,z});
        (z[0] == -1) here;
        (z[1] ==  0) here;
        (z[2] ==  0) here;
        umbra_program_free(p);
    }
    // sel with f16 comparison: pick min
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_16, .ptr=0, .x=0},
            {umbra_load_16, .ptr=1, .x=0},
            {umbra_lt_f16, .x=1, .y=2},
            {umbra_sel_i16, .x=3, .y=1, .z=2},
            {umbra_store_16, .ptr=2, .x=0, .y=4},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        __fp16 x[] = {5,1}, y[] = {2,4};
        __fp16 z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,z});
        equiv((float)z[0], 2) here;
        equiv((float)z[1], 1) here;
        umbra_program_free(p);
    }
}

static void test_imm(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_imm_32, .immi=42},
            {umbra_store_32, .ptr=0, .x=0, .y=1},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int z[3] = {0};
        umbra_program_run(p, 3, (void*[]){z});
        (z[0] == 42) here;
        (z[1] == 42) here;
        (z[2] == 42) here;
        umbra_program_free(p);
    }
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_imm_16, .immi=7},
            {umbra_store_16, .ptr=0, .x=0, .y=1},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        short z[3] = {0};
        umbra_program_run(p, 3, (void*[]){z});
        (z[0] == 7) here;
        (z[1] == 7) here;
        (z[2] == 7) here;
        umbra_program_free(p);
    }
}

static void test_fma_peephole(void) {
    // Peephole: mul then add should fuse into fma.
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_load_32, .ptr=2, .x=0},
            {umbra_mul_f32, .x=1, .y=2},
            {umbra_add_f32, .x=4, .y=3},
            {umbra_store_32, .ptr=3, .x=0, .y=5},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        float x[] = {2,3}, y[] = {4,5}, w[] = {10,20}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,w,z});
        equiv(z[0], 18) here;  // 2*4+10
        equiv(z[1], 35) here;  // 3*5+20
        umbra_program_free(p);
    }
    // Peephole: add where y is the mul.
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_load_32, .ptr=2, .x=0},
            {umbra_mul_f32, .x=1, .y=2},
            {umbra_add_f32, .x=3, .y=4},
            {umbra_store_32, .ptr=3, .x=0, .y=5},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        float x[] = {2,3}, y[] = {4,5}, w[] = {10,20}, z[2] = {0};
        umbra_program_run(p, 2, (void*[]){x,y,w,z});
        equiv(z[0], 18) here;
        equiv(z[1], 35) here;
        umbra_program_free(p);
    }
}

// Test with n > K to exercise both vector bulk and scalar tail paths.
static void test_large_n(void) {
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},
            {umbra_load_32, .ptr=1, .x=0},
            {umbra_add_f32, .x=1, .y=2},
            {umbra_store_32, .ptr=2, .x=0, .y=3},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        float x[11], y[11], z[11];
        for (int i = 0; i < 11; i++) {
            x[i] = (float)i;
            y[i] = (float)(10 - i);
        }
        umbra_program_run(p, 11, (void*[]){x,y,z});
        for (int i = 0; i < 11; i++) {
            equiv(z[i], 10) here;
        }
        umbra_program_free(p);
    }
}

static void test_uni_via_load(void) {
    // load(ptr, imm(0)) broadcasts element 0
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_imm_32, .immi=0},
            {umbra_load_32, .ptr=0, .x=1},
            {umbra_store_32, .ptr=1, .x=0, .y=2},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int src[] = {42, 99, 7};
        int dst[3] = {0};
        umbra_program_run(p, 3, (void*[]){src, dst});
        (dst[0] == 42) here;
        (dst[1] == 42) here;
        (dst[2] == 42) here;
        umbra_program_free(p);
    }
    // load(ptr, imm(2)) broadcasts element 2
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_imm_32, .immi=2},
            {umbra_load_32, .ptr=0, .x=1},
            {umbra_store_32, .ptr=1, .x=0, .y=2},
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int src[] = {42, 99, 7};
        int dst[3] = {0};
        umbra_program_run(p, 3, (void*[]){src, dst});
        (dst[0] == 7) here;
        (dst[1] == 7) here;
        (dst[2] == 7) here;
        umbra_program_free(p);
    }
}

static void test_scatter(void) {
    // Store to reversed indices
    {
        struct umbra_inst const inst[] = {
            {.op=umbra_lane},
            {umbra_load_32, .ptr=0, .x=0},   // load src contiguously
            {umbra_load_32, .ptr=1, .x=0},   // load idx contiguously
            {umbra_store_32, .ptr=2, .x=2, .y=1},  // scatter: idx=reg2, data=reg1
        };
        struct umbra_program *p = umbra_program(inst, len(inst));
        int src[] = {10, 20, 30};
        int idx[] = {2, 1, 0};
        int dst[3] = {0};
        umbra_program_run(p, 3, (void*[]){src, idx, dst});
        (dst[0] == 30) here;
        (dst[1] == 20) here;
        (dst[2] == 10) here;
        umbra_program_free(p);
    }
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
    test_fma_peephole();
    test_large_n();
    test_uni_via_load();
    test_scatter();
    return 0;
}
