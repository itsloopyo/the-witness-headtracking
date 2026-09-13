# Third-Party Notices

TheWitnessHeadTracking bundles, statically links, or credits the third-party components
listed below. Each remains the property of its authors and is used under its own
licence. Where a licence requires the copyright notice, the conditions and the
disclaimer to accompany a binary distribution, the full text is reproduced here
verbatim, and this file ships at the root of every release ZIP we publish.

Nothing in this repository is derived from, or redistributes any part of,
The Witness. No game code, no extracted assets and no data files.

| Component | Version | Licence | How it ships |
|-----------|---------|---------|--------------|
| MinHook | v1.3.4, vendored | BSD-2-Clause | Compiled into `openvr_api.dll` |
| cameraunlock-core | bd22895bb30ab7946d780b0af5782755e33e2cba | MIT | Compiled into `openvr_api.dll` |
| OpenTrack | n/a | ISC | Not bundled; UDP protocol interoperability only |
| OpenVR | n/a | BSD-3-Clause | Not bundled; export names and call signatures only |

---

## MinHook

- **Version:** the `v1.3.4` release tag, with one local change
- **License:** BSD-2-Clause
- **Upstream:** https://github.com/TsudaKageyu/minhook
- **Usage:** Installs the runtime function hooks the camera and window code
  attach to.
- **Bundled:** yes. Vendored inside the cameraunlock-core submodule at
  `cameraunlock-core/vendor/minhook/` and statically linked into
  `openvr_api.dll`, which ships in our release ZIP. Every file there is
  byte-identical to the `v1.3.4` tag apart from `src/hook.c`, which allocates
  from the process heap; `cameraunlock-core/vendor/minhook/LOCAL-CHANGES.md`
  records that change against upstream. Note that upstream's own CMakeLists at
  that tag still declares `MINHOOK_PATCH_VERSION 3`, so the computed
  `MINHOOK_VERSION` string reads 1.3.3 on a v1.3.4 tree - that is upstream's
  and is left alone.

MinHook carries two copyright holders: Tsuda Kageyu for MinHook itself, and
Vyacheslav Patkov for the Hacker Disassembler Engine that `src/hde/` is built
from. Both notices appear below exactly as upstream ships them.

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## cameraunlock-core

- **Version:** `bd22895bb30ab7946d780b0af5782755e33e2cba`
- **License:** MIT
- **Upstream:** https://github.com/itsloopyo/cameraunlock-core
- **Usage:** Supplies the tracker receiver, the pose processing pipeline and
  the shared camera maths.
- **Bundled:** yes. Git submodule at `cameraunlock-core/`, statically linked
  into `openvr_api.dll`.

Our own code, MIT licensed, reproduced here so the notices are complete.

```
MIT License

Copyright (c) 2026 itsloopyo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## OpenTrack

- **Version:** n/a (wire format only, no released version consumed)
- **License:** ISC
- **Upstream:** https://github.com/opentrack/opentrack
- **Usage:** We implement its UDP pose datagram layout so OpenTrack and
  compatible trackers can drive the mod.
- **Bundled:** no. Neither shipped nor linked against.

This mod implements the OpenTrack UDP pose datagram
layout so that OpenTrack (https://github.com/opentrack/opentrack, ISC licence)
and compatible trackers can drive it. No OpenTrack code, headers or binaries
are copied, linked or redistributed, so its licence triggers no notice
obligation here. It is credited because the wire format is its work.

---

## OpenVR

- **Version:** n/a (export names and call signatures only, no released version
  consumed)
- **License:** BSD-3-Clause
- **Upstream:** https://github.com/ValveSoftware/openvr
- **Usage:** The Witness loads `openvr_api.dll` from its own folder at startup,
  so that is the filename this mod's DLL takes. It declares the exports the
  game resolves by name and forwards each one to the game's original DLL, which
  the installer keeps beside it as `openvr_api.dll.backup`. The export names and
  the argument shapes come from the public `headers/openvr.h` in the SDK above.
- **Bundled:** no. No OpenVR source, header or binary is copied, linked or
  redistributed. `src/openvr_proxy.cpp` declares its own prototypes, so the
  licence triggers no notice obligation here. It is credited because the
  interface is Valve's work.

The `openvr_api.dll` we ship is a forwarder of our own and carries none of
Valve's runtime. It takes that filename because the game resolves the imports
by name; the game's own copy is renamed rather than deleted, and the uninstaller
puts it back. OpenVR and Valve are trademarks of Valve Corporation, used here
only to identify the interface this shim stands in front of.

---

## The Witness

Game by Jonathan Blow / Thekla, Inc. This mod is an unofficial, unaffiliated
patch that hooks the game's own executable at runtime. It bundles no game code,
no extracted assets and no proprietary binaries, in this repository or in any
release ZIP, and it requires a legitimately purchased copy of the game.

The Witness and all related names, logos, characters and marks are trademarks
of their respective owners. They are used here only to identify the game this
mod applies to, which is nominative use and not a claim of any right in them.

The engine structure offsets, function addresses and collision mask constants
in the source are measurements of a legitimately owned retail copy, recorded as
numbers. The field names those numbers are commented with, `fov_vertical`,
`z_near`, `user_fov_vertical` and `player_camera_up`, are the game's own tuning
variable names: the engine registers its tuning variables by name at load time,
and that table is what the numbers were matched against. No decompiled or
disassembled game code is stored in this repository.

