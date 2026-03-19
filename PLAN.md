# Multi-pipeline Metal dispatch

## Problem

Slug's render_row runs two programs per row: an accumulator (87 curve
dispatches) and a composite (1 dispatch). The composite reads the
accumulator's output (wind_buf). Currently the accumulator always uses
the JIT because Metal would require a GPU flush between the two programs
to copy wind_buf back to host memory — 480 flushes per frame.

## Goal

Run both programs on the same Metal command buffer so dispatches execute
in-order on the GPU with no intermediate flush. The wind_buf stays on
the GPU between accumulator and composite dispatches.

## Proposed API

```c
void umbra_backend_queue_on(struct umbra_backend *target,
                            struct umbra_backend *source,
                            int n, umbra_buf[]);
```

Queue `source`'s program on `target`'s command buffer. For Metal, this
means encoding a dispatch with `source`'s pipeline state into `target`'s
encoder. For CPU backends (interp/JIT), just call source's queue directly
(they execute immediately, no command buffer to share).

## Metal implementation

`encode_dispatch` already calls `[enc setComputePipelineState:]` before
each dispatch. To support queue_on, extract the pipeline from the source
`umbra_metal` and set it on the target's encoder:

```c
void umbra_metal_queue_on(struct umbra_metal *target,
                          struct umbra_metal *source,
                          int n, umbra_buf buf[]) {
    if (!target->batch_cmdbuf) {
        umbra_metal_begin_batch(target);
    }
    // encode_dispatch but with source's pipeline on target's encoder
    id<MTLComputeCommandEncoder> enc = target->batch_enc;
    [enc setComputePipelineState: source->pipeline];
    // ... rest of encode_dispatch using target's encoder, source's pipeline
}
```

The tricky part: `encode_dispatch` currently uses `m->pipeline` (its own)
and manages buffer allocations through `m->bufs`, `m->batch_data`, etc.
For queue_on, we need to use source's pipeline but target's buffer
management and encoder. This probably means refactoring encode_dispatch
to accept the pipeline as a parameter.

## Buffer sharing

The key correctness requirement: wind_buf must be the SAME GPU buffer
across accumulator and composite dispatches. Currently each
`encode_dispatch` call copies host buffers to GPU buffers. If both
programs reference the same host pointer (wind_buf), the batch_shared
overlap detection should reuse the same GPU buffer. Verify this works
when the pipeline switches mid-batch.

## Slug changes

```c
// Build acc once from the same backend type
if (!st->acc) {
    st->acc = umbra_backend_ctor(backend)(st->acc_bb);
}

// Queue accumulator work on the composite's command buffer
for (int j = 0; j < st->slug->count; j++) {
    // ...setup uniforms...
    umbra_backend_queue_on(backend, st->acc, w, abuf);
}
// No flush here — composite reads wind_buf from the same GPU buffer
// ...setup composite uniforms...
umbra_backend_queue(backend, w, buf);
```

## Expected impact

- Metal slug: ~170 ns/px (same as JIT, no round-trips)
- Demo FPS: consistent across backends for slug slide
- CPU backends: unchanged (queue_on falls through to source->queue)

## Complexity

- New function in backend.c (~10 lines)
- Refactor encode_dispatch to accept pipeline parameter (~20 lines)
- New umbra_metal_queue_on (~15 lines)
- Slug render_row simplification (~5 lines changed)
- Verify batch_shared buffer reuse works across pipeline switches
