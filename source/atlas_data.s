@ Embeds the sprite palettes + pixels (made by tools/convert_assets.py) into the ROM.
    .section .rodata
    .balign 32
    .global atlas_blob
atlas_blob:
    .incbin "assets/gen/atlas.bin"
    .balign 4
