#ifndef TEGRA_PROGRAM_H
#define TEGRA_PROGRAM_H

#include "pipe/p_context.h"
#include "pipe/p_state.h"

struct tegra_shader_state {
   struct pipe_shader_state base;

   uint32_t *vp_insts;
   size_t num_vp_insts;
};

void
tegra_context_program_init(struct pipe_context *pcontext);

#endif
