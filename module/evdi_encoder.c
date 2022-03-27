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

#include <linux/version.h>
#if KERNEL_VERSION(5, 5, 0) <= LINUX_VERSION_CODE || defined(EL8)
#else
#include <drm/drmP.h>
#endif
#include "evdi_drm_drv.h"

#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>

static void evdi_encoder_enable(__always_unused struct drm_encoder *encoder)
{
}

static void evdi_encoder_disable(__always_unused struct drm_encoder *encoder)
{
}

static const struct drm_encoder_helper_funcs evdi_enc_helper_funcs = { .enable = evdi_encoder_enable, .disable = evdi_encoder_disable };

/* dummy encoder */
static void evdi_enc_destroy(struct drm_encoder *encoder)
{
    drm_encoder_cleanup(encoder);
    kfree(encoder);
}

static const struct drm_encoder_funcs evdi_enc_funcs = {
    .destroy = evdi_enc_destroy,
};

struct drm_encoder *evdi_encoder_init(struct evdi_device *dev)
{
    struct drm_encoder *encoder;
    int ret = 0;

    encoder = kzalloc(sizeof(struct drm_encoder), GFP_KERNEL);
    if (!encoder)
        goto err;

    encoder->possible_crtcs = 1;
    // Drivers must initialize the struct drm_encoder possible_crtcs and possible_clones fields before registering the encoder.
    // Both fields are bitmasks of respectively the CRTCs that the encoder can be connected to, and sibling encoders candidate for cloning.
    encoder->possible_clones = 1;

    ret = drm_encoder_init(&dev->ddev, encoder, &evdi_enc_funcs, DRM_MODE_ENCODER_VIRTUAL, "%s", dev_name(dev->ddev.dev));
    if (ret)
    {
        EVDI_ERROR("Failed to initialize encoder: %d\n", ret);
        goto err_encoder;
    }

    drm_encoder_helper_add(encoder, &evdi_enc_helper_funcs);

    return encoder;

err_encoder:
    kfree(encoder);
err:
    return NULL;
}
