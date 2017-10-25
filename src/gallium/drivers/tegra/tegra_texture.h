#ifndef TEGRA_TEXTURE_H
#define TEGRA_TEXTURE_H

#include "pipe/p_state.h"

struct tegra_sampler_state {
	struct pipe_sampler_state base;
};

struct tegra_sampler_view {
	struct pipe_sampler_view base;
};

void
tegra_context_texture_init(struct pipe_context *pcontext);

#endif
