#ifndef gtkglue_h
#define gtkglue_h

#include <sys/types.h>

#include <gtk/gtk.h>

#include <sane/config.h>
#include <sane/sane.h>

struct GSGDialog;

typedef void (*GSGCallback) (struct GSGDialog *dialog, void *arg);

typedef enum
  {
    GSG_TL_X,	/* top-left x */
    GSG_TL_Y,	/* top-left y */
    GSG_BR_X,	/* bottom-right x */
    GSG_BR_Y	/* bottom-right y */
  }
GSGCornerCoordinates;

typedef struct
  {
    /* The option number of the well-known options.  Each of these may
       be -1 in case the backend doesn't define the respective option.  */
    int preview;
    int dpi;
    int coord[4];
  }
GSGWellKnownOptions;

typedef struct
  {
    gchar *label;
    struct GSGDialogElement *elem;
    gint index;
  }
GSGMenuItem;

typedef struct GSGDialogElement
  {
    struct GSGDialog *dialog;	/* wasteful, but is there a better solution? */
    GtkWidget *automatic;	/* auto button for options that support this */
    GtkWidget *widget;
    GtkObject *data;
    int menu_size;		/* # of items in menu (if any) */
    GSGMenuItem *menu;
  }
GSGDialogElement;

typedef struct GSGDialog
  {
    GtkWidget *window;
    GtkWidget *main_hbox;
    GtkWidget *advanced_vbox;
    GtkTooltips *tooltips;
    GdkColor tooltips_fg;
    GdkColor tooltips_bg;
    SANE_Handle *dev;
    const char *dev_name;
    GSGWellKnownOptions well_known;
    int num_elements;
    GSGDialogElement *element;
    gint idle_id;
    u_int rebuild : 1;
    u_int advanced : 1;
    /* This callback gets invoked whenever the backend notifies us
       that the option descriptors have changed.  */
    GSGCallback option_reload_callback;
    void *option_reload_arg;
    /* This callback gets invoked whenever the backend notifies us
       that the parameters have changed.  */
    GSGCallback param_change_callback;
    void *param_change_arg;
  }
GSGDialog;


extern int gsg_message_dialog_active;

/* Construct the path and return it in filename_ret (this buffer must
   be at least max_len bytes long).  The path is constructed as
   follows:

      ~/.sane/${PROG_NAME}/${PREFIX}${DEV_NAME}${POSTFIX}

   If PROG_NAME is NULL, an empty string is used and the leading slash
   is removed.  On success, 0 is returned, on error a negative number and
   ERRNO is set to the appropriate value.  */
extern int gsg_make_path (size_t max_len, char *filename_ret,
			  const char *prog_name,
			  const char *prefix, const char *dev_name,
			  const char *postfix);

extern void gsg_message (gchar *title, gchar * message);
extern void gsg_error (gchar * error_message);
extern void gsg_warning (gchar * warning_message);
extern int gsg_get_filename (const char *label, const char *default_name,
			     size_t max_len, char *filename);

extern GSGDialog *gsg_create_dialog (GtkWidget *window,
				     const char *device_name,
				     GSGCallback option_reload_callback,
				     void *option_reload_arg,
				     GSGCallback param_callback,
				     void *param_arg);
extern void gsg_sync (GSGDialog *dialog);
extern void gsg_refresh_dialog (GSGDialog *dialog);
extern void gsg_update_scan_window (GSGDialog *dialog);
extern void gsg_set_advanced (GSGDialog *dialog, int advanced);
extern void gsg_set_tooltips (GSGDialog *dialog, int enable);
extern void gsg_set_sensitivity (GSGDialog *dialog, int sensitive);
extern void gsg_destroy_dialog (GSGDialog * dialog);

#define gsg_dialog_get_device(dialog)	((dialog)->dev)

#endif /* gtkglue_h */
