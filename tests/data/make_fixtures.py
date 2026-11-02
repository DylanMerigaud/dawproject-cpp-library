#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Regenerate the zip fixtures of tests/data with Python's zipfile module.

    python3 tests/data/make_fixtures.py

python-made.dawproject  a project written by another zip implementation (deflate + stored)
zip-slip.dawproject     an archive with an entry that climbs out of the extraction directory
no-project.dawproject   an archive without project.xml
"""

import pathlib
import struct
import zipfile

HERE = pathlib.Path(__file__).resolve().parent
# Fixed timestamp so the fixtures are byte-for-byte reproducible.
STAMP = (2026, 1, 1, 0, 0, 0)

PROJECT = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Project version="1.0">
  <Application name="Python zipfile" version="3"/>
  <Transport>
    <Tempo unit="bpm" value="90.000000" id="id0"/>
  </Transport>
  <Structure>
    <Track contentType="audio" id="id1" name="Vocals">
      <Channel role="regular" audioChannels="1" id="id2">
        <Volume unit="linear" value="0.700000" id="id3"/>
      </Channel>
    </Track>
  </Structure>
  <Arrangement id="id4">
    <Lanes timeUnit="seconds" id="id5">
      <Clips track="id1" id="id6">
        <Clip time="1.5" duration="0.01" contentTimeUnit="seconds" name="Take 1">
          <Audio channels="1" sampleRate="8000" duration="0.01" id="id7">
            <File path="audio/take1.wav"/>
          </Audio>
        </Clip>
      </Clips>
    </Lanes>
  </Arrangement>
</Project>
"""

METADATA = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<MetaData>
  <Title>Zipped by Python</Title>
  <Artist>Test Artist</Artist>
  <Year>2026</Year>
</MetaData>
"""


def wav(samples: int = 80, rate: int = 8000) -> bytes:
    """A tiny mono 16 bit PCM WAV file of silence."""
    data = b"\x00\x00" * samples
    header = b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE"
    fmt = b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16)
    return header + fmt + b"data" + struct.pack("<I", len(data)) + data


def write(name: str, entries: list) -> None:
    with zipfile.ZipFile(HERE / name, "w") as archive:
        for entry_name, content, compression in entries:
            info = zipfile.ZipInfo(entry_name, date_time=STAMP)
            info.compress_type = compression
            archive.writestr(info, content)


def main() -> None:
    write(
        "python-made.dawproject",
        [
            ("project.xml", PROJECT, zipfile.ZIP_DEFLATED),
            ("metadata.xml", METADATA, zipfile.ZIP_DEFLATED),
            ("audio/take1.wav", wav(), zipfile.ZIP_STORED),
        ],
    )
    write(
        "zip-slip.dawproject",
        [
            ("project.xml", PROJECT, zipfile.ZIP_DEFLATED),
            ("../escaped.txt", "this file must never be written", zipfile.ZIP_DEFLATED),
        ],
    )
    write(
        "no-project.dawproject",
        [("metadata.xml", METADATA, zipfile.ZIP_DEFLATED)],
    )


if __name__ == "__main__":
    main()
