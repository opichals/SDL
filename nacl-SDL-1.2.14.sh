#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# nacl-SDL-1.2.14.sh
#
# usage:  nacl-SDL-1.2.14.sh
#
# this script downloads, patches, and builds SDL for Native Client
#

NACL_ROOT=$HOME/Sites/nacl_sdk/pepper_16/toolchain/mac_x86/x86_64-nacl
NACL_PREFIX=
NACL_BIN_PATH=$NACL_ROOT/bin

NACLCC=${NACL_PREFIX}gcc
NACLCXX=${NACL_PREFIX}g++
NACLAR=${NACL_PREFIX}ar
NACLRANLIB=${NACL_PREFIX}ranlib
# toolchain/mac_x86_newlib/x86_64-nacl/

PREFIX=$HOME/Sites/nacl_sdk/SDL
SDK_USR=$HOME/Sites/nacl_sdk/pepper_16/toolchain/mac_x86/x86_64-nacl
#PREFIX=$SDK_USR
NACL_SDK_USR=$PREFIX
NACL_SDK_USR_LIB=$PREFIX/lib32
NACL_SDK_USR_INCLUDE=$SDK_USR/include

#readonly URL=http://commondatastorage.googleapis.com/nativeclient-mirror/nacl/SDL-1.2.14.tar.gz
# readonly URL=http://www.libsdl.org/release/SDL-1.2.14.tar.gz
#readonly PATCH_FILE=nacl-SDL-1.2.14.patch
readonly PACKAGE_NAME=SDL-1.2.14-nacl

#source ../../build_tools/common.sh

export LIBS=-lnosys

CustomConfigureStep() {
  echo "Configuring ${PACKAGE_NAME}"
  # export the nacl tools
  export CC=${NACLCC}
  export CXX=${NACLCXX}
  export AR=${NACLAR}
  export RANLIB=${NACLRANLIB}
  export PKG_CONFIG_PATH=${NACL_SDK_USR_LIB}/pkgconfig
  export PKG_CONFIG_LIBDIR=${NACL_SDK_USR_LIB}
  export PATH=${NACL_BIN_PATH}:${PATH};
  #cd ${NACL_PACKAGES_REPOSITORY}/${PACKAGE_NAME}
  ./autogen.sh

  # TODO(khim): remove this when nacl-gcc -V doesn't lockup.
  # See: http://code.google.com/p/nativeclient/issues/detail?id=2074
  #TemporaryVersionWorkaround
  #cd ${NACL_PACKAGES_REPOSITORY}/${PACKAGE_NAME}

  rm -rf ${PACKAGE_NAME}-build
  mkdir ${PACKAGE_NAME}-build
  cd ${PACKAGE_NAME}-build
  set -x 
  ../configure \
    --host=nacl \
    --disable-assembly \
    --disable-pthread-sem \
    --disable-shared \
    --prefix=${NACL_SDK_USR} \
    --exec-prefix=${NACL_SDK_USR} \
    --libdir=${NACL_SDK_USR_LIB} \
    --oldincludedir=${NACL_SDK_USR_INCLUDE} CFLAGS=-m32 CXXFLAGS=-m32 LDFLAGS=-m32
  set +x
}

CustomPackageInstall() {
  #DefaultPreInstallStep
  #DefaultDownloadStep
  #DefaultExtractStep
  #DefaultPatchStep
  CustomConfigureStep
  #DefaultBuildStep
  #DefaultInstallStep
  #DefaultCleanUpStep
}

CustomPackageInstall
exit 0
