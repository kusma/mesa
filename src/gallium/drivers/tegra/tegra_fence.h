#ifndef TEGRA_FENCE_H
#define TEGRA_FENCE_H

#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"

struct host1x_fence;

struct tegra_fence {
   struct pipe_reference reference;
   struct host1x_fence *fence;
};

static inline struct tegra_fence *
tegra_fence(struct pipe_fence_handle *fence)
{
   return (struct tegra_fence *)fence;
}

#endif
