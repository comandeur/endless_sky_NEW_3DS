#!/usr/bin/env python3
# convert.py
# Copyright (c) 2026 by the Endless Sky 3DS port contributors
#
# Endless Sky is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later version.
#
# Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <https://www.gnu.org/licenses/>.

"""Convert the Endless Sky game data for the Nintendo 3DS.

The output folder is meant to be copied to sdmc:/3ds/endless-sky/ on the SD card.

- data/: the game's text data, plus the 3DS specific overrides of port/data.
- images.idx + images.pak: every sprite, scaled down for the 3DS screens and
  converted to GPU-ready textures (ETC1A4 or RGBA8, LZ11 compressed, made by
  tex3ds), along with collision masks computed by the game's own code
  (masktool.cpp). The game streams the textures from the pack as needed.
- images/font/: the fonts, which are loaded as plain PNG files.
- sounds/: sound effects as mono 22 kHz IMA ADPCM WAV files (8 times smaller in
  memory than the desktop game's format); music is copied as is.

Requirements: Python 3 with Pillow and numpy, tex3ds (devkitPro) and a C++
compiler (for masktool). tools/convert-assets.sh runs all of this in Docker.
"""

import argparse
import hashlib
import io
import pickle
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
from PIL import Image

# PICA200 texture formats (GPU_TEXCOLOR).
GPU_RGBA8 = 0x0
GPU_L8 = 0x7
GPU_ETC1A4 = 0xD

IMAGE_EXTENSIONS = {'.png', '.jpg', '.jpeg'}
BLEND_MODES = '-=^+~'
# Only these sprites get collision masks (see ImageSet.cpp).
MASKED_FOLDERS = {'ship', 'asteroid'}

# How much each kind of image is scaled down. Sprites in the game world are
# drawn at half size and the top screen shows the world at about 2/3 scale, so
# half resolution is plenty for them. Images shown in menus keep more detail.
SCALES = {
	'ship': .5,
	'asteroid': .5,
	'effect': .35,
	'projectile': .5,
	'planet': .5,
	'star': .35,
	'hardpoint': 1.,
	'land': .75,
	'scene': .75,
	'thumbnail': .75,
	'outfit': 1.,
	'icon': 1.,
	'label': 1.,
	'map': 1.,
	'ui': 1.,
	'_menu': .6,
}
DEFAULT_SCALE = .5
# Menu images: better ETC1 quality, and small ones stay uncompressed.
UI_FOLDERS = {'ui', 'icon', 'label', 'map', 'hardpoint', 'outfit', 'thumbnail', '_menu', 'land', 'scene'}
MAX_TEXTURE = 1024
# Textures smaller than this (in texels) use RGBA8, which looks best.
SMALL_TEXTURE = 64 * 64

# Sound conversion.
SOUND_RATE = 22050
ADPCM_BLOCK_ALIGN = 512


def log(*args):
	print(*args, flush=True)


# ---------------------------------------------------------------------------
# Image names (mirrors ImageFileData.cpp)

def parse_image_name(path, root):
	"""Return (name, frame, blend, is2x, is_mask) for an image file."""
	relative = path.relative_to(root)
	name = (relative.parent / relative.stem).as_posix()
	if name == '.':
		name = relative.stem
	is2x = False
	if name.endswith('@2x'):
		is2x = True
		name = name[:-3]
	if name.endswith('@1x'):
		name = name[:-3]
	is_mask = False
	if name.endswith('@sw'):
		is_mask = True
		name = name[:-3]
	frame = 0
	blend = '-'
	start = len(name)
	while start > 0 and name[start - 1].isdigit():
		start -= 1
	if start > 0 and name[start - 1] in BLEND_MODES:
		digits = name[start:]
		frame = int(digits) if digits else 0
		blend = name[start - 1]
		name = name[:start - 1]
		if blend == '~':
			blend = '^'
	return name, frame, blend, is2x, is_mask


def valid_sequence(frames):
	"""Consecutive frames starting at 0, like ImageSet::AddValid()."""
	result = []
	index = 0
	while index in frames:
		result.append(frames[index])
		index += 1
	return result


def find_sprites(images_root):
	sprites = {}
	for path in sorted(images_root.rglob('*')):
		if not path.is_file() or path.suffix.lower() not in IMAGE_EXTENSIONS:
			continue
		name, frame, blend, is2x, is_mask = parse_image_name(path, images_root)
		if is2x or name.startswith('font/'):
			continue
		entry = sprites.setdefault(name, {'frames': {}, 'masks': {}, 'blend': {}})
		(entry['masks'] if is_mask else entry['frames'])[frame] = path
		if not is_mask:
			entry['blend'][frame] = blend
	result = []
	for name, entry in sorted(sprites.items()):
		frames = valid_sequence(entry['frames'])
		if not frames:
			continue
		masks = valid_sequence(entry['masks'])
		if len(masks) > 1 and len(masks) != len(frames):
			masks = masks[:1]
		if len(masks) > len(frames):
			masks = masks[:len(frames)]
		blends = [entry['blend'].get(i, '-') for i in range(len(frames))]
		result.append((name, [str(p) for p in frames], [str(p) for p in masks], blends))
	return result


# ---------------------------------------------------------------------------
# Image processing

def load_premultiplied(path, blend):
	"""Load an image like ImageBuffer::Read(): RGBA with premultiplied alpha.

	Returns a float32 array of shape (height, width, 4)."""
	image = Image.open(path)
	is_png = path.lower().endswith('.png')
	image = image.convert('RGBA')
	data = np.asarray(image).astype(np.uint32)
	if blend != '=' and (is_png or blend == '+'):
		alpha = data[..., 3:4]
		data[..., :3] = (data[..., :3] * alpha) // 255
		if blend == '^':
			data[..., 3] >>= 2
		elif blend == '+':
			data[..., 3] = 0
	return data.astype(np.float32)


def to_argb(data):
	"""float RGBA -> uint32 0xAARRGGBB, the ImageBuffer layout."""
	d = np.clip(np.rint(data), 0, 255).astype(np.uint32)
	return (d[..., 3] << 24) | (d[..., 0] << 16) | (d[..., 1] << 8) | d[..., 2]


def compute_masks(masktool, frames):
	height, width = frames[0].shape[:2]
	payload = io.BytesIO()
	payload.write(struct.pack('<3I', width, height, len(frames)))
	for frame in frames:
		payload.write(to_argb(frame).astype('<u4').tobytes())
	result = subprocess.run([masktool], input=payload.getvalue(), capture_output=True, check=True)
	out = result.stdout
	pos = 0
	masks = []
	for _ in frames:
		(outline_count,) = struct.unpack_from('<H', out, pos)
		pos += 2
		outlines = []
		for _ in range(outline_count):
			(point_count,) = struct.unpack_from('<H', out, pos)
			pos += 2
			points = struct.unpack_from('<%df' % (2 * point_count), out, pos)
			pos += 8 * point_count
			outlines.append(points)
		masks.append(outlines)
	return masks


def next_pot(value):
	result = 8
	while result < value:
		result *= 2
	return result


def choose_size(width, height, scale):
	"""Scaled size and texture size for an image."""
	# Never upscale; respect the texture size limit.
	scale = min(scale, 1., MAX_TEXTURE / max(width, height))
	w = max(1, round(width * scale))
	h = max(1, round(height * scale))
	# If the image is just a bit larger than a power of two, shrink it to fit:
	# that halves its memory use.
	shrink = 1.
	for size in (w, h):
		pot = next_pot(size)
		if size > 8 and size > pot // 2 and size <= pot // 2 * 1.15:
			shrink = min(shrink, (pot // 2) / size)
	if shrink >= .87:
		w = max(1, math.floor(w * shrink))
		h = max(1, math.floor(h * shrink))
	return w, h, next_pot(w), next_pot(h)


def resize(data, w, h):
	"""Resize premultiplied RGBA data channel by channel (so that PIL does not
	touch the alpha, which is zero in additive sprites)."""
	if data.shape[1] == w and data.shape[0] == h:
		return data
	channels = []
	for c in range(data.shape[2]):
		channel = Image.fromarray(data[..., c].astype(np.float32), mode='F')
		channels.append(np.asarray(channel.resize((w, h), Image.LANCZOS)))
	return np.stack(channels, axis=-1)


def tex3ds_encode(tex3ds, image, fmt, quality, workdir, index):
	png = os.path.join(workdir, 'frame%d.png' % index)
	out = os.path.join(workdir, 'frame%d.bin' % index)
	image.save(png)
	subprocess.run([tex3ds, '-f', fmt, '-q', quality, '-z', 'lz11', '-r', '-o', out, png],
		check=True, capture_output=True)
	with open(out, 'rb') as file:
		return file.read()


# Bump this when the conversion changes, to invalidate the cache.
CACHE_VERSION = 1


def cache_key(job):
	name, frame_paths, mask_paths, blends = job[:4]
	digest = hashlib.sha1()
	digest.update(repr((CACHE_VERSION, name, blends, SCALES.get(name.split('/')[0], DEFAULT_SCALE))).encode())
	for path in frame_paths + mask_paths:
		stat = os.stat(path)
		digest.update(repr((path, stat.st_size, stat.st_mtime_ns)).encode())
	return digest.hexdigest()


def convert_sprite(job):
	"""Convert a sprite, or reuse the result of a previous conversion."""
	cache_dir = job[6]
	path = os.path.join(cache_dir, cache_key(job) + '.pkl') if cache_dir else None
	if path and os.path.exists(path):
		try:
			with open(path, 'rb') as file:
				return pickle.load(file)
		except Exception:
			pass
	record = convert_sprite_uncached(job)
	if path:
		with open(path + '.tmp', 'wb') as file:
			pickle.dump(record, file)
		os.replace(path + '.tmp', path)
	return record


def convert_sprite_uncached(job):
	name, frame_paths, mask_paths, blends, tex3ds, masktool = job[:6]
	folder = name.split('/')[0]
	frames = [load_premultiplied(path, blend) for path, blend in zip(frame_paths, blends)]
	height, width = frames[0].shape[:2]
	# All frames must have the same size.
	frames = [f for f in frames if f.shape[:2] == (height, width)]

	area = 0.
	masks = []
	if folder in MASKED_FOLDERS:
		area = float(np.mean([np.count_nonzero(f[..., 3]) for f in frames]))
		masks = compute_masks(masktool, frames)

	w, h, tex_w, tex_h = choose_size(width, height, SCALES.get(folder, DEFAULT_SCALE))
	small = tex_w * tex_h <= SMALL_TEXTURE
	if small:
		fmt, gpu_format = 'rgba8', GPU_RGBA8
	else:
		fmt, gpu_format = 'etc1a4', GPU_ETC1A4
	quality = 'medium'

	blobs = []
	with tempfile.TemporaryDirectory() as workdir:
		for i, frame in enumerate(frames):
			scaled = resize(frame, w, h)
			canvas = np.zeros((tex_h, tex_w, 4), dtype=np.uint8)
			canvas[:h, :w] = np.clip(np.rint(scaled), 0, 255).astype(np.uint8)
			blobs.append(tex3ds_encode(tex3ds, Image.fromarray(canvas, 'RGBA'), fmt, quality, workdir, i))
		mask_blobs = []
		for i, path in enumerate(mask_paths):
			mask = np.asarray(Image.open(path).convert('RGBA')).astype(np.float32)
			if mask.shape[:2] != (height, width):
				mask_blobs = []
				break
			# The swizzle mask is read from the red channel.
			scaled = resize(mask[..., :1], w, h)[..., 0]
			canvas = np.zeros((tex_h, tex_w), dtype=np.uint8)
			canvas[:h, :w] = np.clip(np.rint(scaled), 0, 255).astype(np.uint8)
			mask_blobs.append(tex3ds_encode(tex3ds, Image.fromarray(canvas, 'L'), 'l8', 'medium', workdir,
				1000 + i))

	return {
		'name': name,
		'format': gpu_format,
		'frames': len(blobs),
		'mask_frames': len(mask_blobs),
		'width': float(width),
		'height': float(height),
		'tex_w': tex_w,
		'tex_h': tex_h,
		'u': w / tex_w,
		'v': 1. - h / tex_h,
		'area': area,
		'blobs': blobs + mask_blobs,
		'masks': masks if len(masks) == len(blobs) else [],
	}


def write_record(index, record, offsets, sizes):
	name = record['name'].encode('utf-8')
	index.write(struct.pack('<H', len(name)))
	index.write(name)
	index.write(struct.pack('<BBHHffHHfff', record['format'], 0, record['frames'], record['mask_frames'],
		record['width'], record['height'], record['tex_w'], record['tex_h'], record['u'], record['v'],
		record['area']))
	index.write(struct.pack('<%dI' % len(offsets), *offsets))
	index.write(struct.pack('<%dI' % len(sizes), *sizes))
	index.write(struct.pack('<H', len(record['masks'])))
	for outlines in record['masks']:
		index.write(struct.pack('<H', len(outlines)))
		for points in outlines:
			index.write(struct.pack('<H', len(points) // 2))
			index.write(struct.pack('<%df' % len(points), *points))


def convert_images(source, output, jobs, tex3ds, masktool, only=None):
	images_root = source / 'images'
	sprites = find_sprites(images_root)
	if only:
		sprites = [s for s in sprites if any(s[0].startswith(prefix) for prefix in only)]
	log('Converting %d sprites...' % len(sprites))

	# Fonts are read as plain PNG files.
	font_out = output / 'images' / 'font'
	font_out.mkdir(parents=True, exist_ok=True)
	for font in (images_root / 'font').glob('*.png'):
		shutil.copy2(font, font_out / font.name)

	cache_dir = output.parent / (output.name + '.cache')
	cache_dir.mkdir(parents=True, exist_ok=True)
	work = [(name, frames, masks, blends, tex3ds, masktool, str(cache_dir)) for name, frames, masks, blends in sprites]
	index_path = output / 'images.idx'
	pack_path = output / 'images.pak'
	done = 0
	with open(pack_path, 'wb') as pack, open(str(index_path) + '.tmp', 'wb') as index:
		index.write(b'ESX1')
		index.write(struct.pack('<I', 0))
		count = 0
		with ProcessPoolExecutor(max_workers=jobs) as pool:
			for record in pool.map(convert_sprite, work, chunksize=4):
				done += 1
				if done % 200 == 0:
					log('  %d / %d' % (done, len(work)))
				if not record['frames']:
					continue
				offsets = []
				sizes = []
				for blob in record['blobs']:
					offsets.append(pack.tell())
					sizes.append(len(blob))
					pack.write(blob)
				write_record(index, record, offsets, sizes)
				count += 1
		index.seek(4)
		index.write(struct.pack('<I', count))
	os.replace(str(index_path) + '.tmp', index_path)
	log('Images: %d sprites, pack is %.1f MB.' % (count, pack_path.stat().st_size / 1e6))


# ---------------------------------------------------------------------------
# Sounds

STEP_TABLE = [
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80,
	88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
	598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749,
	3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
	12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767]
INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]


def adpcm_encode(samples):
	"""Encode mono 16-bit samples as IMA ADPCM WAV blocks."""
	per_block = (ADPCM_BLOCK_ALIGN - 4) * 2 + 1
	out = bytearray()
	index = 0
	for start in range(0, len(samples), per_block):
		block = samples[start:start + per_block]
		if len(block) < per_block:
			block = list(block) + [0] * (per_block - len(block))
		predictor = int(block[0])
		out += struct.pack('<hBB', predictor, index, 0)
		nibbles = []
		for sample in block[1:]:
			step = STEP_TABLE[index]
			diff = int(sample) - predictor
			nibble = 0
			if diff < 0:
				nibble = 8
				diff = -diff
			delta = step >> 3
			if diff >= step:
				nibble |= 4
				diff -= step
				delta += step
			if diff >= step >> 1:
				nibble |= 2
				diff -= step >> 1
				delta += step >> 1
			if diff >= step >> 2:
				nibble |= 1
				delta += step >> 2
			predictor += -delta if nibble & 8 else delta
			predictor = max(-32768, min(32767, predictor))
			index = max(0, min(88, index + INDEX_TABLE[nibble]))
			nibbles.append(nibble)
		for i in range(0, len(nibbles), 2):
			out.append(nibbles[i] | (nibbles[i + 1] << 4))
	return bytes(out), per_block


def convert_sound(job):
	source, target = job
	with wave.open(source, 'rb') as wav:
		channels = wav.getnchannels()
		width = wav.getsampwidth()
		rate = wav.getframerate()
		raw = wav.readframes(wav.getnframes())
	if width != 2:
		shutil.copy2(source, target)
		return
	samples = np.frombuffer(raw, dtype='<i2').astype(np.float32)
	if channels > 1:
		samples = samples.reshape(-1, channels).mean(axis=1)
	# Halve the sample rate, averaging pairs of samples (a simple low-pass).
	if rate >= 2 * SOUND_RATE - 100:
		if len(samples) % 2:
			samples = np.append(samples, samples[-1])
		samples = samples.reshape(-1, 2).mean(axis=1)
		rate //= 2
	samples = np.clip(np.rint(samples), -32768, 32767).astype(np.int16)
	data, per_block = adpcm_encode(samples.tolist())
	with open(target, 'wb') as out:
		fmt = struct.pack('<HHIIHHHH', 0x11, 1, rate, rate * ADPCM_BLOCK_ALIGN // per_block,
			ADPCM_BLOCK_ALIGN, 4, 2, per_block)
		fact = struct.pack('<I', len(samples))
		out.write(b'RIFF')
		out.write(struct.pack('<I', 4 + 8 + len(fmt) + 8 + len(fact) + 8 + len(data)))
		out.write(b'WAVE')
		out.write(b'fmt ' + struct.pack('<I', len(fmt)) + fmt)
		out.write(b'fact' + struct.pack('<I', len(fact)) + fact)
		out.write(b'data' + struct.pack('<I', len(data)) + data)


def convert_sounds(source, output, jobs):
	sounds_root = source / 'sounds'
	work = []
	for path in sorted(sounds_root.rglob('*')):
		if not path.is_file():
			continue
		target = output / 'sounds' / path.relative_to(sounds_root)
		target.parent.mkdir(parents=True, exist_ok=True)
		if path.suffix.lower() == '.wav':
			work.append((str(path), str(target)))
		else:
			shutil.copy2(path, target)
	log('Converting %d sounds...' % len(work))
	with ProcessPoolExecutor(max_workers=jobs) as pool:
		list(pool.map(convert_sound, work, chunksize=2))
	size = sum(f.stat().st_size for f in (output / 'sounds').rglob('*') if f.is_file())
	log('Sounds: %.1f MB.' % (size / 1e6))


# ---------------------------------------------------------------------------
# Text data

def copy_data(source, output, port_data):
	if (output / 'data').exists():
		shutil.rmtree(output / 'data')
	shutil.copytree(source / 'data', output / 'data')
	if port_data.exists():
		# Sorted after every upstream folder, so these definitions win.
		shutil.copytree(port_data, output / 'data' / 'zz-3ds', dirs_exist_ok=True)
	for name in ('credits.txt', 'keys.txt', 'license.txt', 'copyright', 'changelog'):
		if (source / name).exists():
			shutil.copy2(source / name, output / name)
	log('Copied the game data.')


def main():
	root = Path(__file__).resolve().parents[2]
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument('--source', type=Path, default=root / 'upstream', help='Endless Sky resources')
	parser.add_argument('--output', type=Path, default=root / 'assets-out' / 'endless-sky')
	parser.add_argument('--jobs', type=int, default=os.cpu_count())
	parser.add_argument('--tex3ds', default=shutil.which('tex3ds') or '/opt/devkitpro/tools/bin/tex3ds')
	parser.add_argument('--masktool', default=None, help='masktool binary (built if missing)')
	parser.add_argument('--skip', action='append', default=[], choices=['data', 'images', 'sounds'])
	parser.add_argument('--only', action='append', help='only convert sprites whose name starts with this')
	args = parser.parse_args()

	output = args.output
	output.mkdir(parents=True, exist_ok=True)

	if 'data' not in args.skip:
		copy_data(args.source, output, root / 'port' / 'data')
	if 'sounds' not in args.skip:
		convert_sounds(args.source, output, args.jobs)
	if 'images' not in args.skip:
		masktool = args.masktool
		if not masktool:
			masktool = str(output.parent / 'masktool')
			src = args.source / 'source'
			log('Building masktool...')
			subprocess.run(['g++', '-std=c++20', '-O2', '-I', str(src), '-o', masktool,
				str(root / 'tools' / 'assets' / 'masktool.cpp'), str(src / 'image' / 'Mask.cpp'),
				str(src / 'Point.cpp'), str(src / 'Angle.cpp'), str(src / 'Random.cpp')], check=True)
		convert_images(args.source, output, args.jobs, args.tex3ds, masktool, args.only)
	log('Done. Copy %s to sdmc:/3ds/endless-sky/ on the SD card.' % output)


if __name__ == '__main__':
	sys.exit(main())
