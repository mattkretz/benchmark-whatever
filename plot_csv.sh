#!/bin/zsh
# SPDX-License-Identifier: GPL-3.0-or-later */
# Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
#                  Matthias Kretz <m.kretz@gsi.de>

last_title=
ngroups=-1
grep '^"' "$1" \
  | sed -e 's,/\([0-9]\+\)","\,\1,' \
        -e 's,_, ,g' \
        -e 's,<decltype(execution::simd\.\?, ,' \
        -e 's,)>,,' \
        -e 's,(),,g' \
        -e 's,by<\([0-9]\+\)>,by \1,' \
  | while read line; do
  title="${line%%\",*}"
  if [[ "$title" != "$last_title" ]]; then
    echo
    echo
    echo "'$title'" >&2
    last_title="$title"
    ngroups=$((ngroups+1))
  fi
  echo "$line"
done > tmp.csv

xlabel=""
ylabel=""
[[ -n "$3" ]] && xlabel="$3"
[[ -n "$4" ]] && ylabel="$4"

gnuplot <<EOF
set datafile separator comma
set logscale x 2
set xlabel "$xlabel"
set ylabel "$ylabel"
ticformat(x) = x >= 1024 ? x >= 1024*1024 ? sprintf("%dM", x/1024/1024) : sprintf("%dk", x/1024) : sprintf("%d", x)
set output "$1.pdf"
set term pdfcairo enhanced color rounded size 20cm,20cm
plot for [i=0:$ngroups] 'tmp.csv' index i using 2:$2:xtic(ticformat(\$2)) with linespoints title columnheader(1)
set output "$1.png"
set term png enhanced truecolor rounded size 800,800
replot
EOF
