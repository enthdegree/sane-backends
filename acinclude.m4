dnl
dnl Contains the following macros
dnl   SANE_EXTRACT_LDFLAGS(LDFLAGS, LIBS)
dnl   SANE_V4L_VERSION
dnl   SANE_CHECK_PTAL
dnl   SANE_CHECK_JPEG
dnl   JAPHAR_GREP_CFLAGS(flag, cmd_if_missing, cmd_if_present)
dnl   SANE_LINKER_RPATH
dnl   SANE_CHECK_U_TYPES
dnl   SANE_CHECK_GPHOTO2
dnl

#
# Separate LIBS from LDFLAGS to link correctly on HP/UX (and other
# platforms who care about the order of params to ld.  It removes all
# non '-l..'-params from $2(LIBS), and appends them to $1(LDFLAGS)
#
# Use like this: SANE_EXTRACT_LDFLAGS(LDFLAGS, LIBS)
AC_DEFUN(SANE_EXTRACT_LDFLAGS,
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
# Test header file <linux/videodev.h> to check if this is Video for
# Linux 1 or 2.  Sets variable sane_v4l_version to 'v4l' or 'v4l2'
# depending on the detected version.
# Test by Petter Reinholdtsen <pere@td.org.uit.no>, 2000-07-07
#
AC_DEFUN(SANE_V4L_VERSION,
[
  AC_CHECK_HEADER(linux/videodev.h)
  if test "${ac_cv_header_linux_videodev_h}" = "yes"
  then
    AC_CACHE_CHECK([Video4Linux version 1 or 2], sane_v4l_version,
    [AC_EGREP_CPP(v4l2_yes,
      [#include <linux/videodev.h>
#ifdef V4L2_MAJOR_VERSION
      v4l2_yes
#endif
      ],[sane_v4l_version=v4l2],

      [sane_v4l_version=v4l]
    )])
  fi
])

#
# Checks for PTAL, needed for MFP support in HP backend.
AC_DEFUN(SANE_CHECK_PTAL,
[
	PTAL_TMP_HAVE_PTAL=no
	AC_ARG_WITH(ptal,
	  [  --with-ptal=DIR         specify the top-level PTAL directory 
                          [default=/usr/local]])
	if test "$with_ptal" = "no" ; then
		echo disabling PTAL
	else
		PTAL_OLD_CPPFLAGS=${CPPFLAGS}
		PTAL_OLD_LDFLAGS=${LDFLAGS}

		if test "$with_ptal" = "yes" ; then
			with_ptal=/usr/local
		fi
		CPPFLAGS="${CPPFLAGS} -I$with_ptal/include"
		LDFLAGS="${LDFLAGS} -L$with_ptal/lib"

		AC_CHECK_HEADERS(ptal.h,
			AC_CHECK_LIB(ptal,ptalInit,
				AC_DEFINE(HAVE_PTAL, 1, [Is PTAL available?])
				LDFLAGS="${LDFLAGS} -lptal"
				PTAL_TMP_HAVE_PTAL=yes))

		if test "${PTAL_TMP_HAVE_PTAL}" != "yes" ; then
			CPPFLAGS=${PTAL_OLD_CPPFLAGS}
			LDFLAGS=${PTAL_OLD_LDFLAGS}
		fi
	fi

	unset PTAL_TMP_HAVE_PTAL
	unset PTAL_OLD_CPPFLAGS
	unset PTAL_OLD_LDFLAGS
])

#
# Checks for jpeg library >= v6B (61), needed for DC210,  DC240, and 
# GPHOTO2 backends.
AC_DEFUN(SANE_CHECK_JPEG,
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

dnl
dnl JAPHAR_GREP_CFLAGS(flag, cmd_if_missing, cmd_if_present)
dnl
dnl From Japhar.  Report changes to japhar@hungry.com
dnl
AC_DEFUN(JAPHAR_GREP_CFLAGS,
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

AC_DEFUN(SANE_LINKER_RPATH,
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
AC_DEFUN(SANE_CHECK_U_TYPES,
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
AC_DEFUN(SANE_CHECK_GPHOTO2,
[

	GPHOTO2_TMP_HAVE_GPHOTO2=no
	AC_ARG_WITH(gphoto2,
	  [  --with-gphoto2=DIR      specify the top-level GPHOTO2 directory 
                          [default=/usr/local]])


	
	if test "$with_gphoto2" = "no" ; then
		echo disabling GPHOTO2
	else
		AC_CHECK_TOOL(HAVE_GPHOTO2, gphoto2, false)
		

		GPHOTO2_OLD_CPPFLAGS=${CPPFLAGS}
		GPHOTO2_OLD_LDFLAGS=${LDFLAGS}

		if test "$with_gphoto2" = "yes" ; then
			with_gphoto2=/usr/local
		fi


		CPPFLAGS="${CPPFLAGS} -I$with_gphoto2/include/gphoto2"
		LDFLAGS="${LDFLAGS} -L$with_gphoto2/lib -L$with_gphoto2/lib/gphoto2"

		AC_CHECK_HEADERS(gphoto2-core.h,
			AC_CHECK_LIB(gphoto2,gp_camera_init,
				LDFLAGS="${LDFLAGS} -lgphoto2"
				GPHOTO2_TMP_HAVE_GPHOTO2=yes))

		if test "${GPHOTO2_TMP_HAVE_GPHOTO2}" != "yes" ; then
			CPPFLAGS=${GPHOTO2_OLD_CPPFLAGS}
			LDFLAGS=${GPHOTO2_OLD_LDFLAGS}
			HAVE_GPHOTO2="false"
		fi
	fi

	unset GPHOTO2_TMP_HAVE_GPHOTO2
	unset GPHOTO2_OLD_CPPFLAGS
	unset GPHOTO2_OLD_LDFLAGS
])

