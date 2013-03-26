#ifndef TEGRA_STATE_H
#define TEGRA_STATE_H

#include "pipe/p_context.h"
#include "pipe/p_state.h"

struct tegra_blend_state {
   struct pipe_blend_state base;
};

struct tegra_sampler_state {
   struct pipe_sampler_state base;
};

struct tegra_rasterizer_state {
   struct pipe_rasterizer_state base;
};

struct tegra_zsa_state {
   struct pipe_depth_stencil_alpha_state base;
};

void
tegra_context_state_init(struct pipe_context *pcontext);

void
tegra_context_blend_init(struct pipe_context *pcontext);

void
tegra_context_sampler_init(struct pipe_context *pcontext);

void
tegra_context_rasterizer_init(struct pipe_context *pcontext);

void
tegra_context_zsa_init(struct pipe_context *pcontext);

void
tegra_context_vbo_init(struct pipe_context *pcontext);

#endif
