#ifndef LIBBCMSIDEBAND_H_
#define LIBBCMSIDEBAND_H_

#ifdef __cplusplus
extern "C" {
#endif

struct bcmsideband_ctx;

typedef void(*sb_geometry_cb)(void *context, unsigned int x, unsigned int y,
                                    unsigned int width, unsigned int height);

struct bcmsideband_ctx *
libbcmsideband_init_sideband(int index,
                             ANativeWindow *native_window,
                             sb_geometry_cb cb);

/* API used in TV input HAL (needs the native_handle) */
struct bcmsideband_ctx *
libbcmsideband_init_sideband_tif(int index,
                                 native_handle_t **p_native_handle,
                                 sb_geometry_cb cb,
                                 void *cb_ctx);

void
libbcmsideband_release(struct bcmsideband_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LIBBCMSIDEBAND_H_ */
