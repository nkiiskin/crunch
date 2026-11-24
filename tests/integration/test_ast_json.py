#!/usr/bin/env python3
"""Validate that parser output is a complete, valid version-14 JSON AST."""
import json
import pathlib
import subprocess
import sys
import tempfile

lexer = pathlib.Path(sys.argv[1]).resolve()
parser = pathlib.Path(sys.argv[2]).resolve()
fixtures = pathlib.Path(sys.argv[3]).resolve()
valid = ["hello.bc", "arithmetic.bc", "loops.bc", "arrays.bc", "structures.bc", "argv.bc"]

for filename in valid:
    source = fixtures / filename
    lexed = subprocess.run([str(lexer), str(source)], check=True, capture_output=True, text=True)
    parsed = subprocess.run([str(parser)], input=lexed.stdout, check=True, capture_output=True, text=True)
    tree = json.loads(parsed.stdout)
    assert tree["format"] == "CRUNCH-AST", filename
    assert tree["version"] == 14, filename
    assert isinstance(tree["declarations"], list), filename
    for declaration in tree["declarations"]:
        if declaration["kind"] == "function":
            assert isinstance(declaration["noreturn"], bool), filename

loops_source = fixtures / "loops.bc"
lexed = subprocess.run([str(lexer), str(loops_source)], check=True, capture_output=True, text=True)
tree = json.loads(subprocess.run([str(parser)], input=lexed.stdout, check=True, capture_output=True, text=True).stdout)
serialized = json.dumps(tree)
assert '"name": "i"' in serialized
assert '"name": "total"' in serialized
assert '"kind": "for"' in serialized

with tempfile.TemporaryDirectory(prefix="crunch-ast-noreturn-") as td:
    source = pathlib.Path(td) / "noreturn.bc"
    source.write_text("void noreturn stop(int status);\nint main() { return 0; }\n")
    lexed = subprocess.run([str(lexer), str(source)], check=True, capture_output=True, text=True)
    parsed = subprocess.run([str(parser)], input=lexed.stdout, check=True, capture_output=True, text=True)
    declarations = json.loads(parsed.stdout)["declarations"]
    assert declarations[0]["noreturn"] is True
    assert declarations[1]["noreturn"] is False
print("AST JSON integration tests passed")
