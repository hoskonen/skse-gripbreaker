# GripBreaker

Native SKSE plugin for Skyrim Special Edition / Anniversary Edition.

Work-in-progress player weapon-disarm mechanics:

- **Fatigue disarms:** low-stamina melee attacks can cause a weapon drop. Weapon
  skill lowers the effective stamina threshold.
- **Clash disarms:** Precision weapon collisions are grouped into logical clashes.
  Weapon weight, one-/two-handed grip, stamina, and weapon skill determine the
  player's drop chance. The probability curve approaches a 95% maximum.
- **Drop physics:** optional forward/upward impulse after a successful drop;
  currently uses a fixed prototype strength.
- **Diagnostics:** combat, clash, grip, severity, probability, and roll logs.

Fatigue and clash disarms have independent toggles. Clash disarms default to off;
NPC disarming is not implemented. Weapons are dropped through the validated worn
inventory instance, with bound weapons and quest items rejected.

Settings persist in `Data/SKSE/Plugins/GripBreaker.ini` and can be edited through
SKSE Menu Framework. Detailed mechanic notes are in [docs/GripMechanics.md](docs/GripMechanics.md).

## Runtime requirements

- Skyrim SE/AE with matching SKSE64 and Address Library.
- Precision for weapon-clash detection; GripBreaker loads without it.
- SKSE Menu Framework for the settings UI; INI configuration works without it.

Current development/testing targets Skyrim AE 1.6.1170. Balance and throw physics
are still being tuned.

## Development

Requirements:

- Visual Studio C++ toolchain
- XMake
- CommonLibSSE-NG submodule (initialize with `git submodule update --init --recursive`)

Build:

```powershell
xmake build GripBreaker
```

Dependency-free tests are separate XMake targets, for example:

```powershell
xmake build ClashDisarmDecisionTests
xmake run ClashDisarmDecisionTests
```
