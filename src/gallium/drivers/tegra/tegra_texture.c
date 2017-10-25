#include "tegra_texture.h"

#include "pipe/p_context.h"

#include "util/u_memory.h"

#include <assert.h>

static void *
tegra_create_sampler_state(struct pipe_context *pcontext,
                           const struct pipe_sampler_state *template)
{
   struct tegra_sampler_state *so = CALLOC_STRUCT(tegra_sampler_state);
   if (!so)
      return NULL;

   so->base = *template;

   return so;
}

static void
tegra_bind_sampler_states(struct pipe_context *pcontext,
                          enum pipe_shader_type shader,
                          unsigned start, unsigned nr,
                          void **samplers)
{
   assert(shader == PIPE_SHADER_FRAGMENT);
}

static struct pipe_sampler_view *
tegra_create_sampler_view(struct pipe_context *pcontext,
                          struct pipe_resource *presource,
                          const struct pipe_sampler_view *template)
{
   struct tegra_sampler_view *so = CALLOC_STRUCT(tegra_sampler_view);
   if (!so)
      return NULL;

   so->base = *template;

   return &so->base;
}

static void
tegra_set_sampler_views(struct pipe_context *pcontext,
                        enum pipe_shader_type shader,
                        unsigned start, unsigned nr,
                        struct pipe_sampler_view **views)
{
   assert(shader == PIPE_SHADER_FRAGMENT);
}

void
tegra_context_texture_init(struct pipe_context *pcontext)
{
   pcontext->create_sampler_state = tegra_create_sampler_state;
   pcontext->bind_sampler_states = tegra_bind_sampler_states;
   pcontext->create_sampler_view = tegra_create_sampler_view;
   pcontext->set_sampler_views = tegra_set_sampler_views;
}
