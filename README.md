# Pony Dock

Animated ponies that walk around your Windows desktop, with a control panel to pick which ones show up and how they behave. A reimplementation of [Desktop Ponies](https://github.com/RoosterDragon/Desktop-Ponies), whose sprites it uses.

![Pony Dock](docs/screenshot.png)

## Building

Needs MSVC (Visual Studio 2026 or the build tools) with the C++ workload, CMake 3.28+ and Ninja.

```
git clone --recurse-submodules https://github.com/cnevis/Pony-Dock.git
cd Pony-Dock
cmake --preset x64-Release
cmake --build --preset x64-Release
```

The first configure builds the dependencies through vcpkg, so it takes a while. `Packs/` and `Scripts/` are copied next to `Client.exe` on every build.

## How it works

- `Client/Engine/` is generic: sprites, animations, the overlay window and the Lua host. It knows nothing about ponies.
- `Client/Pony/` loads the pony packs in `Packs/` and turns them into things the engine can spawn.
- `Scripts/` holds the behavior, written in Lua. `core.lua` is the default module.

## Writing a module

A module is a Lua file in `Scripts/` that returns a table with `spawn(self)` and `tick(self, dt)`. `self` belongs to one pony and keeps its state between ticks.

```lua
local mod = {}

settings.slider("speed", "Speed multiplier", 1.0, 0.1, 4.0)

function mod.tick(self, dt)
	self.x = self.x + 20 * self:setting("speed") * dt
end

return mod
```

- Read on `self`: `x`, `y`, `width`, `height`, `facing`, `mouse_over`, `dragged` and `pack` (the pony's behavior data)
- Write `x` and `y` to move the pony
- Call `self:play(name, loop)`, `self:set_facing("left")`, `self:setting(id)`, `self:log(message)`
- Call `self:nearby(radius)` to find other ponies and `self:invite(id, behavior)` to ask one to join a behavior

Settings show up in the Modules view. Errors go to the Log view instead of crashing, and scripts reload without a restart.

## License

Code is [MIT](LICENSE). Artwork in `Packs/` is [CC BY-NC-SA 3.0](https://creativecommons.org/licenses/by-nc-sa/3.0/), see [CREDITS.md](CREDITS.md).
