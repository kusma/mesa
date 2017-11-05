#ifndef TEGRA_PROGRAM_H
#define TEGRA_PROGRAM_H

#include "pipe/p_context.h"
#include "pipe/p_state.h"

#include "vpe_ir.h"

struct tgsi_parse_context;

struct tegra_shader_blob {
   uint32_t *commands;
   int num_commands;
};

struct tegra_shader_state {
   struct pipe_shader_state base;

};

void
tegra_context_program_init(struct pipe_context *pcontext);

#endif
