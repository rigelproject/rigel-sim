#!/bin/sh
THREADS=${RIGEL_MAKE_PAR:-4}
MAKE=${MAKE:-make}

#RandomLib doesn't have a configure script, so set PREFIX when
#calling make
$MAKE -C ${RIGEL_SIM}/RandomLib-1.3 -j${THREADS}
$MAKE -C ${RIGEL_SIM}/RandomLib-1.3 PREFIX=${RIGEL_INSTALL}/host install
