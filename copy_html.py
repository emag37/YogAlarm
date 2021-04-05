Import("env")

try:
    import gzip
except:
    env.Execute("$PYTHONEXE -m pip install gzip")
    import gzip

def copy_html():
    compressed_bytes = gzip.compress(open("html/form.html", "r").read().encode('utf-8'))

    with open("src/WebForm.hpp", "w") as file:
        file.write(f"#pragma once\n\nconst char gz_compressed_form[{len(compressed_bytes)}] = {{")
        file.write(', '.join('0x{:02x}'.format(b) for b in compressed_bytes))
        file.write("};")


copy_html()
#env.AddPreAction("buildprog", copy_html)

