#include <sane/sanei.h>

#define ENTRY(name)	PASTE(PASTE(PASTE(sane_,BACKEND_NAME),_),name)

/* The cpp that comes with GNU C 2.5 seems to have troubles understanding
   vararg macros.  */
#if __GNUC__ - 0 > 2 || (__GNUC__ - 0 == 2 && __GNUC_MINOR__ > 5)
# define HAVE_VARARG_MACROS
#endif

#ifndef HAVE_VARARG_MACROS
  extern void sanei_debug (int level, const char *msg, ...);
#endif

#ifdef NDEBUG
# define DBG_INIT(backend, var)
# ifdef HAVE_VARARG_MACROS
#  define DBG(level, msg, args...)
# else
#  define DBG		if (0) sanei_debug
# endif
#  define IF_DBG(x)
#else
# include <stdio.h>

#define DBG_LEVEL	PASTE(sanei_debug_,BACKEND_NAME)

#if defined(BACKEND_NAME) && !defined(STUBS)
int PASTE(sanei_debug_,BACKEND_NAME);
#endif

# define DBG_INIT()					\
  sanei_init_debug (STRINGIFY(BACKEND_NAME),		\
		    &PASTE(sanei_debug_,BACKEND_NAME))

/* The cpp that comes with GNU C 2.5 seems to have troubles understanding
   vararg macros.  */
#ifdef HAVE_VARARG_MACROS
# define DBG(level, msg, args...)					\
  do {									\
    if (DBG_LEVEL >= (level)){			\
      fprintf (stderr, "[" STRINGIFY(BACKEND_NAME) "] " msg, ##args);	\
      fflush(stderr); \
    }			\
  } while (0)

  extern void sanei_init_debug (const char * backend, int * debug_level_var);
#else
# define DBG		sanei_debug
#endif

# define IF_DBG(x)	x
#endif
