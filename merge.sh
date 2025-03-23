#!/bin/sh
###
 # @Author             : Felix
 # @Email              : 307253927@qq.com
 # @Date               : 2025-03-15 20:07:50
 # @LastEditors        : Felix
 # @LastEditTime       : 2025-03-15 20:10:53
### 

# Check if output filename is provided
if [ $# -ne 1 ]; then
    echo "Usage: $0 <output-filename>"
    exit 1
fi

OUTPUT_FILE=$1

cd build
esptool.py --chip esp32s3 merge_bin --output "$OUTPUT_FILE" "@flash_args"