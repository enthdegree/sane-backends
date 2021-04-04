# sane-backends
sane-backends forked from https://gitlab.com/sane-project/backends

Modified to allow longer sweeps on a Plustek 8100, cf https://q3q.net/plustek-8100-6x7.html

No real code is needed. Only two files have changed:

 - `backend/genesys/tables_model.cpp`
 - `backend/genesys.conf.in`
 
# Instructions

    ./autogen.sh    
    ./configure --prefix="$HOME/local_sane/" BACKENDS="genesys"
    make
    make install 

Add the following line to your `udev` rules (e.g. as a line in `/etc/udev/rules.d/10-local.rules`):

    ATTRS{idVendor}=="07b3", ATTRS{idProduct}=="130c", MODE="0666"

Ensure that the following reports the local build.

    LD_LIBRARY_PATH="$HOME/local_sane/lib" ~/local_sane/bin/scanimage -V


