/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gdk/gdktypes.h>
#include <gdk/gdkdevicemanager.h>
#include "gdkdevicemanager-core.h"
#include "gdkdevice-core.h"
#include "gdkkeysyms.h"


#define HAS_FOCUS(toplevel)                           \
  ((toplevel)->has_focus || (toplevel)->has_pointer_focus)

static void    gdk_device_manager_core_finalize    (GObject *object);
static void    gdk_device_manager_core_constructed (GObject *object);

static GList * gdk_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                                     GdkDeviceType     type);


G_DEFINE_TYPE (GdkDeviceManagerCore, gdk_device_manager_core, GDK_TYPE_DEVICE_MANAGER)

static void
gdk_device_manager_core_class_init (GdkDeviceManagerCoreClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_device_manager_core_finalize;
  object_class->constructed = gdk_device_manager_core_constructed;
  device_manager_class->list_devices = gdk_device_manager_core_list_devices;
}

static GdkDevice *
create_core_pointer (GdkDeviceManager *device_manager,
                     GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_DEVICE_CORE,
                       "name", "Core Pointer",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_MOUSE,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", TRUE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static GdkDevice *
create_core_keyboard (GdkDeviceManager *device_manager,
                      GdkDisplay       *display)
{
  return g_object_new (GDK_TYPE_DEVICE_CORE,
                       "name", "Core Keyboard",
                       "type", GDK_DEVICE_TYPE_MASTER,
                       "input-source", GDK_SOURCE_KEYBOARD,
                       "input-mode", GDK_MODE_SCREEN,
                       "has-cursor", FALSE,
                       "display", display,
                       "device-manager", device_manager,
                       NULL);
}

static void
gdk_device_manager_core_init (GdkDeviceManagerCore *device_manager)
{
}

static void
gdk_device_manager_core_finalize (GObject *object)
{
  GdkDeviceManagerCore *device_manager_core;

  device_manager_core = GDK_DEVICE_MANAGER_CORE (object);

  g_object_unref (device_manager_core->core_pointer);
  g_object_unref (device_manager_core->core_keyboard);

  G_OBJECT_CLASS (gdk_device_manager_core_parent_class)->finalize (object);
}

static void
gdk_device_manager_core_constructed (GObject *object)
{
  GdkDeviceManagerCore *device_manager;
  GdkDisplay *display;

  device_manager = GDK_DEVICE_MANAGER_CORE (object);
  display = gdk_device_manager_get_display (GDK_DEVICE_MANAGER (object));
  device_manager->core_pointer = create_core_pointer (GDK_DEVICE_MANAGER (device_manager), display);
  device_manager->core_keyboard = create_core_keyboard (GDK_DEVICE_MANAGER (device_manager), display);

  _gdk_device_set_associated_device (device_manager->core_pointer, device_manager->core_keyboard);
  _gdk_device_set_associated_device (device_manager->core_keyboard, device_manager->core_pointer);
}

static GList *
gdk_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                      GdkDeviceType     type)
{
  GdkDeviceManagerCore *device_manager_core;
  GList *devices = NULL;

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      device_manager_core = (GdkDeviceManagerCore *) device_manager;
      devices = g_list_prepend (devices, device_manager_core->core_keyboard);
      devices = g_list_prepend (devices, device_manager_core->core_pointer);
    }

  return devices;
}