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
