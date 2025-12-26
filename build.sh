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

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--Wall -O3 -std=c99}

mkdir -p out
echo 'Compiling out/clloverview'
$CC $CFLAGS src/clloverview.c -o out/clloverview
