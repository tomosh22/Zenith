"""
Tests for tests/contract.py, the libgraph.json shape validator.

Run with:
    python -m unittest Tools/libgraph/tests/test_contract.py -v
    # or, if module-path invocation fails for your Python/OS combo:
    python -m unittest discover Tools/libgraph/tests -p "test_contract.py" -v

`contract.py` deliberately sits in this same directory and is named so
unittest discovery skips it (see the plan's rationale); this file imports it
directly by inserting this directory onto sys.path, then `import contract`.

Python 3.12 stdlib only.
"""

import copy
import json
import sys
import unittest
from pathlib import Path

# Make this directory importable regardless of CWD, so `import contract`
# resolves to tests/contract.py sitting right beside this file.
_HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(_HERE))

import contract  # noqa: E402

_SAMPLE_PATH = _HERE / "fixtures" / "sample_libgraph.json"


def _load_sample() -> dict:
    with open(_SAMPLE_PATH, "r", encoding="utf-8") as f:
        return json.load(f)


class TestSampleValidatesClean(unittest.TestCase):
    def test_sample_conforms(self):
        doc = _load_sample()
        errors = contract.validate(doc)
        self.assertEqual(errors, [], f"sample_libgraph.json should validate clean, got: {errors}")


class TestTargetedMutations(unittest.TestCase):
    """
    Each of these takes a deep copy of the known-good sample and applies one
    targeted mutation, then asserts validate() reports at least one error.
    """

    def setUp(self):
        self.doc = _load_sample()

    def test_missing_key_at_meta_level(self):
        del self.doc["meta"]["config"]
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "removing meta.config should produce at least one error")
        self.assertTrue(any("meta.config" in e for e in errors), errors)

    def test_missing_meta_symbols(self):
        del self.doc["meta"]["symbols"]
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "removing meta.symbols should produce at least one error")
        self.assertTrue(any("meta.symbols" in e for e in errors), errors)

    def test_meta_symbols_wrong_type(self):
        self.doc["meta"]["symbols"] = "true"
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "meta.symbols must be a bool, not a string")
        self.assertTrue(any("meta.symbols" in e for e in errors), errors)

    def test_missing_health_missing_link_inputs(self):
        del self.doc["meta"]["health"]["missing_link_inputs"]
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "removing meta.health.missing_link_inputs should produce at least one error")
        self.assertTrue(any("missing_link_inputs" in e for e in errors), errors)

    def test_health_missing_link_inputs_wrong_type(self):
        self.doc["meta"]["health"]["missing_link_inputs"] = "GameY: no .obj files"
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "meta.health.missing_link_inputs must be a list of strings, not a bare string")
        self.assertTrue(any("missing_link_inputs" in e for e in errors), errors)

    def test_missing_top_level_key(self):
        del self.doc["libs"]
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "removing the top-level 'libs' key should produce an error")
        self.assertTrue(any("libs" in e for e in errors), errors)

    def test_bad_enum_node_kind(self):
        self.doc["nodes"][0]["kind"] = "bogus-kind"
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "an unrecognised node kind should produce at least one error")
        self.assertTrue(any("kind" in e for e in errors), errors)

    def test_bad_enum_header_role(self):
        self.doc["libs"]["LibA"]["headers"][0]["role"] = "not-a-role"
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "an unrecognised header role should produce at least one error")
        self.assertTrue(any("role" in e for e in errors), errors)

    def test_dangling_edge_endpoint(self):
        self.doc["edges"][0]["dst"] = "NoSuchNode"
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "an edge pointing at a nonexistent node should produce an error")
        self.assertTrue(any("NoSuchNode" in e for e in errors), errors)

    def test_lib_key_not_a_lib_node(self):
        # MidLib is kind=="middleware" in nodes[]; libs{} may only carry
        # kind=="lib" entries.
        self.doc["libs"]["MidLib"] = copy.deepcopy(self.doc["libs"]["LibC"])
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "a libs{} entry for a non-lib node should produce an error")
        self.assertTrue(any("MidLib" in e for e in errors), errors)

    def test_bad_delta_field(self):
        # 'not_a_real_field' is not one of INBOUND_FIELDS.
        self.doc["delta"]["LibA"]["not_a_real_field"] = 3
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "an unknown delta field should produce an error")
        self.assertTrue(any("not_a_real_field" in e for e in errors), errors)

    def test_delta_missing_required_field(self):
        del self.doc["delta"]["LibA"]["entry_headers"]
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "a delta entry missing a required inbound field should produce an error")
        self.assertTrue(any("entry_headers" in e for e in errors), errors)

    def test_game_edge_with_nonempty_classes_is_rejected(self):
        # GameX -> Zenith is a game edge; it must carry classes: [].
        for edge in self.doc["edges"]:
            if edge["src"] == "GameX" and edge["dst"] == "Zenith":
                edge["classes"] = ["declared+used"]
                break
        else:
            self.fail("fixture no longer has a GameX -> Zenith edge to mutate")
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "a non-empty classes[] on a game edge should produce an error")

    def test_edge_with_two_base_classes_is_rejected(self):
        # LibB -> LibA is a real non-game edge; giving it two base classes
        # violates "exactly one base class per non-game edge".
        self.doc["edges"][0]["classes"] = ["declared+used", "declared-unused"]
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "two base classes on one edge should produce an error")

    def test_layer_mismatch_is_rejected(self):
        # MidLib's only layering edge is MidLib -> glmx (declared+used,
        # glmx at layer 0), so MidLib's recomputed layer is 1. Forcing it
        # back to 0 should trip the layer-recompute check.
        for node in self.doc["nodes"]:
            if node["name"] == "MidLib":
                node["layer"] = 0
                break
        else:
            self.fail("fixture no longer has a MidLib node to mutate")
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "a layer that disagrees with the recomputation should produce an error")
        self.assertTrue(any("MidLib" in e and "layer" in e for e in errors), errors)

    def test_in_cycle_node_with_wrong_layer_is_still_rejected_by_recompute(self):
        # LibB and LibC are `in-cycle` (an SCC over declared+used/
        # used-undeclared) but sit on DIFFERENT layers (2 and 1) under the
        # declared-edges-only layering rule — that's expected, not an
        # error (see test_used_undeclared_edge_pointing_up_validates_fine).
        # There is no longer an "in-cycle endpoints share a layer" rule to
        # violate; what still catches a wrong value on an in-cycle node is
        # the ordinary layer-recompute check, same as any other node.
        for node in self.doc["nodes"]:
            if node["name"] == "LibC":
                node["layer"] = 5
                break
        else:
            self.fail("fixture no longer has a LibC node to mutate")
        errors = contract.validate(self.doc)
        self.assertTrue(errors, "LibC's layer disagreeing with the recomputation should produce an error")
        self.assertTrue(any("LibC" in e and "layer" in e for e in errors), errors)

    def test_used_undeclared_edge_pointing_up_validates_fine(self):
        # LibC -> LibB is `used-undeclared` (a real, hidden use), while
        # LibC's layer (1) is <= LibB's layer (2) under the new
        # declared-edges-only layering rule. Per CONTRACT.md, an edge with
        # layer[src] <= layer[dst] is an "up-edge" — exactly the shape this
        # layering rule exists to surface as a DSM violation. The contract
        # does not reject it; only the UI highlights it.
        doc = _load_sample()
        node_layer = {n["name"]: n["layer"] for n in doc["nodes"]}

        up_edge = next(
            (e for e in doc["edges"] if e["src"] == "LibC" and e["dst"] == "LibB"),
            None,
        )
        self.assertIsNotNone(up_edge, "fixture no longer has a LibC -> LibB edge")
        self.assertIn("used-undeclared", up_edge["classes"])
        self.assertLessEqual(
            node_layer["LibC"], node_layer["LibB"],
            "LibC -> LibB should be an up-edge: layer[src] <= layer[dst]",
        )

        errors = contract.validate(doc)
        self.assertEqual(errors, [], f"an up-edge should still validate cleanly, got: {errors}")


class TestExportedConstants(unittest.TestCase):
    """Later agents import these instead of re-typing the string literals."""

    def test_constants_exist_and_are_nonempty(self):
        for name in ("NODE_KINDS", "EDGE_CLASSES", "HEADER_ROLES", "TYPE_MODES",
                     "FILE_TAGS", "INBOUND_FIELDS"):
            value = getattr(contract, name)
            self.assertIsInstance(value, list, f"{name} should be a list")
            self.assertTrue(value, f"{name} should be non-empty")

    def test_edge_classes_is_union_of_base_and_additive(self):
        self.assertEqual(
            set(contract.EDGE_CLASSES),
            set(contract.EDGE_BASE_CLASSES) | set(contract.EDGE_ADDITIVE_CLASSES),
        )


if __name__ == "__main__":
    unittest.main()
