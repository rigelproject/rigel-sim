#!/usr/bin/env bash

echo "Rebuilding test code..."

${MAKE} clean && ${MAKE}

source ../../common.sh

# optionally, run in interactive mode for debug
INTERACTIVE="";
if [ $# -ne 0 ]
then
	INTERACTIVE=" -i ";
  echo "Running in interactive mode...";
fi

echo "Beginning Simulation..."
