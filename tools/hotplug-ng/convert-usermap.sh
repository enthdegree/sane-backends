#!/bin/bash
#
# Converts libsane.usermap to the new SANE hotplug db
#

if [ ! -e libsane.db ]; then
    cat > libsane.db <<EOF
# This file is part of the SANE distribution
#
# USB Vendor/Product IDs for scanners supported by SANE
#
# 0xVVVV<tab>0xPPPP<tab>root:scanner<tab>0660<tab>[/usr/local/bin/foo.sh]
#

EOF
fi

cat "$1" | { while read map; do
    if $(echo "$map" | grep -q ^# > /dev/null); then
	echo $map >> libsane.db
    else
	set $map

	echo -e "$3\t$4\troot:scanner\t0660\t" >> libsane.db
    fi
done }

exit 0
