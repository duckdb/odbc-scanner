# -*- coding: utf-8 -*-

from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument("--platform", required=True, help="Platform name, example: 'windows_amd64_mingw'")
parser.add_argument("extension_file", help="Path to the '.duckdb_extension' file")
args = parser.parse_args()

if not args.extension_file.endswith(".duckdb_extension"):
    raise Exception(f"Specified extension file: '{args.extension_file}' must have '.duckdb_extension' suffix")

platform_ascii = args.platform.encode("ascii")
if len(platform_ascii) > 32:
    raise Exception(f"Specified platform name: '{args.platform}' must be no longer than 32 bytes")

platform_pos = -(256 + 64)
signature_pos = -256

with open(args.extension_file, 'r+b') as fd:
    fd.seek(platform_pos, 2)
    old_platform = fd.read(32).decode("ascii").replace("\x00", "")
    platform_ascii += b"\x00" * (32 - len(platform_ascii))
    fd.seek(platform_pos, 2)
    fd.write(platform_ascii)
    fd.seek(signature_pos, 2)
    fd.write(b"\x00" * 256)
    print(f"Platform name is rewritten from '{old_platform}' to '{args.platform}', signature is stripped: '-unsigned' flag is required")
