#!/usr/bin/env bash

source ../../common.sh

TESTFILE="coherence"
if [ $# -ne 0 ]
then
	TESTFILE=$1;
fi

 rigelas ${TESTFILE}.asm -EL -march=mipsrigel32 -o ${TESTFILE}.ro
 rigelld -EL -Ttext=0 ${TESTFILE}.ro -o ${TESTFILE}.tasks
 rigelobjdump -d -mmipsrigel32 ${TESTFILE}.tasks >${TESTFILE}.obj

$RIGELSIM -clusters 16 -tiles 1 ${TESTFILE}.tasks\
  2>cerr.out 1>cout.out

for ent in `/bin/ls -1 golden/ | grep -e 'rf\.[0-9].*.dump'`; do 
  echo -n "Testing: $ent..."; 
  if [[ "" == "`diff  $ent golden/$ent`" ]]; 
  then 
    echo "SUCCESS"; 
  else 
    echo "FAIL"; 
  fi; 
done;


