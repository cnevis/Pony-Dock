# Pony Dock

MLP ponies that walk around your Windows desktop, with a control panel to pick which ones show up and how they behave. A C++ reimplementation of [Desktop Ponies](https://github.com/RoosterDragon/Desktop-Ponies).

![Pony Dock](docs/screenshot.png)

## Building

Needs MSVC (Visual Studio 2026 or the build tools) with the C++ workload, CMake 3.28+ and Ninja.

```
git clone --recurse-submodules https://github.com/cnevis/Pony-Dock.git
cd Pony-Dock
cmake --preset x64-Release
cmake --build --preset x64-Release
```

vcpkg is a submodule and builds nlohmann-json, EnTT and stb on the first configure, so that step takes a while. Dear ImGui is vendored on the docking branch. `Packs/` is copied next to `Client.exe` on every build.

## License

Code is [MIT](LICENSE). Artwork in `Packs/` is [CC BY-NC-SA 3.0](https://creativecommons.org/licenses/by-nc-sa/3.0/), see [CREDITS.md](CREDITS.md).
