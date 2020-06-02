#!/bin/bash

set -e

travis_fold start "cov-download"
travis_time_start "cov-download"
wget https://scan.coverity.com/download/linux64 --post-data="token=$COVERITY_SCAN_TOKEN&project=Enlightenment+Foundation+Libraries" -O coverity_tool.tgz
tar xzf coverity_tool.tgz
travis_time_finish "cov-download"
travis_fold end "cov-download"
