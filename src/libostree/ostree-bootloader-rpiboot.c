/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2026 Paul van Tilburg <paul.van.tilburg@leftclick.eu>
 *
 * Based on ot-bootloader-sd-boot.c by Sjoerd Simons <sjoerd@luon.net>
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
 * Author: Javier Martinez Canillas <javier.martinez@collabora.co.uk>
 */

#include "config.h"

#include "gio/gio.h"
#include "ostree-sysroot-private.h"
#include "ostree-bootloader-rpiboot.h"

#include <string.h>

static const char rpi_config_path[] = "boot/firmware/config.txt";

struct _OstreeBootloaderRpiboot
{
  GObject       parent_instance;

  OstreeSysroot  *sysroot;
};

typedef GObjectClass OstreeBootloaderRpibootClass;

static void _ostree_bootloader_rpiboot_bootloader_iface_init (OstreeBootloaderInterface *iface);
G_DEFINE_TYPE_WITH_CODE (OstreeBootloaderRpiboot, _ostree_bootloader_rpiboot, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (OSTREE_TYPE_BOOTLOADER, _ostree_bootloader_rpiboot_bootloader_iface_init));

static gboolean
_ostree_bootloader_rpiboot_query (OstreeBootloader *bootloader,
                                  gboolean         *out_is_active,
                                  GCancellable     *cancellable,
                                  GError          **error)
{
  OstreeBootloaderRpiboot *self = OSTREE_BOOTLOADER_RPIBOOT (bootloader);
  struct stat stbuf;

  if (!glnx_fstatat_allow_noent (self->sysroot->sysroot_fd, rpi_config_path, &stbuf,
                                 AT_SYMLINK_NOFOLLOW, error))
    return FALSE;

  *out_is_active = (errno == 0);
  return TRUE;
}

static const char *
_ostree_bootloader_rpiboot_get_name (OstreeBootloader *bootloader)
{
  return "rpiboot";
}

static gboolean
copy_rpiboot_pair (OstreeBootloaderRpiboot *self,
                   const char              *rpiboot,
                   const char              *destination_img,
                   const char              *destination_sig,
                   GCancellable            *cancellable,
                   GError                 **error)
{
  g_autofree char *relative_img = g_strdup(rpiboot);
  g_autofree char *dirname = g_path_get_dirname (rpiboot);
  g_autofree char *relative_sig = g_build_filename (dirname, "boot.sig", NULL);

  g_autofree char *source_img_path = g_build_filename ("boot", relative_img, NULL);
  g_autofree char *source_sig_path = g_build_filename ("boot", relative_sig, NULL);

  g_autoptr (GFile) source_img =
    g_file_resolve_relative_path (self->sysroot->path, source_img_path);
  g_autoptr (GFile) target_img =
    g_file_resolve_relative_path (self->sysroot->path, destination_img);

  if (!g_file_copy (source_img, target_img, G_FILE_COPY_OVERWRITE,
                    cancellable, NULL, NULL, error))
    return glnx_prefix_error (error, "Copying RPIBOOT image");

  g_autoptr (GFile) source_sig =
    g_file_resolve_relative_path (self->sysroot->path, source_sig_path);
  g_autoptr (GFile) target_sig =
    g_file_resolve_relative_path (self->sysroot->path, destination_sig);

  if (!g_file_copy (source_sig, target_sig, G_FILE_COPY_OVERWRITE,
                    cancellable, NULL, NULL, error))
    return glnx_prefix_error (error, "Copying RPIBOOT signature");

  return TRUE;
}

static gboolean
remove_if_present (OstreeBootloaderRpiboot *self,
                   const char              *relative_path,
                   GCancellable            *cancellable,
                   GError                 **error)
{
  g_autoptr (GFile) file = g_file_resolve_relative_path (self->sysroot->path, relative_path);

  if (!g_file_query_exists (file, cancellable))
    return TRUE;

  if (!g_file_delete (file, cancellable, error))
    return glnx_prefix_error (error, "Removing %s", relative_path);

  return TRUE;
}

static gboolean
_ostree_bootloader_rpiboot_write_config (OstreeBootloader *bootloader,
                                         int               bootversion,
                                         GPtrArray        *new_deployments,
                                         GCancellable     *cancellable,
                                         GError          **error)
{
  OstreeBootloaderRpiboot *self = OSTREE_BOOTLOADER_RPIBOOT (bootloader);
  g_autoptr(GPtrArray) boot_loader_configs = NULL;

  if (!_ostree_sysroot_read_boot_loader_configs (self->sysroot, bootversion, &boot_loader_configs,
                                                 cancellable, error))
    return FALSE;

  /* The primary entry is the pending deployment. Its first boot should be attempted via tryboot. */
  if (boot_loader_configs->len == 0)
    return glnx_throw (error, "No bootlader entries found for RPIBOOT");

  OstreeBootconfigParser *primary = boot_loader_configs->pdata[0];
  const char *primary_rpiboot = ostree_bootconfig_parser_get (primary, "rpiboot");

  if (!primary_rpiboot)
    return glnx_throw (error, "Missing key in boot loader config: rpiboot");

  if (!copy_rpiboot_pair (self, primary_rpiboot,
                          "boot/firmware/tryboot.img",
                          "boot/firmware/tryboot.sig",
                          cancellable, error))
    return FALSE;

  /* The secondary entry, if present, is the rollback deployment.
   * On first deployment it may not exist; leave any existing boot.img/boot.sig alone. */
  if (boot_loader_configs->len > 1)
    {
      OstreeBootconfigParser *secondary = boot_loader_configs->pdata[1];
      const char *secondary_rpiboot = ostree_bootconfig_parser_get (secondary, "rpiboot");

      if (!secondary_rpiboot)
        return glnx_throw (error, "Missing key in boot loader config: rpiboot");

      if (!copy_rpiboot_pair (self, secondary_rpiboot,
                              "boot/firmware/boot.img",
                              "boot/firmware/boot.sig",
                              cancellable, error))
        return FALSE;
    }

  /* Other entries are ignored! */

  /* These are no longer necessary now that there is a new deployment */
  if (!remove_if_present (self, "boot/firmware/rollback.img", cancellable, error) ||
      !remove_if_present (self, "boot/firmware/rollback.sig", cancellable, error))
    return FALSE;

  return TRUE;
}

static void
_ostree_bootloader_rpiboot_finalize (GObject *object)
{
  OstreeBootloaderRpiboot *self = OSTREE_BOOTLOADER_RPIBOOT (object);

  g_clear_object (&self->sysroot);
  G_OBJECT_CLASS (_ostree_bootloader_rpiboot_parent_class)->finalize (object);
}

void
_ostree_bootloader_rpiboot_init (OstreeBootloaderRpiboot *self)
{
}

static void
_ostree_bootloader_rpiboot_bootloader_iface_init (OstreeBootloaderInterface *iface)
{
  iface->query = _ostree_bootloader_rpiboot_query;
  iface->get_name = _ostree_bootloader_rpiboot_get_name;
  iface->write_config = _ostree_bootloader_rpiboot_write_config;
}

void
_ostree_bootloader_rpiboot_class_init (OstreeBootloaderRpibootClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = _ostree_bootloader_rpiboot_finalize;
}

OstreeBootloaderRpiboot *
_ostree_bootloader_rpiboot_new (OstreeSysroot *sysroot)
{
  OstreeBootloaderRpiboot *self = g_object_new (OSTREE_TYPE_BOOTLOADER_RPIBOOT, NULL);

  self->sysroot = g_object_ref (sysroot);
  return self;
}
