#ifndef preferences_h
#define preferences_h

#include <sane/sane.h>

typedef struct
  {
    const char *device;		/* name of preferred device (or NULL) */
    const char *filename;	/* default filename */
    int advanced;		/* advanced user? */
    int tooltips_enabled;	/* should tooltips be disabled? */
    double length_unit;		/* 1.0==mm, 10.0==cm, 25.4==inches, etc. */
    int preserve_preview;	/* save/restore preview image(s)? */
    int preview_own_cmap;	/* install colormap for preview */
    double preview_gamma;	/* gamma value for previews */
  }
Preferences;

extern Preferences preferences;

extern void preferences_save (int fd);
extern void preferences_restore (int fd);

#endif /* preferences_h */
