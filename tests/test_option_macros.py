import os
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "crefl_gen"))

import generator
import writer


class OptionMacroTests(unittest.TestCase):
    def test_option_macro_fields_are_reflected_as_options(self):
        original_directory = Path.cwd()
        with tempfile.TemporaryDirectory() as temporary_directory:
            os.chdir(temporary_directory)
            try:
                Path("option_fixture.h").write_text(
                    """\
DECL_OPTION_DYN_ARRAY(u32);
DECL_OPTION_FIXED_ARRAY(u16, 4);

REFLECT(struct) {
    float x;
    float y;
} point_t;
DECL_OPTION(point_t);

REFLECT(struct) {
    OPTION(u32) scalar;
    OPTION(strZ) optional_label;
    OPTION_DYN_ARRAY(u32) dynamic;
    OPTION_FIXED_ARRAY(u16, 4) fixed;
    OPTION(point_t) optional_point;
    strZ label;
    char *legacy_string;
} option_fixture_t;
"""
                )

                checksum, reflected = generator.CReflEngine(
                    ["option_fixture.h"], b""
                ).generate("unused.crefl")
                writer.CreflWriter("fixture.crefl", checksum, reflected).write()
                generated_crefl = Path("fixture.crefl").read_bytes()
            finally:
                os.chdir(original_directory)

        self.assertTrue(generated_crefl.startswith(b"CREFLX64"))
        self.assertEqual(len(reflected["Structs"]), 2)
        reflected_struct = next(
            item for item in reflected["Structs"]
            if item["typename"] == "option_fixture_t"
        )
        self.assertEqual(reflected_struct["typename"], "option_fixture_t")
        fields = {field["name"]: field for field in reflected_struct["fields"]}

        for field_name in ("scalar", "dynamic", "fixed"):
            self.assertEqual(fields[field_name]["kind"], "K_OPTION")

        scalar_type = fields["scalar"]["subtype"]["data"]
        self.assertEqual(scalar_type["kind"], "K_PRIMITIVE")
        self.assertEqual(scalar_type["subtype"]["data"], "P_U32")
        self.assertEqual(scalar_type["offset"], 4)
        self.assertEqual(fields["scalar"]["size"], 8)

        optional_label = fields["optional_label"]
        self.assertEqual(optional_label["subtype"]["data"]["kind"], "K_STRING")
        self.assertEqual(optional_label["subtype"]["data"]["offset"], 8)
        self.assertEqual(optional_label["size"], 16)

        dynamic = fields["dynamic"]["subtype"]["data"]
        self.assertEqual(dynamic["kind"], "K_DYN_ARRAY")
        self.assertEqual(dynamic["offset"], 8)
        self.assertEqual(
            dynamic["subtype"]["data"]["kind"],
            "K_PRIMITIVE",
        )
        self.assertEqual(dynamic["subtype"]["data"]["subtype"]["data"], "P_U32")

        fixed = fields["fixed"]["subtype"]["data"]
        self.assertEqual(fixed["kind"], "K_FIXED_ARRAY")
        self.assertEqual(fixed["offset"], 2)
        self.assertEqual(fixed["size"], 4)
        self.assertEqual(fixed["subtype"]["data"]["kind"], "K_PRIMITIVE")
        self.assertEqual(fixed["subtype"]["data"]["subtype"]["data"], "P_U16")

        optional_point = fields["optional_point"]
        point_payload = optional_point["subtype"]["data"]
        self.assertEqual(point_payload["kind"], "K_STRUCT")
        self.assertEqual(point_payload["offset"], 4)
        self.assertEqual(optional_point["size"], 12)
        self.assertEqual(point_payload["subtype"]["data"]["typename"], "point_t")

        self.assertEqual(fields["label"]["kind"], "K_STRING")
        self.assertEqual(fields["legacy_string"]["kind"], "K_POINTER")


if __name__ == "__main__":
    unittest.main()
