#!/bin/bash -eu
#
# Copyright 2025 Nigel Tao.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

# --------

# ./run_golden_tests.sh
# checks that ./out/clloverview reproduces the existing golden output.
#
# ./run_golden_tests.sh -o
# overwrites the golden output.
#
# By default, with no further arguments, the inputs are test/golden/*
# (excluding *.txt files) and the outputs are test/golden/*.txt (and on
# failures, the incorrect output is test/golden/*.failed.txt).

if [[ ! -e ./out/clloverview ]]; then
	echo "./out/clloverview doesn't exist, run ./build.sh first."
	exit 1
fi

overwrite=0
if [[ $# -gt 0 && $1 == "-o" ]]; then
	overwrite=1
	shift
fi

sources=$@
if [[ $# -eq 0 ]]; then
	sources=test/golden/*
fi

fail=0
for f in $sources; do
	if [[ $f == *.txt ]]; then
		continue
	elif [[ $overwrite -ne 0 ]]; then
		./out/clloverview $f > $f.txt
		echo Overwrote $f.txt
		continue
	fi

	have=$(./out/clloverview $f)
	want=$(<$f.txt)
	if [[ "$have" == "$want" ]]; then
		echo "OK     " $f
		rm -f $f.failed.txt
	else
		echo "FAILED " $f
		echo "$have" >$f.failed.txt
		fail=1
	fi
done

if [[ $fail -ne 0 ]]; then
	exit 1
fi
