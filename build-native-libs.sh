#!/usr/bin/env bash
set -e

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <target_name> <prebuild_target_name>"
  exit 1
fi

TARGET_NAME=$1
PREBUILD_TARGET_NAME=$2

OS_TYPE=$(uname -s)

cd external/greener-reporter
cargo build --release

if [[ "$OS_TYPE" == "Darwin" ]]; then
  install_name_tool -id @rpath/libgreener_reporter.dylib target/release/libgreener_reporter.dylib
  install_name_tool -id @rpath/libgreener_servermock.dylib target/release/libgreener_servermock.dylib
elif [[ "$OS_TYPE" == "MINGW"* || "$OS_TYPE" == "MSYS"* || "$OS_TYPE" == "CYGWIN"* ]]; then
  cp target/release/greener_reporter.dll.lib target/release/greener_reporter.lib
  cp target/release/greener_servermock.dll.lib target/release/greener_servermock.lib
fi

cargo test --release

cd ../..

mkdir -p greener-reporter/dist/include
mkdir -p greener-reporter/dist/lib/"$TARGET_NAME"
mkdir -p greener-servermock/dist/include
mkdir -p greener-servermock/dist/lib/"$TARGET_NAME"

cp -r external/greener-reporter/dist/include/greener_reporter greener-reporter/dist/include/
cp -r external/greener-reporter/dist/include/greener_servermock greener-servermock/dist/include/

if [[ "$OS_TYPE" == "Darwin" ]]; then
  cp external/greener-reporter/target/release/libgreener_reporter.dylib greener-reporter/dist/lib/"$TARGET_NAME"/
  cp external/greener-reporter/target/release/libgreener_servermock.dylib greener-servermock/dist/lib/"$TARGET_NAME"/
elif [[ "$OS_TYPE" == "Linux" ]]; then
  cp external/greener-reporter/target/release/libgreener_reporter.so greener-reporter/dist/lib/"$TARGET_NAME"/
  cp external/greener-reporter/target/release/libgreener_servermock.so greener-servermock/dist/lib/"$TARGET_NAME"/
elif [[ "$OS_TYPE" == "MINGW"* || "$OS_TYPE" == "MSYS"* || "$OS_TYPE" == "CYGWIN"* ]]; then
  cp external/greener-reporter/target/release/greener_reporter.dll greener-reporter/dist/lib/"$TARGET_NAME"/
  cp external/greener-reporter/target/release/greener_reporter.lib greener-reporter/dist/lib/"$TARGET_NAME"/
  cp external/greener-reporter/target/release/greener_servermock.dll greener-servermock/dist/lib/"$TARGET_NAME"/
  cp external/greener-reporter/target/release/greener_servermock.lib greener-servermock/dist/lib/"$TARGET_NAME"/
fi

mkdir -p greener-reporter/prebuilds/"$PREBUILD_TARGET_NAME"
mkdir -p greener-servermock/prebuilds/"$PREBUILD_TARGET_NAME"

if [[ "$OS_TYPE" == "Darwin" ]]; then
  cp external/greener-reporter/target/release/libgreener_reporter.dylib greener-reporter/prebuilds/"$PREBUILD_TARGET_NAME"/
  cp external/greener-reporter/target/release/libgreener_servermock.dylib greener-servermock/prebuilds/"$PREBUILD_TARGET_NAME"/
elif [[ "$OS_TYPE" == "Linux" ]]; then
  cp external/greener-reporter/target/release/libgreener_reporter.so greener-reporter/prebuilds/"$PREBUILD_TARGET_NAME"/
  cp external/greener-reporter/target/release/libgreener_servermock.so greener-servermock/prebuilds/"$PREBUILD_TARGET_NAME"/
elif [[ "$OS_TYPE" == "MINGW"* || "$OS_TYPE" == "MSYS"* || "$OS_TYPE" == "CYGWIN"* ]]; then
  cp external/greener-reporter/target/release/greener_reporter.dll greener-reporter/prebuilds/"$PREBUILD_TARGET_NAME"/
  cp external/greener-reporter/target/release/greener_reporter.lib greener-reporter/prebuilds/"$PREBUILD_TARGET_NAME"/ || true
  cp external/greener-reporter/target/release/greener_servermock.dll greener-servermock/prebuilds/"$PREBUILD_TARGET_NAME"/
  cp external/greener-reporter/target/release/greener_servermock.lib greener-servermock/prebuilds/"$PREBUILD_TARGET_NAME"/ || true
fi
