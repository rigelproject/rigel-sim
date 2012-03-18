RIGELSIM="${RIGEL_BUILD}/sim/rigel-sim/rigelsim"
MAKE=make

function failed {
  echo -e "\e[1;31m<FAILED>\e[0m" $1
}

function passed {
  echo -e "\e[1;32m<PASSED>\e[0m" $1
}
