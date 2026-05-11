# About

Computes initial picks from seismic streams using a Lomax-style filter picker.  The filter picker in short runs many short-term/long-term average filters 
in frequency bands across a range of interest.  Streams are read from the [uDataPacketService](https://github.com/uofuseismo/uDataPacketService) and 
corresponding picks are deposited into the [uFilterPickerMessageStore](https://github.com/uofuseismo/uFilterPickerMessageSTore) for subsequent analysis (e.g., 
refinement, classiffication, association etc.).

<img width="451" height="221" alt="ufilterpicker drawio" src="https://github.com/user-attachments/assets/23a2af00-11d5-4526-b8cb-fcdebb153769" />

# APIs

To be able to import data packets

    git subtree add --prefix uDataPacketServiceAPI https://github.com/uofuseismo/uDataPacketServiceAPI.git main --squash

To be able to export picks 

    git subtree add --prefix uFilterPickerPickBrokerAPI https://github.com/uofuseismo/uFilterPickerPickBrokerAPI.git main --squash

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

    conan install . --build=missing -s build_type=Release -pr:a=Linux-x86_64-clang-21 -s:a compiler.cppstd=23 --output-folder ./conanBuild
    cmake --preset conan-release -DWITH_CONAN=ON -DBUILD_TRAINING=OFF -DUSE_CLANG_TIDY=ON -DCLANG_TIDY_EXECUTABLE=clang-tidy-21 -DMKL_LINK=static -DMKL_THREADING=sequential
    cmake --build --preset conan-release
    ctest --preset conan-release
    cmake --install conanBuild/build/Release

