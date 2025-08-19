// Copyright 2023 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_TOPLEVEL_WRAPPER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_TOPLEVEL_WRAPPER_H_

#include <cstdint>
#include <string>

#include "ui/platform_window/extensions/wayland_extension.h"

struct xdg_surface;
struct xdg_toplevel;

namespace gfx {
class ImageSkia;
class Point;
class Rect;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandOutput;

// NEVA: interface for shell toplevel objects.
//
// Upstream M120 had this as ShellToplevelWrapper, with XDGToplevelWrapperImpl
// as the only in-tree implementation, kept expressly so that downstream
// platforms could substitute their own (see https://crbug.com/1402672). M151
// removed it and made XdgToplevel a concrete class owned directly by
// WaylandToplevelWindow, because no upstream user was left.
//
// webOS is exactly the downstream user that abstraction existed for: its shell
// surface is wl_shell plus the wl_webos_shell extension, not xdg_shell, and it
// is substituted through WaylandExtensions::CreateShellToplevel(). So the seam
// is reinstated here, narrowed to what WaylandToplevelWindow actually calls
// plus the two webOS-only orientation-lock methods.
//
// If a future uprev makes upstream's XdgToplevel API drift again, this is the
// list to reconcile: it must stay a superset of the methods
// WaylandToplevelWindow invokes on its toplevel.
class ShellToplevelWrapper {
 public:
  enum class DecorationMode { kNone, kClientSide, kServerSide };

  virtual ~ShellToplevelWrapper() = default;

  // Initializes the shell toplevel. Returns false on failure.
  virtual bool Initialize() = 0;

  virtual void SetMaximized() = 0;
  virtual void UnSetMaximized() = 0;
  virtual void SetFullscreen(WaylandOutput* wayland_output) = 0;
  virtual void UnSetFullscreen() = 0;
  virtual void SetMinimized() = 0;

  virtual void SurfaceMove(WaylandConnection* connection) = 0;
  virtual void SurfaceResize(WaylandConnection* connection,
                             uint32_t hittest) = 0;

  virtual void SetTitle(const std::u16string& title) = 0;
  virtual void SetAppId(const std::string& app_id) = 0;

  virtual void AckConfigure(uint32_t serial) = 0;
  virtual bool IsConfigured() = 0;

  virtual void SetWindowGeometry(const gfx::Rect& bounds) = 0;
  virtual void SetMinSize(int32_t width, int32_t height) = 0;
  virtual void SetMaxSize(int32_t width, int32_t height) = 0;

  virtual void ShowWindowMenu(WaylandConnection* connection,
                              const gfx::Point& point) = 0;
  virtual void SetDecoration(DecorationMode decoration) = 0;
  virtual void SetSystemModal(bool modal) = 0;
  virtual void SetIcon(const gfx::ImageSkia& icon) = 0;

  // NEVA: webOS-only. Locks the surface to a screen orientation.
  virtual void Lock(WaylandOrientationLockType lock_type) {}
  virtual void Unlock() {}

  // Return the underlying xdg objects, or nullptr for shells that are not
  // xdg_shell based (the webOS one is not). Needed by xdg_popup parenting,
  // xdg_session and the window drag controller, all of which are xdg-only.
  virtual struct xdg_surface* xdg_surface() const = 0;
  virtual struct xdg_toplevel* wl_object() const = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_SHELL_TOPLEVEL_WRAPPER_H_
