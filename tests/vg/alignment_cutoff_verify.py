#!/usr/bin/env python3
"""Parse-time coverage for the count-only GAMP alignment-complexity option."""

import pathlib
import subprocess
import sys
import tempfile


def invoke(program, arguments):
    return subprocess.run([program, "count", *arguments], text=True,
                          capture_output=True, timeout=30)


def invoke_convert(program, arguments):
    return subprocess.run([program, "convert", *arguments], text=True,
                          capture_output=True, timeout=30)


def main():
    program = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="pancollapse-alignment-cutoff-") as tmp:
        output = pathlib.Path(tmp) / "counts"
        common = ["--out-dir", str(output)]
        for value in ("-1", "1.5", "abc", "18446744073709551615",
                      "18446744073709551616"):
            result = invoke(program, ["--max-alignments-per-read", value, *common])
            assert result.returncode != 0, value
            assert "--max-alignments-per-read" in result.stderr, result.stderr
            assert not output.exists(), value

        result = invoke_convert(program, ["--max-alignments-per-read", "0", *common])
        assert result.returncode != 0
        assert "usage:" in result.stderr, result.stderr


if __name__ == "__main__":
    main()
