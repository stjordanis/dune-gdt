#!/usr/bin/env python3

tpl = '''# This file is part of the dune-gdt project:
#   https://github.com/dune-community/dune-gdt
# Copyright 2010-2016 dune-gdt developers and contributors. All rights reserved.
# License: BSD 2-Clause License (http://opensource.org/licenses/BSD-2-Clause)
# Authors:
#   Felix Schindler (2016)

# THIS FILE IS AUTOGENERATED -- DO NOT EDIT #

sudo: required
dist: trusty
language: generic

addons:
  apt:
    sources: &commonsources
    packages: &commonpackages
    - gfortran
    - cmake
    - cmake-data
    - doxygen
    - texlive-base
    - python-virtualenv
    - python3-pytest
    - python-pytest
    - libboost1.55-all-dev
    - python-pip
    - libtbb-dev
    - ccache
    - libsuitesparse-dev
    - lcov
    - curl
    - ninja-build
    - python3-requests
    - libeigen3-dev

before_install:
  - |-
    : ${OPTS:="config.opts/travis.ninja"} && export OPTS
  # workaround for gnutls bug is using git built against curl-openssl
  - sudo -E rm /etc/apt/sources.list.d/*git*.list
  - sudo -E add-apt-repository -y ppa:pymor/travis && sudo -E aptitude update && sudo -E aptitude install git -yq
  - cd $HOME
  - test -d src || git clone https://github.com/dune-community/dune-gdt-super.git src
  - cd ${SUPERDIR}
  - git checkout master
  - git submodule update --init --recursive
  - git submodule status
  - ${SUPERDIR}/.travis/trusty_apt_setup.bash
  - export PATH=/usr/lib/ccache:$PATH
  - ccache -s
  - sudo -E gem install mtime_cache
{% raw %}
  - mtime_cache --verbose dune-*/**/*.{%{cpp}} -c ~/.mtime_cache/cache.json
{%- endraw %}
  # our local scripts look for an OPTS env entry
  - ./local/bin/download_external_libraries.py
  - ./local/bin/build_external_libraries.py
  # ensures ${MY_MODULE} from travis own checkout is used
  - echo removing modules ${MODULES_TO_DELETE}
  - rm -rf ${MODULES_TO_DELETE} ${MY_MODULE}

# command to install dependencies
install:
  - cd ${SUPERDIR}
  #- export INST_DCTRL=$HOME/dune/bin/dunecontrol
  - export SRC_DCTRL=$PWD/dune-common/bin/dunecontrol
  - ${SRC_DCTRL} ${BLD} all
  # move my travis checkout into this source tree
  - cp -ra ${TRAVIS_BUILD_DIR} .

before_script:
    - $HOME/src/.travis/add_swap.sh
    - ${SRC_DCTRL} ${BLD} --only=${MY_MODULE} configure
    - if [[ "x${TESTS}" != "xheadercheck" ]]; then ${SRC_DCTRL} ${BLD} --only=${MY_MODULE} make; fi
    - if [[ "x${TESTS}" != "xheadercheck" ]]; then travis_wait 50 ${SRC_DCTRL} ${BLD} --only=${MY_MODULE} bexec ninja -v test_binaries_builder_${TESTS}; fi

script:
    - if [[ "x${TESTS}" != "xheadercheck" ]]; then travis_wait 50 ${SRC_DCTRL} ${BLD} --only=${MY_MODULE} bexec ctest -j 2 -L "^builder_${TESTS}$"; fi
    - if [[ "x${TESTS}" == "xheadercheck" ]]; then ${SRC_DCTRL} ${BLD} --only=${MY_MODULE} bexec ninja headercheck; fi
    - ${SUPERDIR}/.travis/init_sshkey.sh ${encrypted_95fb78800815_key} ${encrypted_95fb78800815_iv} keys/dune-community/dune-gdt-testlogs
    # retry this step because of the implicated race condition in cloning and pushing with multiple builder running in parallel
    - unset GH_TOKEN
    - if [[ "x${TESTS}" != "xheadercheck" ]]; then travis_retry ${SUPERDIR}/scripts/bash/travis_upload_test_logs.bash ${SUPERDIR}/${MY_MODULE}/${DUNE_BUILD_DIR}/dune/gdt/test/; fi
    - ${SRC_DCTRL} ${BLD} --only=${MY_MODULE} make install | grep -v "Installing"
    - ${SRC_DCTRL} ${BLD} --only=${MY_MODULE} make package_source

before_cache:
    # cleanup build dir to not exceed max travis cache size/time
    - rm -r ${DUNE_BUILD_DIR}/${MY_MODULE}/dune/gdt/test

# runs independent of 'script' failure/success
after_script:
    - |
     if [ "x${CLANG_FORMAT}" != "x" ] ; then
        git config --global hooks.clangformat ${CLANG_FORMAT}
        PYTHONPATH=${SUPERDIR}/scripts/python/ python3 -c "import travis_report as tp; tp.clang_format_status(\\"${TRAVIS_BUILD_DIR}\\")"
     fi
    - |
      if [[ $TRAVIS_JOB_NUMBER == *.1 ]] ; then
        ${SRC_DCTRL} ${BLD} --only=${MY_MODULE} make doc
        ${SUPERDIR}/.travis/init_sshkey.sh ${encrypted_95fb78800815_key} ${encrypted_95fb78800815_iv} keys/dune-community/dune-community.github.io
        ${SUPERDIR}/.travis/deploy_docs.sh ${MY_MODULE} ${DUNE_BUILD_DIR}
      fi
    - ccache -s

notifications:
  email:
    on_success: change
    on_failure: change
    on_start: never
  webhooks:
    urls:
      - https://buildtimetrend.herokuapp.com/travis
      - https://webhooks.gitter.im/e/2a38e80d2722df87f945

branches:
  except:
    - gh-pages

cache:
  directories:
    - $HOME/.ccache
    # don't use potentially not-yet-set matrix var here
    - $HOME/dune_build
    - $HOME/.mtime_cache

env:
  global:
    - SUPERDIR=${HOME}/src
    - PATH=${SUPERDIR}/local/bin:$PATH
    - LD_LIBRARY_PATH=${SUPERDIR}/local/lib64:${SUPERDIR}/local/lib:$LD_LIBRARY_PATH
    - PKG_CONFIG_PATH=${SUPERDIR}/local/lib64/pkgconfig:${SUPERDIR}/local/lib/pkgconfig:${SUPERDIR}/local/share/pkgconfig:$PKG_CONFIG_PATH
    - MY_MODULE=dune-gdt
    - DUNE_BUILD_DIR=${HOME}/dune_build/
    - INSTALL_DIR=$HOME/dune
    - CTEST_OUTPUT_ON_FAILURE=1
    - DCTL_ARGS="--builddir=${DUNE_BUILD_DIR} --use-cmake"
    - DBG="${DCTL_ARGS} --opts=config.opts/travis.ninja"

matrix:
  include:
#   gcc 5
    - os: linux
      addons: &gcc5
        apt:
          sources:
          - *commonsources
          - 'ubuntu-toolchain-r-test'
          packages:
          - *commonpackages
          - ['g++-5', 'gcc-5']
      env: CC=gcc-5 TESTS=0 BLD=${DBG} CXX=g++-5
{% for c in builders %}
    - os: linux
      addons:  *gcc5
      env: CC=gcc-5 TESTS={{c}} BLD=${DBG} CXX=g++-5
{%- endfor %}
    - os: linux
      addons:  *gcc5
      env: CC=gcc-5 TESTS=headercheck BLD=${DBG} CXX=g++-5 CLANG_FORMAT='/usr/bin/clang-format-3.8'

#   clang 3.8
    - os: linux
      compiler: clang
      addons: &clang38
        apt:
          sources:
          - *commonsources
          - ['ubuntu-toolchain-r-test']
          packages:
          - *commonpackages
      env: CC=clang-3.8 TESTS=0 BLD=${DBG} CXX=clang++-3.8
{% for c in builders %}
    - os: linux
      env: CC=clang-3.8 TESTS={{c}} BLD=${DBG} CXX=clang++-3.8
{%- endfor %}
    - os: linux
      addons:  *clang38
      env: CC=clang-3.8 TESTS=headercheck BLD=${DBG} CXX=clang++-3.8 CLANG_FORMAT='/usr/bin/clang-format-3.8'

# THIS FILE IS AUTOGENERATED -- DO NOT EDIT #
'''

import os
import jinja2
tpl = jinja2.Template(tpl)
with open(os.path.join(os.path.dirname(__file__), '.travis.yml'), 'wt') as yml:
    yml.write(tpl.render(builders=range(1,25)))
