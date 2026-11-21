#!/usr/bin/env python3
import argparse
from io import BytesIO
from pathlib import Path
import re
import tarfile
from urllib.request import Request, urlopen

from PIL import Image, ImageDraw, ImageSequence


ROOT = Path(__file__).resolve().parents[1]
STAGES = ROOT / 'source/files/stages'
MAPS = 'https://nesmaps.com/maps/Contra/'
REVISION = '687d651c021fd7020f10d05b970ccb62663c94bd'
REFERENCE = f'https://codeload.github.com/vermiceli/nes-contra-us/tar.gz/{REVISION}'
CONTRAC_REVISION = 'ca4f182bea255eed3f61e411487d975a091dafbc'
CONTRAC_REFERENCE = f'https://codeload.github.com/MobtgZhang/ContraC_us/tar.gz/{CONTRAC_REVISION}'
PLAYER_FRAMES = ('02', '03', '04', '05', '06', '0d', '0e', '0f', '08', '09', '0a', '0b', '17', '16',
                 '10', '11', '12', '13', '14', '15', '50', '54', '55', '51', '52', '53',
                 '0c', '19', '1a', '1b', '1c', '1d')
ENTITY_FRAMES = ('27', '28', '3b', '3c', '3d', '3e', '3f', '40', '41', '42', '43', '44', '45', '46', '4d', '4a', '49', '48',
                 '2a', '2b', '2f', '30', '31', '32', '33', '34', '35', '36', '37',
                 '93', '94', '96', '98', '6e', '70', '72', '7c', '7d', '7b', '7a',
                 'ad', 'ae', 'af', 'b3', 'b4', 'b5', 'b6', 'b8', 'b9', 'ba', 'c3', 'bb', 'bc',
                 '1e', '1f', '21', '4e', '5d', '5e', '5f', '60', '61', '62', '68', '69', '6a', '6b',
                 '6c', '79', '99')


def fetch(cache, relative_path, url):
    target = cache / relative_path
    if not target.exists():
        target.parent.mkdir(parents=True, exist_ok=True)
        with urlopen(Request(url, headers={'User-Agent': 'uContra asset preparation'}), timeout=30) as response:
            target.write_bytes(response.read())
    return target


def blocks(source):
    plain = re.sub(r';[^\n]*', '', source)
    parts = re.split(r'^([a-zA-Z_]\w*):\s*$', plain, flags=re.M)
    return dict(zip(parts[1::2], parts[2::2]))


def byte_values(block):
    return [int(token, 16) for line in re.findall(r'\.byte([^\n]*)', block)
            for token in re.findall(r'\$([0-9a-fA-F]{2})\b', line)]


def super_tiles(data, size):
    result = []
    entries = iter(data)
    while len(result) < size:
        value = next(entries)
        if value >= 128:
            result.extend([next(entries)] * (value & 127))
        else:
            result.append(value)
    if len(result) != size:
        raise ValueError('Incorrect super-tile count')
    return result


def outdoor_tiles(stage, bank2, bank3, width, height):
    vertical = stage == 3
    tiles = byte_values(bank3[f'level_{stage}_supertile_data'])
    tiles += byte_values(bank3[f'level_{stage}_nametable_update_supertile_data'])
    screens = re.findall(r'\.addr\s+(\w+)', bank2[f'level_{stage}_supertiles_screen_ptr_table'])
    output = [[None] * (width // 8) for row in range(height // 8)]
    for screen, label in enumerate(screens):
        origin_y = height - (screen + 1) * 240 if vertical else 0
        origin_x = 0 if vertical else screen * 256
        for index, tile in enumerate(super_tiles(byte_values(bank2[label]), 64 if vertical else 56)):
            for inner in range(16):
                cell_x, cell_y = inner % 4, inner // 4
                pos_x = origin_x // 8 + (index % 8) * 4 + cell_x
                local_y = (index // 8) * 4 + cell_y
                if vertical and local_y >= 30:
                    continue
                pos_y = origin_y // 8 + local_y
                if 0 <= pos_y < len(output) and pos_x < len(output[pos_y]):
                    output[pos_y][pos_x] = tiles[tile * 16 + (cell_y & ~1) * 4 + (cell_x & ~1)]
    return output


def enemies(stage, bank, height):
    indoor = stage in (2, 4)
    labels = re.findall(r'\.addr\s+(\w+)', bank[f'level_{stage}_enemy_screen_ptr_tbl'])
    for screen, label in enumerate(labels):
        values = iter(byte_values(bank[label]))
        if indoor:
            next(values)
        for first in values:
            if first == 255:
                break
            second = next(values)
            if indoor:
                yield ((first & 15) * 16 + (8 if second & 64 else 0),
                       (first >> 4) * 16 + (8 if second & 128 else 0), second & 63, next(values), screen, 0)
            else:
                for repeat in range(1 + (second >> 6)):
                    packed = next(values)
                    trigger = screen * (240 if stage == 3 else 256) + (first & 254)
                    if stage == 3:
                        pos_x = packed & 240
                        pos_y = height - 224 - trigger
                    else:
                        pos_x = trigger + 240
                        pos_y = packed & 240
                    yield (pos_x, pos_y, second & 63, packed & 15, 0, trigger)


def make_stage(stage, cache, bank2, bank3):
    indoor = stage in (2, 4)
    image = Image.open(fetch(cache, f'NESMap{stage}BG.png', f'{MAPS}ContraMapStage{stage}BG.png')).convert('RGB')
    if stage == 3:
        image = image.crop((0, 0, 256, 2176))
    else:
        top = 8 if indoor else 16
        image = image.crop((512, top, image.width, top + 224))
    width, height = image.size
    tile_grid = outdoor_tiles(stage, bank2, bank3, width, height) if not indoor else []
    limits = [byte_values(line)[0:3] for line in bank2[f'level_{stage}_header'].splitlines()
              if len(byte_values(line)) == 3][0]
    image.save(STAGES / f'{stage}.png', optimize=True)
    arena_targets = {1: (0x11, width - 94, 158), 3: (0x14, 128, 62), 5: (0x14, width - 109, 116),
                     6: (0x13, width - 142, 138), 7: (0x16, width - 49, 135), 8: (0x16, width - 49, 102)}
    with (STAGES / f'{stage}.txt').open('w', encoding='ascii') as output:
        print(width, height, file=output)
        for row in tile_grid:
            print(''.join('.' if not tile else '=' if tile < limits[0] else '.' if tile < limits[1] else
                          '~' if tile < limits[2] else '#' for tile in row), file=output)
        profiles = re.findall(r'\.addr\s+(\w+)', bank2['soldier_level_attributes_ptr_tbl'])
        attributes = [] if indoor else byte_values(bank2[profiles[stage - 1]])
        print('soldiers', len(attributes), *attributes, file=output)
        print('entities-v2', file=output)
        for pos_x, pos_y, kind, attribute, room, trigger in enemies(stage, bank2, height):
            if stage in arena_targets and kind == arena_targets[stage][0]:
                _, pos_x, pos_y = arena_targets[stage]
            if stage == 1 and kind == 2 and attribute in (1, 3):
                kind, pos_y = 3, 64 if attribute == 1 else 32
            if stage == 1 and kind == 6 and attribute == 0 and pos_x in (1280, 2368):
                attribute = 1
            if 0 <= pos_x < width and 0 <= pos_y < height:
                print(pos_x, pos_y, kind, attribute, room, trigger, file=output)


def jungle_combat_frames(bank3, cache):
    archive = fetch(cache, f'contrac-{CONTRAC_REVISION}.tar.gz', CONTRAC_REFERENCE)
    with tarfile.open(archive, 'r:gz') as reference:
        with reference.extractfile(f'ContraC_us-{CONTRAC_REVISION}/resource/gfx.dat') as entry:
            graphics = entry.read()
    if len(graphics) != 57568:
        raise ValueError('Unexpected ContraC_us graphics data size')
    patterns = (graphics[0x1e80:0x2080] + graphics[0xb400:0xb520] + graphics[0x2080:0x2360] +
                graphics[0xad50:0xb320] + graphics[0x2360:0x2790])
    fortress = patterns[:0xa80] + graphics[0xc310:0xc890]
    tiles = byte_values(bank3['level_1_nametable_update_supertile_data'])
    attributes = byte_values(bank3['level_1_nametable_update_palette_data'])
    palettes = (
        ((0, 148, 0), (128, 208, 16), (64, 44, 0)),
        ((240, 188, 60), (136, 112, 0), (64, 44, 0)),
        ((216, 40, 0), (252, 252, 252), (188, 188, 188)),
        ((36, 24, 140), (60, 188, 252), (252, 252, 252)),
    )
    frame_ids = (0x03, 0x04, 0x05, 0x06, 0x08, 0x0a, 0x0b, 0x0c, 0x0e, 0x0f,
                 0x00, 0x01, 0x02, *range(0x11, 0x19), *range(0x26, 0x2c))
    for frame in frame_ids:
        image = Image.new('RGB', (32, 32))
        patterns_for_frame = fortress if frame >= 0x26 else patterns
        for index, tile in enumerate(tiles[frame * 16:(frame + 1) * 16]):
            quadrant = (index // 8) * 2 + (index % 4) // 2
            palette = ((0, 0, 0),) + palettes[(attributes[frame] >> (2 * quadrant)) & 3]
            offset = tile * 16
            for row in range(8):
                for column in range(8):
                    bit = 7 - column
                    low = (patterns_for_frame[offset + row] >> bit) & 1
                    high = (patterns_for_frame[offset + row + 8] >> bit) & 1
                    color = 0 if tile == 0 else low | (high << 1)
                    image.putpixel((index % 4 * 8 + column, index // 4 * 8 + row), palette[color])
        yield image


def make_sprites(read_reference, bank3, cache):
    names = sorted(set(PLAYER_FRAMES + ENTITY_FRAMES))
    names += [f'{frame}_p2' for frame in PLAYER_FRAMES if frame not in ('0c', '19', '1a', '1b', '1c', '1d')]
    tank_row = (len(names) + 15) // 16 * 64
    atlas = Image.new('RGBA', (16 * 64, tank_row + 80))
    with (STAGES / 'sprites.txt').open('w', encoding='ascii') as output:
        for index, name in enumerate(names):
            filename = 'sprite_4e.gif' if name == '4e' else f'sprite_{name}.png'
            source = f'docs/sprite_library/contra_sprites/{filename}'
            sprite = Image.open(BytesIO(read_reference(source))).convert('RGBA')
            sprite = sprite.crop(sprite.getbbox())
            pos_x, pos_y = index % 16 * 64, index // 16 * 64
            if sprite.width > 64 or sprite.height > 64:
                raise ValueError(f'{filename} exceeds atlas cell size')
            atlas.paste(sprite, (pos_x, pos_y))
            print(name, pos_x, pos_y, sprite.width, sprite.height, file=output)
        with Image.open(fetch(cache, 'Tank.gif', f'{MAPS}sprites/Tank.gif')) as tank:
            if tank.size != (96, 64) or tank.n_frames != 4:
                raise ValueError('Unexpected NES tank sprite sheet')
            for index, frame in enumerate(ImageSequence.Iterator(tank)):
                sprite = Image.new('RGBA', (98, 66), (0, 0, 0, 255))
                sprite.paste(frame.convert('RGBA'), (1, 1))
                ImageDraw.floodfill(sprite, (0, 0), (0, 0, 0, 0))
                atlas.paste(sprite.crop((1, 1, 97, 65)), (96 * index, tank_row))
                print(f'{0xe0 + index:02x}', 96 * index, tank_row, 96, 64, file=output)
        for index, sprite in enumerate(jungle_combat_frames(bank3, cache)):
            pos_x = 96 * 4 + 32 * (index % 20)
            pos_y = tank_row + 32 * (index // 20)
            atlas.paste(sprite, (pos_x, pos_y))
            print(f'{0xe4 + index:02x}', pos_x, pos_y, 32, 32, file=output)
            if 10 <= index < 13:
                hatch_x = 32 * (index - 10)
                atlas.paste(sprite.crop((8, 8, 24, 24)), (hatch_x, tank_row + 64))
                print(f'{0xd8 + index - 10:02x}', hatch_x, tank_row + 64, 16, 16, file=output)
    atlas.save(STAGES / 'sprites.png', optimize=True)


def main():
    parser = argparse.ArgumentParser(description='Prepare the optional NES art and stage layout pack; no ROM is downloaded.')
    parser.add_argument('--cache', type=Path, default=ROOT / '.asset-cache')
    args = parser.parse_args()
    STAGES.mkdir(parents=True, exist_ok=True)
    with tarfile.open(fetch(args.cache, f'reference-{REVISION}.tar.gz', REFERENCE), 'r:gz') as reference:
        def read_reference(path):
            with reference.extractfile(f'nes-contra-us-{REVISION}/{path}') as entry:
                return entry.read()

        bank2, bank3 = (blocks(read_reference(f'src/bank{bank}.asm').decode('utf-8')) for bank in (2, 3))
        for stage in range(1, 9):
            make_stage(stage, args.cache, bank2, bank3)
        make_sprites(read_reference, bank3, args.cache)


if __name__ == '__main__':
    main()
