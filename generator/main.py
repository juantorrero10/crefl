import generator
import argparse as ap
import sys


def parse_args() -> tuple[str, list[str], str]:
    parser = ap.ArgumentParser(description="Reflective types generator", add_help=True)
    parser.add_argument('-o', '--output', help="Output .crefl file", type=str, default="generated_types.crefl")
    parser.add_argument('-i', '--input', help="Input header (.h) files with types to reflect (order is important)", default=[], nargs='+')
    args = parser.parse_args()
    return args.output, args.input



if __name__ == "__main__":
    output, header_files = parse_args()
    if len(header_files) == 0:
        print("No input header files given.", file=sys.stderr)
    gen = generator.CReflEngine(header_files)
    gen.generate(output)