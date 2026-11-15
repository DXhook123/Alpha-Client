# Alpha Client

**Alpha** is an internal client for **Minecraft: Bedrock Edition 1.26.5x** (signatures and offsets target **1.26.51.1**, Windows x64), built mainly for **anarchy** servers.

This repository is the open-source base of Alpha. It ships with a nearly complete SDK, the hook framework, the module/setting/config system, the Hachimi-style ClickGUI and a HUD editor. Use it as a starting point for your own modules: KillAura, AutoCrystal, Fly, Velocity and so on can all be written on top of what is already here.

> Alpha was designed and developed by **CRyy** and **NYY**.

---

## Developers

| Developer | Role |
|-----------|------|
| **CRyy** ([@DXhook123](https://github.com/DXhook123)) | Lead developer, project owner. Core architecture, SDK & reverse engineering, hooks, ClickGUI, HUD |
| **NYY** | Lead developer, project owner. Core architecture, SDK & reverse engineering, hooks, ClickGUI, HUD |

Alpha is an original project by CRyy and NYY. If you use this code in your own client, please keep the credits and link back to this repository.

---

## Features

- **Injection & hooks**
  - DirectX 11 / 12 present hook with an ImGui overlay (kiero + MinHook)
  - Game hooks: ClientInstance update, key/mouse input, actor tick, rotation, gamma, packet send / receive, level render, UI screen view, ping
- **SDK** for 1.26.51.1
  - Actor / Mob / Player / LocalPlayer, GameMode, Level, BlockSource, Block, Item / ItemStack, inventory and container
  - EnTT component access (state vector, rotation, AABB, runtime id, flags, ...)
  - Packets (Text, PlayerAuthInput, MovePlayer, SetActorMotion, ...)
  - Offsets and vtable slots are written directly in each header, e.g. `CLASS_MEMBER(std::string, nameTag, 0x2E0)`
- **Client framework**
  - Module / setting system (bool, slider, enum, color, keybind, text)
  - JSON configs with auto-save, per-module keybinds (toggle or hold)
  - Hachimi-style ClickGUI (Astolfo / Mio / Custom themes)
  - HUD editor: ArrayList, Watermark, Welcome, Position, FPS, Ping, Speed, TPS, Potions
  - English / Chinese UI
- **Modules included**: ClickGui, Color, CustomFont, Settings, DebugInfo, TabList, HudEditor, Fullbright, FeetTrap

## Commands

The default prefix is `.`

| Command | Description |
|---------|-------------|
| `.bind <module> <key>` | Bind a key to a module |
| `.itemid` | Show the id and name of the held item |
| `.save [name]` | Save the current config, or save it under a new name |
| `.eject` | Unload the client |

Default ClickGUI key: **Insert**.

## Building

Requirements:

- Windows 10/11 x64
- Visual Studio 2022 with the **Desktop development with C++** workload (MSVC v143, C++20)

Steps:

1. Open `Alpha.slnx` in Visual Studio 2022.
2. Select **Release | x64**.
3. Build. The output is `x64\Release\Alpha.dll`.

Or from a Developer Command Prompt:

```bat
msbuild Alpha\Alpha.vcxproj -p:Configuration=Release -p:Platform=x64
```

## Usage

1. Start **Minecraft Bedrock 1.26.51.1**. Other versions will crash, because all signatures and offsets are version specific.
2. Inject `Alpha.dll` with any DLL injector.
3. Press **Insert** to open the ClickGUI.

Configs are stored in `%LOCALAPPDATA%\Packages\Microsoft.MinecraftUWP_8wekyb3d8bbwe\RoamingState\Alpha\`.

## Project layout

```
Alpha.slnx
Alpha/
├─ dllmain.cpp            entry point
├─ Client/
│  ├─ Client.cpp          init / shutdown
│  └─ Managers/
│     ├─ HookManager/     DirectX and game hooks
│     ├─ ModuleManager/   modules and settings
│     └─ CommandManager/  commands and configs
├─ sdk/                   Minecraft classes, components and signatures
├─ Renderer/              ImGui drawing helpers
├─ Utils/                 math, memory, world/inventory helpers
└─ Libs/                  third-party libraries
```

## Updating to a new game version

Everything version-specific is in `Alpha/sdk`:

- `sdk/Signatures.h`: byte patterns for hooked or called functions
- `sdk/GameVersion.h`: vtable slots and offsets used by `ClientInstance`, `GuiData`, etc.
- `CLASS_MEMBER(...)` offsets and `callVirtualFunc<...>(index, ...)` slots in each class header

## Credits

Third-party libraries:

- [Dear ImGui](https://github.com/ocornut/imgui)
- [MinHook](https://github.com/TsudaKageyu/minhook)
- [kiero](https://github.com/Rebzzel/kiero)
- [libhat](https://github.com/BasedInc/libhat)
- [GLM](https://github.com/g-truc/glm)
- [EnTT](https://github.com/skypjack/entt)
- [nlohmann/json](https://github.com/nlohmann/json)
- Fonts: Source Han Sans, LXGW WenKai Lite, MiSans, Noto Sans, Mojangles (see `Alpha/Libs/fonts` for licenses)

## Disclaimer

This project is for educational purposes and for use on anarchy servers where modified clients are allowed. Using it on servers that forbid cheats may get you banned. Alpha is not affiliated with Mojang or Microsoft.

## Copyright

Copyright © 2026 **CRyy** & **NYY**. All rights reserved.

Alpha Client (https://github.com/DXhook123/Alpha-Client) was created by CRyy and NYY. Redistribution or reuse of this code must keep the original credits and a link to this repository.
