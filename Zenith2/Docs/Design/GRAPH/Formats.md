# GRAPH design — dump formats

This file is part of the GRAPH design family; the entry document is `Zenith2/Docs/Design/GRAPH.md`. It holds the JSON Schemas (draft 2020-12) of the two dump formats whose content rules are `Zenith2/Docs/Architecture/ZenithGraph.md` → "JSON dump" and `Zenith2/Docs/Design/GRAPH.md` → "Graph dump schema zenith2.graph-dump version 1" and "Frame dump schema zenith2.frame-dump version 1". The committed copies are `Zenith2/Engine/ZenithGraph/Tests/Data/zenith2.graph-dump.v1.schema.json` and `Zenith2/Engine/ZenithJobs/Tests/Data/zenith2.frame-dump.v1.schema.json`; the test `Test_GraphDump_GoldenValidatesAgainstSchema` validates every golden dump against the committed copy, and `Test_GraphDump_SchemaFileMatchesDesign` keeps each committed copy byte-identical to the block below (Z2-R-GRAPH-056).

## Graph dump JSON Schema

```json
{
	"$schema": "https://json-schema.org/draft/2020-12/schema",
	"$id": "urn:zenith2:graph-dump:v1",
	"type": "object",
	"$defs": {
		"hex64": { "type": "string", "pattern": "^0x[0-9a-f]{16}$" },
		"u32": { "type": "integer", "minimum": 0, "maximum": 4294967295 },
		"u64": { "oneOf": [ { "type": "integer", "minimum": 0, "maximum": 9007199254740991 }, { "$ref": "#/$defs/hex64" } ] },
		"mode": { "enum": ["READ", "WRITE", "READ_WRITE", "WRITE_DISCARD"] },
		"reason": { "enum": ["RAW", "WAR", "WAW", "ORDERING"] },
		"subrange": { "type": "array", "prefixItems": [ { "$ref": "#/$defs/u32" }, { "$ref": "#/$defs/u32" } ], "minItems": 2, "maxItems": 2 },
		"node": {
			"type": "object",
			"required": ["id", "name", "lane", "cost", "flags", "payload"],
			"properties": {
				"id": { "$ref": "#/$defs/u32" },
				"name": { "type": "string", "maxLength": 255 },
				"lane": { "$ref": "#/$defs/u32" },
				"cost": { "$ref": "#/$defs/u32" },
				"flags": { "type": "array", "items": { "enum": ["SIDE_EFFECT"] }, "uniqueItems": true },
				"culled": { "type": "boolean" },
				"position": { "$ref": "#/$defs/u32" },
				"priority": { "$ref": "#/$defs/u64" },
				"dependencyCount": { "$ref": "#/$defs/u32" },
				"payload": { "$ref": "#/$defs/hex64" }
			},
			"additionalProperties": true
		},
		"resource": {
			"type": "object",
			"required": ["id", "name", "subresources", "flags", "latestVersion", "payload"],
			"properties": {
				"id": { "$ref": "#/$defs/u32" },
				"name": { "type": "string", "maxLength": 255 },
				"subresources": { "type": "integer", "minimum": 1, "maximum": 16777216 },
				"flags": { "type": "array", "items": { "enum": ["IMPORTED", "OUTPUT", "REQUIRE_DEFINED_READS", "FIRST_WRITE_DISCARD", "FIRST_WRITE_DISCARD_PER_SUBRESOURCE"] }, "uniqueItems": true },
				"latestVersion": { "$ref": "#/$defs/u32" },
				"lifetime": { "oneOf": [ { "type": "null" }, { "type": "object", "required": ["first", "last"], "properties": { "first": { "$ref": "#/$defs/u32" }, "last": { "$ref": "#/$defs/u32" } } } ] },
				"payload": { "$ref": "#/$defs/hex64" }
			},
			"additionalProperties": true
		},
		"access": {
			"type": "object",
			"required": ["node", "resource", "mode", "subranges", "usage"],
			"properties": {
				"node": { "$ref": "#/$defs/u32" },
				"resource": { "$ref": "#/$defs/u32" },
				"mode": { "$ref": "#/$defs/mode" },
				"versionRead": { "$ref": "#/$defs/u32" },
				"versionWritten": { "$ref": "#/$defs/u32" },
				"subranges": { "type": "array", "items": { "$ref": "#/$defs/subrange" }, "minItems": 1 },
				"usage": { "$ref": "#/$defs/u32" }
			},
			"additionalProperties": true
		},
		"edge": {
			"type": "object",
			"required": ["from", "to", "reason"],
			"properties": {
				"from": { "$ref": "#/$defs/u32" },
				"to": { "$ref": "#/$defs/u32" },
				"reason": { "$ref": "#/$defs/reason" },
				"resource": { "$ref": "#/$defs/u32" },
				"version": { "$ref": "#/$defs/u32" },
				"ordering": { "$ref": "#/$defs/u32" }
			},
			"additionalProperties": true
		}
	},
	"required": ["schema", "schemaVersion", "graph", "frontEnd", "structureHash", "nodes", "resources", "accesses", "orderings"],
	"properties": {
		"schema": { "const": "zenith2.graph-dump" },
		"schemaVersion": { "const": 1 },
		"graph": { "type": "string" },
		"frontEnd": { "type": "string", "maxLength": 15 },
		"frame": { "$ref": "#/$defs/u64" },
		"structureHash": { "$ref": "#/$defs/hex64" },
		"nodes": { "type": "array", "items": { "$ref": "#/$defs/node" } },
		"resources": { "type": "array", "items": { "$ref": "#/$defs/resource" } },
		"accesses": { "type": "array", "items": { "$ref": "#/$defs/access" } },
		"orderings": { "type": "array", "items": { "type": "object", "required": ["before", "after", "reason"], "properties": { "before": { "$ref": "#/$defs/u32" }, "after": { "$ref": "#/$defs/u32" }, "reason": { "type": "string" } } } },
		"edges": { "type": "array", "items": { "$ref": "#/$defs/edge" } },
		"schedule": {
			"type": "object",
			"required": ["order", "culled", "reducedEdges", "reductionSkipped"],
			"properties": {
				"order": { "type": "array", "items": { "$ref": "#/$defs/u32" } },
				"culled": { "$ref": "#/$defs/u32" },
				"reducedEdges": { "$ref": "#/$defs/u32" },
				"reductionSkipped": { "type": "boolean" }
			}
		},
		"timings": { "type": "array", "items": { "type": "object", "required": ["node", "worker", "startNs", "endNs"], "properties": { "node": { "$ref": "#/$defs/u32" }, "worker": { "$ref": "#/$defs/u32" }, "startNs": { "$ref": "#/$defs/u64" }, "endNs": { "$ref": "#/$defs/u64" } } } }
	},
	"additionalProperties": true
}
```

Schema notes:

- `additionalProperties: true` everywhere expresses "readers ignore unknown keys" ([Z2-N-ARCH-341]).
- The key order the writer emits cannot be expressed in JSON Schema; the golden-file test pins it (Z2-R-GRAPH-054).
- A reader refuses `schemaVersion` above `uGRAPH_DUMP_SCHEMA_VERSION` with the text of `Zenith_GraphDumpVersionMessage` (Z2-R-GRAPH-057).

## Frame dump JSON Schema

```json
{
	"$schema": "https://json-schema.org/draft/2020-12/schema",
	"$id": "urn:zenith2:frame-dump:v1",
	"type": "object",
	"required": ["schema", "schemaVersion", "subgraph", "scheduleHash", "core", "runs", "affinity", "transients"],
	"properties": {
		"schema": { "const": "zenith2.frame-dump" },
		"schemaVersion": { "const": 1 },
		"subgraph": { "enum": ["Tick", "Epilogue", "Render"] },
		"frame": { "type": "integer", "minimum": 0 },
		"scheduleHash": { "type": "string", "pattern": "^0x[0-9a-f]{16}$" },
		"core": { "$ref": "urn:zenith2:graph-dump:v1" },
		"runs": {
			"type": "array",
			"items": {
				"type": "object",
				"required": ["node", "kind", "affinity", "group", "stage", "builderNode"],
				"properties": {
					"node": { "type": "integer", "minimum": 0 },
					"kind": { "enum": ["SYSTEM", "BEHAVIOURS", "STAGE", "ARBITRATION", "SYNC_POINT", "TICK_BOUNDARY"] },
					"affinity": { "enum": ["ANY", "MAIN", "RENDER_SUBMIT"] },
					"group": { "type": "integer", "minimum": 0, "maximum": 65535 },
					"stage": { "type": "integer", "minimum": 0, "maximum": 65535 },
					"builderNode": { "oneOf": [ { "type": "integer", "minimum": 0 }, { "type": "null" } ] }
				}
			}
		},
		"affinity": {
			"type": "object",
			"required": ["ANY", "MAIN", "RENDER_SUBMIT"],
			"properties": { "ANY": { "type": "integer" }, "MAIN": { "type": "integer" }, "RENDER_SUBMIT": { "type": "integer" } }
		},
		"transients": {
			"type": "object",
			"required": ["blockBytes", "placements"],
			"properties": {
				"blockBytes": { "type": "integer", "minimum": 0 },
				"placements": { "type": "array", "items": { "type": "object", "required": ["resource", "offset", "bytes"], "properties": { "resource": { "type": "integer" }, "offset": { "type": "integer" }, "bytes": { "type": "integer" } } } }
			}
		},
		"timeline": {
			"type": "object",
			"properties": {
				"tickCount": { "type": "integer" },
				"pipelineDepth": { "type": "integer", "minimum": 0, "maximum": 1 },
				"iterationStartNs": { "type": "integer" },
				"simulationEndNs": { "type": "integer" },
				"epilogueEndNs": { "type": "integer" },
				"renderStartNs": { "type": "integer" },
				"renderEndNs": { "type": "integer" },
				"iterationEndNs": { "type": "integer" }
			}
		}
	},
	"additionalProperties": true
}
```

Schema notes:

- The schemas identify themselves by absolute URNs, so the frame schema's `$ref` to the graph schema resolves in any validator that has both loaded, independent of file locations.
- `core` is the graph dump of the subgraph's `Zenith_Graph` and schedule, embedded verbatim ([Z2-N-ARCH-347]); its `frontEnd` is `cpu`.
- `runs[i]` describes core node `i`; `builderNode` is `null` for a generated stage fence.
- `transients.placements` lists every sized transient of the subgraph with its offset in the per-instance block (Z2-R-GRAPH-053).
