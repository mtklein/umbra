// Method-swizzles -[MTLDevice newLibraryWithSource:options:error:] (and the
// completionHandler variant) so that MSL source handed to Metal by
// wgpu-native (via naga) can be captured to a file.  Compiles as plain C
// against the Objective-C runtime, same style as src/metal.c.

#include "wgpu_msl_intercept.h"

#if !defined(__APPLE__) || !defined(__aarch64__) || defined(__wasm__)

void umbra_wgpu_msl_intercept_to(char const *path) { (void)path; }

#else

#pragma clang diagnostic ignored "-Wcast-function-type-strict"

#include <objc/runtime.h>
#include <stdio.h>
#include <string.h>

typedef struct objc_object *id;
extern id      objc_msg_utf8 (id, SEL)                        __asm__("_objc_msgSend");
extern void    objc_msg_v    (id, SEL)                        __asm__("_objc_msgSend");
extern id      MTLCreateSystemDefaultDevice(void);

static char g_path[512] = {0};
static IMP  g_orig_sync  = 0;
static IMP  g_orig_async = 0;

static void save_msl(id source) {
    if (!g_path[0] || !source) { return; }
    char const *s = (char const*)objc_msg_utf8(source, sel_registerName("UTF8String"));
    if (s) {
        FILE *f = fopen(g_path, "w");
        if (f) {
            fputs(s, f);
            fclose(f);
        }
    }
    g_path[0] = '\0';
}

typedef id   (*sync_fn )(id, SEL, id, id, id*);
typedef void (*async_fn)(id, SEL, id, id, id);

static id swizzled_sync(id self, SEL _cmd, id source, id options, id *error) {
    save_msl(source);
    return ((sync_fn)g_orig_sync)(self, _cmd, source, options, error);
}

static void swizzled_async(id self, SEL _cmd, id source, id options, id handler) {
    save_msl(source);
    ((async_fn)g_orig_async)(self, _cmd, source, options, handler);
}

static void install_once(void) {
    if (g_orig_sync || g_orig_async) { return; }
    id dev = MTLCreateSystemDefaultDevice();
    if (!dev) { return; }
    Class cls = object_getClass(dev);

    SEL sel_sync  = sel_registerName("newLibraryWithSource:options:error:");
    SEL sel_async = sel_registerName("newLibraryWithSource:options:completionHandler:");

    Method m_sync  = class_getInstanceMethod(cls, sel_sync);
    Method m_async = class_getInstanceMethod(cls, sel_async);
    if (m_sync) {
        g_orig_sync = method_getImplementation(m_sync);
        method_setImplementation(m_sync, (IMP)swizzled_sync);
    }
    if (m_async) {
        g_orig_async = method_getImplementation(m_async);
        method_setImplementation(m_async, (IMP)swizzled_async);
    }

    objc_msg_v(dev, sel_registerName("release"));
}

void umbra_wgpu_msl_intercept_to(char const *path) {
    install_once();
    if (path) {
        snprintf(g_path, sizeof g_path, "%s", path);
    } else {
        g_path[0] = '\0';
    }
}

#endif
