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
				AC_DEFINE(HAVE_PTAL)
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
# Checks for jpeg library >= v6B (61), needed for DC210 backend.
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
