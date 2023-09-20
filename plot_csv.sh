#!/bin/zsh
# SPDX-License-Identifier: GPL-3.0-or-later */
# Copyright © 2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH
#                  Matthias Kretz <m.kretz@gsi.de>

has_size=false
if (($# >= 1)) && [[ -f "$1" ]]; then
  firstdata="$(grep '^"' "$1"|head -n1)"
  if [[ ${firstdata%%,*} =~ '/[0-9]+"$' ]]; then
    has_size=true
  fi
  typeset -A columns_idx_for_name
  column_names=()
  i=1
  grep '^name,' "$1"|head -n1|tr ',' '\n'| while read t; do
    t="${t//\"/}"
    columns_idx_for_name+=([$t]=$i)
    column_names+="$t"
    if ((i == 1)) && $has_size; then
      ((++i))
      columns_idx_for_name+=([size]=$i)
      column_names+="size"
    fi
    ((++i))
  done
fi

print_columns() {
  for ((i=1; i<=${#column_names}; ++i)); do
    echo "  $i: ${column_names[$i]}"
  done
}

if (($# <= 1)) || [[ " $* " =~ " (-h|--help) " || ! -f "$1" ]]; then
  cat<<EOF
Usage: $0 <Google benchmark CSV file> <column> [<options>]

Options:
  -x, --xlabel=STRING      label for the x axis
  -y, --ylabel=STRING      label for the y axis
  -f, --filter=REGEX       select a subset of benchmark names
  -s, --substitute=STRING  zsh syntax to rename entries in the legend (e.g.
                           'prefer aligned/old impl'). Multiple substitutions
                           are possible.

EOF
  if [[ -n "$column_names" ]]; then
    echo "Valid <column> values (number or regex):"
    print_columns
  fi
  exit 1
fi

substitutions=()
filter=".*"
xlabel=""
ylabel=""
if $has_size; then
  xlabel="Size"
fi

n=3
while ((n <= $#)); do
  case "${@[n]}" in
    -x|--xlabel)
      ((++n))
      xlabel="${@[n]}"
      ;;
    --xlabel=*)
      xlabel="${@[n]#--xlabel=}"
      ;;
    -y|--ylabel)
      ((++n))
      ylabel="${@[n]}"
      ;;
    --ylabel=*)
      ylabel="${@[n]#--ylabel=}"
      ;;
    -f|--filter)
      ((++n))
      filter="${@[n]}"
      ;;
    --filter=*)
      filter="${@[n]#--filter=}"
      ;;
    -s|--substitute)
      ((++n))
      substitutions+="${@[n]}"
      ;;
    --substitute=*)
      substitutions+="${@[n]#--substitute=}"
      ;;
    *)
      echo "Unknown option '${@[n]}'"
      exit 1
      ;;
  esac
  ((++n))
done

if [[ "$2" =~ '[^0-9]' || "$2" -gt ${#column_names} ]]; then
  col=${columns_idx_for_name[$2]}
  if [[ -z "$col" ]]; then
    for k in ${column_names}; do
      if [[ "$k" =~ "$2" ]]; then
        if [[ -n "$col" ]]; then
          echo "Ambiguous column '$2'. Match exactly one of:"
          print_columns
          exit 1
        fi
        col=${columns_idx_for_name[$k]}
        if [[ -z "$ylabel" ]]; then
          ylabel="$k"
        fi
      fi
    done
  elif [[ -z "$ylabel" ]]; then
    ylabel="$2"
  fi
  if [[ -z "$col" ]]; then
    echo "Unknown column '$2'. Expected one of:"
    print_columns
    exit 1
  fi
else
  col=$2
  if [[ -z "$ylabel" ]]; then
    ylabel="${column_names[$2]}"
  fi
fi

titles=()
last_title=
last_size=0
ngroups=-1
grep '^"' "$1" \
  | sed -e 's,/\([0-9]\+\)","\,\1,' \
        -e 's,_, ,g' \
        -e 's,<decltype(vir::execution::simd\.\?\(.*\))>, \1,' \
        -e 's,()\., ,g' \
        -e 's,(),,g' \
        -e 's,by<\([0-9]\+\)>,by \1,' \
  | while read line; do
  size=${${line#*\",}%%,*}
  if (((size & (size - 1)) != 0)); then
    line="${line/\",/ (non-power2)\",}"
  fi
  title="${line%%\",*}"
  title="${title:1}"
  tmp="$title"
  for s in "${substitutions[@]}"; do
    tmp="$(eval "echo \${tmp/${s}}")"
  done
  line="${line/$title/$tmp}"
  title="$tmp"
  if [[ "$title" != "$last_title" ]] || ((size < last_size)); then
    if [[ ! "$title" =~ "$filter" ]]; then
      continue
    fi
    titles+="$title"
    if [[ -n "$last_title" ]]; then
      echo
      echo
    fi
    echo "${(j:,:)column_names}"
    echo " * ${title}" >&2
    last_title="$title"
    ngroups=$((ngroups+1))
  fi
  last_size=$size
  echo "$line"
done > tmp.csv

gnuplot <<EOF
array TITLES[$((ngroups+1))] = [ "${(j:", ":)titles}" ]
set datafile separator comma
set logscale x 2
set xlabel "$xlabel"
set ylabel "$ylabel"
ticformat(x) = x >= 1024 ? x >= 1024*1024 ? sprintf("%dM", x/1024/1024) : sprintf("%dk", x/1024) : sprintf("%d", x)
set output "$1.pdf"
set term pdfcairo enhanced color rounded size 20cm,20cm
plot for [i=0:$ngroups] 'tmp.csv' index i using 2:$col:xtic(ticformat(\$2)) with linespoints title TITLES[i+1]
set output "$1.png"
set term png enhanced truecolor rounded size 800,800
replot
EOF
