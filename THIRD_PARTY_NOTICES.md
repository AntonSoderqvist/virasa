# Third-Party Notices

Virasa is licensed under the Apache License, Version 2.0 (see `LICENSE`).

This product bundles, links against, or otherwise incorporates the following
third-party components. Each component remains subject to its own license,
reproduced or referenced below.

---

## SDL (Simple DirectMedia Layer)

- Source: https://github.com/libsdl-org/SDL
- License: zlib license
- Location: `vendor/SDL/`

Full license text: see `vendor/SDL/LICENSE.txt`.

---

## Quill

- Source: https://github.com/odygrd/quill
- License: MIT
- Location: `vendor/quill/`

Full license text: see `vendor/quill/LICENSE`.

---

## GoogleTest

- Source: https://github.com/google/googletest
- License: BSD 3-Clause
- Location: `vendor/googletest/`

Full license text: see `vendor/googletest/LICENSE`.

---

## GLM

- Source: https://github.com/g-truc/glm
- License: The Happy Bunny License (Modified MIT) or MIT
- Used via: `find_package(glm)` (system / package-managed dependency, not vendored).

---

## FreeType

- Source: https://gitlab.freedesktop.org/freetype/freetype
- License: The FreeType Project License (FTL), a BSD-style permissive license.
  Virasa elects the FTL option (FreeType is dual-licensed under FTL or GPLv2;
  the FTL option is selected for Apache-2.0 compatibility).
- Location: `vendor/freetype/`

Required attribution:

> Portions of this software are copyright © The FreeType Project
> (www.freetype.org). All rights reserved.

Full license text: see `vendor/freetype/docs/FTL.TXT`.

---

## stb_image

- Source: https://github.com/nothings/stb
- License: Public Domain (or MIT, at user's option) — dual-licensed per
  the notice in `stb_image.h`.
- Location: `vendor/stb/` (single-header: `stb_image.h`)

License text is reproduced at the bottom of `vendor/stb/stb_image.h`.

---

## tinygltf

- Source: https://github.com/syoyo/tinygltf
- License: MIT
- Location: `vendor/tinygltf/` (`tiny_gltf.h`, `tinygltf_impl.cpp`)

The implementation TU is compiled with `TINYGLTF_NO_STB_IMAGE`,
`TINYGLTF_NO_STB_IMAGE_WRITE`, and `TINYGLTF_NO_EXTERNAL_IMAGE`; image
decoding is performed by Virasa's `editor.io.ImageLoader` via the
separately vendored stb_image.

License text is reproduced at the top of `vendor/tinygltf/tiny_gltf.h`.

---

## nlohmann/json

- Source: https://github.com/nlohmann/json
- License: MIT
- Location: `vendor/tinygltf/json.hpp` (bundled inside tinygltf as the
  single-header `json.hpp`; not vendored separately).

License text is reproduced at the top of `vendor/tinygltf/json.hpp`.

---

## MikkTSpace

- Source: http://www.mikktspace.com/
  (reference implementation by Morten S. Mikkelsen)
- License: zlib-style permissive license (see header).
- Location: `vendor/mikktspace/` (`mikktspace.c`, `mikktspace.h`)

Required attribution:

> Copyright (C) 2011 by Morten S. Mikkelsen. Provided 'as-is', without
> any express or implied warranty.

Full license text: see the header block at the top of
`vendor/mikktspace/mikktspace.h`.

---

## JetBrains Mono

- Source: https://github.com/JetBrains/JetBrainsMono
- License: SIL Open Font License 1.1 (OFL)
- Location: `vendor/JetBrainsMono/`

JetBrains Mono is a trademark of JetBrains s.r.o.

Full license text: see `vendor/JetBrainsMono/OFL.txt`.
