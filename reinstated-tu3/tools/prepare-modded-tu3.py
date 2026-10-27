"""Stage the TU3 mod port without altering either working installation.

The output is deliberately not launchable until its address/layout audit is
complete. In particular, never silently retain an unresolved TU0 hook address.
"""
from pathlib import Path
import hashlib
import json
import re
import shutil

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'compatibility-v16/PerfectDarkZeroRecomp'
TARGET = ROOT / 'compatibility-v16/ModdedTU3'
REMOVED_HOOKS = [
    '82334C38', '8242C438', '824180E0', '82436FC0',
    '824292C8', '82143D58', '8241C6E0',
]

def stage():
    TARGET.mkdir(exist_ok=True)
    staged = TARGET / 'src-tu0'
    shutil.copytree(SOURCE / 'src', staged, dirs_exist_ok=True)
    path = staged / 'native_campaign.cpp'
    text = path.read_text()
    start = text.index('// Retail disc code destroys the bot manager for every DarkOps mode.')
    end = text.index('REX_EXTERN(__imp__sub_823F4F00);', start)
    removed = text[start:end]
    assert re.findall(r'REX_HOOK_RAW\(sub_([0-9A-F]+)\)', removed) == REMOVED_HOOKS
    text = text[:start] + '// TU3 supplies native DarkOps bots, census, allegiance and round lives.\n' + text[end:]
    for address in REMOVED_HOOKS:
        text = re.sub(r'REX_EXTERN\(__imp__sub_' + address + r'\);\s*', '', text)
    path.write_text(text)

    path = staged / 'native_mp_menu.inl'
    text = path.read_text()
    start = text.index(' if(sceneId.ends_with(u"darkops_standard")){')
    end = text.index(' const bool teams=', start)
    text = text[:start] + ' // Keep TU3\'s authored DarkOps bot controls.\n' + text[end:]
    text = text.replace(' std::array<uint32_t,2> darkOpsBots{};\n', '')
    text = text.replace(' int displayedBotCount=-1;\n', '')
    # Remove the custom Add/Remove handler as well as its visual controls.
    match = re.search(r' for\([^\n]*darkOpsBots[^\n]*\{', text)
    if match:
        start = match.start()
        brace = text.index('{', start)
        depth, end = 1, brace + 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
        text = text[:start] + text[end:]
    assert 'darkOpsBots' not in text, 'Unremoved custom bot-button handler'
    path.write_text(text)

    files = []
    hooks, references, literals = {}, {}, {}
    for path in sorted(staged.rglob('*')):
        if not path.is_file() or path.suffix not in {'.cpp', '.h', '.inl'}:
            continue
        text = path.read_text()
        name = path.relative_to(staged).as_posix()
        files.append({'path': name, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        for address in set(re.findall(r'REX_HOOK\w*\(sub_([0-9A-F]{8})', text)):
            hooks.setdefault(address, []).append(name)
        for address in set(re.findall(r'\bsub_([0-9A-F]{8})\b', text)):
            references.setdefault(address, []).append(name)
        if name in {'blood_decal_pixels.inl', 'surface_intro_animation_data.h'}:
            continue
        for address in set(re.findall(r'\b0x(8[2-3][0-9A-Fa-f]{6})(?![0-9A-Fa-f])', text)):
            literals.setdefault(address.upper(), []).append(name)
    assert not set(REMOVED_HOOKS) & hooks.keys()
    result = {
        'status': 'staged; not linked or installed; address and structure audit required',
        'source': str(SOURCE), 'removed_darkops_hooks': REMOVED_HOOKS,
        'native_darkops_controls': True, 'files': files,
        'hooks': hooks, 'function_references': references, 'address_literals': literals,
        'music': 'Use StockTU3 restored music, never overwrite with TU0 generated code',
    }
    (TARGET / 'port-inventory.json').write_text(json.dumps(result, indent=2))
    print(f'Staged {len(files)} source files; {len(hooks)} hooks, '
          f'{len(references)} function references, {len(literals)} address literals')

if __name__ == '__main__':
    stage()
