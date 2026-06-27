import subprocess

# symbols that gd imports from cocos, at least in 2.2074
with open("gd-cocos-imports.txt") as file:
    gd_symbols = set(x.strip() for x in file.readlines())

# symbols that libExtension.dll imports
with open("gd-extension-imports.txt") as file:
    gd_symbols = gd_symbols.union(set(x.strip() for x in file.readlines()))

# symbols that we export
output = subprocess.run(["C:/Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Tools\\MSVC\\14.50.35717\\bin\\Hostx86\\x86\\dumpbin.exe", "/exports", "build/libcocos2d.dll", "/nologo"], capture_output=True).stdout.decode("utf-8")

# Filter lines that actually contain symbol data (ordinal, hint, RVA, name)
symbols = set()
for line in output.splitlines():
    parts = line.split()
    if len(parts) >= 4 and parts[0].isdigit():
        symbols.add(parts[3])

missing = list(gd_symbols - symbols)

def get_method_name(symbol):
    names = symbol.lstrip('?').split("@@", 1)[0].split("@")
    names.reverse()
    return "::".join(names)

missing.sort(key=get_method_name)
print("\n".join(missing))
print(f"\nMissing methods: {len(missing)}")