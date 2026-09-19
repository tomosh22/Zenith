---
id: "c39ebeb4-d674-479e-bffc-de52f2b31008"
key: "HE-8"
type: "STORY"
title: "HE_Bindings (touch and desktop profiles, MOUSE in none) and HE_Clock (speeds 0/1/3/10, calendar, sun angle)"
category: "Hearth"
parent: "HE-1"
sprint: "he-u2exlj3764t6lnl5ehdwdwea"
status: "To Do"
assignee: null
priority: "HIGH"
complexity: "MODERATE"
risk: "LOW"
storyPoints: 5
position: 8
labels: []
fixVersions: []
blocks: [{"key":"HE-11","reason":"needs decay on the sim clock"}]
updatedAt: "2026-09-19T19:24:18.627Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
TDD §9 and §4. `Source/HE_Bindings.h` on the `ZM_Bindings.h` model: profiles `HE_P_TOUCH {TOUCH}` and `HE_P_DESKTOP {KEYBOARD|MOUSE}` with MOUSE in no profile (UI taps ride the pointer table unconditionally), actions from id 16: SELECT, CANCEL (+`SystemBack`), SPEED_0/1/2/3, MODE_BUILD, MODE_MAP, FLOOR_UP/DOWN, ROTATE_OBJECT, DELETE, UNDO, REDO, and the desktop camera rows; the one file allowed raw key codes, with a grep unit. `Source/Core/HE_Clock`: sim-minutes advance = real-dt × speed × 60, speeds {0, 1, 3, 10}, day index and weekday, `SunAngleDegrees` published once per frame to the scene `Zenith_SunComponent` (the `CB_DayNight` pattern), ultra auto-drop to 1x on arrival/completion/crisis as a request the scheduler answers (GDD §10). Units: bindings-table sanity (unique ids, every action in exactly the intended profiles), clock arithmetic at every speed, day rollover, sun angle at 06:00/12:00/18:00, the no-wall-clock grep over `Source/`.

## Definition of Done
- [ ] `HE_Bindings.h` is the only Hearth file spelling a raw key code, asserted by a unit
- [ ] `HE_Clock` advances sim time at the four speeds with calendar rollover and publishes the sun angle, all unit-pinned
- [ ] A unit greps `Source/` for wall-clock calls and finds none
- [ ] Hearth pinned baseline bumped from an OBSERVED Null run (the row is added by HE-5; until then this line is inert)
