import generator
import argparse as ap
import writer
import sys
import struct

def parse_args() -> tuple[str, list[str], str]:
    parser = ap.ArgumentParser(description="Reflective types generator", add_help=True)
    parser.add_argument('-o', '--output', help="Output .crefl file", type=str, default="generated_types.crefl")
    parser.add_argument('-i', '--input', help="Input header (.h) files with types to reflect (order is important)", default=[], nargs='+')
    parser.add_argument('-f', '--ignore_checksum', help="Create a file despite a matching checksum", type=bool, default=False)
    args = parser.parse_args()
    return args.output, args.input, args.ignore_checksum

def get_prev_checksum(output_file: str):
    try:
        with open(output_file, "rb") as f:
            header = f.read(writer.SIZE_HEADER)
    except FileNotFoundError:
        return "0"

    (checksum,) = struct.unpack_from("<16s", header, writer.CHECKSUM_OFFSET)
    return checksum




if __name__ == "__main__":
    output, header_files, ic = parse_args()
    if len(header_files) == 0:
        print("No input header files given.", file=sys.stderr)
        exit(1)
    old_cs = "0"
    if not ic: old_cs = get_prev_checksum(output)

    gen = generator.CReflEngine(header_files, old_cs)
    cs, result = gen.generate(output)
    if result is None:
        print("Nothing to be done.")
        exit(0)
    # print(result)
    fw = writer.CreflWriter(output, cs, result)
    fw.write()