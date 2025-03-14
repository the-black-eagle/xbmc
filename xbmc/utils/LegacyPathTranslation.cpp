/*
 *  Copyright (C) 2013-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "LegacyPathTranslation.h"

#include "URL.h"
#include "utils/StringUtils.h"

#include <algorithm>
#include <string>
#include <unordered_map>

namespace
{

// ATTENTION: Make sure the longer match strings go first
// because the string match is performed with ::starts_with()
const auto s_videoDbTranslator = std::unordered_map<std::string, std::string>{
    {"videodb://1/1", "videodb://movies/genres"},
    {"videodb://1/2", "videodb://movies/titles"},
    {"videodb://1/3", "videodb://movies/years"},
    {"videodb://1/4", "videodb://movies/actors"},
    {"videodb://1/5", "videodb://movies/directors"},
    {"videodb://1/6", "videodb://movies/studios"},
    {"videodb://1/7", "videodb://movies/sets"},
    {"videodb://1/8", "videodb://movies/countries"},
    {"videodb://1/9", "videodb://movies/tags"},
    {"videodb://1", "videodb://movies"},
    {"videodb://2/1", "videodb://tvshows/genres"},
    {"videodb://2/2", "videodb://tvshows/titles"},
    {"videodb://2/3", "videodb://tvshows/years"},
    {"videodb://2/4", "videodb://tvshows/actors"},
    {"videodb://2/5", "videodb://tvshows/studios"},
    {"videodb://2/9", "videodb://tvshows/tags"},
    {"videodb://2", "videodb://tvshows"},
    {"videodb://3/1", "videodb://musicvideos/genres"},
    {"videodb://3/2", "videodb://musicvideos/titles"},
    {"videodb://3/3", "videodb://musicvideos/years"},
    {"videodb://3/4", "videodb://musicvideos/artists"},
    {"videodb://3/5", "videodb://musicvideos/albums"},
    {"videodb://3/9", "videodb://musicvideos/tags"},
    {"videodb://3", "videodb://musicvideos"},
    {"videodb://4", "videodb://recentlyaddedmovies"},
    {"videodb://5", "videodb://recentlyaddedepisodes"},
    {"videodb://6", "videodb://recentlyaddedmusicvideos"}};

// ATTENTION: Make sure the longer match strings go first
// because the string match is performed with ::starts_with()
const auto s_musicDbTranslator =
    std::unordered_map<std::string, std::string>{{"musicdb://10", "musicdb://singles"},
                                                 {"musicdb://1", "musicdb://genres"},
                                                 {"musicdb://2", "musicdb://artists"},
                                                 {"musicdb://3", "musicdb://albums"},
                                                 {"musicdb://4", "musicdb://songs"},
                                                 {"musicdb://5/1", "musicdb://top100/albums"},
                                                 {"musicdb://5/2", "musicdb://top100/songs"},
                                                 {"musicdb://5", "musicdb://top100"},
                                                 {"musicdb://6", "musicdb://recentlyaddedalbums"},
                                                 {"musicdb://7", "musicdb://recentlyplayedalbums"},
                                                 {"musicdb://8", "musicdb://compilations"},
                                                 {"musicdb://9", "musicdb://years"}};

std::string TranslatePath(const std::string& legacyPath,
                          const std::unordered_map<std::string, std::string>& translationMap)
{
  std::string newPath = legacyPath;
  const std::string lowPath = StringUtils::ToLower(legacyPath);
  const auto it = std::find_if(translationMap.begin(), translationMap.end(),
                               [&lowPath](const auto& e) { return lowPath.starts_with(e.first); });
  if (it != translationMap.end())
    StringUtils::Replace(newPath, it->first, it->second);

  return newPath;
}

} // namespace

std::string CLegacyPathTranslation::TranslateVideoDbPath(const CURL& legacyPath)
{
  return TranslatePath(legacyPath.Get(), s_videoDbTranslator);
}

std::string CLegacyPathTranslation::TranslateMusicDbPath(const CURL& legacyPath)
{
  return TranslatePath(legacyPath.Get(), s_musicDbTranslator);
}

std::string CLegacyPathTranslation::TranslateVideoDbPath(const std::string& legacyPath)
{
  return TranslatePath(legacyPath, s_videoDbTranslator);
}

std::string CLegacyPathTranslation::TranslateMusicDbPath(const std::string& legacyPath)
{
  return TranslatePath(legacyPath, s_musicDbTranslator);
}
