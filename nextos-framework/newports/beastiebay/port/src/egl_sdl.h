#ifndef FP2_EGL_SDL_H
#define FP2_EGL_SDL_H

#include <EGL/egl.h>

/* Select the proven backend split once: SDL owns KMS/Wayland contexts and
 * page flips, while the legacy SDL "mali" backend keeps raw EGL/fbdev. */
int fp2_sdl_video_init(void);
int fp2_sdl_video_active(void);
/* Current SDL drawable size, for the Android display-metrics emulation
 * (jni.c answers DisplayMetrics/Display with these; the game computes
 * texture sizes from them -- zeros produced "out of range width / height"). */
void fp2_sdl_screen_size(int *width, int *height);
/* Live GL string lookup (GL_VENDOR / GL_RENDERER / GL_VERSION /
 * GL_SHADING_LANGUAGE_VERSION / GL_EXTENSIONS).  The Java side reads these
 * through android.opengl.GLES20.glGetString, and Kairosoft's boot check
 * rejects an empty extension string with its own error 100, so the JNI
 * adapter needs the real driver answer rather than a stub.  Returns NULL
 * before the context exists or for an unknown name. */
const char *fp2_sdl_gl_string(unsigned int name);
void *fp2_sdl_gl_proc(const char *name);
void *fp2_sdl_egl_proc(const char *name);
EGLBoolean fp2_sdl_swap_buffers(EGLDisplay display, EGLSurface surface);

#endif
