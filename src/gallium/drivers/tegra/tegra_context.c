#include <errno.h>
#include <stdio.h>

#include "util/u_bitcast.h"
#include "util/u_memory.h"
#include "util/u_upload_mgr.h"

#include "tegra_context.h"
#include "tegra_program.h"
#include "tegra_resource.h"
#include "tegra_screen.h"
#include "tegra_state.h"
#include "tegra_surface.h"

#include "host1x01_hardware.h"

static int init(struct tegra_stream *stream)
{
	int err = tegra_stream_begin(stream);
	if (err < 0) {
		fprintf(stderr, "tegra_stream_begin() failed: %d\n", err);
		return err;
	}

	tegra_stream_push_setclass(stream, HOST1X_CLASS_GR3D);

	/* Tegra30 specific stuff */
	tegra_stream_push(stream, host1x_opcode_incr(0x750, 0x0010));
	for (int i = 0; i < 16; i++)
		tegra_stream_push(stream, 0x00000000);

	tegra_stream_push(stream, host1x_opcode_imm(0x907, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x908, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x909, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x90a, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x90b, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb00, 0x0003));
	tegra_stream_push(stream, host1x_opcode_imm(0xb01, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb04, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb06, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb07, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb08, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb09, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb0a, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb0b, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb0c, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb0d, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb0e, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb0f, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb10, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb11, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb12, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xb14, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xe40, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xe41, 0x0000));

	/* Common stuff */
	tegra_stream_push(stream, host1x_opcode_imm(0x00d, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x00e, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x00f, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x010, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x011, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x012, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x013, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x014, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x015, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x120, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x122, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x124, 0x0007));
	tegra_stream_push(stream, host1x_opcode_imm(0x125, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x126, 0x0000));

	tegra_stream_push(stream, host1x_opcode_incr(0x200, 0x0005));
	tegra_stream_push(stream, 0x00000011);
	tegra_stream_push(stream, 0x0000ffff);
	tegra_stream_push(stream, 0x00ff0000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);

	tegra_stream_push(stream, host1x_opcode_imm(0x209, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x20a, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x20c, 0x0003));
	tegra_stream_push(stream, host1x_opcode_imm(0x300, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x301, 0x0000));

	tegra_stream_push(stream, host1x_opcode_incr(0x343, 0x0019));
	tegra_stream_push(stream, 0xb8e00000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000105);
	tegra_stream_push(stream, 0x3f000000);
	tegra_stream_push(stream, 0x3f800000);
	tegra_stream_push(stream, 0x3f800000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x3f000000);
	tegra_stream_push(stream, 0x3f800000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000205);

	tegra_stream_push(stream, host1x_opcode_mask(0x352, 0x001b));
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x41800000);
	tegra_stream_push(stream, 0x41800000);

	tegra_stream_push(stream, host1x_opcode_mask(0x354, 0x0009));
	tegra_stream_push(stream, 0x3efffff0);
	tegra_stream_push(stream, 0x3efffff0);

	tegra_stream_push(stream, host1x_opcode_incr(0x358, 0x0003));
	tegra_stream_push(stream, u_bitcast_f2u(1.0f));
	tegra_stream_push(stream, u_bitcast_f2u(1.0f));
	tegra_stream_push(stream, u_bitcast_f2u(1.0f));

	tegra_stream_push(stream, host1x_opcode_imm(0x363, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x364, 0x0000));

	tegra_stream_push(stream, host1x_opcode_imm(0x400, 0x07ff));
	tegra_stream_push(stream, host1x_opcode_imm(0x401, 0x07ff));

	tegra_stream_push(stream, host1x_opcode_incr(0x402, 0x0012));
	tegra_stream_push(stream, 0x00000040);
	tegra_stream_push(stream, 0x00000310);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x000fffff);
	tegra_stream_push(stream, 0x00000001);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x1fff1fff);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000006);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000008);
	tegra_stream_push(stream, 0x00000048);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);

	tegra_stream_push(stream, host1x_opcode_imm(0x500, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x501, 0x0007));
	tegra_stream_push(stream, host1x_opcode_imm(0x502, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x503, 0x0000));

	tegra_stream_push(stream, host1x_opcode_incr(0x520, 0x0020));
	for (int i = 0; i < 32; i++)
		tegra_stream_push(stream, 0x00000000);

	tegra_stream_push(stream, host1x_opcode_imm(0x540, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x542, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x543, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x544, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x545, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x546, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x60e, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x702, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x740, 0x0001));
	tegra_stream_push(stream, host1x_opcode_imm(0x741, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x742, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x902, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0x903, 0x0000));

	tegra_stream_push(stream, host1x_opcode_incr(0xa00, 0x000d));
	tegra_stream_push(stream, 0x00000e00);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x000001ff);
	tegra_stream_push(stream, 0x000001ff);
	tegra_stream_push(stream, 0x000001ff);
	tegra_stream_push(stream, 0x00000030);
	tegra_stream_push(stream, 0x00000020);
	tegra_stream_push(stream, 0x000001ff);
	tegra_stream_push(stream, 0x00000100);
	tegra_stream_push(stream, 0x0f0f0f0f);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);
	tegra_stream_push(stream, 0x00000000);

	tegra_stream_push(stream, host1x_opcode_imm(0xe20, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xe21, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xe22, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xe25, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xe26, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xe27, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xe28, 0x0000));
	tegra_stream_push(stream, host1x_opcode_imm(0xe29, 0x0000));

	tegra_stream_end(stream);
	tegra_stream_flush(stream);

	return 0;
}

static int tegra_channel_create(struct tegra_context *context,
				enum drm_tegra_class class,
				struct tegra_channel **channelp)
{
	struct tegra_screen *screen = tegra_screen(context->base.screen);
	int err;
	struct drm_tegra_channel *drm_channel;
	struct tegra_channel *channel;

	err = drm_tegra_channel_open(&drm_channel, screen->drm, class);
	if (err < 0)
		return err;

	channel = CALLOC_STRUCT(tegra_channel);
	if (!channel)
		return -ENOMEM;

	channel->context = context;

	err = tegra_stream_create(screen->drm, drm_channel, &channel->stream, 32768);
	if (err < 0) {
		FREE(channel);
		drm_tegra_channel_close(drm_channel);
		return err;
	}

	*channelp = channel;

	return 0;
}

static void tegra_channel_delete(struct tegra_channel *channel)
{
	tegra_stream_destroy(&channel->stream);
	drm_tegra_channel_close(channel->stream.channel);
	FREE(channel);
}

static void tegra_context_destroy(struct pipe_context *pcontext)
{
	struct tegra_context *context = tegra_context(pcontext);

	slab_destroy_child(&context->transfer_pool);

	tegra_channel_delete(context->gr3d);
	tegra_channel_delete(context->gr2d);
	FREE(context);
}

static void tegra_context_flush(struct pipe_context *pcontext,
				struct pipe_fence_handle **pfence,
				enum pipe_flush_flags flags)
{
	if (pfence) {
		struct tegra_fence *fence;

		fence = CALLOC_STRUCT(tegra_fence);
		if (!fence) {
			perror("calloc failed"); /* TODO: need a better way of handling this */
			return;
		}

		pipe_reference_init(&fence->reference, 1);

		*pfence = (struct pipe_fence_handle *)fence;
	}
}

struct pipe_context *tegra_screen_context_create(struct pipe_screen *pscreen,
						 void *priv, unsigned flags)
{
	struct tegra_screen *screen = tegra_screen(pscreen);
	int err;

	struct tegra_context *context = CALLOC_STRUCT(tegra_context);
	if (!context)
		return NULL;

	context->base.screen = pscreen;
	context->base.priv = priv;

	err = tegra_channel_create(context, DRM_TEGRA_GR2D, &context->gr2d);
	if (err < 0) {
		fprintf(stderr, "tegra_channel_create() failed: %d\n", err);
		return NULL;
	}

	err = tegra_channel_create(context, DRM_TEGRA_GR3D, &context->gr3d);
	if (err < 0) {
		fprintf(stderr, "tegra_channel_create() failed: %d\n", err);
		return NULL;
	}

	init(&context->gr3d->stream);

	slab_create_child(&context->transfer_pool, &screen->transfer_pool);

	context->base.destroy = tegra_context_destroy;
	context->base.flush = tegra_context_flush;
	context->base.stream_uploader = u_upload_create_default(&context->base);
	context->base.const_uploader = context->base.stream_uploader;

	tegra_context_resource_init(&context->base);
	tegra_context_surface_init(&context->base);
	tegra_context_state_init(&context->base);
	tegra_context_blend_init(&context->base);
	tegra_context_sampler_init(&context->base);
	tegra_context_rasterizer_init(&context->base);
	tegra_context_zsa_init(&context->base);
	tegra_context_program_init(&context->base);
	tegra_context_vbo_init(&context->base);

	return &context->base;
}
