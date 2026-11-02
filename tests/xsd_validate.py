#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate every XML document the unit tests wrote against the DAWproject XSD files.

Usage: xsd_validate.py <directory with .xml files> <directory with Project.xsd and MetaData.xsd>

Exit codes: 0 all valid, 1 at least one invalid document (or none found), 77 lxml is missing
(ctest reports the test as skipped).
"""

import pathlib
import sys

SKIP = 77


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    try:
        from lxml import etree
    except ImportError:
        print("lxml is not installed (pip install lxml): schema validation skipped")
        return SKIP

    out_dir = pathlib.Path(sys.argv[1])
    spec_dir = pathlib.Path(sys.argv[2])
    schemas = {
        "Project": etree.XMLSchema(etree.parse(str(spec_dir / "Project.xsd"))),
        "MetaData": etree.XMLSchema(etree.parse(str(spec_dir / "MetaData.xsd"))),
    }

    documents = sorted(out_dir.glob("*.xml")) if out_dir.is_dir() else []
    if not documents:
        print(f"no XML documents found in {out_dir}: the unit tests did not run")
        return 1

    failures = 0
    counts = {name: 0 for name in schemas}
    for path in documents:
        tree = etree.parse(str(path))
        root = tree.getroot().tag
        schema = schemas.get(root)
        if schema is None:
            print(f"FAIL {path.name}: unexpected root element <{root}>")
            failures += 1
            continue
        counts[root] += 1
        if not schema.validate(tree):
            failures += 1
            print(f"FAIL {path.name}")
            for error in schema.error_log:
                print(f"  line {error.line}: {error.message}")

    summary = ", ".join(f"{count} {name}" for name, count in counts.items())
    print(f"validated {len(documents)} documents ({summary}), {failures} invalid")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
