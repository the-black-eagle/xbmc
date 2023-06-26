/*
*  Copyright (C) 2023 Team Kodi
*  This file is part of Kodi - https://kodi.tv
*
*  SPDX-License-Identifier: GPL-2.0-or-later
*  See LICENSES/README.md for more information.
*/

#include "WinSystemWaylandWebOS.h"

#include "Connection.h"
#include "OSScreenSaverWebOS.h"
#include "Registry.h"
#include "ShellSurfaceWebOSShell.h"
#include "cores/AudioEngine/Sinks/AESinkStarfish.h"
#include "cores/VideoPlayer/DVDCodecs/Video/DVDVideoCodecStarfish.h"
#include "cores/VideoPlayer/VideoRenderers/HwDecRender/RendererStarfish.h"
#include "utils/log.h"

#include <CompileInfo.h>

namespace KODI::WINDOWING::WAYLAND
{

bool CWinSystemWaylandWebOS::InitWindowSystem()
{
  if (!CWinSystemWayland::InitWindowSystem())
    return false;

  CDVDVideoCodecStarfish::Register();
  CRendererStarfish::Register();
  CAESinkStarfish::Register();

  m_webosRegistry = std::make_unique<CRegistry>(*GetConnection());
  m_webosRegistry->RequestSingleton(m_compositor, 1, 4);
  // available since webOS 5.0
  m_webosRegistry->RequestSingleton(m_webosForeign, 1, 2, false);
  m_webosRegistry->Bind();

  return true;
}

bool CWinSystemWaylandWebOS::CreateNewWindow(const std::string& name,
                                             bool fullScreen,
                                             RESOLUTION_INFO& res)
{
  if (!CWinSystemWayland::CreateNewWindow(name, fullScreen, res))
    return false;

  if (m_webosForeign)
  {
    m_exportedSurface = m_webosForeign.export_element(
        GetMainSurface(),
        static_cast<uint32_t>(wayland::webos_foreign_webos_exported_type::video_object));
    m_exportedSurface.on_window_id_assigned() = [this](std::string window_id,
                                                       uint32_t exported_type) {
      CLog::Log(LOGDEBUG,
                "CreateNewWindow:CDVDVideoCodecStarfish: Foreign video surface exported {}",
                window_id);
      this->m_exportedWindowName = window_id;
    };
  }

  return true;
}

CWinSystemWaylandWebOS::~CWinSystemWaylandWebOS() noexcept
{
  m_exportedSurface = wayland::webos_exported_t{};
  m_webosForeign = wayland::webos_foreign_t{};

  if (m_webosRegistry)
  {
    m_webosRegistry->UnbindSingletons();
  }
  m_webosRegistry.reset();
}

bool CWinSystemWaylandWebOS::HasCursor()
{
  return false;
}

std::string CWinSystemWaylandWebOS::GetExportedWindowName()
{
  return m_exportedWindowName;
}

bool CWinSystemWaylandWebOS::SetExportedWindow(CRect orig, CRect src, CRect dest)
{
  if (m_webosForeign)
  {
    CLog::LogF(LOGINFO,
               "CWinSystemWaylandWebOS: orig {} {} {} {} src {} {} {} {} -> dest {} {} {} {}",
               orig.x1, orig.y1, orig.x2, orig.y2, src.x1, src.y1, src.x2, src.y2, dest.x1, dest.y1,
               dest.x2, dest.y2);
    wayland::region_t origRegion = m_compositor.create_region();
    wayland::region_t srcRegion = m_compositor.create_region();
    wayland::region_t dstRegion = m_compositor.create_region();
    origRegion.add(static_cast<int>(orig.x1), static_cast<int>(orig.y1),
                   static_cast<int>(orig.Width()), static_cast<int>(orig.Height()));
    srcRegion.add(static_cast<int>(src.x1), static_cast<int>(src.y1), static_cast<int>(src.Width()),
                  static_cast<int>(src.Height()));
    dstRegion.add(static_cast<int>(dest.x1), static_cast<int>(dest.y1),
                  static_cast<int>(dest.Width()), static_cast<int>(dest.Height()));
    m_exportedSurface.set_crop_region(origRegion, srcRegion, dstRegion);

    return true;
  }

  return false;
}

IShellSurface* CWinSystemWaylandWebOS::CreateShellSurface(const std::string& name)
{
  return new CShellSurfaceWebOSShell(*this, *GetConnection(), GetMainSurface(), name,
                                     std::string(CCompileInfo::GetAppName()));
}

std::unique_ptr<KODI::WINDOWING::IOSScreenSaver> CWinSystemWaylandWebOS::GetOSScreenSaverImpl()
{
  return std::make_unique<COSScreenSaverWebOS>();
}

} // namespace KODI::WINDOWING::WAYLAND
