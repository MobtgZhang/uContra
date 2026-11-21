# Contra NES / Famicom reference and asset sources

Research checked 2026-11-21. This is a reference inventory, not an asset license.

## What the build uses

- `make assets` downloads the eight [NES Maps background plates][maps-bg] and crops them to the playable scenes used by uContra. It reads the fixed US disassembly revision below to derive collision and enemy placement tables and pack the sprite library. No ROM is downloaded. Generated files in `source/files/stages/` and the fetch cache in `.asset-cache/` are not tracked.
- The Snow Field Dogra tank uses the four frames in the [NES Maps sprite gallery][maps-sprites]. The Jungle keeps the US spawn triggers, but moves the first Machine Gun and the later wall-mounted Spread Gun into airborne capsules for this remake.
- The Jungle's fixed gray turret is assembled at native `32 × 32` from the US [super-tile table][bank3] and the Jungle pattern tiles in [ContraC_us `gfx.dat`][turret-patterns]. The left-facing frame matches the corresponding tile in the [NES Maps Jungle rip][jungle-rip] pixel for pixel. The attachment is not used as an asset.
- The Jungle weapon hatch, rising red turret and fortress bomb turrets use the same US super-tile source, including the alternate Jungle patterns loaded at the fortress.
- [MobtgZhang/ContraC_us][contrac-us] is also a behavioral reference for spawn coordinates, rifleman ambushes, Waterfall air cars, Hangar spike walls, base gates, the base electric field and forward input. No C code is imported from that port.
- The local `uMario` is an architectural reference only. `Application` and `MenuManager` still drive the game, while `Map` owns gameplay state and `Player` owns player behavior. The stage loader does not own actors or the main loop.

## Publisher and original references

- [Konami: Contra Anniversary Collection (Japan)][collection] lists the original arcade game, 1988 Japanese Famicom **魂斗羅**, and 1988 North American NES **CONTRA** separately. The collection includes both home versions for comparison and a bonus book with controls, interviews, and development material. Use the matching home version as the gameplay reference, not the arcade game, a sequel, or a bootleg.
- [Konami's North American NES instruction booklet][us-manual] (Digital Press scan): printed pp. 6–7 cover side-view and behind-the-player controls, p. 8 gives the eight-stage order and objectives, and p. 9 identifies pickups. [Konami's Japanese FC instruction booklet][jp-manual] (collector scans): pp. 5–6 identify Bill as 1P and Lance as 2P and describe simultaneous co-op; pp. 7–12 cover controls, including water and 3-D; pp. 13–14 cover pickups; pp. [15–16][jp-areas-1] and [17–18][jp-areas-2] identify all eight areas and their view/scroll modes. These are original Konami documents on third-party hosts, not redistributable art packs.
- The [bibliographic record for Konami's Japanese Contra Victory Manual][jp-guide] (1988, ISBN `4876550123`) identifies a separate strategy guide with stage maps. This is a lead for FC cross-checking, not a verified full-resolution FC atlas; the guide's maps were not inspected here.

## Stage layouts

[NES Maps: labeled atlas][maps] is useful for placement landmarks; its [background-only atlas][maps-bg] isolates scenery. The sizes below are **background-only atlas image sizes**, not game-world bounds or scroll limits. The sequence and modes are cross-checked against both Konami booklets and the [US level headers][bank2].

1. **Jungle** — horizontal; `3840 × 240`. Check the bridge gaps, water, weapon boxes, and final defense wall independently of the backdrop.
2. **Base 1** — forward-facing pseudo-3D; `2048 × 240` montage. Successive gated rooms and a boss chamber, not one continuous side-scrolling world.
3. **Waterfall** (FC booklet: **Water Fall**) — upward scroll; `256 × 2656`. The labeled image is `288 × 2656`; do not mistake its extra width for playable terrain.
4. **Base 2** — forward-facing pseudo-3D; `2816 × 240` montage. Shares the base view with stage 2, but has its own room sequence and boss.
5. **Snow Field** — horizontal; `5616 × 240`. Check cliff and tank encounters; treat falling snow as a version-specific render layer, not collision.
6. **Energy Zone** — horizontal; `3712 × 240`. Fire jets are timed hazards, not permanent solid tiles.
7. **Hangar Zone** (FC booklet: **Hangar**) — horizontal; `4352 × 240`. Moving carts and claws need their behavior checked separately from the map.
8. **Alien's Lair** — horizontal; `3328 × 240`. Check the lair actors and final heart separately from background animation.

### Map data versus map art

- [Vermiceli's annotated US ROM disassembly][disasm] is direct evidence of the **US** game's data and logic, not Konami source or a Japanese ROM decoder. At revision `687d651c021fd7020f10d05b970ccb62663c94bd`, [`bank2.asm`][bank2] contains level headers, RLE screen layouts, fixed enemy lists, and soldier generation; [`bank3.asm`][bank3] contains supertile definitions and palette assignments; [`bank7.asm`][bank7] contains camera/nametable and collision logic. The US disassembly README identifies the expected headered base ROM with MD5 `7BDAD8B4A7A56A634C9649D20BD3011B`. [Physical cartridge records for the US][cart-us] and [Japan][cart-jp] show different boards (UNROM/iNES mapper 2 versus Konami VRC2/iNES mapper 23); do not apply US ROM offsets to FC.
- [Graphics notes][graphics] document `32 × 32` background supertiles built from `4 × 4` NES pattern tiles. The US horizontal background chunks hold `8 × 7` supertiles; the vertical chunks hold `8 × 8`. Outside, collision is sampled separately at `16 × 16` spacing and distinguishes empty, one-way floor, water, and solid. The bases use room-specific boundaries rather than outdoor tile collision. [Enemy notes][enemies] distinguish fixed screen-indexed placements from generated soldiers. A stitched PNG cannot recover either collision semantics or spawn timing.
- Background crops and original placement data are distinct. uContra treats static atlas scenery separately from interactive bridges, hazards, doors, and boss targets. The Snow Field and Energy Zone atlas crops end before the original boss spawn scroll coordinate; their boss triggers are capped at the end of the cropped maps. Scenery changes and some boss positions are approximations, not a frame-exact reconstruction.
- Player movement and pose data use the US routines in [bank2][bank2] and [bank7][bank7]: the horizontal scroll points are 128 px solo and 176 px with two players, the standing pose is sprite `0f`, and the forward roll uses `08,09,08,09` with both axes flipped for the last two entries. Waterfall rock platforms place the player sprite origin 28 px above the rock origin; that is 16 px in uContra's floor-relative feet coordinates.

## Sprite-sheet provenance and permissions

The [Contra catalog at The Spriters Resource][tsr] has character, enemy, boss, stage, title, and pickup references. Separate scenery from actors and HUD, and check whether a sheet has the regional variants and frames needed. The catalog has dedicated [Stage 1 tiles][stage1-tiles], [Stage 3 tiles][stage3-tiles], and a [shared Base 1/Base 2 visual sheet][base-sheet] uploaded by `mr_jfw`; the shared sheet does not define either base's room order. The disassembly's [labeled sprite library][sprite-library] is an alternative visual index, not a reuse license.

- `source/files/maps/NES - Contra - Level 1.png`: local `3456 × 245` size and embedded Zephiel87 credit agree with the [Stage 1 rip listing][stage1-sheet]. The credit strip requests attribution and includes palette notes; it is not map geometry. This is a strong provenance match, not a verified byte-for-byte match.
- `source/files/maps/NES - Contra - Bill Rizer & Lance Bean.png`: local `416 × 528`; the current [Bill & Lance listing][player-sheet] is `2081 × 1075` and credits `Superjustinbros` and `Tox2564`. The current listing is a visual reference, not confirmed provenance for the local sheet. The comparison should cover outdoor poses, water/prone sprites, and base poses separately.
- `source/files/maps/NES - Contra - Weapon Pickups.png`: local `124 × 32`; the current [Weapon Pickups listing][pickup-sheet] is `176 × 52` and lists uploader `shadowman44`. Exact origin and reuse permission for the local file are unverified.
- [Konami Europe's copyright FAQ][konami-rights] describes qualified tolerance for noncommercial fan use, disclaims granting permissions for graphics, logos, music, and other content through that channel, and reserves takedown rights. [The Spriters Resource terms][tsr-terms] restrict commercial use (including free releases on established marketplaces) and address redistribution and credit. A ripper's credit request, a map download, and this repository's [MIT license](LICENSE) do not establish permission to distribute Konami artwork, ROM data, audio, or third-party sheets. No public redistribution grant was identified for these files or the map rips.
- For a public build, ship original or explicitly licensed replacement assets unless applicable written permissions cover the exact files, license, and distribution channel. Keep a separate record of creator, uploader/ripper, game revision, source URL, and grant for each shipped asset. Existing title art and audio were not audited in this sidecar; do not treat them as cleared by default.

## Gameplay and presentation differences

- **Shared baseline:** Both booklets describe eight areas and simultaneous two-player play. Side-view controls distinguish directional shooting, prone fire, and jumping/drop-down; base rooms use lateral movement, forward movement, and forward shots. Compare the booklet inputs against actual behavior, especially water, pit deaths, and co-op camera limits. Verify M/L/F/S weapon replacements, R rapid fire, B barrier, and the unlettered screen-clear pickup separately rather than making every pickup a new gun. See the [US booklet][us-manual] pp. 4–9 and [JP booklet][jp-manual] pp. 5–18.
- **Transitions:** [Konami's FC description][collection] calls out inter-stage demos. The JP manual p. 8 documents using Start to skip the opening demo. [Speed Demos Archive's regional timing tests][sda] report that Start can also skip FC cutscene and map screens between stages, whereas the US stage/score transition is mandatory. Match the region's sequence and skip behavior rather than treating the difference as text localization alone.
- **Visual differences:** [SDA's player comparison][sda] notes moving Jungle foliage and changed cliff art (1), different Waterfall cliff/water visuals (3), foreground snow in parts of Snow Field (5), and pulsating Alien's Lair scenery (8). [TASVideos' Japanese version record][jp-replays] links region-identified replays for frame checks; speedruns may skip demos and use nonstandard movement. A static NES atlas is not enough to reconstruct FC animations.
- **Behavior not assumed identical:** The [same SDA comparison][sda] reports differences in the two-player placement of the sniper before the first Spread pickup (stage 1) and in Snow Field bombs (stage 5). Treat these as targeted replay checks with both players and matching inputs, not as an exhaustive or byte-level FC spawn comparison. No complete FC tile-collision or spawn dump was verified in this research.

[collection]: https://www.konami.com/games/50th/ac/contra/jp/ja/
[us-manual]: https://www.digitpress.com/library/manuals/nes/Contra.pdf
[jp-manual]: https://gamemanual.midnightmeattrain.com/entry/%E9%AD%82%E6%96%97%E7%BE%85
[jp-areas-1]: https://cdn-ak.f.st-hatena.com/images/fotolife/j/jackallsasaki/20211026/20211026154349.jpg
[jp-areas-2]: https://cdn-ak.f.st-hatena.com/images/fotolife/j/jackallsasaki/20211026/20211026154356.jpg
[jp-guide]: https://www.thevideogamelibrary.org/book/family-computer-konami-special-contra-victory-manual
[maps]: https://www.nesmaps.com/maps/Contra/Contra.html
[maps-bg]: https://www.nesmaps.com/maps/Contra/ContraBG.html
[maps-sprites]: https://www.nesmaps.com/maps/Contra/sprites/ContraSprites.html
[jungle-rip]: https://www.nesmaps.com/maps/Contra/ContraMapStage1.html
[turret-patterns]: https://github.com/MobtgZhang/ContraC_us/blob/ca4f182bea255eed3f61e411487d975a091dafbc/resource/gfx.dat
[disasm]: https://github.com/vermiceli/nes-contra-us/tree/687d651c021fd7020f10d05b970ccb62663c94bd
[bank2]: https://raw.githubusercontent.com/vermiceli/nes-contra-us/687d651c021fd7020f10d05b970ccb62663c94bd/src/bank2.asm
[bank3]: https://github.com/vermiceli/nes-contra-us/blob/687d651c021fd7020f10d05b970ccb62663c94bd/src/bank3.asm
[bank7]: https://raw.githubusercontent.com/vermiceli/nes-contra-us/687d651c021fd7020f10d05b970ccb62663c94bd/src/bank7.asm
[cart-us]: https://nescartdb.com/profile/view/52/contra
[cart-jp]: https://nescartdb.com/profile/view/3986/contra
[graphics]: https://github.com/vermiceli/nes-contra-us/blob/687d651c021fd7020f10d05b970ccb62663c94bd/docs/Graphics%20Documentation.md
[enemies]: https://github.com/vermiceli/nes-contra-us/blob/687d651c021fd7020f10d05b970ccb62663c94bd/docs/Enemy%20Routines.md
[sprite-library]: https://github.com/vermiceli/nes-contra-us/blob/687d651c021fd7020f10d05b970ccb62663c94bd/docs/sprite_library/README.md
[tsr]: https://www.spriters-resource.com/nes/contra/
[stage1-tiles]: https://www.spriters-resource.com/nes/contra/asset/249839/
[stage3-tiles]: https://www.spriters-resource.com/nes/contra/asset/250128/
[base-sheet]: https://www.spriters-resource.com/nes/contra/asset/249847/
[stage1-sheet]: https://www.spriters-resource.com/nes/contra/asset/7680/
[player-sheet]: https://www.spriters-resource.com/nes/contra/asset/90325/
[pickup-sheet]: https://www.spriters-resource.com/nes/contra/asset/125246/
[tsr-terms]: https://www.spriters-resource.com/page/tou/
[konami-rights]: https://eu-support.konami.com/hc/en-gb/articles/9648771731479-Copyrights-Career-Opportunities-Goodies
[jp-replays]: https://tasvideos.org/Games/18/Versions/View/169
[sda]: https://kb.speeddemosarchive.com/index.php?title=Contra/Game_Mechanics_and_Glitches&oldid=36566
[contrac-us]: https://github.com/MobtgZhang/ContraC_us/tree/cea22b7d084dc4831f4690ec3be015470e7b3c0a
