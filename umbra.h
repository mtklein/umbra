#pragma once

struct umbra_color {
    __fp16 r,g,b,a;
};

struct umbra_matrix {
    float sx,kx,tx,
          ky,sy,ty,
          px,py,ps;
};
