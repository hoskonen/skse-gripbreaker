# GripBreaker

Native SKSE plugin for Skyrim Special Edition / Anniversary Edition.

Current development goal:

- detect melee attacks reliably
- identify attacking hand and weapon
- read stamina and attack state
- drop equipped weapons under configured conditions
- expose settings through SKSE Menu Framework

Future ideas:

- weapon clash disarms using Precision
- weapon weight / skill based grip calculations
- NPC support

## Development

Requirements:

- Visual Studio C++ toolchain
- XMake
- SKSE64
- Address Library
- CommonLibSSE-NG

Build:

```powershell
xmake build