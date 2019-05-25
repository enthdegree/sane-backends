#!/bin/bash
test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

autoreconf --force --install --verbose --warnings=all "$srcdir"
patch "$srcdir/ltmain.sh" "$srcdir/ltmain.sh.patch"
