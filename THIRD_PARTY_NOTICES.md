# NoJobEngine — Third-Party Notices

NoJobEngine uses third-party open-source software. Those components are not
covered by the NoJobEngine proprietary license. They remain subject to their
respective licenses and copyrights.

This file is intended to provide a clear dependency inventory for source and
binary distributions. When distributing NoJobEngine, preserve the applicable
upstream license texts and notices supplied with each dependency.

## Dependency inventory

| Component | Version / revision used | License | Upstream |
|---|---|---|---|
| GLFW | 3.4 | zlib/libpng | https://github.com/glfw/glfw |
| GLM | 1.0.1 | MIT (selected from upstream alternatives) | https://github.com/g-truc/glm |
| Assimp | v5.4.3 | BSD 3-Clause-style license | https://github.com/assimp/assimp |
| glad | v2.0.8 generator | Permissive; generated code may include Public Domain/WTFPL/CC0 and Khronos material subject to its applicable terms | https://github.com/Dav1dde/glad |
| stb | commit `2c980bb59875b0d32144a71867fbdebb2f77cd20` | Public Domain or MIT | https://github.com/nothings/stb |
| miniaudio | 0.11.25 | Public Domain or MIT-0 | https://github.com/mackron/miniaudio |
| Dear ImGui | v1.92.5-docking | MIT | https://github.com/ocornut/imgui |
| ImGuizmo | commit `18cef5e031d8c6973d80284c67f60549fafd78c1` | MIT | https://github.com/CedricGuillemet/ImGuizmo |
| Jolt Physics | v5.6.0 | MIT | https://github.com/jrouwe/JoltPhysics |

---

## GLFW

Copyright (c) 2002-2006 Marcus Geelnard
Copyright (c) 2006-2019 Camilla Löwy <elmindreda@glfw.org>

This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use of
this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to
the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

---

## GLM

NoJobEngine elects to use GLM under the MIT license option provided by the
upstream project. Preserve the GLM copyright and MIT permission notice included
with the fetched GLM source when redistributing GLM or substantial portions of
it.

---

## Assimp

Open Asset Import Library (assimp)
Copyright (c) 2006-2026, assimp team
All rights reserved.

Assimp permits redistribution and use in source and binary forms, with or
without modification, subject to the conditions in its upstream LICENSE file.
Source distributions must retain its copyright notice, conditions and
disclaimer. Binary distributions must reproduce them in documentation and/or
other materials supplied with the distribution. The names of the assimp team
and contributors may not be used to endorse derived products without specific
prior written permission.

The complete authoritative Assimp license is supplied by the dependency fetched
from https://github.com/assimp/assimp and should be included verbatim with any
redistribution containing Assimp.

---

## glad

NoJobEngine uses glad 2.0.8 to generate/load graphics API bindings.

The glad project states that generated code is available under permissive terms
such as Public Domain, WTFPL or CC0, while generated material originating from
Khronos specifications may be subject to additional terms, including Apache
License 2.0 for applicable specifications.

For a release build, retain the license/notices emitted with or applicable to
the exact generated glad output used by that release.

---

## stb

stb libraries are dual offered as public domain or under the MIT license.
NoJobEngine may rely on the MIT option where a conventional copyright license is
preferred. Each stb source file contains its applicable license statement.

Pinned revision:
`2c980bb59875b0d32144a71867fbdebb2f77cd20`

---

## miniaudio

miniaudio 0.11.25 is offered as public domain or under MIT-0. NoJobEngine may
rely on MIT-0 where a conventional copyright license is preferred. Preserve the
license statement contained in the distributed `miniaudio.h` when redistributing
the library.

---

## Dear ImGui

Dear ImGui is licensed under the MIT License. Copyright belongs to Omar Cornut
and Dear ImGui contributors.

Permission is granted under the upstream MIT terms to use, copy, modify, merge,
publish, distribute, sublicense and/or sell copies, provided that the upstream
copyright and permission notice are included in all copies or substantial
portions of the software.

---

## ImGuizmo

MIT License

Copyright (c) 2016-2026 Cedric Guillemet and contributors

Permission is granted under the upstream MIT terms to use, copy, modify, merge,
publish, distribute, sublicense and/or sell copies, provided that the upstream
copyright and permission notice are included in all copies or substantial
portions of the software.

Pinned revision:
`18cef5e031d8c6973d80284c67f60549fafd78c1`

---

## Jolt Physics

Jolt Physics is distributed under the MIT License. Preserve the copyright and
MIT permission notice contained in the Jolt Physics distribution when
redistributing Jolt Physics or substantial portions of it.

---

## Distribution checklist

Before publishing a source or binary release of NoJobEngine:

1. Keep this `THIRD_PARTY_NOTICES.md` with the release documentation.
2. Include the complete upstream license text for every dependency actually
   shipped with the release.
3. Keep copyright/license headers in third-party source files intact.
4. For glad, preserve notices applicable to the exact generated bindings and
   Khronos material included in the release.
5. Re-run the dependency/license audit whenever a dependency is added, replaced,
   upgraded, vendored, or its license changes.
6. Audit third-party assets (models, textures, HDRIs, fonts, audio and demo
   content) separately before redistributing them; software dependency licenses
   do not cover those assets.

## Scope

This notice documents the dependencies identified in NoJobEngine's CMake build
configuration as audited in October 2026. It is not legal advice and does not
replace the authoritative license text distributed by each upstream project.
