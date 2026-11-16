// Host-side stand-in for source/atlas_data.s (run from the project root).
__asm__(".section .rodata\n.balign 32\n.global atlas_blob\natlas_blob:\n.incbin \"assets/gen/atlas.bin\"\n.balign 4\n.text\n");
