# Metal / Vulkan buffer management TODO

## Current state

### Leaks
- `leaks --atExit` runs on all host tests.  Working.

### Test memory alignment
- Every test runs twice: once with page-aligned allocations
  (`posix_memalign` to page size), once with non-page-aligned
  (page-aligned + 16 bytes offset).  This deterministically
  exercises both the Metal nocopy and copy buffer paths.
- `talloc`/`tfree` are plumbed through every test function.
  Internally-allocated memory (uniforms from `umbra_draw_build`)
  uses `calloc`/`free` directly, not the test allocator.
- Slides receive alloc/free through `slides_init` for internal
  buffers like the slug wind_buf.

### Metal nocopy
- Page-aligned buffers use `newBufferWithBytesNoCopy`
  (zero-copy shared mapping).  Read-only and writable.
- Non-page-aligned buffers use `newBufferWithLength` + `memcpy`
  (copy in), with `memcpy` back from `.contents` on flush for
  writable buffers.
- Writable nocopy buffers skip the copyback (`can_nocopy` guard)
  since the GPU writes directly to host memory.

### Vulkan nocopy
- Page-aligned writable buffers use `VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT`
  to import host memory directly (`host_import_align`).
  Writable imported buffers skip copyback (`ce->nocopy`).
- Non-importable buffers use `vkAllocateMemory` + `vkMapMemory` +
  `memcpy` in, `memcpy` back on flush.

### Metal fences
- `MTLFence` used between compute dispatches within the same
  command encoder: `updateFence:` after each dispatch,
  `waitForFence:` before the next.  This replaced
  `memoryBarrierWithScope:MTLBarrierScopeBuffers` which was
  unreliable for read-after-write under leaks.

### Slug flushing
- **Problem**: the slug accumulator does 14 curve dispatches that
  read-after-write the same wind_buf.  With MTLFence between them
  (single command buffer, no flush), this fails ~49/100 under
  leaks.  With `be->flush(be)` after each dispatch (14 separate
  command buffers), it passes 0/100.
- **Current workaround**: slug.c flushes after every dispatch
  (`be->flush(be)` inside the loop).  This kills batching — 14
  command buffers instead of 1.
- **Root cause unknown**.  MTLFence should provide execution
  ordering within a command buffer.  The fence works for other
  multi-dispatch cases (the fence-only commit passed 0/100 before
  the slug flush was reverted).  Something about the slug's
  specific dispatch pattern — or the interaction between the
  fence and the `batch_shared` buffer cache — breaks under leaks.
- **TODO**: understand why the fence alone doesn't work for slug.
  If fixed, remove the per-dispatch flush and restore full
  batching.

### Performance
- Most slides render at the same speed as before the refactoring.
- Slide 14 (Slug Text) is slow because of the per-dispatch flush.
  Metal is now ~5x slower than Vulkan on this slide.  Fixing the
  fence issue above would restore performance.
- Vulkan is faster than Metal across the board, which suggests
  Metal buffer management has overhead beyond the slug flush.
  Worth profiling.
