# GripBreaker Mechanics Design

## Goals

GripBreaker models loss of weapon control under fatigue and external pressure.

Core design goals:

- Stamina management should matter.
- Blindly spamming attacks at very low stamina should be risky.
- Weapon skill should improve control and reduce stamina requirements.
- Blocking at low stamina should become dangerous.
- Weapon matchups should matter later.
- Heavy weapons should exert more pressure in clashes than light weapons.
- The system should be extensible to physical strength, magic, resistances,
  perks, effects, and other sources of pressure.
- Disarms should emerge from circumstances rather than arbitrary random chance.

## Core Concepts

### Grip Stability

Grip Stability represents the actor's ability to maintain control of their weapon.

Initial inputs:

- Current stamina percentage
- Relevant skill:
  - One-Handed
  - Two-Handed
  - Block

Future inputs may include:

- Strength
- Perks
- magic effects
- potions
- enchantments
- race
- temporary status effects

### External Pressure

External Pressure represents forces acting against Grip Stability.

Future inputs may include:

- Opponent weapon weight
- Weapon type
- Attack classification
- Collision intensity
- Opponent skill
- Physical strength
- Incoming magic
- Environmental forces

## Relevant Skill

Attack with one-handed weapon:
- One-Handed

Attack with two-handed weapon:
- Two-Handed

Blocking:
- Block

Future versions may combine skills, but the initial model uses one primary skill only.

## Phase 1: Fatigue Grip Model

Each action has a safe stamina threshold.

Relevant skill lowers that threshold.

Conceptually:

effectiveThreshold =
    baseThreshold × skillModifier

If:

currentStamina >= effectiveThreshold

then:

fatigueRisk = 0

Otherwise:

fatigueRisk =
    (effectiveThreshold - currentStamina)
    / effectiveThreshold

Clamp fatigueRisk to 0..1.

`fatigueRisk` represents physical grip instability. It is not itself a drop
probability.

For the initial player-only fatigue disarm prototype:

```text
riskCurve = fatigueRisk × fatigueRisk
dropChancePercent = BaseDropChance × riskCurve
```

`BaseDropChance` comes from GripBreaker's persisted settings. Relevant weapon
skill is applied only when calculating the effective stamina threshold; it is
not applied again to the drop probability.

This probability is evaluated once per qualifying player weapon attack. A
successful roll drops one proven, hand-specific worn instance through the
player's authoritative inventory changes. It is not used for Precision logical
clashes. Clash disarming remains a separate future external-pressure source.

No weapon weight, clash force, strength, or magic contributes to this prototype.

The timing of fatigue-only disarm evaluation is still being measured against
Skyrim's attack stamina expenditure. It currently evaluates at `HitFrame`, but
may eventually move to a post-swing or recovery boundary if runtime diagnostics
show that this better represents the intended fatigue state.

## Phase 2: Weapon Clash Pressure

Precision logical clashes add external pressure.

Initial likely factors:

- Player weapon weight
- Opponent weapon weight
- Collision magnitude
- Attack type

Weapon weight should modify how much stamina/control is required to withstand
the clash rather than simply being a flat disarm bonus.

## Phase 3: Weapon Characteristics

Weapon class can add characteristics beyond raw weight.

Examples:

- dagger: low impact, high control
- sword: moderate impact, good control
- mace: higher impact
- warhammer: very high impact, slower handling

Exact values are TBD.

## Phase 4: Strength

Physical strength can improve Grip Stability or reduce effective incoming
pressure.

Potential sources:

- potions
- enchantments
- perks
- magic effects
- racial bonuses

Implementation must avoid hard-coding specific mods where possible.

## Phase 5: Magic Pressure

Magic may eventually create GripBreaker pressure independently of
weapon clashes.

Examples:

- fire impact
- frost impact
- shock
- dragon breath
- magical explosions

Resistance should mitigate the corresponding pressure.

Example concept:

effectiveMagicPressure =
    rawMagicPressure × (1 - relevantResistance)

Exact mechanics are TBD.

## Architecture

Keep Grip Stability separate from pressure sources.

Conceptually:

GripEvaluation
    <- Player/Actor context

PressureSource
    <- Fatigue
    <- Weapon clash
    <- Magic
    <- Other future sources

DisarmEvaluation
    <- GripEvaluation + PressureSource

This separation should allow new pressure sources without rewriting the core
grip model.

## Current Development Rule

Build incrementally.

The player-only fatigue disarm path and the player clash disarm path have
independent toggles. Completed Precision logical clashes now use the canonical
player-side severity probability to gate the established safe player drop path.
NPC probabilities remain diagnostic only.

## Disarm Probability

### Player clash probability

Completed player-involved clashes calculate a probability for each participant
after the severity evaluation. The canonical Hill curve is:

```text
chancePercent = 95 / (1 + (1.85 / severity)^2.8)
chancePercent(0) = 0
```

The internal constants are centralized in `ClashProbabilityEvaluation::Curve`.
Severity 1.85 gives half the maximum (47.5%). The curve is monotonic and approaches
95%, so no severity implies a guaranteed drop. Floating-point rounding may reach
the 95% cap for extreme inputs, but never 100%.

Example estimates: severity 0.5 -> 2.4%, 0.75 -> 7.0%, 1 -> 14.4%,
1.25 -> 23.8%, 1.5 -> 33.9%, 2 -> 52.7%, 3 -> 75.5%, 5 -> 89.5%,
8 -> 93.5%. Missing, negative, or non-finite severity is unavailable.

The probability calculation uses copied severity data only. Once a logical clash
finishes its existing 75 ms quiet window, enabled player clash disarms queue one
game-thread decision. Valid player severity and probability, actor context, and
captured weapon/hand identity are required before drawing a fresh roll in [0, 100).
Only `roll < playerChancePercent` permits the existing guarded `PlayerDisarm`
operation. Equality means KEEP. Invalid input means SKIPPED with no random roll.
KEEP never calls the inventory/drop path or schedules drop physics. DROP still
requires the existing live weapon and exact worn inventory instance validation.

The temporary forced player clash-drop branch has been replaced. There is one
player roll per eligible completed logical clash and no NPC roll. The fatigue
path draws its own independent rolls using the same percentage RNG method.
No frequency adjustment or probability tuning setting is introduced; the
canonical probability remains separate from the deterministic roll decision.

Grip/fatigue risk is not itself the final probability of disarming.

Risk describes how unstable the actor's grip is.

A separate probability layer converts that risk into an actual disarm chance.

Possible structure:

disarmChance =
    baseChance
    × riskCurve(gripRisk)
    × contextualModifiers

Potential controls:

- Minimum risk threshold before rolling
- Curved/non-linear risk response
- Base chance by context
- Cooldown between attempts
- One roll per logical action/clash
- Strength/perk/effect modifiers
- Weapon handling modifiers

This separation allows severe grip instability without making disarms excessively frequent.

Dropped weapon physics:
- first resolve returned world reference safely
- wait until its 3D / rigid body exists if necessary
- apply controlled impulse only after verified physics state
- direction should come from attack/clash context
- strength should be independently tunable
