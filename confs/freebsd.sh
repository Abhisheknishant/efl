#!/bin/sh -e
meson -Dsystemd=false -Dv4l2=false -Deeze=false -Devas-loaders-disabler=json \
 $@ . build