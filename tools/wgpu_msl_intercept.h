#pragma once

// Set `path` non-NULL before triggering a wgpu shader compile: the very next
// newLibraryWithSource: call saves the MSL source to that path and then
// clears the path (one-shot).  Pass NULL to disable explicitly.  Safe to
// call on platforms without Metal -- becomes a no-op.
void umbra_wgpu_msl_intercept_to(char const *path);
