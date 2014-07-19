/*
 * Copyright (C) 2014 Marcin Ślusarz <marcin.slusarz@gmail.com>.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef LIBDRM_AVAILABLE
#include "demmt_drm.h"
#include "demmt.h"
#include <drm.h>
#include <nouveau_drm.h>
#include "util.h"

static void dump_drm_nouveau_gem_info(struct drm_nouveau_gem_info *info)
{
	uint32_t domain = info->domain;
	int dprinted = 0;

	mmt_log_cont("handle: %s%3d%s, domain: %s", colors->num, info->handle,
			colors->reset, colors->eval);
	int len = 0;
	if (info->domain & NOUVEAU_GEM_DOMAIN_CPU)
	{
		mmt_log_cont("%s", "CPU");
		domain &= ~NOUVEAU_GEM_DOMAIN_CPU;
		dprinted = 1;
		len += 3;
	}
	if (info->domain & NOUVEAU_GEM_DOMAIN_VRAM)
	{
		mmt_log_cont("%s%s", dprinted ? ", " : "", "VRAM");
		domain &= ~NOUVEAU_GEM_DOMAIN_VRAM;
		len += 4 + (dprinted ? 2 : 0);
		dprinted = 1;
	}
	if (info->domain & NOUVEAU_GEM_DOMAIN_GART)
	{
		mmt_log_cont("%s%s", dprinted ? ", " : "", "GART");
		domain &= ~NOUVEAU_GEM_DOMAIN_GART;
		len += 4 + (dprinted ? 2 : 0);
		dprinted = 1;
	}
	if (info->domain & NOUVEAU_GEM_DOMAIN_MAPPABLE)
	{
		mmt_log_cont("%s%s", dprinted ? ", " : "", "MAPPABLE");
		domain &= ~NOUVEAU_GEM_DOMAIN_MAPPABLE;
		len += 8 + (dprinted ? 2 : 0);
		dprinted = 1;
	}
	if (domain)
	{
		mmt_log_cont("%sUNK%x", dprinted ? ", " : "", domain);
		len += 4 + (dprinted ? 2 : 0);
	}

	while (len++ < 14)
		mmt_log_cont("%s", " ");

	mmt_log_cont("%s (0x%x), size: %s%8ld%s, offset: %s0x%-8lx%s, map_handle: %s0x%-10lx%s, tile_mode: %s0x%02x%s, tile_flags: %s0x%04x%s",
			colors->reset, info->domain, colors->num, info->size, colors->reset,
			colors->mem, info->offset, colors->reset, colors->num, info->map_handle,
			colors->reset, colors->iname, info->tile_mode, colors->reset,
			colors->iname, info->tile_flags, colors->reset);
}

static void dump_drm_nouveau_gem_new(struct drm_nouveau_gem_new *g)
{
	dump_drm_nouveau_gem_info(&g->info);
	mmt_log_cont(", channel_hint: %s%d%s, align: %s0x%06x%s\n", colors->num,
			g->channel_hint, colors->reset, colors->num, g->align, colors->reset);
}

static char *nouveau_param_names[] = {
		"?",
		"?",
		"?",
		"PCI_VENDOR",
		"PCI_DEVICE",
		"BUS_TYPE",
		"FB_PHYSICAL",
		"AGP_PHYSICAL",
		"FB_SIZE",
		"AGP_SIZE",
		"PCI_PHYSICAL",
		"CHIPSET_ID",
		"VM_VRAM_BASE",
		"GRAPH_UNITS",
		"PTIMER_TIME",
		"HAS_BO_USAGE",
		"HAS_PAGEFLIP",
};

int demmt_drm_ioctl_pre(uint8_t dir, uint8_t nr, uint16_t size, struct mmt_buf *buf, void *state)
{
	void *ioctl_data = buf->data;

	if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GETPARAM)
	{
		struct drm_nouveau_getparam *data = ioctl_data;

		if (0) // -> post
			mmt_log("%sDRM_NOUVEAU_GETPARAM%s, param: %s%14s%s (0x%lx)\n",
					colors->rname, colors->reset, colors->eval,
					data->param < ARRAY_SIZE(nouveau_param_names) ? nouveau_param_names[data->param] : "???",
					colors->reset, data->param);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_SETPARAM)
	{
		struct drm_nouveau_setparam *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_SETPARAM%s, param: %s (0x%lx), value: 0x%lx\n",
				colors->rname, colors->reset,
				data->param < ARRAY_SIZE(nouveau_param_names) ? nouveau_param_names[data->param] : "???",
				data->param, data->value);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_CHANNEL_ALLOC)
	{
		struct drm_nouveau_channel_alloc *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_CHANNEL_ALLOC%s pre,  fb_ctxdma: 0x%0x, tt_ctxdma: 0x%0x\n",
				colors->rname, colors->reset, data->fb_ctxdma_handle, data->tt_ctxdma_handle);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_CHANNEL_FREE)
	{
		struct drm_nouveau_channel_free *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_CHANNEL_FREE%s, channel: %d\n", colors->rname,
				colors->reset, data->channel);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GROBJ_ALLOC)
	{
		struct drm_nouveau_grobj_alloc *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GROBJ_ALLOC%s, channel: %d, handle: %s0x%x%s, class: %s0x%x%s\n",
				colors->rname, colors->reset, data->channel, colors->num,
				data->handle, colors->reset, colors->eval, data->class, colors->reset);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_NOTIFIEROBJ_ALLOC)
	{
		struct drm_nouveau_notifierobj_alloc *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_NOTIFIEROBJ_ALLOC%s pre,  channel: %d, handle: 0x%0x, size: %d\n",
				colors->rname, colors->reset, data->channel, data->handle, data->size);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GPUOBJ_FREE)
	{
		struct drm_nouveau_gpuobj_free *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GPUOBJ_FREE%s, channel: %d, handle: 0x%0x\n",
				colors->rname, colors->reset, data->channel, data->handle);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_NEW)
	{
		struct drm_nouveau_gem_new *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GEM_NEW%s pre,  ", colors->rname, colors->reset);
		dump_drm_nouveau_gem_new(data);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_PUSHBUF)
	{
		struct drm_nouveau_gem_pushbuf *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GEM_PUSHBUF%s pre,  channel: %d, nr_buffers: %s%d%s, buffers: 0x%lx, nr_relocs: %s%d%s, relocs: 0x%lx, nr_push: %s%d%s, push: 0x%lx, suffix0: 0x%x, suffix1: 0x%x, vram_available: %lu, gart_available: %lu\n",
				colors->rname, colors->reset, data->channel, colors->num,
				data->nr_buffers, colors->reset, data->buffers, colors->num,
				data->nr_relocs, colors->reset,data->relocs, colors->num,
				data->nr_push, colors->reset, data->push, data->suffix0,
				data->suffix1, data->vram_available, data->gart_available);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_CPU_PREP)
	{
		struct drm_nouveau_gem_cpu_prep *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GEM_CPU_PREP%s, handle: %s%3d%s, flags: %s0x%0x%s\n",
				colors->rname, colors->reset, colors->num, data->handle,
				colors->reset, colors->eval, data->flags, colors->reset);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_CPU_FINI)
	{
		struct drm_nouveau_gem_cpu_fini *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GEM_CPU_FINI%s, handle: %d\n", colors->rname,
				colors->reset, data->handle);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_INFO)
	{
		struct drm_nouveau_gem_info *data = ioctl_data;
		mmt_log("%sDRM_NOUVEAU_GEM_INFO%s pre,  handle: %s%3d%s\n", colors->rname,
				colors->reset, colors->num, data->handle, colors->reset);
	}
	else if (nr == 0x00)
	{
		if (0) // -> post
			mmt_log("%sDRM_IOCTL_VERSION%s\n", colors->rname, colors->reset);
	}
	else if (nr == 0x02)
	{
		if (0) // -> post
			mmt_log("%sDRM_IOCTL_GET_MAGIC%s\n", colors->rname, colors->reset);
	}
	else if (nr == 0x09)
	{
		struct drm_gem_close *data = ioctl_data;
		mmt_log("%sDRM_IOCTL_GEM_CLOSE%s, handle: %s%3d%s\n", colors->rname,
				colors->reset, colors->num, data->handle, colors->reset);
	}
	else if (nr == 0x0b)
	{
		struct drm_gem_open *data = ioctl_data;

		mmt_log("%sDRM_IOCTL_GEM_OPEN%s pre,  name: %s%d%s\n", colors->rname,
				colors->reset, colors->eval, data->name, colors->reset);
	}
	else if (nr == 0x0c)
	{
		struct drm_get_cap *data = ioctl_data;

		if (0) // -> post
			mmt_log("%sDRM_IOCTL_GET_CAP%s, capability: %llu\n", colors->rname,
					colors->reset, data->capability);
	}
	else
	{
		mmt_log("%sunknown drm ioctl%s %x\n", colors->err, colors->reset, nr);
		return 1;
	}

	return 0;
}

int demmt_drm_ioctl_post(uint8_t dir, uint8_t nr, uint16_t size, struct mmt_buf *buf, void *state)
{
	void *ioctl_data = buf->data;

	int i;
	if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GETPARAM)
	{
		struct drm_nouveau_getparam *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GETPARAM%s, param: %s%14s%s (0x%lx), value: %s0x%lx%s\n",
				colors->rname, colors->reset, colors->eval,
				data->param < ARRAY_SIZE(nouveau_param_names) ? nouveau_param_names[data->param] : "???",
				colors->reset, data->param, colors->num, data->value, colors->reset);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_SETPARAM)
	{
		struct drm_nouveau_setparam *data = ioctl_data;

		if (0) // -> pre
			mmt_log("%sDRM_NOUVEAU_SETPARAM%s, param: %s (0x%lx), value: 0x%lx\n",
					colors->rname, colors->reset,
					data->param < ARRAY_SIZE(nouveau_param_names) ? nouveau_param_names[data->param] : "???",
					data->param, data->value);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_CHANNEL_ALLOC)
	{
		struct drm_nouveau_channel_alloc *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_CHANNEL_ALLOC%s post, fb_ctxdma: 0x%0x, tt_ctxdma: 0x%0x, channel: %d, pushbuf_domains: %d, notifier: 0x%0x, nr_subchan: %d",
				colors->rname, colors->reset, data->fb_ctxdma_handle,
				data->tt_ctxdma_handle, data->channel, data->pushbuf_domains,
				data->notifier_handle, data->nr_subchan);
		for (i = 0; i < 8; ++i)
			if (data->subchan[i].handle || data->subchan[i].grclass)
				mmt_log_cont(" subchan[%d]=<h:0x%0x, c:0x%0x>", i, data->subchan[i].handle, data->subchan[i].grclass);
		mmt_log_cont("%s\n", "");
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_CHANNEL_FREE)
	{
		struct drm_nouveau_channel_free *data = ioctl_data;

		if (0) // -> pre
			mmt_log("%sDRM_NOUVEAU_CHANNEL_FREE%s, channel: %d\n", colors->rname,
					colors->reset, data->channel);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GROBJ_ALLOC)
	{
		struct drm_nouveau_grobj_alloc *data = ioctl_data;

		if (0) // -> pre
			mmt_log("%sDRM_NOUVEAU_GROBJ_ALLOC%s, channel: %d, handle: %s0x%x%s, class: %s0x%x%s\n",
					colors->rname, colors->reset, data->channel, colors->num,
					data->handle, colors->reset, colors->eval, data->class,
					colors->reset);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_NOTIFIEROBJ_ALLOC)
	{
		struct drm_nouveau_notifierobj_alloc *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_NOTIFIEROBJ_ALLOC%s post, channel: %d, handle: 0x%0x, size: %d, offset: %d\n",
				colors->rname, colors->reset, data->channel, data->handle,
				data->size, data->offset);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GPUOBJ_FREE)
	{
		struct drm_nouveau_gpuobj_free *data = ioctl_data;

		if (0) // -> pre
			mmt_log("%sDRM_NOUVEAU_GPUOBJ_FREE%s, channel: %d, handle: 0x%0x\n",
					colors->rname, colors->reset, data->channel, data->handle);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_NEW)
	{
		struct drm_nouveau_gem_new *g = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GEM_NEW%s post, ", colors->rname, colors->reset);
		dump_drm_nouveau_gem_new(g);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_PUSHBUF)
	{
		struct drm_nouveau_gem_pushbuf *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GEM_PUSHBUF%s post, channel: %d, nr_buffers: %s%d%s, buffers: 0x%lx, nr_relocs: %s%d%s, relocs: 0x%lx, nr_push: %s%d%s, push: 0x%lx, suffix0: 0x%x, suffix1: 0x%x, vram_available: %lu, gart_available: %lu\n",
				colors->rname, colors->reset, data->channel, colors->num,
				data->nr_buffers, colors->reset, data->buffers, colors->num,
				data->nr_relocs, colors->reset,data->relocs, colors->num,
				data->nr_push, colors->reset, data->push, data->suffix0,
				data->suffix1, data->vram_available, data->gart_available);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_CPU_PREP)
	{
		struct drm_nouveau_gem_cpu_prep *data = ioctl_data;

		if (0) // -> pre
			mmt_log("%sDRM_NOUVEAU_GEM_CPU_PREP%s, handle: %d, flags: 0x%0x\n",
					colors->rname, colors->reset, data->handle, data->flags);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_CPU_FINI)
	{
		struct drm_nouveau_gem_cpu_fini *data = ioctl_data;

		if (0) // -> pre
			mmt_log("%sDRM_NOUVEAU_GEM_CPU_FINI%s, handle: %d\n", colors->rname,
					colors->reset, data->handle);
	}
	else if (nr == DRM_COMMAND_BASE + DRM_NOUVEAU_GEM_INFO)
	{
		struct drm_nouveau_gem_info *data = ioctl_data;

		mmt_log("%sDRM_NOUVEAU_GEM_INFO%s post, ", colors->rname, colors->reset);
		dump_drm_nouveau_gem_info(data);
		mmt_log_cont("%s\n", "");
	}
	else if (nr == 0x00)
	{
		struct drm_version *data = ioctl_data;

		mmt_log("%sDRM_IOCTL_VERSION%s, version: %s%d.%d.%d%s, name_addr: %p, name_len: %zd, date_addr: %p, date_len: %zd, desc_addr: %p, desc_len: %zd\n",
				colors->rname, colors->reset, colors->eval, data->version_major,
				data->version_minor, data->version_patchlevel, colors->reset,
				data->name, data->name_len, data->date, data->date_len,
				data->desc, data->desc_len);
	}
	else if (nr == 0x02)
	{
		struct drm_auth *data = ioctl_data;

		mmt_log("%sDRM_IOCTL_GET_MAGIC%s, magic: 0x%08x\n", colors->rname,
				colors->reset, data->magic);
	}
	else if (nr == 0x09)
	{
		struct drm_gem_close *data = ioctl_data;

		if (0) // -> pre
			mmt_log("%sDRM_IOCTL_GEM_CLOSE%s, handle: %d\n", colors->rname,
					colors->reset, data->handle);
	}
	else if (nr == 0x0b)
	{
		struct drm_gem_open *data = ioctl_data;

		mmt_log("%sDRM_IOCTL_GEM_OPEN%s post, name: %s%d%s, handle: %s%d%s, size: %llu\n",
				colors->rname, colors->reset, colors->eval, data->name, colors->reset,
				colors->num, data->handle, colors->reset, data->size);
	}
	else if (nr == 0x0c)
	{
		struct drm_get_cap *data = ioctl_data;

		mmt_log("%sDRM_IOCTL_GET_CAP%s, capability: %s%llu%s, value: %s%llu%s\n",
				colors->rname, colors->reset, colors->iname, data->capability,
				colors->reset, colors->num, data->value, colors->reset);
	}
	else
	{
		mmt_log("%sunknown drm ioctl%s %x\n", colors->err, colors->reset, nr);
		return 1;
	}

	return 0;
}
#endif
