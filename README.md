# Pony Dock

This is a reimplementation of [Desktop Ponies](https://github.com/RoosterDragon/Desktop-Ponies). The sprites come from that project. see [License](#license).

Animated ponies walk around on your Windows desktop. A control panel picks which ones show up and how they behave.

![Pony Dock](docs/screenshot.png)

## Details for nerds

Behavior runs in C++ on an EnTT registry. Feature controls and pack overrides are built into the control panel.

### Building

MSVC (Visual Studio 2026 or the build tools) with the C++ workload, CMake 3.28 or newer, and Ninja are required.

```
git clone --recurse-submodules https://github.com/kuningashallinta/Pony-Dock.git
cd Pony-Dock
cmake --preset x64-Release
cmake --build --preset x64-Release
```

vcpkg is a submodule and builds nlohmann-json, EnTT and stb on the first configure, so expect that step to take a while. Dear ImGui is vendored as well, on the docking branch.

The overlay is a click-through layered window over the virtual desktop, taking clicks only while the cursor is on a pony. `Packs/` is copied next to `Client.exe` after every build and loaded from there, so the build output already has the layout a release needs.

## License

Pony Dock is licensed under the [MIT License](LICENSE). Artwork stays under [CC BY-NC-SA 3.0](https://creativecommons.org/licenses/by-nc-sa/3.0/), which makes `Packs/` non-commercial and share-alike. [CREDITS.md](CREDITS.md).
