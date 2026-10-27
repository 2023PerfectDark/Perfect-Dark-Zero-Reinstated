"""Build the small installer delta from locally supplied, verified loaded images.

Inputs are private build artifacts, never release payloads. Use the SDK tool-mode
loader to dump the US Rev 1 image and the original US image with TU3 applied,
including its post-patch XEX header. This script does not download game content.
"""
import argparse
import ctypes as c
import hashlib
import struct
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev1-image', type=Path, required=True)
parser.add_argument('--tu3-image', type=Path, required=True)
parser.add_argument('--tu3-header', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = args.rev1_image.read_bytes()
target_image = args.tu3_image.read_bytes()
header = bytearray(args.tu3_header.read_bytes())
def require_hash(data, expected):
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError('Unexpected input/output image hash')
require_hash(source, 'a3ef12d51bfba1c35127d5f9bb349beaf345de64a7133c25d0ff1023d0c1cb8e')
require_hash(target_image, 'fc87f5b7e5555bf1a8cb103a689bf16d920900a16c07d1d68288722994206dea')
if len(header) != 16384 or header[:4] != b'XEX2':
    raise ValueError('Expected the loaded TU3 XEX header')
found = 0
for i in range(struct.unpack_from('>I', header, 20)[0]):
    key, offset = struct.unpack_from('>II', header, 24 + i * 8)
    if key == 0x3ff:
        struct.pack_into('>IHH', header, offset, 8, 0, 0)
        found += 1
if found != 1:
    raise ValueError('Missing or duplicate XEX format header')
target = bytes(header) + target_image
require_hash(target, 'f6fbe8935aaf164888e3e662344d8426316066684fd5ecec4b6a732599734916')

class Input(c.Structure):
    _fields_ = [('data', c.c_void_p), ('size', c.c_size_t), ('editable', c.c_int)]
class Output(c.Structure):
    _fields_ = [('data', c.c_void_p), ('size', c.c_size_t)]
delta = c.WinDLL('msdelta', use_last_error=True)
delta.CreateDeltaB.argtypes = [c.c_int64, c.c_int64, c.c_int64, Input, Input,
                               Input, Input, Input, c.c_void_p, c.c_uint, c.POINTER(Output)]
delta.ApplyDeltaB.argtypes = [c.c_int64, Input, Input, c.POINTER(Output)]
delta.DeltaFree.argtypes = [c.c_void_p]
a, b = c.create_string_buffer(source), c.create_string_buffer(target)
ai, bi = Input(c.addressof(a), len(source), 0), Input(c.addressof(b), len(target), 0)
out = Output()
if not delta.CreateDeltaB(1, 0, 0, ai, bi, Input(), Input(), Input(), None, 0, c.byref(out)):
    raise OSError(c.get_last_error(), 'CreateDeltaB failed')
try:
    patch = c.string_at(out.data, out.size)
finally:
    delta.DeltaFree(out.data)
buffer = c.create_string_buffer(patch)
out = Output()
if not delta.ApplyDeltaB(0, ai, Input(c.addressof(buffer), len(patch), 0), c.byref(out)):
    raise OSError(c.get_last_error(), 'ApplyDeltaB failed')
try:
    if c.string_at(out.data, out.size) != target:
        raise ValueError('Delta round-trip failed')
finally:
    delta.DeltaFree(out.data)
if len(patch) > 1024 * 1024:
    raise ValueError('Unexpected patch size')
args.output.write_bytes(patch)
print(f'Verified delta: {len(patch)} bytes; SHA256 {hashlib.sha256(patch).hexdigest()}')
