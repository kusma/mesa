#include <stdio.h>

#include "util/u_helpers.h"
#include "util/u_memory.h"

#include "tegra_context.h"
#include "tegra_state.h"

#include "tgr_3d.xml.h"

#include "host1x01_hardware.h"

#define unimplemented() printf("TODO: %s()\n", __func__)

#define TGR3D_VAL(reg_name, field_name, value) \
   (((value) << TGR3D_ ## reg_name ## _ ## field_name ## __SHIFT) & \
           TGR3D_ ## reg_name ## _ ## field_name ## __MASK)

#define TGR3D_BOOL(reg_name, field_name, boolean) \
   ((boolean) ? TGR3D_ ## reg_name ## _ ## field_name : 0)

static void
tegra_set_sample_mask(struct pipe_context *pcontext,
                      unsigned int sample_mask)
{
   unimplemented();
}

static void
tegra_set_constant_buffer(struct pipe_context *pcontext,
                          unsigned int shader, unsigned int index,
                          const struct pipe_constant_buffer *buffer)
{
   fprintf(stdout, "> %s(pcontext=%p, shader=%d, index=%d, buffer=%p)\n",
      __func__, pcontext, shader, index, buffer);
   fprintf(stdout, "  buffer:\n");

   if (buffer) {
      fprintf(stdout, "    buffer: %p\n", buffer->buffer);
      fprintf(stdout, "    buffer_offset: %u\n", buffer->buffer_offset);
      fprintf(stdout, "    buffer_size: %u\n", buffer->buffer_size);
      assert(!buffer->user_buffer);
   }

   fprintf(stdout, "< %s()\n", __func__);
}

static void
tegra_set_framebuffer_state(struct pipe_context *pcontext,
                            const struct pipe_framebuffer_state *framebuffer)
{
   struct tegra_context *context = tegra_context(pcontext);
   unsigned int i;

   for (i = 0; i < framebuffer->nr_cbufs; i++) {
      struct pipe_surface *ref = framebuffer->cbufs[i];

      if (i >= framebuffer->nr_cbufs)
         ref = NULL;

      pipe_surface_reference(&context->framebuffer.base.cbufs[i],
                   ref);
   }

   pipe_surface_reference(&context->framebuffer.base.zsbuf,
                framebuffer->zsbuf);

   context->framebuffer.base.width = framebuffer->width;
   context->framebuffer.base.height = framebuffer->height;
   context->framebuffer.base.nr_cbufs = framebuffer->nr_cbufs;
}

static void
tegra_set_polygon_stipple(struct pipe_context *pcontext,
                          const struct pipe_poly_stipple *stipple)
{
   unimplemented();
}

static void
tegra_set_scissor_states(struct pipe_context *pcontext,
                         unsigned start_slot,
                         unsigned num_scissors,
                         const struct pipe_scissor_state * scissors)
{
   assert(num_scissors == 1);
   unimplemented();
}

static void
tegra_set_viewport_states(struct pipe_context *pcontext,
                          unsigned start_slot,
                          unsigned num_viewports,
                          const struct pipe_viewport_state *viewports)
{
   assert(num_viewports == 1);
   unimplemented();
}

static void
tegra_set_vertex_buffers(struct pipe_context *pcontext,
                         unsigned int start, unsigned int count,
                         const struct pipe_vertex_buffer *buffer)
{
   struct tegra_context *context = tegra_context(pcontext);
   struct tegra_vertexbuf_state *vbs = &context->vbs;
   unsigned int i;

   fprintf(stdout, "> %s(pcontext=%p, start=%u, count=%u, buffer=%p)\n",
      __func__, pcontext, start, count, buffer);
   fprintf(stdout, "  buffers:\n");

   for (i = 0; buffer != NULL && i < count; i++) {
      const struct pipe_vertex_buffer *vb = &buffer[i];

      assert(!vb->is_user_buffer);

      fprintf(stdout, "    %u:\n", i);
      fprintf(stdout, "      stride: %u\n", vb->stride);
      fprintf(stdout, "      buffer_offset: %u\n", vb->buffer_offset);
      fprintf(stdout, "      buffer: %p\n", vb->buffer.resource);
   }

   util_set_vertex_buffers_mask(vbs->vb, &vbs->enabled, buffer, start,
                 count);
   vbs->count = util_last_bit(vbs->enabled);

   fprintf(stdout, "< %s()\n", __func__);
}


static void
tegra_set_sampler_views(struct pipe_context *pcontext,
                        unsigned shader,
                        unsigned start_slot, unsigned num_views,
                        struct pipe_sampler_view **views)
{
   unimplemented();
}

void
tegra_context_state_init(struct pipe_context *pcontext)
{
   pcontext->set_sample_mask = tegra_set_sample_mask;
   pcontext->set_constant_buffer = tegra_set_constant_buffer;
   pcontext->set_framebuffer_state = tegra_set_framebuffer_state;
   pcontext->set_polygon_stipple = tegra_set_polygon_stipple;
   pcontext->set_scissor_states = tegra_set_scissor_states;
   pcontext->set_viewport_states = tegra_set_viewport_states;
   pcontext->set_sampler_views = tegra_set_sampler_views;
   pcontext->set_vertex_buffers = tegra_set_vertex_buffers;
}

static void *
tegra_create_blend_state(struct pipe_context *pcontext,
                         const struct pipe_blend_state *template)
{
   struct tegra_blend_state *so = CALLOC_STRUCT(tegra_blend_state);
   if (!so)
      return NULL;

   so->base = *template;

   return so;
}

static void
tegra_bind_blend_state(struct pipe_context *pcontext, void *so)
{
   unimplemented();
}

static void
tegra_delete_blend_state(struct pipe_context *pcontext, void *so)
{
   FREE(so);
}

void
tegra_context_blend_init(struct pipe_context *pcontext)
{
   pcontext->create_blend_state = tegra_create_blend_state;
   pcontext->bind_blend_state = tegra_bind_blend_state;
   pcontext->delete_blend_state = tegra_delete_blend_state;
}

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
                          unsigned shader, unsigned start_slot,
                          unsigned num_samplers, void **samplers)
{
   unimplemented();
}

static void
tegra_delete_sampler_state(struct pipe_context *pcontext, void *so)
{
   FREE(so);
}

void
tegra_context_sampler_init(struct pipe_context *pcontext)
{
   pcontext->create_sampler_state = tegra_create_sampler_state;
   pcontext->bind_sampler_states = tegra_bind_sampler_states;
   pcontext->delete_sampler_state = tegra_delete_sampler_state;
}

static void *
tegra_create_rasterizer_state(struct pipe_context *pcontext,
                              const struct pipe_rasterizer_state *template)
{
   struct tegra_rasterizer_state *so = CALLOC_STRUCT(tegra_rasterizer_state);
   if (!so)
      return NULL;

   so->base = *template;

   return so;
}

static void
tegra_bind_rasterizer_state(struct pipe_context *pcontext, void *so)
{
   unimplemented();
}

static void
tegra_delete_rasterizer_state(struct pipe_context *pcontext, void *so)
{
   FREE(so);
}

void
tegra_context_rasterizer_init(struct pipe_context *pcontext)
{
   pcontext->create_rasterizer_state = tegra_create_rasterizer_state;
   pcontext->bind_rasterizer_state = tegra_bind_rasterizer_state;
   pcontext->delete_rasterizer_state = tegra_delete_rasterizer_state;
}

static void *
tegra_create_zsa_state(struct pipe_context *pcontext,
                       const struct pipe_depth_stencil_alpha_state *template)
{
   struct tegra_zsa_state *so = CALLOC_STRUCT(tegra_zsa_state);
   if (!so)
      return NULL;

   so->base = *template;

   return so;
}

static void
tegra_bind_zsa_state(struct pipe_context *pcontext, void *so)
{
   unimplemented();
}

static void
tegra_delete_zsa_state(struct pipe_context *pcontext, void *so)
{
   FREE(so);
}

void
tegra_context_zsa_init(struct pipe_context *pcontext)
{
   pcontext->create_depth_stencil_alpha_state = tegra_create_zsa_state;
   pcontext->bind_depth_stencil_alpha_state = tegra_bind_zsa_state;
   pcontext->delete_depth_stencil_alpha_state = tegra_delete_zsa_state;
}

static void *
tegra_create_vertex_state(struct pipe_context *pcontext, unsigned int count,
                          const struct pipe_vertex_element *elements)
{
   struct tegra_vertex_state *vtx = CALLOC_STRUCT(tegra_vertex_state);
   if (!vtx)
      return NULL;

   memcpy(vtx->elements, elements, sizeof(*elements) * count);
   vtx->num_elements = count;

   return vtx;
}

static void
tegra_bind_vertex_state(struct pipe_context *pcontext, void *so)
{
   struct tegra_vertex_state *vs = so;
   unsigned int i;

   fprintf(stdout, "> %s(pcontext=%p, so=%p)\n", __func__, pcontext, so);
   if (vs) {
      fprintf(stdout, "  vs:\n");
      fprintf(stdout, "    num_elements: %u\n", vs->num_elements);

      for (i = 0; i < vs->num_elements; i++) {
         struct pipe_vertex_element *element = &vs->elements[i];

         fprintf(stdout, "      %u:\n", i);
         fprintf(stdout, "        src_offset: %u\n", element->src_offset);
         fprintf(stdout, "        instance_divisor: %u\n", element->instance_divisor);
         fprintf(stdout, "        vertex_buffer_index: %u\n", element->vertex_buffer_index);
         fprintf(stdout, "        src_format: %d\n", element->src_format);
      }
   }
   fprintf(stdout, "< %s()\n", __func__);
}

static void
tegra_delete_vertex_state(struct pipe_context *pcontext, void *so)
{
   FREE(so);
}

static void
tegra_draw_vbo(struct pipe_context *pcontext,
               const struct pipe_draw_info *info)
{
   int err;
   struct tegra_context *context = tegra_context(pcontext);
   struct tegra_channel *gr3d = context->gr3d;

   fprintf(stdout, "> %s(pcontext=%p, info=%p)\n", __func__, pcontext,
      info);
   fprintf(stdout, "  info:\n");
   fprintf(stdout, "    index_size: %d\n", info->index_size);
   fprintf(stdout, "    mode: %u\n", info->mode);
   fprintf(stdout, "    start: %u\n", info->start);
   fprintf(stdout, "    count: %u\n", info->count);

   err = tegra_stream_begin(&gr3d->stream);
   if (err < 0) {
      fprintf(stderr, "tegra_stream_begin() failed: %d\n", err);
      return;
   }

   tegra_stream_push_setclass(&gr3d->stream, HOST1X_CLASS_GR3D);

   /* TODO: draw */

   tegra_stream_end(&gr3d->stream);

   tegra_stream_flush(&gr3d->stream);

   fprintf(stdout, "< %s()\n", __func__);
}

void
tegra_context_vbo_init(struct pipe_context *pcontext)
{
   pcontext->create_vertex_elements_state = tegra_create_vertex_state;
   pcontext->bind_vertex_elements_state = tegra_bind_vertex_state;
   pcontext->delete_vertex_elements_state = tegra_delete_vertex_state;
   pcontext->draw_vbo = tegra_draw_vbo;
}
