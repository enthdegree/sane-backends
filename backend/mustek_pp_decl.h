
#ifndef MUSTEK_PP_DECL_H
#define MUSTEK_PP_DECL_H
/* debug driver, version 0.11-devel, author Jochen Eisinger */
extern SANE_Status	debug_drv_init (SANE_Int options, SANE_String_Const port,
					SANE_String_Const name, SANE_Attach_Callback attach);
extern void		debug_drv_capabilities (SANE_Int info, SANE_String *model,
						SANE_String *vendor, SANE_String *type,
						SANE_Int *maxres, SANE_Int *minres,
						SANE_Int *maxhsize, SANE_Int *maxvsize,
						SANE_Int *caps);
extern SANE_Status	debug_drv_open (SANE_String port, SANE_Int caps, SANE_Int *fd);
extern void		debug_drv_setup (SANE_Handle hndl);
extern SANE_Status	debug_drv_config (SANE_Handle hndl,
					  SANE_String_Const optname,
                                          SANE_String_Const optval);
extern void		debug_drv_close (SANE_Handle hndl);
extern SANE_Status	debug_drv_start (SANE_Handle hndl);
extern void		debug_drv_read (SANE_Handle hndl, SANE_Byte *buffer);
extern void		debug_drv_stop (SANE_Handle hndl);


/* CIS drivers for 600CP, 1200CP, and 1200CP+
   Version 0.11-devel, author Eddy De Greef */

extern SANE_Status	cis600_drv_init  (SANE_Int options, 
					  SANE_String_Const port,
				      	  SANE_String_Const name, 
                                          SANE_Attach_Callback attach);
extern SANE_Status	cis1200_drv_init (SANE_Int options, 
					  SANE_String_Const port,
				      	  SANE_String_Const name, 
                                          SANE_Attach_Callback attach);
extern SANE_Status	cis1200p_drv_init(SANE_Int options, 
				 	  SANE_String_Const port,
				      	  SANE_String_Const name, 
                                          SANE_Attach_Callback attach);
extern void		cis_drv_capabilities(SANE_Int info, 
					     SANE_String *model,
					     SANE_String *vendor, 
                                             SANE_String *type,
					     SANE_Int *maxres, 
                                             SANE_Int *minres,
					     SANE_Int *maxhsize, 
                                             SANE_Int *maxvsize,
					     SANE_Int *caps);
extern SANE_Status	cis_drv_open (SANE_String port, SANE_Int caps, SANE_Int *fd);
extern void		cis_drv_setup (SANE_Handle hndl);
extern SANE_Status	cis_drv_config (SANE_Handle hndl,
					SANE_String_Const optname,
                                        SANE_String_Const optval);
extern void		cis_drv_close (SANE_Handle hndl);
extern SANE_Status	cis_drv_start (SANE_Handle hndl);
extern void		cis_drv_read (SANE_Handle hndl, SANE_Byte *buffer);
extern void		cis_drv_stop (SANE_Handle hndl);

/* CCD drivers for 300 dpi models
   Version 0.11-devel, author Jochen Eisinger */

extern SANE_Status	ccd300_init  (SANE_Int options, 
					  SANE_String_Const port,
				      	  SANE_String_Const name, 
                                          SANE_Attach_Callback attach);
extern void		ccd300_capabilities(SANE_Int info, 
					     SANE_String *model,
					     SANE_String *vendor, 
                                             SANE_String *type,
					     SANE_Int *maxres, 
                                             SANE_Int *minres,
					     SANE_Int *maxhsize, 
                                             SANE_Int *maxvsize,
					     SANE_Int *caps);
extern SANE_Status	ccd300_open (SANE_String port, SANE_Int caps, SANE_Int *fd);
extern void		ccd300_setup (SANE_Handle hndl);
extern SANE_Status	ccd300_config (SANE_Handle hndl,
					SANE_String_Const optname,
                                        SANE_String_Const optval);
extern void		ccd300_close (SANE_Handle hndl);
extern SANE_Status	ccd300_start (SANE_Handle hndl);
extern void		ccd300_read (SANE_Handle hndl, SANE_Byte *buffer);
extern void		ccd300_stop (SANE_Handle hndl);

#endif
