import subprocess as sb
import os

def get_offsets_of_type(dot_h_path: str, type_name: str, fields: list[int]) -> list[int]:
    

    ccode = f'#include "{dot_h_path}"\n#include <stddef.h>\n#include <stdio.h>\n'
    ccode += "int main(){\n"
    
    for field in fields:
        ccode += f'printf("%zu\\n", offsetof({type_name}, {field}));\n'

    ccode += "}\n"

    c_filename = f"__tmp__probe__{type_name}.c"
    elf_filename = f"__{type_name}__prober"

    with open(c_filename, "w") as f:
        f.write(ccode)

    try:
        sb.run(["gcc", c_filename, "-o", elf_filename], check=True)
    except sb.CalledProcessError:
        os.remove(c_filename)
        raise NameError
    result = sb.run([f"./{elf_filename}"], capture_output=True, check=True)

    os.remove(c_filename)
    os.remove(elf_filename)

    return [int(line) for line in result.stdout.splitlines()]


if (__name__ == '__main__'):
    fields = ["id", "username", "some", "final"]
    offsets = get_offsets_of_type("../src/type.h", "data", fields)
    for idx, off in enumerate(offsets):
        print(f"field: {fields[idx]} -> +{off}")