/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "ImusicInfoTagLoader.h"
#include "MusicCodecInfoFFmpeg.h"

#include <vector>

namespace MUSIC_INFO
{
  class CMusicInfoTagLoaderFFmpeg: public IMusicInfoTagLoader
  {
  public:
    CMusicInfoTagLoaderFFmpeg(void);
    ~CMusicInfoTagLoaderFFmpeg() override;

    bool Load(const std::string& strFileName, CMusicInfoTag& tag, EmbeddedArt *art = NULL) override;
  private:
    void AddRole(const std::vector<std::string>& data,
                                                      const std::vector<std::string>& separators,
                                                      MUSIC_INFO::CMusicInfoTag& musictag);
  };
}
