from pathlib import Path
import re
import tomllib

root = Path(__file__).resolve().parents[1]
saved = Path(r'H:\Perfect Dark Zero Xbox360\Perfect Dark Zero Reinstated\settings\keybindings.toml').read_text()
binds = tomllib.loads(saved)
sdk = root / 'compatibility-v16/sdk/src/input/mnk/mnk_input_driver.cpp'
text = sdk.read_text()
for key, value in binds.items():
    if key in ('keybind_walk', 'bind_third_person'):
        continue
    text, count = re.subn(r'(REXCVAR_DEFINE_STRING\(' + key + r', )"[^"]*"', lambda m: m[1] + '"' + value + '"', text)
    assert count == 1, key
sdk.write_text(text)
third = root / 'compatibility-v16/ModdedTU3/src/mod/third_person.cpp'
text = third.read_text()
text, count = re.subn(r'(RegisterBind\("bind_third_person", )"[^"]*"', lambda m: m[1] + '"' + binds['bind_third_person'] + '"', text)
assert count == 1
third.write_text(text)
header = root / 'compatibility-v16/ModdedTU3/src/mod/third_person.h'
header.write_text(header.read_text().replace('default Y', 'default V'))
settings = root / 'compatibility-v16/ModdedTU3/settings'
(settings / 'keybindings.toml').write_text(saved)
for name in ('hardware.toml', 'mapping.toml'):
    path = settings / name
    text = path.read_text()
    for key, value in binds.items():
        text = re.sub(r'^' + key + r'\s*=.*$', lambda m: key + ' = "' + value + '"', text, flags=re.M)
    path.write_text(text)
core = root / 'installer/Core.cs'
text = core.read_text()
anchor = '    Safe.Write(System.IO.Path.Combine(stage,"settings/hardware.toml"),hardware);'
literal = saved.replace('"', '""')
line = '\n    Safe.Write(System.IO.Path.Combine(stage,"settings/keybindings.toml"),@"' + literal + '");'
assert anchor in text and 'settings/keybindings.toml' not in text
core.write_text(text.replace(anchor, anchor + line))
print(f'Applied {len(binds)} saved bindings to runtime defaults and installer.')
