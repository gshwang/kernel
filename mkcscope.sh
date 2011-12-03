#!/bin/sh
rm -rf cscope.*
find . -name "*.[ch]" > cscope.files
cscope -bqv -i cscope.files
