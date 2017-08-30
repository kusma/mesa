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

static inline struct tegra_fence *
tegra_fence_create(struct host1x_fence *fence)
{
   struct tegra_fence *ret;

   ret = CALLOC_STRUCT(tegra_fence);
   if (!ret)
      return NULL;

   pipe_reference_init(&ret->reference, 1);
   ret->fence = fence;

   return ret;
}

#endif
