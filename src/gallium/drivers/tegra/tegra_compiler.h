#ifndef TEGRA_COMPILER_H
#define TEGRA_COMPILER_H

#include "vpe_ir.h"

struct tgsi_parse_context;

struct tegra_vpe_shader {
   struct vpe_instr instructions[256];
   int num_instructions;
};

void
tegra_tgsi_to_vpe(struct tegra_vpe_shader *vpe, struct tgsi_parse_context *tgsi);

#endif
