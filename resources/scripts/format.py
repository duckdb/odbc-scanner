#!/usr/bin/python

from argparse import ArgumentParser
from glob import glob
from os import path
from subprocess import check_call


parser = ArgumentParser()
parser.add_argument("--check", action="store_true")
args = parser.parse_args()

template = ["clang-format", "-i"]
if args.check:
    template += ["--dry-run", "--Werror"]

scripts_dir = path.dirname(path.realpath(__file__))
project_dir = path.dirname(path.dirname(scripts_dir))

glob_paths = [
    path.join(project_dir, "src/include/**/*.hpp"),
    path.join(project_dir, "src/**/*.c"),
    path.join(project_dir, "src/**/*.cpp"),
    path.join(project_dir, "test/**/*.cpp"),
]

file_list = []
for gp in glob_paths:
    globbed = glob(gp, recursive=True)
    globbed.sort()
    file_list.extend(globbed)

verb = "Checking" if args.check else "Formatting"

for name in file_list:
    print(verb, name)
    check_call(template + [name])
