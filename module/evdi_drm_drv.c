// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2012 Red Hat
 * Copyright (c) 2015 - 2020 DisplayLink (UK) Ltd.
 *
 * Based on parts on udlfb.c:
 * Copyright (C) 2009 its respective authors
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License v2. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include "evdi_drm_drv.h"

#include "evdi_cursor.h"
#include "evdi_debug.h"
#include "evdi_drm.h"
#include "evdi_platform_drv.h"

#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_ioctl.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>
#include <linux/version.h>

static struct drm_driver driver;

struct drm_ioctl_desc evdi_painter_ioctls[] = {
    DRM_IOCTL_DEF_DRV(EVDI_CONNECT, evdi_painter_connect_ioctl, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(EVDI_REQUEST_UPDATE, evdi_painter_request_update_ioctl, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(EVDI_GRABPIX, evdi_painter_grabpix_ioctl, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(EVDI_DDCCI_RESPONSE, evdi_painter_ddcci_response_ioctl, DRM_RENDER_ALLOW),
    DRM_IOCTL_DEF_DRV(EVDI_ENABLE_CURSOR_EVENTS, evdi_painter_enable_cursor_events_ioctl, DRM_RENDER_ALLOW),
};

#if KERNEL_VERSION(5, 11, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
static const struct vm_operations_struct evdi_gem_vm_ops = {
    .fault = evdi_gem_fault,
    .open = drm_gem_vm_open,
    .close = drm_gem_vm_close,
};
#endif

static struct file_operations evdi_driver_fops = {
    .owner = THIS_MODULE,
    .open = drm_open,
    .mmap = evdi_drm_gem_mmap,
    .poll = drm_poll,
    .read = drm_read,
    .unlocked_ioctl = drm_ioctl,
    .release = drm_release,
    .llseek = noop_llseek,
};

static struct drm_driver driver = {
    .driver_features = DRIVER_MODESET | DRIVER_GEM | DRIVER_ATOMIC,
    .unload = evdi_driver_unload,

    .open = evdi_driver_open,
    .postclose = evdi_driver_postclose,

    .dumb_create = evdi_dumb_create,
    .dumb_map_offset = evdi_gem_mmap,

    .ioctls = evdi_painter_ioctls,
    .num_ioctls = ARRAY_SIZE(evdi_painter_ioctls),

    .fops = &evdi_driver_fops,

    .prime_fd_to_handle = drm_gem_prime_fd_to_handle,
    .gem_prime_import = drm_gem_prime_import,
    .prime_handle_to_fd = drm_gem_prime_handle_to_fd,

    .gem_prime_import_sg_table = evdi_prime_import_sg_table,

    .name = DRIVER_NAME,
    .desc = DRIVER_DESC,
    .date = DRIVER_DATE,
    .major = DRIVER_MAJOR,
    .minor = DRIVER_MINOR,
    .patchlevel = DRIVER_PATCH,
};

void evdi_driver_unload(struct drm_device *dev)
{
    struct evdi_device *evdi = dev_to_evdi(dev);

    EVDI_CHECKPT();

    drm_kms_helper_poll_fini(dev);

#ifdef CONFIG_FB
    evdi_fbdev_unplug(dev);
#endif /* CONFIG_FB */
    if (evdi->cursor)
        evdi_cursor_free(evdi->cursor);

    evdi_painter_cleanup(evdi->painter);
#ifdef CONFIG_FB
    evdi_fbdev_cleanup(evdi);
#endif /* CONFIG_FB */
    evdi_modeset_cleanup(dev);

    //    kfree(evdi);
}

int evdi_driver_open(struct drm_device *drm_dev, __always_unused struct drm_file *file)
{
    struct evdi_device *evdi = dev_to_evdi(drm_dev);
    char buf[100];

    evdi_log_process(buf, sizeof(buf));
    EVDI_INFO("(card%d) Opened by %s\n", evdi->dev_index, buf);
    return 0;
}

static void evdi_driver_close(struct drm_device *drm_dev, struct drm_file *file)
{
    struct evdi_device *evdi = dev_to_evdi(drm_dev);

    EVDI_CHECKPT();
    if (evdi)
        evdi_painter_close(evdi, file);
}

void evdi_driver_preclose(struct drm_device *drm_dev, struct drm_file *file)
{
    evdi_driver_close(drm_dev, file);
}

void evdi_driver_postclose(struct drm_device *drm_dev, struct drm_file *file)
{
    struct evdi_device *evdi = dev_to_evdi(drm_dev);
    char buf[100];

    evdi_log_process(buf, sizeof(buf));
    EVDI_INFO("(card%d) Closed by %s\n", evdi->dev_index, buf);

    evdi_driver_close(drm_dev, file);
}

struct evdi_device *evdi_drm_device_create(struct device *parent)
{
    struct evdi_device *evdi;
    int ret;

    evdi = devm_drm_dev_alloc(parent, &driver, struct evdi_device, ddev);
    if (IS_ERR(evdi))
        return evdi;

    evdi->dev_index = evdi->ddev.primary->index;
    evdi->cursor_events_enabled = false;

    ret = evdi_cursor_init(&evdi->cursor);
    if (ret)
        goto err_free;

    EVDI_CHECKPT();
    evdi_modeset_init(evdi);

#ifdef CONFIG_FB
    ret = evdi_fbdev_init(evdi);
    if (ret)
        goto err_cursor;
#endif /* CONFIG_FB */

    ret = drm_vblank_init(&evdi->ddev, 1);
    if (ret)
        goto err_fb;

    ret = evdi_painter_init(evdi);
    if (ret)
        goto err_fb;

    drm_kms_helper_poll_init(&evdi->ddev);

    ret = drm_dev_register(&evdi->ddev, 0);
    if (ret)
        goto err_free;

    return evdi;

err_fb:
#ifdef CONFIG_FB
    evdi_fbdev_cleanup(evdi);
err_cursor:
#endif /* CONFIG_FB */
    evdi_cursor_free(evdi->cursor);
err_free:
    EVDI_ERROR("Failed to setup drm device %d\n", ret);
    kfree(evdi);

    return ERR_PTR(ret);
}

int evdi_drm_device_remove(struct drm_device *dev)
{
    drm_dev_unplug(dev);
    return 0;
}
