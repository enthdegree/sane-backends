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
