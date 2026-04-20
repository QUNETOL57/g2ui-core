#pragma once

#include <stddef.h>

#include "esp_err.h"

#include "guimintlab/guimintlab.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Parse the IR JSON buffer and populate `gml` (which must already be
 * initialised via guimintlab_new). On success, the builder has been driven
 * to create every screen and widget described by the IR, and the initial
 * screen (if any) is active.
 */
esp_err_t gml_loader_load_from_memory(guimintlab_t *gml, const void *json, size_t size);

#ifdef __cplusplus
}
#endif
