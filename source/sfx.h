// Tiny sound effects: short square-wave blips on the DS PSG voices (no sample data needed).
#ifndef SFX_H
#include <stdbool.h>
#define SFX_H

void sfx_init(void);
void sfx_tick(void);  // call once per frame
void sfx_set_enabled(bool on);

#endif
