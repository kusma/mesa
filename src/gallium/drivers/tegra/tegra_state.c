#include <stdio.h>

#include "util/u_bitcast.h"
#include "util/u_helpers.h"
#include "util/u_memory.h"
#include "util/u_format.h"

#include "tegra_context.h"
#include "tegra_resource.h"
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
   struct tegra_context *context = tegra_context(pcontext);

   assert(index == 0);
   assert(!buffer || buffer->user_buffer);

   util_copy_constant_buffer(&context->constant_buffer[shader], buffer);
}

static void
tegra_set_framebuffer_state(struct pipe_context *pcontext,
                            const struct pipe_framebuffer_state *framebuffer)
{
   struct tegra_context *context = tegra_context(pcontext);
   struct pipe_framebuffer_state *cso = &context->framebuffer.base;
   unsigned int i;
   uint32_t mask = 0;

   if (framebuffer->zsbuf) {
      struct tegra_resource *res = tegra_resource(framebuffer->zsbuf->texture);
      uint32_t rt_params;

      rt_params  = TGR3D_VAL(RT_PARAMS, FORMAT, res->format);
      rt_params |= TGR3D_VAL(RT_PARAMS, PITCH, res->pitch);
      rt_params |= TGR3D_BOOL(RT_PARAMS, TILED, res->tiled);

      context->framebuffer.rt_params[0] = rt_params;
      context->framebuffer.bos[0] = res->bo;
      mask |= 1;
   } else {
      context->framebuffer.rt_params[0] = 0;
      context->framebuffer.bos[0] = NULL;
   }

   pipe_surface_reference(&context->framebuffer.base.zsbuf,
                framebuffer->zsbuf);

   for (i = 0; i < framebuffer->nr_cbufs; i++) {
      struct pipe_surface *ref = framebuffer->cbufs[i];
      struct tegra_resource *res = tegra_resource(ref->texture);
      uint32_t rt_params;

      rt_params  = TGR3D_VAL(RT_PARAMS, FORMAT, res->format);
      rt_params |= TGR3D_VAL(RT_PARAMS, PITCH, res->pitch);
      rt_params |= TGR3D_BOOL(RT_PARAMS, TILED, res->tiled);

      context->framebuffer.rt_params[1 + i] = rt_params;
      context->framebuffer.bos[1 + i] = res->bo;
      mask |= 1 << (1 + i);

      pipe_surface_reference(&cso->cbufs[i], ref);
   }

   for (; i < cso->nr_cbufs; i++)
      pipe_surface_reference(&cso->cbufs[i], NULL);

   context->framebuffer.num_rts = 1 + i;
   context->framebuffer.mask = mask;

   context->framebuffer.base.width = framebuffer->width;
   context->framebuffer.base.height = framebuffer->height;
   context->framebuffer.base.nr_cbufs = framebuffer->nr_cbufs;

   /* prepare the scissor-registers for the non-scissor case */
   context->no_scissor[0]  = TGR3D_VAL(SCISSOR_HORIZ, MIN, 0);
   context->no_scissor[0] |= TGR3D_VAL(SCISSOR_HORIZ, MAX, framebuffer->width);
   context->no_scissor[1]  = TGR3D_VAL(SCISSOR_VERT, MIN, 0);
   context->no_scissor[1] |= TGR3D_VAL(SCISSOR_VERT, MAX, framebuffer->height);
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
   struct tegra_context *context = tegra_context(pcontext);
   static const float zeps = powf(2.0f, -21);

   assert(num_viewports == 1);
   assert(start_slot == 0);

   context->viewport[0] = u_bitcast_f2u(viewports[0].translate[0] * 16.0f);
   context->viewport[1] = u_bitcast_f2u(viewports[0].translate[1] * 16.0f);
   context->viewport[2] = u_bitcast_f2u(viewports[0].translate[2] - zeps);
   context->viewport[3] = u_bitcast_f2u(viewports[0].scale[0] * 16.0f);
   context->viewport[4] = u_bitcast_f2u(viewports[0].scale[1] * 16.0f);
   context->viewport[5] = u_bitcast_f2u(viewports[0].scale[2] - zeps);

   /**
    * It seems wrong to calculate the guard-band based on the viewport, as
    * fragments can be produced outside it.
    *
    * However, this seems to be what the BLOB does, so let's stick to it
    * for now.
    **/

   assert(viewports[0].scale[0] >= 0.0f);
   context->guardband[0] = u_bitcast_f2u((4096 - viewports[0].scale[0]) / viewports[0].scale[0]);
   context->guardband[1] = u_bitcast_f2u((4096 - fabs(viewports[0].scale[1])) / fabs(viewports[0].scale[1]));
   context->guardband[2] = u_bitcast_f2u((4 - viewports[0].scale[2]) / viewports[0].scale[2] - powf(2.0f, -12));
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

/*
 * Note: this does not include the stride, which needs to be mixed in later
 **/
static uint32_t tegra_attrib_mode(const struct pipe_vertex_element *e)
{
   const struct util_format_description *desc = util_format_description(e->src_format);
   const int c = util_format_get_first_non_void_channel(e->src_format);
   uint32_t type, format;

   assert(!desc->is_mixed);
   assert(c >= 0);

   switch (desc->channel[c].type) {
   case UTIL_FORMAT_TYPE_UNSIGNED:
   case UTIL_FORMAT_TYPE_SIGNED:
      switch (desc->channel[c].size) {
      case 8:
         type = TGR3D_ATTRIB_TYPE_UBYTE;
         break;

      case 16:
         type = TGR3D_ATTRIB_TYPE_USHORT;
         break;

      case 32:
         type = TGR3D_ATTRIB_TYPE_UINT;
         break;

      default:
         unreachable("invalid channel-size");
      }

      if (desc->channel[c].type == UTIL_FORMAT_TYPE_SIGNED)
         type += 2;

      if (desc->channel[c].normalized)
         type += 1;

      break;

   case UTIL_FORMAT_TYPE_FIXED:
      assert(desc->channel[c].size == 32);
      type = TGR3D_ATTRIB_TYPE_FIXED16;
      break;

   case UTIL_FORMAT_TYPE_FLOAT:
      assert(desc->channel[c].size == 32); /* TODO: float16 ? */
      type = TGR3D_ATTRIB_TYPE_FLOAT32;
      break;
   }

   format  = TGR3D_VAL(ATTRIB_MODE, TYPE, type);
   format |= TGR3D_VAL(ATTRIB_MODE, SIZE, desc->nr_channels);
   return format;
}

static void *
tegra_create_vertex_state(struct pipe_context *pcontext, unsigned int count,
                          const struct pipe_vertex_element *elements)
{
   unsigned int i;
   struct tegra_vertex_state *vtx = CALLOC_STRUCT(tegra_vertex_state);
   if (!vtx)
      return NULL;

   for (i = 0; i < count; ++i) {
      const struct pipe_vertex_element *src = elements + i;
      struct tegra_vertex_element *dst = vtx->elements + i;
      dst->attrib = tegra_attrib_mode(src);
      dst->buffer_index = src->vertex_buffer_index;
      dst->offset = src->src_offset;
   }

   vtx->num_elements = count;

   return vtx;
}

static void
tegra_bind_vertex_state(struct pipe_context *pcontext, void *so)
{
   tegra_context(pcontext)->vs = so;
}

static void
tegra_delete_vertex_state(struct pipe_context *pcontext, void *so)
{
   FREE(so);
}

static void
emit_attribs(struct tegra_context *context)
{
   unsigned int i;
   struct tegra_stream *stream = &context->gr3d->stream;

   assert(context->vs);

   for (i = 0; i < context->vs->num_elements; ++i) {
      const struct pipe_vertex_buffer *vb;
      const struct tegra_vertex_element *e = context->vs->elements + i;
      const struct tegra_resource *r;

      assert(e->buffer_index < context->vbs.count);
      vb = context->vbs.vb + e->buffer_index;
      assert(!vb->is_user_buffer);
      r = tegra_resource(vb->buffer.resource);

      uint32_t attrib = e->attrib;
      attrib |= TGR3D_VAL(ATTRIB_MODE, STRIDE, vb->stride);

      tegra_stream_push(stream, host1x_opcode_incr(TGR3D_ATTRIB_PTR(i), 2));
      tegra_stream_push_reloc(stream, r->bo, vb->buffer_offset + e->offset);
      tegra_stream_push(stream, attrib);
   }
}

static void emit_render_targets(struct tegra_context *context)
{
   unsigned int i;
   struct tegra_stream *stream = &context->gr3d->stream;
   const struct tegra_framebuffer_state *fb = &context->framebuffer;

   tegra_stream_push(stream, host1x_opcode_incr(TGR3D_RT_PARAMS(0), fb->num_rts));
   for (i = 0; i < fb->num_rts; ++i) {
      uint32_t rt_params = fb->rt_params[i];
      /* TODO: setup dither */
      /* rt_params |= TGR3D_BOOL(RT_PARAMS, DITHER_ENABLE, enable_dither); */
      tegra_stream_push(stream, rt_params);
   }

   tegra_stream_push(stream, host1x_opcode_incr(TGR3D_RT_PTR(0), fb->num_rts));
   for (i = 0; i < fb->num_rts; ++i)
      tegra_stream_push_reloc(stream, fb->bos[i], 0);

   tegra_stream_push(stream, host1x_opcode_incr(TGR3D_RT_ENABLE, 1));
   tegra_stream_push(stream, fb->mask);
}

static void push_words(struct tegra_stream *stream, const uint32_t words[], int num_words)
{
   int i;
   for (i = 0; i < num_words; ++i)
      tegra_stream_push(stream, words[i]);
}

static void emit_scissor(struct tegra_context *context)
{
   struct tegra_stream *stream = &context->gr3d->stream;

   tegra_stream_push(stream, host1x_opcode_incr(TGR3D_SCISSOR_HORIZ, 2));
   push_words(stream, context->no_scissor, 2);
}

static void emit_viewport(struct tegra_context *context)
{
   struct tegra_stream *stream = &context->gr3d->stream;

   tegra_stream_push(stream, host1x_opcode_incr(TGR3D_VIEWPORT_X_BIAS, 6));
   push_words(stream, context->viewport, 6);
}

static void emit_guardband(struct tegra_context *context)
{
   struct tegra_stream *stream = &context->gr3d->stream;

   tegra_stream_push(stream, host1x_opcode_incr(TGR3D_GUARDBAND_WIDTH, 3));
   push_words(stream, context->guardband, 3);
}

static void emit_depth_range(struct tegra_context *context)
{
   struct tegra_stream *stream = &context->gr3d->stream;

   tegra_stream_push(stream, host1x_opcode_incr(TGR3D_DEPTH_RANGE_NEAR, 2));
   /* TODO: use the values from the context */
   tegra_stream_push(stream, 0);
   tegra_stream_push(stream, (1 << 16) - 1);
}

static void emit_vs_uniforms(struct tegra_context *context)
{
   struct tegra_stream *stream = &context->gr3d->stream;
   struct pipe_constant_buffer *constbuf = &context->constant_buffer[PIPE_SHADER_VERTEX];
   int len;

   if (constbuf->user_buffer != NULL) {
      assert(constbuf->buffer_size % sizeof(uint32_t) == 0);

      len = constbuf->buffer_size / 4;
      assert(len < 256 * 4);

      tegra_stream_push(stream, host1x_opcode_imm(TGR3D_VP_UPLOAD_CONST_ID, 0));
      tegra_stream_push(stream, host1x_opcode_nonincr(TGR3D_VP_UPLOAD_CONST, len));
      push_words(stream, constbuf->user_buffer, len);
   }
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

   emit_render_targets(context);
   emit_viewport(context);
   emit_guardband(context);
   emit_scissor(context);
   emit_depth_range(context);
   emit_attribs(context);
   emit_vs_uniforms(context);

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
