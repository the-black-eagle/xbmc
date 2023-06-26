/*
 *  Copyright (C) 2023 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <memory>
#include <string>

class CTexture;

namespace IMAGE_FILES
{

/*!
 * @brief An interface to load special image files into a texture for the cache.
 *
 * Special image files are generated or embedded images, like album covers embedded
 * in music files or thumbnails generated from video files.
*/
class ISpecialImageFileLoader
{
public:
  virtual bool CanLoad(std::string specialType) const = 0;
  virtual std::unique_ptr<CTexture> Load(std::string specialType,
                                         std::string filePath,
                                         unsigned int preferredWidth,
                                         unsigned int preferredHeight) const = 0;
  virtual ~ISpecialImageFileLoader() = default;
};

} // namespace IMAGE_FILES
