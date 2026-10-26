/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2026 Paul van Tilburg <paul.van.tilburg@leftclick.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the licence or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include "ostree-bootloader.h"

G_BEGIN_DECLS

#define OSTREE_TYPE_BOOTLOADER_RPIBOOT (_ostree_bootloader_rpiboot_get_type ())
#define OSTREE_BOOTLOADER_RPIBOOT(inst) \
  (G_TYPE_CHECK_INSTANCE_CAST ((inst), OSTREE_TYPE_BOOTLOADER_RPIBOOT, OstreeBootloaderRpiboot))
#define OSTREE_IS_BOOTLOADER_RPIBOOT(inst) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((inst), OSTREE_TYPE_BOOTLOADER_RPIBOOT))

typedef struct _OstreeBootloaderRpiboot OstreeBootloaderRpiboot;

GType _ostree_bootloader_rpiboot_get_type (void) G_GNUC_CONST;

OstreeBootloaderRpiboot * _ostree_bootloader_rpiboot_new (OstreeSysroot *sysroot);

G_END_DECLS
