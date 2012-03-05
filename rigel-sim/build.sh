#!/usr/bin/env bash

MAKE=${MAKE:-make}

RSDIR=${RIGEL_SIM}/rigel-sim
LOGFILE=${RSDIR}/build.log

PROTOC=protoc

# check for required ENV vars
: ${RIGEL_BUILD?"Need to set RIGEL_BUILD"}
: ${RIGEL_INSTALL?"Need to set RIGEL_INSTALL"}
: ${RIGEL_CODEGEN?"Need to set RIGEL_SIM"}

# build directories
BUILDDIR=${RIGEL_BUILD}/sim/rigel-sim
INSTALLDIR=${RIGEL_INSTALL}/host

RELEASEBUILD=release
RELEASEDIR=$BUILDDIR/$RELEASEBUILD

DEBUGBUILD=debug
DEBUGDIR=$BUILDDIR/$DEBUGBUILD

DEFAULTBUILD=$RELEASEBUILD

# Make parallelism
THREADS=${RIGEL_MAKE_PAR:-4} # default to 4 if not set
echo "Using $THREADS threads"

#Copy over default user.config if it doesn't exist
if [ -e user.config ]
then
	echo "using existing user.config"
else
	echo "user.config does not exist, copying user.config.default -> user.config"
	cp user.config.default user.config
fi

#Copy over default Makefile.external if it doesn't exist
if [ -e Makefile.external ]
then
  echo "using existing Makefile.external"
else
  echo "Makefile.external does not exist, copying Makefile.external.default -> Makefile.external"
  cp Makefile.external.default Makefile.external
fi

###############################################################################
# protocol buffers
###############################################################################
pushd ${RIGEL_SIM}/rigel-sim >/dev/null 2>&1;
echo "Generating protocol buffer classes" | tee ${LOGFILE};
${PROTOC} -Isrc/protobuf --cpp_out=src/protobuf src/protobuf/rigelsim.proto

if [ $? -ne 0 ]
then
  echo "[RIGELSIM BUILD SCRIPT] Protocol buffer class generation failed."
  exit 1
fi

###############################################################################
# autotools
###############################################################################
echo "[RIGELSIM BUILD SCRIPT] (Re)generating autotools files with autoreconf -i" | tee -a ${LOGFILE};
autoreconf -i >> ${LOGFILE} 2>&1;

#Call it a second time; sometimes you'll get an unrecognized macro error
#the first time on AC_SEARCH_LIBS
autoreconf -i >> ${LOGFILE} 2>&1;

if [ $? -ne 0 ]
then
  echo "[RIGELSIM BUILD SCRIPT] autoreconf -i failed."
  exit 1
fi

###############################################################################
# release
###############################################################################
rm -rf $RELEASEDIR/Makefile
mkdir -p $RELEASEDIR
pushd $RELEASEDIR >/dev/null 2>&1;

# configure
echo "Running configure script for release build" | tee -a ${LOGFILE};
${RSDIR}/configure --prefix=${INSTALLDIR} -program-suffix=.$RELEASEBUILD --enable-gplv3 2>&1 | tee -a ${LOGFILE};

if [ $? -ne 0 ]
then
  echo "[RIGELSIM BUILD SCRIPT] release configure failed."
  exit 1
fi

# build release
echo "Building release rigel-sim in $RELEASEDIR" | tee -a ${LOGFILE};
$MAKE -j$THREADS 2>&1 | tee -a ${LOGFILE};

if [ $? -ne 0 ]
then
  echo "[RIGELSIM BUILD SCRIPT] release build failed."
  exit 1
fi

$MAKE install

if [ $? -ne 0 ]
then
  echo "[RIGELSIM BUILD SCRIPT] release install failed."
  exit 1
fi

popd >/dev/null 2>&1;

###############################################################################
# debug 
###############################################################################
DEBUG_CONFIG_OPTIONS=""
rm -rf $DEBUGDIR/Makefile
mkdir -p $DEBUGDIR;
pushd $DEBUGDIR >/dev/null 2>&1;

# configure
echo "Running configure script for debug build" | tee -a ${LOGFILE};
${RSDIR}/configure $DEBUG_CONFIG_OPTIONS --prefix=${INSTALLDIR} -program-suffix=.$DEBUGBUILD --enable-gplv3 --enable-debug --enable-asserts 2>&1 | tee -a ${LOGFILE};

if [ $? -ne 0 ]
then
  echo "[RIGELSIM BUILD SCRIPT] debug configure failed."
  exit 1
fi

# build debug
echo "Building debug rigel-sim in $DEBUGDIR" | tee -a ${LOGFILE};
$MAKE -j$THREADS 2>&1 | tee -a ${LOGFILE};

if [ $? -ne 0 ]
then
  echo "[RIGELSIM BUILD SCRIPT] debug build failed."
  exit 1
fi

$MAKE install

if [ $? -ne 0 ]
then
  echo "[RIGELSIM BUILD SCRIPT] debug install failed."
  exit 1
fi

popd >/dev/null 2>&1;

###############################################################################
# finish up
###############################################################################

# symlink to release rigel-sim from build by default
# for BUILD dir
pushd $BUILDDIR >/dev/null
ln -fs $DEFAULTBUILD/rigelsim rigelsim
popd >/dev/null
# and for INSTALL dir
pushd $INSTALLDIR/bin >/dev/null
ln -fs rigelsim.$DEFAULTBUILD rigelsim
popd >/dev/null

popd >/dev/null 2>&1;

echo "Done." | tee -a ${LOGFILE};

