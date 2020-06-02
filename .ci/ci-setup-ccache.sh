#!/bin/bash

set -e

travis_fold start "ccache-setup"
travis_time_start "ccache-setup"
if [ "$DISTRO" != "" ] ; then
  docker exec $(cat $HOME/cid)  bash -c ".ci/docker-ccache-setup.sh $1"
  docker exec $(cat $HOME/cid)  ccache -pz
elif [ "$TRAVIS_OS_NAME" = "osx" ]; then
  cp .ci/ccache.conf ~/.ccache
  sed -iE '/^base_dir/d' ~/.ccache/ccache.conf
  echo "base_dir = $pwd" >> ~/.ccache/ccache.conf
  ccache -pz
fi
travis_time_finish "ccache-setup"
travis_fold end "ccache-setup"
