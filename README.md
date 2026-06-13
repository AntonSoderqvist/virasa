# virasa

virasa is a (currently not so good) vi-like 3d editor built with [calchestra](https://github.com/AntonSoderqvist/calchestra).

I am a big fan of 3D applications and vim. I figured it would be kind of nice to merge both worlds.

## Status

Early days on this project, as of May 19 2026, virasa can load gltf files and accepts a limited amount of commands to modify the scene.

## Goals

I am also considering implementing some AI native mechanisms to autonomously interact with the 3d viewport.

The aspirations are strong, but the flesh is weak. Very happy to entertain suggestions, but as you might be able to tell, my work is cut out for me in the immediate future. Leave a note on the Discussions page!

## Roadmap

Will be adding ECS behavior system support, improving draw efficiency, hot reloading shaders/assets, and improving/expanding the UI capabilities.

## Build

On Windows, configure and build from any Command Prompt with Visual Studio 2022 x64:

```powershell
cmake -S . -B build
cmake --build build
```

If you prefer an explicit preset instead of the default generator, use:

```powershell
cmake --preset windows-vs2022-x64
cmake --build --preset windows-vs2022-x64
```

If `build/` was already generated with a different CMake generator, remove that directory once so CMake can regenerate the cache cleanly.

## License

[Apache 2.0](LICENSE)

## Notes

The real goal of this project is to expand [calchestra](https://github.com/AntonSoderqvist/calchestra) to the realm of C++ with a meaningfully challenging project.

I will be publishing the contracts for virasa in due time, but for now they are volatile as I stamp out the dynamics.
