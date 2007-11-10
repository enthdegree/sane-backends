dnl
dnl Contains the following macros
dnl   SANE_SET_CFLAGS(is_release)
dnl   SANE_CHECK_MISSING_HEADERS
dnl   SANE_SET_LDFLAGS
dnl   SANE_CHECK_DLL_LIB
dnl   SANE_EXTRACT_LDFLAGS(LDFLAGS, LIBS)
dnl   SANE_CHECK_JPEG
dnl   SANE_CHECK_IEEE1284
dnl   SANE_CHECK_PTHREAD
dnl   SANE_CHECK_LOCKING
dnl   JAPHAR_GREP_CFLAGS(flag, cmd_if_missing, cmd_if_present)
dnl   SANE_LINKER_RPATH
dnl   SANE_CHECK_U_TYPES
dnl   SANE_CHECK_GPHOTO2
dnl   SANE_CHECK_IPV6
dnl   SANE_PROTOTYPES
dnl   AC_PROG_LIBTOOL
dnl

# SANE_SET_CFLAGS(is_release)
# Set CFLAGS. Enable/disable compilation warnings if we gcc is used.
# Warnings are enabled by default when in development cycle but disabled
# when a release is made. The argument is_release is either yes or no.
AC_DEFUN([SANE_SET_CFLAGS],
[
if test "${ac_cv_c_compiler_gnu}" = "yes"; then
  NORMAL_CFLAGS="\
      -W \
      -Wall"
  WARN_CFLAGS="\
      -W \
      -Wall \
      -Wcast-align \
      -Wcast-qual \
      -Wmissing-declarations \
      -Wmissing-prototypes \
      -Wpointer-arith \
      -Wreturn-type \
      -Wstrict-prototypes \
      -pedantic"

  # OS/2 and others don't include some headers with -ansi enabled
  ANSI_FLAG=-ansi
  case "${host_os}" in  
    solaris* | hpux* | os2* | darwin* )
      ANSI_FLAG=
      ;;
  esac
  WARN_CFLAGS="${WARN_CFLAGS} ${ANSI_FLAG}"

  AC_ARG_ENABLE(warnings,
    AC_HELP_STRING([--enable-warnings],
                   [turn on tons of compiler warnings (GCC only)]),
    [
      if eval "test x$enable_warnings = xyes"; then 
        for flag in $WARN_CFLAGS; do
          JAPHAR_GREP_CFLAGS($flag, [ CFLAGS="$CFLAGS $flag" ])
        done
      else
        for flag in $NORMAL_CFLAGS; do
          JAPHAR_GREP_CFLAGS($flag, [ CFLAGS="$CFLAGS $flag" ])
        done
      fi
    ],
    [if test x$1 = xno; then
       # Warnings enabled by default (development)
       for flag in $WARN_CFLAGS; do
         JAPHAR_GREP_CFLAGS($flag, [ CFLAGS="$CFLAGS $flag" ])
       done
    else
       # Warnings disabled by default (release)
       for flag in $NORMAL_CFLAGS; do
         JAPHAR_GREP_CFLAGS($flag, [ CFLAGS="$CFLAGS $flag" ])
       done
    fi])
fi # ac_cv_c_compiler_gnu
])

dnl SANE_CHECK_MISSING_HEADERS
dnl Do some sanity checks. It doesn't make sense to proceed if those headers
dnl aren't present.
AC_DEFUN([SANE_CHECK_MISSING_HEADERS],
[
  MISSING_HEADERS=
  if test "${ac_cv_header_fcntl_h}" != "yes" ; then
    MISSING_HEADERS="${MISSING_HEADERS}\"fcntl.h\" "
  fi
  if test "${ac_cv_header_sys_time_h}" != "yes" ; then
    MISSING_HEADERS="${MISSING_HEADERS}\"sys/time.h\" "
  fi
  if test "${ac_cv_header_unistd_h}" != "yes" ; then
    MISSING_HEADERS="${MISSING_HEADERS}\"unistd.h\" "
  fi
  if test "${ac_cv_header_stdc}" != "yes" ; then
    MISSING_HEADERS="${MISSING_HEADERS}\"ANSI C headers\" "
  fi
  if test "${MISSING_HEADERS}" != "" ; then
    echo "*** The following essential header files couldn't be found:"
    echo "*** ${MISSING_HEADERS}"
    echo "*** Maybe the compiler isn't ANSI C compliant or not properly installed?"
    echo "*** For details on what went wrong see config.log."
    AC_MSG_ERROR([Exiting now.])
  fi
])

# SANE_SET_LDFLAGS
# Add special LDFLAGS
AC_DEFUN([SANE_SET_LDFLAGS],
[
  case "${host_os}" in  
    aix*) #enable .so libraries, disable archives
      LDFLAGS="$LDFLAGS -Wl,-brtl"
      ;;
    darwin*) #include frameworks
      LIBS="$LIBS -framework CoreFoundation -framework IOKit"
      ;;
  esac
])

# SANE_CHECK_DLL_LIB
# Find dll library
AC_DEFUN([SANE_CHECK_DLL_LIB],
[
  dnl Checks for dll libraries: dl
  DL_LIB=""
  if test "${enable_dynamic}" != "no"; then
      # dlopen
      AC_CHECK_HEADERS(dlfcn.h,
      [AC_CHECK_LIB(dl,dlopen, DL_LIB=-ldl)
       saved_LIBS="${LIBS}"
       LIBS="${LIBS} ${DL_LIB}"
       AC_CHECK_FUNCS(dlopen, enable_dynamic=yes,)
       LIBS="${saved_LIBS}"
      ],)
      # HP/UX DLL handling
      AC_CHECK_HEADERS(dl.h,
      [AC_CHECK_LIB(dld,shl_load, DL_LIB=-ldld)
       saved_LIBS="${LIBS}"
       LIBS="${LIBS} ${DL_LIB}"
       AC_CHECK_FUNCS(shl_load, enable_dynamic=yes,)
       LIBS="${saved_LIBS}"
      ],)
      if test -z "$DL_LIB" ; then
      # old Mac OS X/Darwin (without dlopen)
      AC_CHECK_HEADERS(mach-o/dyld.h,
      [AC_CHECK_FUNCS(NSLinkModule, enable_dynamic=yes,)
      ],)
      fi
  fi
  AC_SUBST(DL_LIB)

  DYNAMIC_FLAG=
  if test "${enable_dynamic}" = yes ; then
    DYNAMIC_FLAG=-module
  fi
  AC_SUBST(DYNAMIC_FLAG)

  #check if links for dynamic libs should be created
  case "${host_os}" in
  darwin*) 
    USE_LINKS=no
    ;;
  *)
    USE_LINKS=yes
  esac
  AC_SUBST(USE_LINKS)
])

#
# Separate LIBS from LDFLAGS to link correctly on HP/UX (and other
# platforms who care about the order of params to ld.  It removes all
# non '-l..'-params from $2(LIBS), and appends them to $1(LDFLAGS)
#
# Use like this: SANE_EXTRACT_LDFLAGS(LDFLAGS, LIBS)
AC_DEFUN([SANE_EXTRACT_LDFLAGS],
[tmp_LIBS=""
for param in ${$2}; do
  case "${param}" in
    -l*)
         tmp_LIBS="${tmp_LIBS} ${param}"
         ;;
     *)
         $1="${$1} ${param}"
         ;;
  esac
done
$2="${tmp_LIBS}"
unset tmp_LIBS
unset param
])

#
# Checks for ieee1284 library, needed for canon_pp backend.
AC_DEFUN([SANE_CHECK_IEEE1284],
[
  AC_CHECK_HEADER(ieee1284.h, [
    AC_CACHE_CHECK([for libieee1284 >= 0.1.5], sane_cv_use_libieee1284, [
      AC_TRY_COMPILE([#include <ieee1284.h>], [
	struct parport p; char *buf; 
	ieee1284_nibble_read(&p, 0, buf, 1);
 	], 
        [sane_cv_use_libieee1284="yes"; LIBS="${LIBS} -lieee1284"
      ],[sane_cv_use_libieee1284="no"])
    ],)
  ],)
  if test "$sane_cv_use_libieee1284" = "yes" ; then
    AC_DEFINE(HAVE_LIBIEEE1284,1,[Define to 1 if you have the `ieee1284' library (-lcam).])
  fi
])

#
# Checks for pthread support
AC_DEFUN([SANE_CHECK_PTHREAD],
[

  case "${host_os}" in
  darwin*) # currently only enabled on MacOS X
    use_pthread=yes
    ;;
  *)
    use_pthread=no
  esac

  #
  # now that we have the systems preferences, we check
  # the user
  AC_ARG_ENABLE( [fork-process],
    AC_HELP_STRING([--enable-fork-process],
                   [use fork instead of pthread (default=no for MacOS X, yes for everything else)]),
    [
      if test $enableval != yes ; then
        use_pthread=yes
      fi
    ])
  AC_CHECK_HEADERS(pthread.h,
    [
       AC_CHECK_LIB(pthread,pthread_create)
       have_pthread=yes
       AC_CHECK_FUNCS([pthread_create pthread_kill pthread_join pthread_detach pthread_cancel pthread_testcancel],
	,[ have_pthread=no; use_pthread=no ])
    ],)
 
  if test $use_pthread = yes ; then
    AC_DEFINE_UNQUOTED(USE_PTHREAD, "$use_pthread",
                   [Define if pthreads should be used instead of forked processes.])
  fi
  if test "$have_pthread" = "yes" ; then
    CPPFLAGS="${CPPFLAGS} -D_REENTRANT"
  fi
  AC_MSG_CHECKING([whether to enable pthread support])
  AC_MSG_RESULT([$have_pthread])
  AC_MSG_CHECKING([whether to use pthread instead of fork])
  AC_MSG_RESULT([$use_pthread])
])

#
# Checks for jpeg library >= v6B (61), needed for DC210,  DC240,
# GPHOTO2 and dell1600n_net backends.
AC_DEFUN([SANE_CHECK_JPEG],
[
  AC_CHECK_LIB(jpeg,jpeg_start_decompress, 
  [
    AC_CHECK_HEADER(jconfig.h, 
    [
      AC_MSG_CHECKING([for jpeglib - version >= 61 (6a)])
      AC_EGREP_CPP(sane_correct_jpeg_lib_version_found,
      [
        #include <jpeglib.h>
        #if JPEG_LIB_VERSION >= 61
          sane_correct_jpeg_lib_version_found
        #endif
      ],[sane_cv_use_libjpeg="yes"; LIBS="${LIBS} -ljpeg"; 
      AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
    ],)
  ],)
])

# Checks for tiff library dell1600n_net backend.
AC_DEFUN([SANE_CHECK_TIFF],
[
  AC_CHECK_LIB(tiff,TIFFFdOpen, 
  [
    AC_CHECK_HEADER(tiffio.h, 
    [sane_cv_use_libtiff="yes"; LIBS="${LIBS} -ltiff"],)
  ],)
])

#
# Checks for pthread support
AC_DEFUN([SANE_CHECK_LOCKING],
[
  LOCKPATH_GROUP=uucp
  use_locking=yes
  case "${host_os}" in
    os2* )
      use_locking=no
      ;;
  esac

  #
  # we check the user
  AC_ARG_ENABLE( [locking],
    AC_HELP_STRING([--enable-locking],
                   [activate device locking (default=yes, but only used by some backends)]),
    [
      if test $enableval = yes ; then
        use_locking=yes
      else
        use_locking=no
      fi
    ])
  if test $use_locking = yes ; then
    AC_ARG_WITH([group],
      AC_HELP_STRING([--with-group],
                     [use the specified group for lock dir @<:@default=uucp@:>@]),
        [LOCKPATH_GROUP="$withval"]
    )
    # check if the group does exist
    lasterror=""
    touch sanetest.file
    chgrp $LOCKPATH_GROUP sanetest.file 2>/dev/null || lasterror=$?
    rm -f sanetest.file
    if test ! -z "$lasterror"; then
      AC_MSG_WARN([Group $LOCKPATH_GROUP does not exist on this system.])
      AC_MSG_WARN([Locking feature will be disabled.])
      use_locking=no
    fi
  fi
  if test $use_locking = yes ; then
    INSTALL_LOCKPATH=install-lockpath
    AC_DEFINE([ENABLE_LOCKING], 1, 
              [Define to 1 if device locking should be enabled.])
  else
    INSTALL_LOCKPATH=
  fi
  AC_MSG_CHECKING([whether to enable device locking])
  AC_MSG_RESULT([$use_locking])
  if test $use_locking = yes ; then
    AC_MSG_NOTICE([Setting lockdir group to $LOCKPATH_GROUP])
  fi
  AC_SUBST(INSTALL_LOCKPATH)
  AC_SUBST(LOCKPATH_GROUP)
])

dnl
dnl JAPHAR_GREP_CFLAGS(flag, cmd_if_missing, cmd_if_present)
dnl
dnl From Japhar.  Report changes to japhar@hungry.com
dnl
AC_DEFUN([JAPHAR_GREP_CFLAGS],
[case "$CFLAGS" in
"$1" | "$1 "* | *" $1" | *" $1 "* )
  ifelse($#, 3, [$3], [:])
  ;;
*)
  $2
  ;;
esac
])

dnl
dnl SANE_LINKER_RPATH
dnl
dnl Detect how to set runtime link path (rpath).  Set variable
dnl LINKER_RPATH.  Typical content will be '-Wl,-rpath,' or '-R '.  If
dnl set, add '${LINKER_RPATH}${libdir}' to $LDFLAGS
dnl

AC_DEFUN([SANE_LINKER_RPATH],
[dnl AC_REQUIRE([AC_SUBST])dnl This line resulted in an empty AC_SUBST() !!
  AC_CACHE_CHECK([linker parameter to set runtime link path], LINKER_RPATH,
    [LINKER_RPATH=
    case "$host_os" in
    linux* | freebsd* | netbsd* | openbsd* | irix*)
      # I believe this only works with GNU ld [pere 2001-04-16]
      LINKER_RPATH="-Wl,-rpath,"
      ;;
    solaris*)
      LINKER_RPATH="-R "
      ;;
    esac
    ])
  AC_SUBST(LINKER_RPATH)dnl
])

dnl
dnl   SANE_CHECK_U_TYPES
dnl
dnl Some of the following  types seem to be defined only in sys/bitypes.h on
dnl some systems (e.g. Tru64 Unix). This is a problem for files that include
dnl sys/bitypes.h indirectly (e.g. net.c). If this is true, we add
dnl sys/bitypes.h to CPPFLAGS.
AC_DEFUN([SANE_CHECK_U_TYPES],
[if test "$ac_cv_header_sys_bitypes_h" = "yes" ; then
  AC_MSG_CHECKING([for u_int8_t only in sys/bitypes.h])
  AC_EGREP_CPP(u_int8_t,
  [
  #include "confdefs.h"
  #include <sys/types.h>
  #if STDC_HEADERS
  #include <stdlib.h>
  #include <stddef.h>
  #endif
  ],[AC_MSG_RESULT([no, also in standard headers])],
    [AC_EGREP_HEADER(u_int8_t,netinet/in.h,
       [AC_DEFINE(NEED_SYS_BITYPES_H, 1, [Do we need <sys/bitypes.h>?])
	AH_VERBATIM([NEED_SYS_BITYPES_H_IF],
         [/* Make sure that sys/bitypes.h is included on True64 */
          #ifdef NEED_SYS_BITYPES_H
          #include <sys/bitypes.h>
          #endif])
	AC_MSG_RESULT(yes)],
       [AC_MSG_RESULT([no, not even included with netinet/in.h])])])
fi
AC_CHECK_TYPE(u_int8_t, unsigned char)
AC_CHECK_TYPE(u_int16_t, unsigned short)
AC_CHECK_TYPE(u_int32_t, unsigned int)
AC_CHECK_TYPE(u_char, unsigned char)
AC_CHECK_TYPE(u_int, unsigned int)
AC_CHECK_TYPE(u_long, unsigned long)
])

#
# Checks for gphoto2 libs, needed by gphoto2 backend
AC_DEFUN([SANE_CHECK_GPHOTO2],
[
	AC_ARG_WITH(gphoto2,
	  AC_HELP_STRING([--with-gphoto2],
                         [include the gphoto2 backend @<:@default=yes@:>@]),
	[
	
	# If --with-gphoto2=no or --without-gphoto2, disable backend
	# as "$with_gphoto2" will be set to "no"

	])

	# If --with-gphoto2=yes (or not supplied), first check if 
	# pkg-config exists, then use it to check if libgphoto2 is
	# present.  If all that works, then see if we can actually link
        # a program.   And, if that works, then add the -l flags to 
	# LIBS and any other flags to GPHOTO2_LDFLAGS to pass to 
	# sane-config.
	if test "$with_gphoto2" != "no" ; then 

		AC_CHECK_TOOL(HAVE_GPHOTO2, pkg-config, false)
	
		if test $HAVE_GPHOTO2 != "false" ; then
			if pkg-config --exists libgphoto2 ; then
				with_gphoto2=`pkg-config --modversion libgphoto2`
				CPPFLAGS="${CPPFLAGS} `pkg-config --cflags libgphoto2`"
				GPHOTO2_LIBS="`pkg-config --libs libgphoto2`"
				SANE_EXTRACT_LDFLAGS(GPHOTO2_LDFLAGS, GPHOTO2_LIBS)
				LDFLAGS="$LDFLAGS $GPHOTO2_LDFLAGS"

				AC_SUBST(GPHOTO2_LDFLAGS)

			 	saved_LIBS="${LIBS}"
				LIBS="${LIBS} ${GPHOTO2_LIBS}"
				# Make sure we an really use the library
				AC_CHECK_FUNCS(gp_camera_init,HAVE_GPHOTO2=true, 
					[ LIBS="${saved_LIBS}"
					HAVE_GPHOTO2=false ])
			else
				HAVE_GPHOTO2=false
			fi
		fi
	fi

])

#
# Check for AF_INET6, determines whether or not to enable IPv6 support
# Check for ss_family member in struct sockaddr_storage
AC_DEFUN([SANE_CHECK_IPV6],
[
  AC_MSG_CHECKING([whether to enable IPv6]) 
  AC_ARG_ENABLE(ipv6, 
    AC_HELP_STRING([--disable-ipv6],[disable IPv6 support]), 
      [  if test "$enableval" = "no" ; then
         AC_MSG_RESULT([no, manually disabled]) 
         ipv6=no 
         fi
      ])

  if test "$ipv6" != "no" ; then
    AC_TRY_COMPILE([
	#define INET6 
	#include <sys/types.h> 
	#include <sys/socket.h> ], [
	 /* AF_INET6 available check */  
 	if (socket(AF_INET6, SOCK_STREAM, 0) < 0) 
   	  exit(1); 
 	else 
   	  exit(0); 
      ],[
        AC_MSG_RESULT(yes) 
        AC_DEFINE([ENABLE_IPV6], 1, [Define to 1 if the system supports IPv6]) 
        ipv6=yes
      ],[
        AC_MSG_RESULT([no (couldn't compile test program)]) 
        ipv6=no
      ])
  fi

  if test "$ipv6" != "no" ; then
    AC_MSG_CHECKING([whether struct sockaddr_storage has an ss_family member])
    AC_TRY_COMPILE([
	#define INET6
	#include <sys/types.h>
	#include <sys/socket.h> ], [
	/* test if the ss_family member exists in struct sockaddr_storage */
	struct sockaddr_storage ss;
	ss.ss_family = AF_INET;
	exit (0);
    ], [
	AC_MSG_RESULT(yes)
	AC_DEFINE([HAS_SS_FAMILY], 1, [Define to 1 if struct sockaddr_storage has an ss_family member])
    ], [
		AC_TRY_COMPILE([
		#define INET6
		#include <sys/types.h>
		#include <sys/socket.h> ], [
		/* test if the __ss_family member exists in struct sockaddr_storage */
		struct sockaddr_storage ss;
		ss.__ss_family = AF_INET;
		exit (0);
	  ], [
		AC_MSG_RESULT([no, but __ss_family exists])
		AC_DEFINE([HAS___SS_FAMILY], 1, [Define to 1 if struct sockaddr_storage has __ss_family instead of ss_family])
	  ], [
		AC_MSG_RESULT([no])
		ipv6=no
    	  ])
    ])
  fi	
])

#
# Generate prototypes for functions not available on the system
AC_DEFUN([SANE_PROTOTYPES],
[
AH_BOTTOM([

/* Prototype for getenv */
#ifndef HAVE_GETENV
char * getenv(const char *name);
#endif

/* Prototype for inet_ntop */
#ifndef HAVE_INET_NTOP
#include <sys/types.h>
const char * inet_ntop (int af, const void *src, char *dst, size_t cnt);
#endif

/* Prototype for inet_pton */
#ifndef HAVE_INET_PTON
int inet_pton (int af, const char *src, void *dst);
#endif

/* Prototype for isfdtype */
#ifndef HAVE_ISFDTYPE
int isfdtype(int fd, int fdtype);
#endif

/* Prototype for sigprocmask */
#ifndef HAVE_SIGPROCMASK
int sigprocmask (int how, int *new, int *old);
#endif

/* Prototype for snprintf */
#ifndef HAVE_SNPRINTF
#include <sys/types.h>
int snprintf (char *str,size_t count,const char *fmt,...);
#endif

/* Prototype for strdup */
#ifndef HAVE_STRDUP
char *strdup (const char * s);
#endif

/* Prototype for strndup */
#ifndef HAVE_STRNDUP
#include <sys/types.h>
char *strndup(const char * s, size_t n);
#endif

/* Prototype for strsep */
#ifndef HAVE_STRSEP
char *strsep(char **stringp, const char *delim);
#endif

/* Prototype for usleep */
#ifndef HAVE_USLEEP
unsigned int usleep (unsigned int useconds);
#endif

/* Prototype for vsyslog */
#ifndef HAVE_VSYSLOG
#include <stdarg.h>
void vsyslog(int priority, const char *format, va_list args);
#endif
])
])

m4_include([m4/libtool.m4])
m4_include([m4/byteorder.m4])
m4_include([m4/stdint.m4])
