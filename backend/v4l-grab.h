#define VIDEO_RGB08          1	/* bt848 dithered */
#define VIDEO_GRAY           2
#define VIDEO_RGB15_LE       3	/* 15 bpp little endian */
#define VIDEO_RGB16_LE       4	/* 16 bpp little endian */
#define VIDEO_RGB15_BE       5	/* 15 bpp big endian */
#define VIDEO_RGB16_BE       6	/* 16 bpp big endian */
#define VIDEO_BGR24          7	/* bgrbgrbgrbgr (LE) */
#define VIDEO_BGR32          8	/* bgr-bgr-bgr- (LE) */
#define VIDEO_RGB24          9	/* rgbrgbrgbrgb (BE) */
#define VIDEO_RGB32         10	/* -rgb-rgb-rgb (BE) */
#define VIDEO_LUT2          11	/* lookup-table 2 byte depth */
#define VIDEO_LUT4          12	/* lookup-table 4 byte depth */

#define CAN_AUDIO_VOLUME     1

#define TRAP(txt) fprintf(stderr,"%s:%d:%s\n",__FILE__,__LINE__,txt);exit(1);

/* ------------------------------------------------------------------------- */

struct STRTAB
{
  long nr;
  char *str;
};

struct OVERLAY_CLIP
{
  int x1, x2, y1, y2;
};

struct GRABBER
{
  char *name;
  int flags;
  struct STRTAB *norms;
  struct STRTAB *inputs;

  int (*grab_open) (char *opt);
  int (*grab_close) (void);

  int (*grab_setupfb) (int sw, int sh, int format, void *base, int bpl);
  int (*grab_overlay) (int x, int y, int width, int height, int format,
		       struct OVERLAY_CLIP * oc, int count);

  int (*grab_setparams) (int format, int *width, int *height,
			 int *linelength);
  void *(*grab_capture) (int single);
  void (*grab_cleanup) (void);

  int (*grab_tune) (unsigned long freq);
  int (*grab_tuned) (void);
  int (*grab_input) (int input, int norm);
  int (*grab_picture) (int color, int bright, int hue, int contrast);
  int (*grab_audio) (int mute, int volume, int *mode);
};

/* ------------------------------------------------------------------------- */

extern int debug;
extern int have_dga;

extern char *device;
extern int fd_grab;
extern struct GRABBER *grabber;

extern unsigned int format2depth[];
extern unsigned char *format_desc[];

/* ------------------------------------------------------------------------- */

int grabber_open (int sw, int sh, void *base, int format, int width);
int grabber_setparams (int format, int *width, int *height,
		       int *linelength, int lut_valid);
void *grabber_capture (void *dest, int dest_linelength, int single);
