#ifndef _SANEI_DEBUG_H
#define _SANEI_DEBUG_H

#include <sane/sanei.h>

#define ENTRY(name)     PASTE(PASTE(PASTE(sane_,BACKEND_NAME),_),name)

/* The cpp that comes with GNU C 2.5 seems to have troubles understanding
   vararg macros.  */
#if __GNUC__ - 0 > 2 || (__GNUC__ - 0 == 2 && __GNUC_MINOR__ > 5)
# define HAVE_VARARG_MACROS
#endif

extern void sanei_debug (int level, const char *msg, ...);

#ifdef NDEBUG
# define DBG_LEVEL  (0)
# define DBG_INIT()
# ifdef HAVE_VARARG_MACROS
#  define DBG(level, msg, args...)
# else
#  define DBG           if (0) sanei_debug
# endif
#  define IF_DBG(x)
#else
# include <stdio.h>

#define DBG_LEVEL       PASTE(sanei_debug_,BACKEND_NAME)

#if defined(BACKEND_NAME) && !defined(STUBS)
int DBG_LEVEL;
#endif

# define DBG_INIT()                                     \
  sanei_init_debug (STRINGIFY(BACKEND_NAME), &DBG_LEVEL)

/* The cpp that comes with GNU C 2.5 seems to have troubles understanding
   vararg macros.  */
#ifdef HAVE_VARARG_MACROS
extern void sanei_debug_max (int level, int max_level, const char *msg, ...);

# define DBG(level, msg, args...)                                       \
  do {                                                                  \
      sanei_debug_max ( level, DBG_LEVEL,                               \
                        "[" STRINGIFY(BACKEND_NAME) "] " msg, ##args);  \
  } while (0)

  extern void sanei_init_debug (const char * backend, int * debug_level_var);
#else
# define DBG            sanei_debug
#endif /* HAVE_VARARG_MACROS */

# define IF_DBG(x)      x
#endif /* NDEBUG */

#endif /* _SANEI_DEBUG_H */
