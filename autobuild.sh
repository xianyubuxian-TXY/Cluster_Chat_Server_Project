##################################################
# File Name: autobuild.sh
# Author: tang xian yu
# mail: 2646163045@qq.com
# Created Time: 2025年8月6日 星期三 11:43:50
#################################################
# !/bin/bash

set -x

rm -rf $(pwd)/build/*
cd $(pwd)/build &&
cmake .. &&
make

