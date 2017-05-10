#include <stdio.h>

#include "pipe/p_state.h"

#include "tegra_common.h"
#include "tegra_context.h"
#include "tegra_draw.h"
#include "tegra_state.h"


static void
tegra_draw_vbo(struct pipe_context *pcontext,
               const struct pipe_draw_info *info)
{
   int err;
   struct tegra_context *context = tegra_context(pcontext);
   struct tegra_channel *gr3d = context->gr3d;

   err = tegra_stream_begin(&gr3d->stream);
   if (err < 0) {
      fprintf(stderr, "tegra_stream_begin() failed: %d\n", err);
      return;
   }

   tegra_stream_push_setclass(&gr3d->stream, HOST1X_CLASS_GR3D);

   tegra_emit_state(&gr3d->stream);

   /* TODO: draw */

   tegra_stream_end(&gr3d->stream);

   tegra_stream_flush(&gr3d->stream);
}

void
tegra_context_draw_init(struct pipe_context *pcontext)
{
   pcontext->draw_vbo = tegra_draw_vbo;
}
