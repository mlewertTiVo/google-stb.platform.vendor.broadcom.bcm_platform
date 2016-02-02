#ifndef LIBBCMSIDEBAND_H_
#define LIBBCMSIDEBAND_H_

#ifdef __cplusplus
extern "C" {
#endif

struct bcmsideband_ctx;

typedef void(*sb_geometry_cb)(unsigned int x, unsigned int y,
                                    unsigned int width, unsigned int height);

struct bcmsideband_ctx *
libbcmsideband_init_sideband(ANativeWindow *native_window, int *video_id,
                             int *audio_id, int *surface_id, sb_geometry_cb cb = NULL);

void
libbcmsideband_release(struct bcmsideband_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* LIBBCMSIDEBAND_H_ */
