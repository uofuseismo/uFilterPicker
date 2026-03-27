
To be able to import data packets

    git subtree add --prefix uDataPacketServiceAPI https://github.com/uofuseismo/uDataPacketServiceAPI.git main --squash


# Conan

Create a profile Linux-x86_64-clang-21

    [buildenv]
    CC=/usr/bin/clang-21
    CXX=/usr/bin/clang++-21

    [settings]
    arch=x86_64
    build_type=Release
    compiler=clang
    compiler.cppstd=20
    compiler.libcxx=libstdc++11
    compiler.version=21
    os=Linux

    [conf]
    tools.cmake.cmaketoolchain:generator=Ninja

build-missing downloads and installs missing packages
Release versions of packages (could set to Debug or other cmake build types)

    conan install . --build=missing -s build_type=Release -pr:a=Linux-x86_64-clang-21 --output-folder ./conanBuild
    cmake --preset conan-release -DWITH_CONAN=ON -DBUILD_TRAINING=OFF
    cmake --build --preset conan-release
    ctest --preset conan-release
    cmake --install conanBuild/build/Release

