# UI design: authoring documents and cooked formats

This file is part of the UI design family; the entry document is `Zenith2/Docs/Design/UI.md`. It specifies the five UI authoring document types (UI documents, themes, font families, the localization project and string tables) with their JSON schemas, validation rules, canonical forms and migration chains; the message syntax of localized strings; the style property catalogue; and the three cooked formats UI owns (`.zui`, `.zfont` with its MSDF atlas pages, `.zloc`), the binding manifest and the parser limits. The runtime meaning of documents is in `Zenith2/Docs/Design/UI/Layout.md`, `Zenith2/Docs/Design/UI/Text.md`, `Zenith2/Docs/Design/UI/Interaction.md` and `Zenith2/Docs/Design/UI/Localization.md`.

## Scope of this file

| Topic | Section |
|---|---|
| which documents exist, where they live, what they cook into | "Authoring document family" |
| rules every UI authoring document obeys | "Common authoring rules" |
| the UI document | "UI document schema" |
| themes: classes, palettes, tokens, sprites, animations | "Theme document schema" |
| every style property, its grammar, default and inheritance | "Style property catalogue" |
| font families | "Font family document schema" |
| the localization project and string tables | "Localization project document schema", "String table document schema" |
| localized message text | "Message syntax" |
| canonical member order and the migration registry | "Canonical schemas and document types", "Migration chains and fixtures" |
| validation diagnostics | "Validation rules" |
| cooked bytes | "Cooked UI document format", "Cooked font format", "Cooked localization table format", "Binding manifest file" |
| parser limits | "Parser limits" |

## Authoring document family

| Type | File pattern | Location | Stable name | Schema tag | Cooks to | Asset kind |
|---|---|---|---|---|---|---|
| UI document | `*.ui.json` | anywhere under a content root (`Games/<Name>/Assets/UI/…` by convention) | `Zenith.UI.UIDocument` | `zenith2.ui` | `.zui` | `ASSET_KIND_UI_DOCUMENT` (13) |
| theme | `*.uitheme.json` | content roots | `Zenith.UI.ThemeDocument` | `zenith2.uitheme` | folded into each `.zui` that names it (classes, palettes, metric tokens, sprites, animations); no cooked file of its own | — (a source without a primary product) |
| font family | `*.fontfamily.json` | content roots; the face files beside it, or a third-party data path (UI9) | `Zenith.UI.FontFamilyDocument` | `zenith2.fontfamily` | `.zfont` | `ASSET_KIND_FONT` (12) |
| localization project | `localization.json` | `Games/<Name>/Localization/localization.json`, one per game; the engine's at `Zenith2/Engine/ZenithUI/Internal/Content/Localization/localization.json` | `Zenith.UI.LocalizationProjectDocument` | `zenith2.localization` | read by the `ui.localization` step; produces no asset of its own | — |
| string table | `*.strings.json` | `<project directory>/<locale>/<namespace>.strings.json` | `Zenith.UI.StringTableDocument` | `zenith2.strings` | merged per locale into one `.zloc` per locale | `ASSET_KIND_LOCALIZATION_TABLE` (20) |

The asset kinds, extensions and magics are ASSET's table (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`, rows 12, 13 and 20); UI is the owner of the three kinds and specifies their formats and loaders. A theme is never loaded at run time: the `ui.document` step opens the document's theme and its `extends` chain through ASSET's `OpenSourceDocument` (`Zenith2/Docs/Design/ASSET/Cook.md → "Source documents in cook steps"`) and compiles their classes, palettes, metric tokens, sprites and animations into the document's own `.zui`, so a style block's token and class indices name records of the same file and the theme's bytes key every document item that uses it (`Zenith2/Docs/Design/UI.md → "Cook steps and derived-data keys"`). Two documents naming one theme each carry their own copy, and editing the theme re-cooks both. Switching the colour-vision or high-contrast palette at run time selects among the palettes already in the loaded document ("Palettes and tokens"); no requirement asks for replacing a document's theme at run time. A theme source's asset record has no primary product (`ASSET_KIND_NONE`, `Zenith2/Docs/Design/ASSET/Database.md → "Records"`), so its GUID names no cooked asset.

UI documents, themes and string tables are UI documents in the sense of `Zenith2/Docs/Contracts.md → "Compatibility policy per kind"` ([Z2-N-CONTRACT-552] lists "UI documents"); the font-family and localization-project documents are on that list (`Zenith.UI.FontFamilyDocument`, `Zenith.UI.LocalizationProjectDocument`) since cross-doc item X236 (8), cross-area row UI10, because [Z2-N-CONTRACT-552] says a new authoring-document type joins the list in the diff that introduces it and the core list is edited only by the core-doc batch.

## Common authoring rules

| Rule | Detail |
|---|---|
| encoding | UTF-8 JSON read with CORE's reader (`Zenith2/Docs/Design/CORE/Text.md → "JSON reader"`) under the JSON parser limits of `Zenith2/Docs/Architecture.md → "Security architecture"` plus the limits of "Parser limits" below; duplicate keys refused |
| identity | every document has `"id"`: its `Zenith_PersistentID` in the 36-character canonical form; every element of a UI document has `"uid"`, a persistent ID unique within the document, assigned once by the editor or the authoring tool and never reused; overrides and editor selection key on it ([Z2-N-CONTRACT-534]) |
| versions | `"schemaVersion"`: `uint32`, 1 for every type at its introduction; a newer value than the build's opens read-only with a diagnostic ([Z2-N-CONTRACT-537]) |
| `$schema` | the schema tag of the table above; the document type is chosen by the suffix, and a mismatching tag is validation rule 2 |
| paths | asset references are paths relative to the referencing document's directory (`"../Fonts/Body.fontfamily.json"`) or logical paths (`"content:/Engine/UI/Default.uitheme.json"`), resolved by the cook to GUIDs through the asset database; a path outside every content root is rule 31 |
| unknown members | kept on round-trip with `MIGRATION_WARNING_UNKNOWN_FIELD` ([Z2-N-CONTRACT-543], [Z2-N-CONTRACT-544]); members starting with `x-` are tool annotations kept verbatim without a warning |
| numbers | lengths are JSON numbers in canvas units (`u`) or strings with a unit suffix (`"50%"`); colours are `"#RRGGBB"`, `"#RRGGBBAA"` (sRGB, straight alpha) or a token `"$name"`; booleans are JSON booleans |
| bindings | a member that accepts a binding takes the string `"@<slot>"`, naming a slot of the document's `bindings` object |

## UI document schema

### Document root

```json
{
	"$schema": "zenith2.ui",
	"schemaVersion": 1,
	"id": "1f0c3d84-6b32-4a7e-9a51-0b5c2d1e7f90",
	"theme": "../Themes/Main.uitheme.json",
	"canvas": { "scaleMode": "reference", "referenceWidth": 1920, "referenceHeight": 1080, "match": 0.5 },
	"strings": "menu",
	"cancelEvent": "Back",
	"bindings": { "score": { "component": "Game2D.HudState", "property": "Score" } },
	"templates": { "slotItem": { "type": "button", "uid": "…", "children": [] } },
	"root": { "type": "panel", "uid": "6a2c…", "children": [] }
}
```

| Member | Type | Required | Meaning |
|---|---|---|---|
| `$schema` | string | yes | `zenith2.ui` |
| `schemaVersion` | integer | yes | 1 |
| `id` | persistent ID | yes | the document's identity |
| `theme` | path | no | the theme document; absent: the engine default theme `content:/Engine/UI/Default.uitheme.json` |
| `canvas` | object | no | `scaleMode`: `"dp"` (default; 1 u = 1 dp × UI scale) or `"reference"`; for `reference`: `referenceWidth`, `referenceHeight` (canvas units, 64 … 16,384) and `match` (0 = match width, 1 = match height, default 0.5); `safeArea` (bool, default true: the root is inset by the platform's safe area), following "Canvas units and scaling" and "Safe area and surface changes" of Layout.md |
| `strings` | string | no | the default localization namespace: a text key without a dot is prefixed with it |
| `focusScope` | string | no | `"auto"` (default): the screen takes keyboard and gamepad focus when it qualifies (Interaction.md "Focus scopes"); `"never"`: a pointer-only screen (HUD, on-screen controls) |
| `cancelAction` | string | no | `"none"` (default) or `"resumeWorld"`: Cancel on this screen publishes the resume record and then clears the world's menu pause, before emitting `cancelEvent` (Interaction.md "Pause menus") |
| `cancelEvent` | string | no | the event name `UI.Cancelled` carries when UI.Cancel reaches this screen; absent: UI.Cancel is not consumed by the screen |
| `bindings` | object | no | binding slots, keyed by slot name (`^[a-z][A-Za-z0-9_]{0,63}$`), § "Bindings" |
| `templates` | object | no | named element subtrees for lists and `instance` reuse within the document, keyed by `^[a-z][A-Za-z0-9_]{0,63}$` |
| `root` | element | yes | the root element; its type is `panel`, `scroll` or `instance` |

### Element common members

| Member | Type | Default | Meaning |
|---|---|---|---|
| `type` | string | — (required) | one of § "Element types" |
| `uid` | persistent ID | — (required) | the authoring identity |
| `id` | string `^[A-Za-z][A-Za-z0-9_]{0,63}$` | absent | the script-facing name: event `Element` fields, `UI.Focus`, `UI.ScrollTo`, `nav` targets; unique within the document (rule 12) |
| `class` | array of strings | `[]` | theme class names, applied after the theme's `typeDefaults` for the element's type and before `style` |
| `style` | object | `{}` | inline style properties (§ "Style property catalogue") |
| `states` | object | `{}` | inline state variants: keys `hover`, `pressed`, `focused`, `focusVisible`, `disabled`, `checked`, `selected`, `editing`, `invalid`, each a style object |
| `visible` | bool or binding | `true` | not visible: takes no space, draws nothing, receives no input, absent from the semantic tree |
| `enabled` | bool or binding | `true` | disabled: draws its `disabled` state, receives no activation or focus |
| `focusable` | bool | `true` for interactive types, `false` otherwise | |
| `nav` | object | `{}` | § "Navigation members" |
| `events` | object | `{}` | event names: `activate`, `change`, `toggle`, `select`, `commit` (strings `^[A-Za-z][A-Za-z0-9_.]{0,63}$`) carried in the events of `Zenith2/Docs/Design/UI/Interaction.md → "Event kinds and payloads"` |
| `accessibility` | object | `{}` | § "Accessibility members" |
| `children` | array of elements | `[]` | child elements in layout and paint order; only container types take children (rule 14) |

### Element types

| `type` | Container | Interactive | Type-specific members |
|---|---|---|---|
| `panel` | yes | no | none (layout comes from style properties `layout`, `direction`, …) |
| `text` | no | no | `text` (a text value, § "Text content values"); `selectable` (bool, default false: drag selects and copy is allowed) |
| `image` | no | no | `image`: `{ "texture": path }` or `{ "sprite": "<theme sprite name>" }`, or a binding `"@slot"` of kind ASSET; `slice` overrides the sprite's |
| `button` | yes | yes | children are its content; activation emits `UI.Activated`; `presentationAction` (`none` default, `resumeWorld`: activation first publishes the resume record and clears the world's menu pause, Interaction.md "Pause menus") |
| `toggle` | yes | yes | `checked` (bool or binding, default false); activation flips it and emits `UI.Toggled`; `group` (string): toggles of one group within a screen form a radio set (exactly one checked) |
| `slider` | no | yes | `value` (number or binding), `min` (0), `max` (1), `step` (0 = continuous), `orientation` (`horizontal`); commit emits `UI.ValueChanged` |
| `textField` | no | yes | `text` (initial value or binding of kind STRING_ID/TEXT), `placeholder` (text value), `maxBytes` (1 … 4,096, default 256), `multiline` (false), `purpose` (`text`, `name`, `email`, `username`, `password`, `number`, `numberPassword`, PLAT's `Zenith_TextInputPurpose`), `commitOnEnter` (true) |
| `scroll` | yes | yes (scrollable, not activatable) | `axes` (`vertical`, `horizontal`, `both`), `inertia` (true), `scrollbar` (`auto`, `always`, `never`) |
| `list` | no (items are generated) | yes | `items` (binding of kind LIST), `itemTemplate` (template name), `itemExtent` (canvas units along the list axis; absent: variable extent), `axis` (`vertical`), `selectable` (false), `overscan` (items, default 2) |
| `progress` | no | no | `value` (number or binding), `min`, `max`, `orientation` |
| `choice` | no | yes | `options` (array of text values, or a LIST binding of STRING_ID keys), `selected` (integer or binding); left and right navigation or activation cycles; emits `UI.SelectionChanged` |
| `inputPrompt` | no | no | `action` (input action name, `Context.Action` grammar of PLAT), `context` (input context name, default the action's first context), `part` (composite part index, absent = all) |
| `instance` | no | no (its content may be) | `document` (path of another UI document) or `template` (a name in `templates`); `overrides`: object keyed by the target element's uid, each value an object of element members to replace (`text`, `style`, `visible`, …) |
| `rebind` | no | yes | `action`, `context`, `scheme` (`keyboardMouse`, `gamepad`, `touch`, or `active`), `bindingIndex` (default 0), `reset` (`none` default, `action`, `all`: activation resets instead of rebinding); shows the binding prompt and rebinds on activation (Interaction.md "Input prompts and remapping") |
| `captionArea` | no | no | `kinds` (array of `subtitle`, `sound`; default both), `maxLines` (3); where the world's captions render; absent from a document: the engine caption screen draws them |
| `onScreenControl` | no | yes | `control` (`button` or `stick`), `virtualControl` (the control name on UI's virtual device), `radius` (stick, canvas units), `deadzone` (0.15), `mode` (`fixed` or `floating`) (Interaction.md "On-screen controls") |

### Text content values

A text value is one of:

| Form | Example | Meaning |
|---|---|---|
| key | `{ "key": "start" }`, `{ "key": "menu.start", "args": { "count": "@coins" } }` | a localization key (the document's `strings` namespace is prefixed when the key has no dot); `args` maps message argument names to bindings or JSON literals (numbers, strings, booleans) |
| binding | `"@playerName"` | the slot's value: a STRING_ID slot is literal text (user content, never looked up), unless the slot declares `"textMode": "key"`; numeric slots format with the active locale's number format |
| literal | `{ "literal": "Debug" }` | authored text shown verbatim; a literal in a non-development document is rule 52 (warning; error when the project sets `"literalsAreErrors": true`) |

### Bindings

```json
"bindings": {
	"score":     { "component": "Game2D.HudState", "property": "Score" },
	"health":    { "component": "OpenWorldSlice.Stats", "property": "Stamina", "entity": "singleton" },
	"volume":    { "config": "audio.masterVolume" },
	"cards":     { "component": "Game2D.Hand", "property": "Cards", "item": { "name": "Name", "icon": "Icon" } },
	"music":     { "component": "Game2D.Settings", "property": "MusicOn", "writeBack": true },
	"title":     { "component": "Game2D.HudState", "property": "Title", "textMode": "key" }
}
```

| Member | Type | Meaning |
|---|---|---|
| `component` | stable name | the component type (C++-registered or data-defined) read by the screen's world |
| `property` | name | the property's script name; nested struct fields with `.` (`"Stats.Stamina"`) |
| `entity` | `"self"` (default), `"parent"`, `"singleton"`, or a persistent ID | `self`: the screen entity; `parent`: its scene parent; `singleton`: the entity with the lowest stable simulation ID that has the component; a persistent ID: that entity |
| `config` | configuration key | a presentation configuration value instead of a component (the settings screens); only keys absent from the simulation-affecting list (rule 46); read every frame, written by controls through `Zenith_Config::SetUserValue` |
| `item` | object | LIST bindings over a fixed array or container of structs: maps item-template slot names to struct field names; the item template binds `"@item.<name>"` |
| `writeBack` | bool | `self` component bindings alone (rule 44): a committed control value is written back as a property command (Interaction.md "Two-way bindings") |
| `textMode` | `"literal"` (default) or `"key"` | how a STRING_ID value is shown |
| `default` | JSON value | shown while the source is absent (entity missing, asset unloaded) |

### Navigation members

| Member | Type | Meaning |
|---|---|---|
| `nav.up`, `nav.down`, `nav.left`, `nav.right` | element `id` | an explicit directional target overriding the spatial search (`Zenith2/Docs/Design/UI/Interaction.md → "Directional navigation algorithm"`) |
| `nav.group` | string | the element and its descendants form a focus group: directional search stays inside unless no candidate exists |
| `nav.wrap` | bool | a focus group wraps around at its edges |
| `nav.order` | integer | tab order key within the screen (ascending, ties by document order) |
| `nav.initial` | bool | the screen's default initial focus when `Zenith.UI.Screen.InitialFocus` is empty |
| `nav.trap` | bool | modal dialogs: focus never leaves the element's subtree while it is visible |

### Accessibility members

| Member | Type | Meaning |
|---|---|---|
| `accessibility.role` | role name | overrides the type's default role (`Zenith2/Docs/Design/UI/Accessibility.md → "Roles and states"`) |
| `accessibility.label` | text value | the accessible name; absent: derived per Accessibility.md "Names and descriptions" |
| `accessibility.labelledBy` | element id | the element whose formatted text is the accessible name when `label` is absent |
| `accessibility.hint` | text value | the description announced after the name and state |
| `accessibility.live` | `"off"`, `"polite"`, `"assertive"` | a live region: text changes are announced |
| `accessibility.hidden` | bool | excluded from the semantic tree (decorative) |
| `accessibility.heading` | integer 1–6 | heading level for the `heading` role |

### Templates and instances

- A template is instantiated per use: `list` items and `instance` elements with `template` copy the subtree; every element of a copy keeps the template element's uid, and an instance's overrides name those uids.
- An `instance` with `document` instantiates the other document's `root`; that document's `bindings` become slots of the instance, bound by the instance's `bindings` member (an object mapping the inner slot name to an outer slot or literal); inner documents cannot reference their outer document (no cycles, rule 33).
- Instance depth is at most 8 (rule 34); the cook flattens instances into the cooked element table, so the runtime instantiates no document recursively.

## Theme document schema

```json
{
	"$schema": "zenith2.uitheme",
	"schemaVersion": 1,
	"id": "9b7e…",
	"extends": "content:/Engine/UI/Default.uitheme.json",
	"fonts": { "body": "../Fonts/Body.fontfamily.json", "title": "../Fonts/Title.fontfamily.json" },
	"metrics": { "space.s": 4, "space.m": 8, "radius": 6, "text.body": 18 },
	"palettes": {
		"default":       { "accent": "#3A7BFFFF", "text": "#F2F2F2FF", "bad": "#E0443EFF", "good": "#3CC060FF", "panel": "#1B1F27E6" },
		"protanopia":    { "bad": "#D98A00FF", "good": "#2F7DE1FF" },
		"deuteranopia":  { "bad": "#D98A00FF", "good": "#2F7DE1FF" },
		"tritanopia":    { "bad": "#E0443EFF", "good": "#1FA39AFF" },
		"achromatopsia": { "bad": "#FFFFFFFF", "good": "#7F7F7FFF" },
		"highContrast":  { "text": "#FFFFFFFF", "panel": "#000000FF", "accent": "#FFFF00FF" }
	},
	"typeDefaults": { "button": ["button"], "text": ["text.body"] },
	"classes": {
		"button": {
			"style": { "padding": ["$space.m", "$space.m"], "backgroundColour": "$panel", "cornerRadius": "$radius" },
			"states": { "hover": { "backgroundColour": "$accent" }, "focusVisible": { "focusRingWidth": 2 } }
		}
	},
	"sprites": { "coin": { "texture": "../Textures/Icons.png", "rect": [0, 0, 64, 64], "slice": [0, 0, 0, 0] } },
	"animations": {
		"pulse": { "duration": 0.8, "loop": true, "essential": false, "easing": "easeInOut",
			"keys": [ { "t": 0.0, "style": { "scale": [1, 1] } }, { "t": 0.5, "style": { "scale": [1.08, 1.08] } }, { "t": 1.0, "style": { "scale": [1, 1] } } ] }
	},
	"speakerColours": ["#FFFFFFFF", "#FFD27FFF", "#9FD3FFFF", "#C9A0FFFF"],
	"focusRing": { "colour": "$accent", "width": 2, "offset": 2 }
}
```

### Styles and selectors

- Selection is by class name alone: an element's classes are its type's `typeDefaults`, then its `class` array in order; there are no descendant, sibling or attribute selectors, so resolving an element's style is O(number of its classes) with no tree search.
- A class has `style` (the base) and `states` (the same keys as an element's `states`).
- `extends` names one parent theme; classes, tokens, sprites and animations of the child replace the parent's entries of the same name entirely (no member-wise merge); palettes merge per token. Chains are at most 4 deep (rule 72).

### Palettes and tokens

- A token is `$name` in any colour member; a metric token `$name` in any length member. Token names follow `^[a-z][A-Za-z0-9_.]{0,63}$`.
- Palette resolution order for a colour token: the active colour-vision palette (`protanopia`, `deuteranopia`, `tritanopia`, `achromatopsia`) when that setting is on, then `highContrast` when high contrast is on, then `default`; the first palette defining the token wins. `default` defines every token any palette defines (rule 74).
- Colours are converted to linear `Zenith_Colour` at cook time (`Zenith_ColourFromSrgb8`, `Zenith2/Docs/Design/CORE/Maths.md → "Zenith_Colour"`) and stored per palette in the PALETTES section, so a palette switch at run time is a table swap with no reparse.

### Sprites

A sprite names a rectangle of a texture asset (`rect`: x, y, width, height in texels) and an optional nine-slice inset (`slice`: left, top, right, bottom in texels). The texture is a GEOM texture asset (`Flux.Texture`, usage `colour`), imported with streaming disabled (UI's import preset `ui.sprite`, which sets the GEOM texture importer settings `texture.usage = "colour"` and `texture.streaming = false`; CONTENT owns presets, cross-area row UI11).

### Animations and transitions

| Member | Type | Meaning |
|---|---|---|
| `duration` | seconds, 0.016 … 60 | one cycle |
| `loop` | bool | repeats until the element leaves the state or the animation is replaced |
| `essential` | bool, default `false` | with reduced motion on, a non-essential animation is not played (the element shows its final key's style); an essential one plays as a cross-fade of opacity alone over the same duration (Accessibility.md "Reduced motion") |
| `easing` | `linear`, `easeIn`, `easeOut`, `easeInOut`, `step` | the curve between keys |
| `keys` | array of `{ t, style }`, t ascending in [0, 1] | style values to interpolate; only properties marked animatable in the catalogue |

Transitions are the style property `transition`: `{ "properties": ["backgroundColour", "scale"], "duration": 0.12, "easing": "easeOut" }`, applied when a state change alters those properties; reduced motion sets every transition's duration to 0.

## Style property catalogue

Lengths are canvas units unless marked; "%" is a percentage of the containing block's corresponding content size. "Inherit" means a child without its own value takes the parent's computed value. "Layout" means a change invalidates layout (otherwise only paint). "Anim" means animatable.

| ID | Authoring name | Value grammar | Default | Inherit | Layout | Anim |
|---|---|---|---|---|---|---|
| 1 | `width` | length, `"<n>%"`, `"auto"`, `"minContent"`, `"maxContent"` | `auto` | no | yes | yes |
| 2 | `height` | as width | `auto` | no | yes | yes |
| 3, 4 | `minWidth`, `minHeight` | length, % | 0 | no | yes | yes |
| 5, 6 | `maxWidth`, `maxHeight` | length, %, `"none"` | none | no | yes | yes |
| 7 | `margin` | 1, 2 or 4 lengths (all; vertical horizontal; top right bottom left — CSS order) | 0 | no | yes | yes |
| 8 | `padding` | as margin | 0 | no | yes | yes |
| 9 | `borderWidth` | as margin | 0 | no | yes | yes |
| 10 | `cornerRadius` | 1 or 4 lengths (top-left, top-right, bottom-right, bottom-left) | 0 | no | no | yes |
| 11 | `aspectRatio` | number > 0, or `"none"` | none | no | yes | no |
| 16 | `grow` | number ≥ 0 | 0 | no | yes | no |
| 17 | `shrink` | number ≥ 0 | 1 | no | yes | no |
| 18 | `basis` | length, %, `"auto"` | auto | no | yes | no |
| 19 | `alignSelf` | `auto`, `start`, `centre`, `end`, `stretch`, `baseline` | auto | no | yes | no |
| 20 | `position` | `flow`, `anchored` | flow | no | yes | no |
| 21, 22 | `anchorMin`, `anchorMax` | [x, y] in [0, 1] of the parent's content box | [0, 0], [0, 0] | no | yes | yes |
| 23 | `pivot` | [x, y] in [0, 1] | [0.5, 0.5] | no | yes | yes |
| 24, 25 | `offsetMin`, `offsetMax` | [x, y] lengths | [0, 0], [100, 100] | no | yes | yes |
| 26, 27 | `gridColumn`, `gridRow` | integer ≥ 1, or `"auto"` | auto | no | yes | no |
| 28, 29 | `gridColumnSpan`, `gridRowSpan` | integer 1 … 64 | 1 | no | yes | no |
| 30 | `zIndex` | integer −1,000 … 1,000 | 0 | no | no (paint and hit-test order among siblings, Rendering.md "Paint order") | no |
| 40 | `layout` | `stack`, `grid`, `overlay` | stack | no | yes | no |
| 41 | `direction` | `row`, `column`, `rowReverse`, `columnReverse` (`row` runs from the inline start: right to left in an RTL layout context, Layout.md "Logical direction and mirroring") | column | no | yes | no |
| 42 | `justify` | `start`, `centre`, `end`, `spaceBetween`, `spaceAround`, `spaceEvenly` | start | no | yes | no |
| 43 | `alignItems` | `start`, `centre`, `end`, `stretch`, `baseline` | stretch | no | yes | no |
| 44 | `wrap` | bool | false | no | yes | no |
| 45 | `gap` | length | 0 | no | yes | yes |
| 46 | `rowGap` | length, or `"gap"` | gap | no | yes | yes |
| 47, 48 | `gridColumns`, `gridRows` | array of track sizes: length, `"<n>fr"`, `"auto"`, `"minContent"`, `"maxContent"` | `["1fr"]`, `[]` (implicit `auto` rows) | no | yes | no |
| 49 | `layoutDirection` | `auto`, `ltr`, `rtl` | auto | yes | yes | no |
| 64 | `backgroundColour` | colour | transparent | no | no | yes |
| 65 | `backgroundGradient` | `{ "from": colour, "to": colour, "axis": "x"|"y" }` | none | no | no | yes |
| 66 | `backgroundImage` | `{ "sprite": name }` or `{ "texture": path, "rect": [x,y,w,h] }` | none | no | no | no |
| 67 | `backgroundSlice` | [l, t, r, b] texels, or `"sprite"` | sprite | no | no | no |
| 68 | `borderColour` | colour | transparent | no | no | yes |
| 69 | `shadowColour` | colour | transparent | no | no | yes |
| 70 | `shadowOffset` | [x, y] | [0, 0] | no | no | yes |
| 71 | `shadowBlur` | length 0 … 64 | 0 | no | no | yes |
| 72 | `opacity` | 0 … 1 | 1 | multiplies | no | yes |
| 73 | `translate` | [x, y] lengths | [0, 0] | no | no (visual) | yes |
| 74 | `scale` | [x, y] numbers | [1, 1] | no | no (visual) | yes |
| 75 | `rotate` | degrees | 0 | no | no (visual) | yes |
| 76 | `transformOrigin` | [x, y] in [0, 1] of the border box | [0.5, 0.5] | no | no | no |
| 77 | `clip` | bool | false (`scroll` and `list`: true) | no | no | no |
| 78 | `tint` | colour | white | no | no | yes |
| 79 | `imageFit` | `fill`, `contain`, `cover`, `none` | fill | no | yes | no |
| 80 | `pointerEvents` | `auto`, `none`, `passThrough` | auto | yes | no | no |
| 81 | `focusRingColour` | colour | the theme's `focusRing.colour` | yes | no | yes |
| 82 | `focusRingWidth` | length 0 … 16 | the theme's | yes | no | yes |
| 83 | `focusRingOffset` | length −16 … 16 | the theme's | yes | no | no |
| 84 | `mirrorInRtl` | bool | false | no | no | no |
| 96 | `fontFamily` | font token (`"body"`) or array of tokens (a fallback prefix) | `body` | yes | yes | no |
| 97 | `fontSize` | length 4 … 512 | 16 | yes | yes | yes |
| 98 | `fontWeight` | 100 … 900, `"normal"` (400), `"bold"` (700) | 400 | yes | yes | no |
| 99 | `fontStyle` | `normal`, `italic` | normal | yes | yes | no |
| 100 | `textColour` | colour | `$text` | yes | no | yes |
| 101 | `lineHeight` | multiple 0.5 … 4 | 1.2 | yes | yes | no |
| 102 | `letterSpacing` | em −0.5 … 1 | 0 | yes | yes | no |
| 103 | `textAlign` | `start`, `end`, `centre`, `justify` | start | yes | no (paint offset) | no |
| 104 | `textWrap` | `none`, `word`, `anywhere` | word | yes | yes | no |
| 105 | `textOverflow` | `visible`, `clip`, `ellipsis` | visible | yes | yes | no |
| 106 | `maxLines` | integer 0 … 1,000 (0 unbounded) | 0 | no | yes | no |
| 107, 108 | `textOutlineColour`, `textOutlineWidth` | colour; em 0 … 0.3 | transparent, 0 | yes | no | yes |
| 109, 110 | `textShadowColour`, `textShadowOffset` | colour; [x, y] em | transparent, [0, 0] | yes | no | yes |
| 111 | `textDirection` | `auto`, `ltr`, `rtl` | auto | yes | yes | no |
| 112 | `tabularNumbers` | bool | false | yes | yes | no |
| 113 | `underline` | bool | false | yes | no | no |
| 114 | `textScalePolicy` | `scale`, `fixed` | scale | yes | yes | no |
| 128 | `transition` | object of § "Animations and transitions" | none | no | no | no |
| 129 | `animation` | animation name, or `"none"` | none | no | no | no |
| 130 | `essentialMotion` | bool | false | no | no | no |

Style values are cooked into typed blocks (§ "Style block encoding"); an authored property that is not in this table is rule 20; a value outside its grammar is rule 21. The table is the whole style vocabulary: clipping is `clip` (77), paint order among siblings `zIndex` (30), shadows the three `shadow*` properties (69–71), and visibility the element member `visible`, not a style property; the design uses no other name for them.

## Font family document schema

```json
{
	"$schema": "zenith2.fontfamily",
	"schemaVersion": 1,
	"id": "4d2a…",
	"name": "Body",
	"faces": [
		{ "file": "thirdparty:noto-fonts/NotoSans/NotoSans-Regular.ttf", "weight": 400, "italic": false },
		{ "file": "thirdparty:noto-fonts/NotoSans/NotoSans-Bold.ttf", "weight": 700, "italic": false }
	],
	"msdf": { "emPixels": 40, "rangePixels": 6, "pageSize": 1024, "padding": 1, "maxPages": 16 },
	"bake": { "ranges": ["U+0020-007E", "U+00A0-017F", "U+2010-2027"], "fromLocalization": true },
	"dynamic": true
}
```

| Member | Type | Meaning |
|---|---|---|
| `name` | string ≤ 64 bytes | the family's display name (editor, diagnostics) |
| `faces` | array of 1 … 8 | `file`: path of a `.ttf`, `.otf` or `.ttc` (relative, logical, or `thirdparty:<manifest name>/<path>`, cross-area row UI9); `weight` 100 … 900; `italic`; `collectionIndex` for `.ttc` (default 0); two faces with the same (weight, italic) are rule 81 |
| `msdf` | object | the MTSDF atlas parameters of `Zenith_FontBakeDesc` (`Zenith2/Docs/Design/UI/ServicesAPI.md → "Zenith_UIFontBake header"`) |
| `bake.ranges` | array of `"U+XXXX"` or `"U+XXXX-YYYY"` | code points baked into the cooked atlas pages |
| `bake.fromLocalization` | bool, default true | also bake every code point used by the game's string tables of locales that select this family (Localization.md "Font selection per locale") |
| `dynamic` | bool, default true | glyphs outside the baked set are generated at run time; `false`: they draw as `.notdef` and are reported by the cook as missing |

## Localization project document schema

```json
{
	"$schema": "zenith2.localization",
	"schemaVersion": 1,
	"id": "a90e…",
	"sourceLocale": "en-GB",
	"locales": [
		{ "tag": "en-GB" }, { "tag": "fr-FR" }, { "tag": "de-DE" }, { "tag": "ja-JP" },
		{ "tag": "ar", "fallback": ["en-GB"] }, { "tag": "he-IL" }, { "tag": "hi-IN" }, { "tag": "th-TH" }, { "tag": "zh-Hans-CN" }
	],
	"namespaces": ["menu", "hud", "dialogue", "captions"],
	"fonts": {
		"default": { "body": "../Assets/Fonts/Body.fontfamily.json", "title": "../Assets/Fonts/Title.fontfamily.json" },
		"locales": { "ja-JP": { "body": "../Assets/Fonts/NotoSansJP.fontfamily.json" } },
		"scripts": { "Arab": ["content:/Engine/Fonts/NotoSansArabic.fontfamily.json"], "Hani": ["../Assets/Fonts/NotoSansSC.fontfamily.json"] }
	},
	"pseudo": { "expansion": 1.4, "markers": true },
	"literalsAreErrors": false,
	"checks": { "contrast": "error", "maxWidth": "warning" }
}
```

| Member | Meaning |
|---|---|
| `sourceLocale` | the locale whose tables are authoritative for keys and arguments; must be in `locales` (rule 90) |
| `locales` | every supported locale; `fallback` overrides the default chain for that locale (Localization.md "Locales and negotiation") |
| `namespaces` | the string-table namespaces; every table file names one of them (rule 93) |
| `fonts.default` | font tokens to family documents; every token a theme uses must resolve here or in the theme's `fonts` (rule 94) |
| `fonts.locales` | per-locale token overrides, matched by negotiation on the locale tag |
| `fonts.scripts` | per ISO 15924 script code: families appended to every stack for code points of that script (Text.md "Font fallback") |
| `pseudo` | pseudo-localization parameters (Localization.md "Pseudo-localization and stress modes") |
| `literalsAreErrors` | rule 52's severity |
| `checks` | severities of the accessibility contrast lint and the `maxWidthEm` lint: `off`, `warning`, `error` |

The engine's own project (`Zenith2/Engine/ZenithUI/Internal/Content/Localization/localization.json`) declares the `engine` namespace (accessibility templates, remapping prompts, caption speaker formatting, the loading and error strings) with source locale `en-GB`; a game's project inherits it: the `engine` namespace's keys are merged into every game's tables, and a game table may override an engine key of the same full name.

## String table document schema

```json
{
	"$schema": "zenith2.strings",
	"schemaVersion": 1,
	"locale": "fr-FR",
	"namespace": "menu",
	"strings": {
		"start":    { "text": "Commencer" },
		"coins":    { "text": "{count, plural, one {# pièce} other {# pièces}}", "comment": "HUD coin counter" },
		"continue": { "text": "Continuer", "maxWidthEm": 14 }
	}
}
```

| Member | Meaning |
|---|---|
| `locale`, `namespace` | must equal the file's directory and stem (rule 95) |
| `strings` | keyed map (canonical rule C5) of names `^[a-z][A-Za-z0-9_.]{0,63}$` to entries |
| `text` | the message source, § "Message syntax"; at most 8,192 bytes (rule 97) |
| `comment` | translator context, exported to XLIFF `<note>`; source locale only |
| `sourceHash` | target locales only: the low 64 bits of `Zenith_Hash64` of the source text this translation was made from, as 16 hex digits, written by the XLIFF import (Localization.md "Extraction report and exchange files") |
| `maxWidthEm` | a translator constraint: the longest line of the formatted message at the reference font (the first face of `body`) may not exceed it (the `maxWidth` lint, Localization.md "Localization validation") |
| `args` | source locale only: declared argument names and kinds `{ "count": "integer" }`; absent: inferred from the source message |

## Message syntax

The message syntax is a subset of ICU MessageFormat 1 with rich-text tags:

```text
message      := part*
part         := text | argument | tag | escape
text         := any characters except '{', '}', '<', '\' and, inside a plural case, '#'
escape       := "\{" | "\}" | "\<" | "\\" | "\#"
argument     := "{" name "}"
              | "{" name "," "number" [ "," numberStyle ] "}"
              | "{" name "," "plural" "," [ "offset:" integer ] pluralCase+ "}"
              | "{" name "," "selectordinal" "," pluralCase+ "}"
              | "{" name "," "select" "," selectCase+ "}"
numberStyle  := "integer" | "percent" | "::." digits      (fixed fraction digits, 0–6)
pluralCase   := ( "zero" | "one" | "two" | "few" | "many" | "other" | "=" integer ) "{" message "}"
selectCase   := keyword "{" message "}"                   (keyword [a-z][a-zA-Z0-9_]*, "other" required)
tag          := "<b>" message "</b>" | "<i>" message "</i>" | "<u>" message "</u>"
              | "<c=" token ">" message "</c>"            (a palette token, without '$')
              | "<prompt action=\"" actionName "\"/>"     (an inline input prompt)
              | "<img sprite=\"" spriteName "\"/>"        (an inline sprite)
              | "<br/>"
name         := [a-zA-Z][a-zA-Z0-9_]{0,31}
```

- Whitespace inside an argument's syntax is ignored; in text it is significant.
- `#` inside a plural case is the plural argument's value minus the offset, formatted with the locale's integer format.
- Nesting depth of arguments and tags is at most 8 (LOCALIZATION_ERROR_NESTING); tags must close in order (LOCALIZATION_ERROR_TAG).
- Every plural or selectordinal argument has an `other` case, and its categories are among the locale's categories (LOCALIZATION_ERROR_MISSING_OTHER, LOCALIZATION_ERROR_PLURAL_CATEGORY).
- A translation uses only argument names the source message declares (LOCALIZATION_ERROR_UNKNOWN_ARGUMENT) and gives each the same kind (LOCALIZATION_ERROR_ARGUMENT_MISMATCH).

## Canonical schemas and document types

The five types are registered tool-side by `Zenith_RegisterUIDocumentTypes` (`Zenith2/Docs/Design/UI/ServicesAPI.md → "Tool-side headers"`) into SCENE's `Zenith_MigrationRegistry` (`Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Zenith_MigrationRegistry header"`), following ENV's precedent, so ZenithUI has no `ZenithDocument` edge; ASSET's list of registered types gains UI's row (cross-area row UI12).

| Row | `m_szStableName` | `m_szSchemaTag` | `m_aszSuffixes` | `m_szIDKey` |
|---|---|---|---|---|
| 0 | `Zenith.UI.UIDocument` | `zenith2.ui` | `.ui.json` | `id` |
| 1 | `Zenith.UI.ThemeDocument` | `zenith2.uitheme` | `.uitheme.json` | `id` |
| 2 | `Zenith.UI.FontFamilyDocument` | `zenith2.fontfamily` | `.fontfamily.json` | `id` |
| 3 | `Zenith.UI.LocalizationProjectDocument` | `zenith2.localization` | `/localization.json` | `id` |
| 4 | `Zenith.UI.StringTableDocument` | `zenith2.strings` | `.strings.json` | null: `Zenith_MakePathDocumentID` of the path |

Each desc has `m_szVersionKey` `"schemaVersion"`, `m_uCurrentVersion` `uUI_DOCUMENT_SCHEMA_VERSION` (1), `m_pfnVisitFieldSets` null and `m_pUserData` null. Canonical member order (rules of `Zenith2/Docs/Design/SCENE/DocumentFramework.md → "Canonical rules"`; R = required):

| Type | Root object, members in canonical order | Nested rules |
|---|---|---|
| UI document | `$schema` R, `schemaVersion` R, `id` R, `theme`, `canvas`, `strings`, `focusScope`, `cancelAction`, `cancelEvent`, `bindings`, `templates`, `root` R (every member of § "Document root", in that table's order) | `bindings` and `templates` keyed maps (C5); `canvas` members `scaleMode`, `referenceWidth`, `referenceHeight`, `match`, `safeArea`; an element is an object with members `type` R, `uid` R, `id`, `class`, `style`, `states`, `visible`, `enabled`, `focusable`, `nav`, `events`, `accessibility`, then type-specific members in the order of § "Element types", then `children`; `children` an ordered array (C8), whose order is data; `style` and every state object keyed maps (C5, so properties sort by name); vectors and edge lists inline arrays (C7) |
| theme | `$schema` R, `schemaVersion` R, `id` R, `extends`, `fonts`, `metrics`, `palettes`, `typeDefaults`, `classes`, `sprites`, `animations`, `speakerColours`, `focusRing` | every map keyed (C5); `keys` of an animation an ordered array (C8) |
| font family | `$schema` R, `schemaVersion` R, `id` R, `name`, `faces`, `msdf`, `bake`, `dynamic` | `faces` ordered array (C8); `bake.ranges` inline array (C7) |
| localization project | `$schema` R, `schemaVersion` R, `id` R, `sourceLocale` R, `locales` R, `namespaces`, `fonts`, `pseudo`, `literalsAreErrors`, `checks` | `locales` ordered array of objects (`tag` R, `fallback`); `fonts` keyed maps |
| string table | `$schema` R, `schemaVersion` R, `locale` R, `namespace` R, `strings` R | `strings` keyed map (C5); entry objects `text` R, `comment`, `maxWidthEm`, `args` |

Two saves of one document are byte-identical, so string tables merge line by line in source control (one key per line, sorted).

## Migration chains and fixtures

- Each type is at version 1; every increment adds a `Zenith_PfnMigrationStep` registered by `Zenith_RegisterMigrationStep` and a fixture pair in the same diff ([Z2-N-CONTRACT-549]).
- Fixtures: `Zenith2/Tools/ZenithCook/Tests/Fixtures/Migration/UI/<Type>/v<N>.json` and `v<N>.expected.json` for `<Type>` in `UIDocument`, `ThemeDocument`, `FontFamilyDocument`, `LocalizationProjectDocument`, `StringTableDocument` ([Z2-N-CONTRACT-548]); version 1's pair is the identity (the canonical form of the input), which proves canonical writing and unknown-member preservation.
- A step that renames or moves an element member rewrites every instance override naming it (overrides are keyed by uid and member pointer, [Z2-N-CONTRACT-547]).
- The cook and the runtime read only current-version bytes: the cook opens documents through `Zenith_CookContext::OpenSourceDocument`, which migrates in memory (`Zenith2/Docs/Design/ASSET/Cook.md → "Source documents in cook steps"`).

## Validation rules

`Zenith_UIDocumentModel::Validate`, the font importer and the `ui.localization` step report these rules; an error fails the asset's cook item with `COOK_ERROR_STEP_FAILED` and its diagnostics in the cook report; a warning is reported and cooking continues.

| Rule | Severity | Condition |
|---|---|---|
| 1 | error | the document is not a JSON object, or a required root member is missing |
| 2 | error | `$schema` does not match the suffix's type |
| 3 | error | `schemaVersion` above the build's (opened read-only, never cooked) |
| 10 | error | an element without `type` or `uid`, or an unknown `type` |
| 11 | error | two elements with the same `uid` |
| 12 | error | two elements with the same `id` |
| 13 | error | `id` outside its grammar |
| 14 | error | `children` on a non-container type |
| 15 | error | the root's type is not `panel`, `scroll` or `instance` |
| 16 | error | a type-specific member on the wrong type, or a type-specific member outside its range (`maxBytes`, `min` ≥ `max`, `step` < 0) |
| 17 | warning | an interactive element without `id` (its events carry an empty `Element`) |
| 18 | warning | an element with `clip: true` (explicit or a `scroll` or `list` default) under an ancestor with a non-zero `rotate` (the clip becomes its bounding rectangle, Rendering.md "Clipping") |
| 19 | warning | `clip: true` on an element with a non-zero `cornerRadius` (the clip is rectangular) |
| 20 | warning | an unknown style property (kept, ignored) |
| 21 | error | a style value outside its grammar |
| 22 | error | an unknown state key |
| 23 | error | a non-animatable property in an animation key; a layout-affecting animatable property in an animation key is accepted with the warning diagnostic `ui.doc.layout_animation` (Layout.md "Animations and transitions") |
| 30 | error | a malformed path |
| 31 | error | a path that resolves to no asset, or to an asset of the wrong kind |
| 32 | error | an unknown sprite, template, class or animation name |
| 33 | error | an instance cycle |
| 34 | error | instance depth above 8 |
| 35 | error | an `overrides` key naming no element of the instanced subtree |
| 40 | error | a binding's component or property unknown to the frozen type registry (data-defined components included) |
| 41 | error | a binding used where its value kind cannot convert (a LIST binding on a `text`) |
| 42 | error | a `@slot` reference to an undeclared slot |
| 43 | error | an `item` mapping naming a field absent from the list element struct |
| 44 | error | `writeBack` on a non-`self` binding, or on a property whose type cannot hold the control's value |
| 45 | error | an `entity` value outside its grammar |
| 46 | error | a `config` binding to an unknown key or a simulation-affecting key |
| 50 | error | a localization key the source-locale tables do not define |
| 51 | error | `args` naming an argument the source message does not declare, or omitting one it declares |
| 52 | warning (error by project) | a `literal` text in a non-development document |
| 53 | error | an `inputPrompt` or `rebind` action or context the cooked bindings do not declare |
| 60 | error | a `nav` target naming an unknown `id` |
| 61 | error | two `nav.initial` elements in one screen |
| 62 | warning | an interactive element with no accessible name after derivation (Accessibility.md "Names and descriptions") |
| 63 | warning (error by project) | a text colour whose contrast against its effective background is below the WCAG threshold in any palette (Accessibility.md "Colour-vision modes and contrast") |
| 64 | warning | an interactive element smaller than 44 × 44 dp in a document whose project targets a touch scheme |
| 65 | error | `accessibility.labelledBy` naming an unknown `id` |
| 70 | error | a theme class name or token outside its grammar |
| 72 | error | an `extends` chain deeper than 4 or cyclic |
| 74 | error | a token defined in a non-default palette but not in `default` |
| 75 | error | a theme font token that resolves to no family document |
| 80 | error | a face file that fails to open in the shaping backend (`FONT_ERROR_UNSUPPORTED_PROGRAM`) |
| 81 | error | two faces with the same weight and style |
| 82 | warning | a baked code point no face covers |
| 83 | error | the baked set needs more pages than `maxPages` (`FONT_ERROR_PAGE_LIMIT`) |
| 84 | error | an `msdf` member outside its range |
| 90 | error | `sourceLocale` missing from `locales`; a locale tag outside the grammar |
| 91 | error | a string table for a locale the project does not list |
| 92 | warning | a key present in the source locale and missing in a target locale (the fallback chain serves it; the extraction report lists it) |
| 93 | error | a table namespace the project does not list |
| 94 | error | a font token without a family for some locale |
| 95 | error | a table's `locale` or `namespace` disagreeing with its path |
| 96 | error | a message that fails `Zenith_CompileMessage` (its `Zenith_LocalizationErrorCode` in the detail) |
| 97 | error | a message source above 8,192 bytes |
| 98 | warning (by project) | a formatted message wider than its `maxWidthEm` |

## Cooked UI document format

### UI document sections

<!-- elaboration: the cooked UI document layout -->
A `.zui` file is an ASSET cooked asset file (`Zenith2/Docs/Design/ASSET/Formats.md → "Header layout"`, "Section table", "Checks"): magic `ZUID`, format version 1, kind 13. Section kinds (UI-defined, [Z2-N-CONTRACT-527]):

| Kind | Name | Count | Content |
|---|---|---|---|
| 1 | HEADER | 1 | the 64-byte record below |
| 2 | STRINGS | 1 | the string pool: `uint32` count, then per string `uint32` byte offset and `uint32` length, then the UTF-8 bytes (each followed by a NUL); index 0 is the empty string |
| 3 | ELEMENTS | 1 | element records (§ "Element record layout"), depth-first pre-order, index 0 the root |
| 4 | STYLES | 1 | style blocks (§ "Style block encoding") |
| 5 | CLASSES | 0 or 1 | the folded theme's class records `{ uint32 nameString; uint32 baseBlock; uint32 stateCount; then stateCount × { uint16 stateFlags; uint16 reserved; uint32 block } }` sorted by name string bytes |
| 6 | BINDINGS | 0 or 1 | binding records (§ "Binding records") |
| 7 | TEXTS | 0 or 1 | text content records `{ uint8 form (1 key, 2 binding, 3 literal); uint8 argCount; uint16 reserved; uint64 keyHash; uint32 literalString; uint32 bindingSlot; then argCount × { uint64 nameHash; uint8 source (1 binding, 2 integer, 3 number, 4 string, 5 bool); uint8 reserved[3]; uint32 bindingSlotOrString; sint64 or double value } }` |
| 8 | NAV | 0 or 1 | navigation records `{ uint32 element; uint32 up, down, left, right (element indices or 0xFFFFFFFF); uint32 groupString; sint32 order; uint8 flags (bit 0 wrap, 1 initial, 2 trap); uint8 reserved[3] }` |
| 9 | A11Y | 0 or 1 | accessibility records `{ uint32 element; uint8 role; uint8 live; uint8 heading; uint8 flags (bit 0 hidden); uint32 labelText; uint32 hintText }` (text indices into TEXTS) |
| 10 | TEMPLATES | 0 or 1 | template records `{ uint32 nameString; uint32 firstElement; uint32 elementCount }`; template elements are stored in ELEMENTS after the main tree |
| 11 | ANIMATIONS | 0 or 1 | animation records `{ uint32 nameString; float duration; uint8 loop; uint8 essential; uint8 easing; uint8 keyCount; then keyCount × { float t; uint32 block } }` |
| 12 | SPRITES | 0 or 1 | sprite records `{ uint32 nameString; uint32 dependencyIndex (texture); uint16 rect[4]; uint16 slice[4] }` |
| 13 | PALETTES | 0 or 1 | `uint32 tokenCount; uint32 nameString[tokenCount]; then 6 palettes (default, protanopia, deuteranopia, tritanopia, achromatopsia, highContrast) × tokenCount × { float r, g, b, a; uint8 defined; uint8 reserved[3] }` |
| 14 | DEPENDENCIES | 1 | `uint32 count`, then 16-byte GUIDs: fonts, textures and instanced documents in first-reference order, the folded theme's fonts and sprite textures included; the theme itself is never a dependency |
| 15 | RMLUI | 0 or 1 | the generated RML document and RCSS style sheet as two NUL-terminated UTF-8 strings (Layout.md "The RmlUi integration", cook-time input), present in every cooked UI document while D-SEL-UIRT (open, Stage 1/2) is open, because the RmlUi candidate is compiled and measured then, and after a ruling that selects RmlUi; absent after a ruling that selects the custom runtime, whose ticket deletes the section writer |
| 16 | METRICS | 0 or 1 | the folded theme's metric tokens `{ uint32 nameString; float value }` sorted by name |
| 17 | TYPEDATA | 0 or 1 | per-type element records and binding default values (§ "Typed data records") |
| 18 | EVENTS | 0 or 1 | `uint32 count;` then `count × uint32` string indices: each element with events owns five consecutive entries, in the order `activate`, `change`, `toggle`, `select`, `commit`, string 0 (the empty string) for an absent name |

### Header record

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | flags: bit 0 reserved (0), bit 1 contains world-only features, bit 2 development document |
| 4 | 4 | element count (main tree) |
| 8 | 4 | total element records (with templates) |
| 12 | 4 | binding count |
| 16 | 4 | template count |
| 20 | 1 | scale mode (0 dp, 1 reference) |
| 21 | 1 | safe area (`canvas.safeArea`: 0 the full canvas, 1 inset by the safe area) |
| 22 | 1 | focus scope (`focusScope`: 0 `auto`, 1 `never`) |
| 23 | 1 | cancel action (`cancelAction`: 0 `none`, 1 `resumeWorld`) |
| 24 | 4 | reference width (f32) |
| 28 | 4 | reference height (f32) |
| 32 | 4 | match (f32) |
| 36 | 4 | cancel event string |
| 40 | 4 | default namespace string |
| 44 | 16 | document persistent ID |
| 60 | 4 | reserved (0) |

A loader refuses a value outside the listed ones at offsets 20 to 23 (`UI_ERROR_DOCUMENT_CORRUPT`).

### Element record layout

64 bytes, little-endian, field by field:

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | type (`Zenith_UIElementType`) |
| 1 | 1 | flags: bit 0 focusable, 1 visible (constant), 2 enabled (constant), 3 visible bound, 4 enabled bound, 5 selectable text, 6 has events, 7 reserved |
| 2 | 2 | extended flags: bits 0–2 type-specific option index (slider orientation, list axis, scroll axes), bit 3 multiline, bit 4 checked constant, bits 5–15 reserved (0) |
| 4 | 4 | parent index (0xFFFFFFFF for the root and template roots) |
| 8 | 4 | first child |
| 12 | 4 | next sibling |
| 16 | 4 | id string (0 for none) |
| 20 | 4 | inline style block (0xFFFFFFFF for none) |
| 24 | 16 | uid (persistent ID, binary form) |
| 40 | 2 | first class index into the element-class table (the STYLES section's class list) |
| 42 | 2 | class count |
| 44 | 4 | text record (0xFFFFFFFF none) |
| 48 | 4 | type data offset: the byte offset, divided by 4, of the element's record in the TYPEDATA section (§ "Typed data records"), whose layout the element's type selects; 0xFFFFFFFF for a type with no record (`panel`, `text`) |
| 52 | 4 | event names: the index in the EVENTS section of the element's first of five entries; 0xFFFFFFFF when flag bit 6 (has events) is clear |
| 56 | 4 | nav record (0xFFFFFFFF none) |
| 60 | 4 | a11y record (0xFFFFFFFF none) |

### Style block encoding

The STYLES section holds `uint32 blockCount; uint32 blockOffset[blockCount];` then the blocks, then `uint32 classIndexCount;` and the element-class table (`uint32` class indices). A block is `uint16 propertyCount; uint16 reserved;` then `propertyCount` entries sorted by property ID, each `{ uint16 id; uint8 valueKind; uint8 count; then count × 4-byte words }`:

| `valueKind` | Words | Meaning |
|---|---|---|
| 1 length | 1 (f32) | canvas units |
| 2 percent | 1 (f32) | percent of the containing block |
| 3 keyword | 1 (uint32) | the keyword's index in the property's grammar (`auto` 0 …) |
| 4 colour | 4 (f32) | linear RGBA |
| 5 token | 1 (uint32) | the PALETTES token index (colour) or METRICS token index (length) |
| 6 number | 1 (f32) | unitless |
| 7 integer | 1 (i32) | |
| 8 track list | 2 per track (kind, f32) | grid tracks: kind 1 length, 2 fr, 3 auto, 4 min-content, 5 max-content |
| 9 reference | 1 (uint32) | a string (font tokens, animation names) or dependency index (images) |
| 10 binding | 1 (uint32) | a binding slot (bindable members of § "Element common members") |

Unknown property IDs and value kinds fail the load (`UI_ERROR_DOCUMENT_CORRUPT`); `count` is checked against the kind (1, 2 or 4 lengths for edges, 4 for colour).

### Typed data records

<!-- elaboration: the per-type record layouts -->
The TYPEDATA section is `uint32 byteCount;` followed by `byteCount` bytes of records, each 4-byte aligned and little-endian. An element's record starts at byte `4 × typeDataOffset` of that payload and has the size its type gives below; the loader checks `4 × offset + size ≤ byteCount` before reading (`UI_ERROR_DOCUMENT_CORRUPT` otherwise). "Slot" fields are binding slot indices, 0xFFFFFFFF for none; "string" fields index STRINGS; "text" fields index TEXTS.

| Element type | Size | Fields in order |
|---|---|---|
| `button` | 4 | `uint8 presentationAction` (0 `none`, 1 `resumeWorld`); `uint8 reserved[3]` |
| `toggle` | 12 | `uint32 groupString`; `uint32 checkedSlot`; `uint8 checkedConstant`; `uint8 reserved[3]` |
| `slider`, `progress` | 20 | `float min`; `float max`; `float step`; `uint32 valueSlot`; `float valueConstant` (orientation in extended flags bits 0–2) |
| `textField` | 16 | `uint32 placeholderText`; `uint32 textSlot`; `uint16 maxBytes`; `uint8 purpose` (`Zenith_TextInputPurpose` order of § "Element types"); `uint8 flags` (bit 0 `commitOnEnter`); `uint32 reserved` |
| `scroll` | 4 | `uint8 axes` (0 vertical, 1 horizontal, 2 both); `uint8 inertia`; `uint8 scrollbar` (0 auto, 1 always, 2 never); `uint8 reserved` |
| `list` | 16 | `uint32 itemsSlot`; `uint32 templateIndex`; `float itemExtent` (≤ 0: variable extent); `uint8 axis` (0 vertical, 1 horizontal); `uint8 selectable`; `uint16 overscan` |
| `choice` | 20 | `uint32 firstOptionText` (consecutive TEXTS records); `uint16 optionCount`; `uint16 reserved`; `uint32 optionsSlot`; `uint32 selectedSlot`; `sint32 selectedConstant` |
| `image` | 16 | `uint32 image` (bit 31 set: a sprite name string in bits 0–30; clear: a DEPENDENCIES index of a texture); `uint32 imageSlot`; `uint16 slice[4]` (texels; all 0xFFFF: the sprite's own) |
| `inputPrompt` | 12 | `uint32 actionString`; `uint32 contextString`; `sint32 part` (−1 all parts) |
| `rebind` | 12 | `uint32 actionString`; `uint32 contextString`; `uint8 scheme` (0 keyboardMouse, 1 gamepad, 2 touch, 3 active); `uint8 reset` (0 none, 1 action, 2 all); `uint16 bindingIndex` |
| `captionArea` | 4 | `uint8 kindMask` (bit 0 subtitle, bit 1 sound); `uint8 maxLines`; `uint16 reserved` |
| `onScreenControl` | 16 | `uint8 control` (0 button, 1 stick); `uint8 mode` (0 fixed, 1 floating); `uint16 reserved`; `uint32 virtualControlString`; `float radius`; `float deadzone` |
| binding default value (BINDINGS offset 44) | 12 | `uint8 valueKind` (`Zenith_UIValueKind`); `uint8 reserved[3]`; 8 value bytes (f64, i64, a STRINGS index in the low 4 bytes, or a DEPENDENCIES index) |

Instances are flattened by the cook, so no runtime record exists for `instance`; the instance's binding map becomes binding slots of the flattened tree.

### Binding records

`uint32 count;` then 48-byte records:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | name string |
| 4 | 1 | source: 1 component, 2 config |
| 5 | 1 | entity selector: 1 self, 2 parent, 3 singleton, 4 persistent |
| 6 | 1 | value kind (`Zenith_UIValueKind`) |
| 7 | 1 | flags: bit 0 write back, bit 1 text mode key, bit 2 has default |
| 8 | 8 | component type ID (`Zenith_TypeID`), or the config key string for config sources (index in the low 4 bytes) |
| 16 | 4 | property ID path: the first property |
| 20 | 4 | nested property ID (0 when none) |
| 24 | 16 | persistent entity ID (selector 4) |
| 40 | 4 | item map: index of `{ uint32 count; then count × { uint32 slotName; uint32 fieldPropertyID } }` |
| 44 | 4 | default value: the byte offset ÷ 4 of a 12-byte default-value record in TYPEDATA (§ "Typed data records"), 0xFFFFFFFF without `default` |

## Cooked font format

### Font sections

<!-- elaboration: the cooked font layout and the ZMSD page payload -->
A `.zfont` is an ASSET cooked asset file with magic `ZFNT`, format version 1, kind 12, written by the `ui.font` step from the payloads `Zenith_BakeFontFamily` produces:

| Kind | Name | Index | Content |
|---|---|---|---|
| 1 | FAMILY | 0 | `uint32 faceCount; uint32 pageCount; uint32 flags (bit 0 dynamic allowed); uint32 nameString; float emPixels; float rangePixels; uint16 pageSize; uint16 padding; then the name as UTF-8 with its byte length` |
| 2 | FACES | 0 | `faceCount` face records (§ "Face table record") |
| 3 | PROGRAM | face index | the face's font program, the source file's bytes unchanged (stored compressed with the cook's resolved codec) |
| 4 | GLYPHS | face index | `uint32 count;` then glyph records (§ "Glyph record") sorted by glyph ID |
| 5 | COVERAGE | face index | `uint32 rangeCount;` then `{ uint32 first; uint32 last; }` code-point ranges the face's cmap maps, sorted, disjoint |
| 6 | PAGE | page index | one MSDF atlas page payload (§ "MSDF atlas page payload") |
| 7 | BAKED_SET | 0 | the sorted baked code points (`uint32` each), for diagnostics and the dynamic-glyph decision |

### Face table record

64 bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 2 | weight |
| 2 | 1 | italic (0 or 1) |
| 3 | 1 | reserved (0) |
| 4 | 2 | units per em |
| 6 | 2 | reserved (0) |
| 8 | 4 | glyph count of the program |
| 12 | 28 | ascent, descent, line gap, cap height, x-height, underline position, underline thickness (7 × f32, em) |
| 40 | 4 | em pixels (f32) |
| 44 | 4 | range pixels (f32) |
| 48 | 4 | baked glyph count |
| 52 | 4 | collection index |
| 56 | 8 | Zenith_Hash64 of the program bytes (the dynamic-glyph cache key's font identity) |

### Glyph record

32 bytes: `uint32 glyphID; uint16 page (0xFFFF: no image, an empty glyph); uint16 flags (bit 0 baked); uint16 x, y, width, height (texels in the page); float planeLeft, planeTop, planeRight, planeBottom (em, relative to the pen and baseline, +Y down)`.

### MSDF atlas page payload

Every atlas page, baked (PAGE sections) or generated at run time (the glyph cache's dynamic pages), has one byte layout: a 32-byte versioned header followed by the pixels.

| Offset | Size | Field | Rule |
|---|---|---|---|
| 0 | 4 | magic | `ZMSD` (`5A 4D 53 44`) |
| 4 | 2 | page format version | 1; a reader refuses another value (`FONT_ERROR_CORRUPT`) |
| 6 | 2 | flags | bit 0 colour page (sRGB RGBA image, not a distance field); other bits 0 |
| 8 | 2 | width | 512, 1024 or 2048 |
| 10 | 2 | height | equal to width |
| 12 | 1 | channels | 4 |
| 13 | 1 | encoding | 1 = MTSDF RGBA8 UNORM: RGB the multi-channel signed distance, A the true signed distance; 0.5 is the edge; values map distance `d` (pixels, positive inside) to `clamp(0.5 + d / rangePixels, 0, 1)` quantised round-half-to-even to 8 bits |
| 14 | 2 | reserved | 0 |
| 16 | 4 | em pixels | f32, equal to the family's |
| 20 | 4 | range pixels | f32 |
| 24 | 4 | glyph count | glyphs placed on the page |
| 28 | 4 | pixel check | the low 32 bits of `Zenith_Hash64` of the pixel bytes |
| 32 | w × h × 4 | pixels | rows top-down, tightly packed, RGBA order |

A page whose check fails, whose size is not `32 + w × h × 4`, or whose em and range differ from the family's fails the font load (`FONT_ERROR_CORRUPT`); the font then uses its kind's SUBSTITUTE fallback (the engine default family).

## Cooked localization table format

### Localization sections

<!-- elaboration: the cooked localization table layout -->
A `.zloc` is an ASSET cooked asset file with magic `ZLOC`, format version 1, kind 20; one per (game, locale), the engine namespace merged in:

| Kind | Name | Content |
|---|---|---|
| 1 | HEADER | `uint8 tagLength; char tag[31]; uint8 direction (0 LTR, 1 RTL); uint8 numberingSystem (0 latn, 1 arab, 2 arabext, 3 deva, 4 thai, 5 hanidec); uint8 pluralCardinalMask; uint8 pluralOrdinalMask; uint32 messageCount; uint32 fallbackCount; then fallbackCount × 32-byte tags` |
| 2 | KEYS | `messageCount × { uint64 keyHash; uint32 messageOffset; uint32 messageBytes }` sorted by key hash; two keys with one hash fail the cook with both keys named |
| 3 | MESSAGES | the compiled messages (§ "Message bytecode") |
| 4 | STRINGS | the text pool the messages reference (UTF-8, NUL-separated) |
| 5 | PLURALS | `uint8 ruleCount` (0 … 10: at most 5 cardinal and 5 ordinal rules, one per category `zero` … `many`, since `other` never has a rule); `uint8 reserved[3]`; then `ruleCount × { uint8 category (0 zero … 4 many); uint8 ordinal (0 or 1); uint16 programBytes; uint8 program[programBytes] }`, each record padded to 4 bytes; a category absent from the list is never selected except `other` (§ "Plural rule programs") |
| 6 | NUMBERS | `{ char decimal[4]; char group[4]; char minus[4]; char percent[8]; uint8 primaryGroup; uint8 secondaryGroup; uint8 minimumGroupingDigits; uint8 reserved; uint32 digits[10] }` (the numbering system's digit code points) |

Key hashes are `Zenith_Hash64` of the full key's UTF-8 bytes (`menu.start`), the same value `UI.Loc` interns and the TEXTS records store.

### Message bytecode

<!-- elaboration: the message bytecode -->
A message is `uint8 argCount; uint64 argNameHash[argCount];` then ops; every op starts with an opcode byte:

| Opcode | Name | Operands | Semantics |
|---|---|---|---|
| 0x00 | END | — | end of the message or of a case body |
| 0x01 | TEXT | `uint32 offset; uint32 bytes` | append pool text |
| 0x02 | ARG | `uint8 arg; uint8 style (0 default, 1 integer, 2 percent, 3 fixed); uint8 fractionDigits` | append the formatted argument |
| 0x03 | PLURAL | `uint8 arg; uint8 ordinal; sint32 offset; uint8 caseCount;` then cases `{ uint8 selector (0–5 a category, 0x80 exact); sint64 exact; uint32 bodyBytes; body }` | evaluate exact cases first, then the category |
| 0x04 | SELECT | `uint8 arg; uint8 caseCount;` cases `{ uint64 keywordHash; uint32 bodyBytes; body }` (the last case is `other`) | |
| 0x05 | POUND | — | append (plural value − offset) in the integer style |
| 0x06 | TAG_OPEN | `uint8 tag (1 b, 2 i, 3 u, 4 colour); uint32 parameter (token index)` | start a rich-text span |
| 0x07 | TAG_CLOSE | `uint8 tag` | |
| 0x08 | PROMPT | `uint32 actionString` | an inline input prompt object |
| 0x09 | SPRITE | `uint32 spriteString` | an inline sprite object |
| 0x0A | LINE_BREAK | — | a hard line break |

The interpreter validates every operand against the section bounds before use and fails a malformed message with `LOCALIZATION_ERROR_CORRUPT` (the key then formats as missing); recursion is bounded by the nesting depth 8.

### Plural rule programs

<!-- elaboration: plural rule programs -->
A CLDR plural rule (`n % 10 = 1 and n % 100 != 11`) compiles to a stack program over the operands `n` (absolute value), `i` (integer digits), `v` (visible fraction digit count), `w` (visible fraction digits without trailing zeros), `f`, `t`, `c` and `e` (compact exponent, always 0 here):

| Opcode | Operands | Effect |
|---|---|---|
| 0x10 PUSH_OPERAND | `uint8 operand` | push the operand |
| 0x11 PUSH_INT | `sint64` | push |
| 0x12 MOD | — | pop b, a; push a mod b (b > 0, checked at compile) |
| 0x13 IN_RANGES | `uint8 count; then count × { sint64 lo; sint64 hi }` | pop x; push x ∈ any [lo, hi] (integers only: a non-integer `n` never matches a range, the CLDR rule) |
| 0x14 NOT | — | |
| 0x15 AND, 0x16 OR | — | |
| 0x17 RETURN | — | the top is the rule's result |

The category order of evaluation is zero, one, two, few, many; the first true rule's category wins, `other` otherwise. Rules come from the pinned CLDR data (Localization.md "Plural rules").

## Binding manifest file

The global-post step `ui.manifest` writes `UI/BindingManifest.zbin` into the cooked tree: magic `ZUIM`, `uint16` version 1, `uint16` reserved, `uint32` type count, then per type sorted by ID `{ uint64 typeID; uint32 propertyCount; uint32 propertyID[propertyCount] (sorted) }`, then a `uint32` check (low 32 bits of `Zenith_Hash64` of the preceding bytes). It lists every component property any cooked UI document of the game binds, so the UI extractor declares exactly those reads at `AttachWorld` (Interaction.md "Requests from simulation" and the entry document's "Frame-graph placement"); a missing or corrupt manifest makes the extractor declare no binding reads and every component binding shows its default, logged once.

## Parser limits

These limits are constants in ZenithUI and ZenithCook, each tested at the limit and at limit + 1 ([Z2-N-ARCH-953]); they are the UI rows of `Zenith2/Docs/Architecture.md → "Parser limits"` (`.zui`, `.zfont` and its `ZMSD` page payload, `.zloc`, the binding manifest and the publication payload; cross-area row UI13, cross-doc item X236 (7)).

| Parser | Limits (a violation is a recoverable error) |
|---|---|
| UI document and theme JSON | the JSON limits of the table, plus: elements 16,384 per document after instance flattening; depth 64; children per element 4,096; bindings 256; templates 256; classes per element 16; theme classes 4,096; tokens 1,024 per palette; sprites 4,096; animations 512, keys 32 each |
| cooked `.zui` | element records 32,768; string pool 4 MiB, strings 65,536; style blocks 65,536, properties per block 128; binding records 256; TYPEDATA 4 MiB; EVENTS entries 5 × 32,768; every index checked against its section before use |
| font family JSON | faces 8; ranges 1,024; baked code points 65,536 |
| cooked `.zfont` | faces 8; pages 64 per file; glyph records 65,535 per face; coverage ranges 65,536 per face; program 64 MiB per face; page size 512, 1024 or 2048 |
| MSDF page payload | the header rules of § "MSDF atlas page payload" |
| localization project | locales 128; namespaces 256; fallback chain length 8 |
| string table JSON | keys 65,536 per table; message source 8,192 bytes |
| cooked `.zloc` | messages 262,144; pool 64 MiB; message bytes 65,536; plural programs 256 bytes each; nesting 8 |
| publication payload | 448 bytes; names 64 bytes; text 256 bytes |
| binding manifest | types 4,096; properties 65,536 in total |
