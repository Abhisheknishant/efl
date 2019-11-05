#!/bin/sh

CI_BUILD_TYPE="$1"

cp .ci/ccache.conf ~/.ccache

if [ "$1" = "release-ready" ] ; then
  ccache -o base_dir="$(pwd)/$(grep '^PACKAGE_STRING' config.log|cut -d\' -f2|tr ' ' -)"
else
  sed -iE '/^base_dir/d' ~/.ccache/ccache.conf
  echo "base_dir = $pwd" >> ~/.ccache/ccache.conf
fi
