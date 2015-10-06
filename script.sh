#!/bin/bash

bar_char="#"
width=60

if [[ $# -ne 2 ]]
then
	echo "Usage: $0 [max_players] [team_number]"
	exit 1
fi
if [[ $2 -lt 2 ]] || [[ $2 -gt 9 ]]
then
	echo "[team_number must be between 2 and 9]"
	exit 1
fi
./a.out $1 1 &
for ((i = 0 ; i <= $(($1 - 1)) ; i += 1))
do
	perc=$(( (($i + 1) * 100) / $1 ))
	num=$(( (($i + 1) * $width) / $1))
	bar=
	if [ $num -gt 0 ]; then
		bar=$(printf "%0.s${bar_char}" $(seq 1 $num))
	fi
	line=$(printf "[%-${width}s] (%d%%)" "$bar" "$perc")
	echo -en "${line}\r"
	./a.out $(((($i + 1) % $2) + 1)) &
done
echo ""
