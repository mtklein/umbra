#include "len.h"
#include "test.h"
#include "umbra.h"

int main(void) {
    struct umbra_inst const inst[] = {
        [0] = {umbra_load_32, .ptr=0},
        [1] = {umbra_load_32, .ptr=1},
        [2] = {umbra_mul_f32, .x=0, .y=1},
        [3] = {umbra_store_32, .ptr=2},
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
    return 0;
}
