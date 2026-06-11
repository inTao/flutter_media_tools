#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

build_macos() {
    echo "Building for macOS..."
    cargo build --release --manifest-path "$SCRIPT_DIR/Cargo.toml"
    cp "$SCRIPT_DIR/target/release/libnative.dylib" "$PROJECT_ROOT/macos/Libraries/libnative.dylib"
    echo "Done: macos/Libraries/libnative.dylib"
}

build_ios() {
    echo "Building for iOS..."
    rustup target add aarch64-apple-ios 2>/dev/null || true
    cargo build --release --target aarch64-apple-ios --manifest-path "$SCRIPT_DIR/Cargo.toml"
    mkdir -p "$PROJECT_ROOT/ios/Libraries"
    cp "$SCRIPT_DIR/target/aarch64-apple-ios/release/libnative.a" "$PROJECT_ROOT/ios/Libraries/libnative.a"
    echo "Done: ios/Libraries/libnative.a"
}

build_linux() {
    echo "Building for Linux..."
    cargo build --release --manifest-path "$SCRIPT_DIR/Cargo.toml"
    mkdir -p "$PROJECT_ROOT/linux/libs"
    cp "$SCRIPT_DIR/target/release/libnative.so" "$PROJECT_ROOT/linux/libs/libnative.so"
    echo "Done: linux/libs/libnative.so"
}

build_windows() {
    echo "Building for Windows..."
    cargo build --release --manifest-path "$SCRIPT_DIR/Cargo.toml"
    mkdir -p "$PROJECT_ROOT/windows/libs"
    cp "$SCRIPT_DIR/target/release/native.dll" "$PROJECT_ROOT/windows/libs/native.dll"
    echo "Done: windows/libs/native.dll"
}

build_android() {
    echo "Building for Android..."
    rustup target add \
        aarch64-linux-android \
        armv7-linux-androideabi \
        x86_64-linux-android \
        i686-linux-android 2>/dev/null || true

    local targets=("aarch64-linux-android:arm64-v8a" "armv7-linux-androideabi:armeabi-v7a" "x86_64-linux-android:x86_64" "i686-linux-android:x86")
    
    for target_pair in "${targets[@]}"; do
        local target="${target_pair%%:*}"
        local abi="${target_pair##*:}"
        echo "  Building for $abi..."
        cargo build --release --target "$target" --manifest-path "$SCRIPT_DIR/Cargo.toml"
        mkdir -p "$PROJECT_ROOT/android/src/main/jniLibs/$abi"
        cp "$SCRIPT_DIR/target/$target/release/libnative.so" \
           "$PROJECT_ROOT/android/src/main/jniLibs/$abi/libnative.so"
    done
    echo "Done: android/src/main/jniLibs/"
}

case "${1:-all}" in
    macos)   build_macos ;;
    ios)     build_ios ;;
    linux)   build_linux ;;
    windows) build_windows ;;
    android) build_android ;;
    all)
        build_macos
        build_ios
        build_linux
        build_android
        ;;
    *)
        echo "Usage: $0 {macos|ios|linux|windows|android|all}"
        exit 1
        ;;
esac
