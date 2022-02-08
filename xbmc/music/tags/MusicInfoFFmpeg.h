/*
 *  Copyright (C) 2005-2022 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include <string>

typedef struct fileInfo
{
  int bitsPerSample = 0;
  int sampleRate = 0;
  int bitRate = 0;
  int channels = 0;
  std::string codecName;
}
fileInfo;

class CMusicInfoFFmpeg
{
public:
  static bool GetMusicCodecInfo(const std::string& strFileName, fileInfo& codec_info);
};
