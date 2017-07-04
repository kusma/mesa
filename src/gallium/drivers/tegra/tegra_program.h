#ifndef TEGRA_PROGRAM_H
#define TEGRA_PROGRAM_H

#include "pipe/p_context.h"
#include "pipe/p_state.h"

struct tegra_vs_state {
   struct pipe_shader_state base;
   uint32_t *vp_insts;
   size_t num_vp_insts;
};

struct tegra_fs_state {
   struct pipe_shader_state base;

   uint32_t *pseq_insts;
   size_t num_pseq_insts;

   uint32_t *mfu_scheds;
   size_t num_mfu_scheds;

   uint32_t *mfu_insts;
   size_t num_mfu_insts;

   uint32_t *tex_insts;
   size_t num_tex_insts;

   uint32_t *alu_scheds;
   size_t num_alu_scheds;

   uint32_t *alu_insts;
   size_t num_alu_insts;

   uint32_t *dw_insts;
   size_t num_dw_insts;
};

void
tegra_context_program_init(struct pipe_context *pcontext);

#endif
