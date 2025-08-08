from subprocess import check_call
from glob import glob
from argparse import ArgumentParser

parser = ArgumentParser()
parser.add_argument('--check', action='store_true')
args = parser.parse_args()

template = ['clang-format', '-i']
if args.check:
    template += ['--dry-run', '--Werror']

hpp_files = set(glob('src/include/*.hpp'))
c_files = set(glob('src/*.c'))
cpp_files = set(glob('src/*.cpp'))
cpp_subdir_files = set(glob('src/**/*.cpp'))
test_cpp_files = set(glob('test/*.cpp'))

for name in [*hpp_files] + [*c_files] + [*cpp_files] + [*cpp_subdir_files] + [*test_cpp_files]:
    print('Formatting', name)
    check_call(template + [name])
