#!/bin/bash
cd wslay
autoreconf -i
automake
autoconf
./configure
make