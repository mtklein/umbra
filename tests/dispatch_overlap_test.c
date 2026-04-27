#include "../src/dispatch_overlap.h"
#include "test.h"

TEST(test_dispatch_overlap_no_writes_no_conflict) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;
    !dispatch_overlap_check(&d, &buf_a, 0, 0, 100, 100) here;
}

TEST(test_dispatch_overlap_same_rect_same_buf) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;

    dispatch_overlap_record(&d, &buf_a, 0, 0, 100, 100);
    dispatch_overlap_check(&d, &buf_a, 0, 0, 100, 100) here;
}

TEST(test_dispatch_overlap_nonoverlapping_rects) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;

    dispatch_overlap_record(&d, &buf_a, 0, 0, 50, 100);
    !dispatch_overlap_check(&d, &buf_a, 50, 0, 100, 100) here;
}

TEST(test_dispatch_overlap_nonoverlapping_rows) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;

    dispatch_overlap_record(&d, &buf_a, 0, 0, 100, 50);
    !dispatch_overlap_check(&d, &buf_a, 0, 50, 100, 100) here;
}

TEST(test_dispatch_overlap_different_bufs) {
    struct dispatch_overlap d = {0};
    int buf_a = 1, buf_b = 2;

    dispatch_overlap_record(&d, &buf_a, 0, 0, 100, 100);
    !dispatch_overlap_check(&d, &buf_b, 0, 0, 100, 100) here;
}

TEST(test_dispatch_overlap_read_hits_previous_write) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;

    dispatch_overlap_record(&d, &buf_a, 0, 0, 100, 100);
    dispatch_overlap_check(&d, &buf_a, 0, 0, 100, 100) here;
}

TEST(test_dispatch_overlap_reset_clears) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;

    dispatch_overlap_record(&d, &buf_a, 0, 0, 100, 100);
    d.writes == 1 here;

    dispatch_overlap_reset(&d);
    d.writes == 0 here;

    !dispatch_overlap_check(&d, &buf_a, 0, 0, 100, 100) here;
}

TEST(test_dispatch_overlap_swatch_pattern) {
    struct dispatch_overlap d = {0};
    int buf_dst = 1;

    enum { COLS = 5, ROWS = 2, W = 640, H = 480 };
    int const cw = W / COLS,
              ch = H / ROWS;

    for (int i = 0; i < COLS * ROWS; i++) {
        int col = i % COLS,
            row = i / COLS;
        int l = col * cw,       t = row * ch;
        int r = (col + 1) * cw, b = (row + 1) * ch;

        !dispatch_overlap_check(&d, &buf_dst, l, t, r, b) here;
        dispatch_overlap_record(&d, &buf_dst, l, t, r, b);
    }
    d.writes == COLS * ROWS here;
}

TEST(test_dispatch_overlap_slug_accumulator_pattern) {
    struct dispatch_overlap d = {0};
    int buf_wind = 1;

    dispatch_overlap_record(&d, &buf_wind, 0, 0, 640, 480);

    dispatch_overlap_check(&d, &buf_wind, 0, 0, 640, 480) here;

    dispatch_overlap_reset(&d);
    dispatch_overlap_record(&d, &buf_wind, 0, 0, 640, 480);

    dispatch_overlap_check(&d, &buf_wind, 0, 0, 640, 480) here;
}

TEST(test_dispatch_overlap_partial_overlap) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;

    dispatch_overlap_record(&d, &buf_a, 0, 0, 60, 60);
    dispatch_overlap_check(&d, &buf_a, 50, 50, 100, 100) here;
}

TEST(test_dispatch_overlap_edge_touching_no_overlap) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;

    dispatch_overlap_record(&d, &buf_a, 0, 0, 50, 50);

    !dispatch_overlap_check(&d, &buf_a, 50, 0, 100, 50) here;
    !dispatch_overlap_check(&d, &buf_a, 0, 50, 50, 100) here;
    !dispatch_overlap_check(&d, &buf_a, 50, 50, 100, 100) here;
}

TEST(test_dispatch_overlap_multi_buf_conflict) {
    struct dispatch_overlap d = {0};
    int buf_a = 1, buf_b = 2;

    dispatch_overlap_record(&d, &buf_b, 0, 0, 100, 100);

    !dispatch_overlap_check(&d, &buf_a, 0, 0, 100, 100) here;
    dispatch_overlap_check(&d, &buf_b, 0, 0, 100, 100) here;
}

TEST(test_dispatch_overlap_cap_forces_conflict) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;

    for (int i = 0; i < DISPATCH_OVERLAP_CAP; i++) {
        dispatch_overlap_record(&d, &buf_a, i, 0, i + 1, 1);
    }
    d.writes == DISPATCH_OVERLAP_CAP here;

    int buf_b = 2;
    dispatch_overlap_check(&d, &buf_b, 999, 999, 1000, 1000) here;
}

TEST(test_dispatch_overlap_record_past_cap_is_silent) {
    struct dispatch_overlap d = {0};
    int buf_a = 1;

    for (int i = 0; i < DISPATCH_OVERLAP_CAP + 5; i++) {
        dispatch_overlap_record(&d, &buf_a, i, 0, i + 1, 1);
    }
    d.writes == DISPATCH_OVERLAP_CAP here;
}
