/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DirectoryNodeAlbum.h"

#include "QueryParams.h"
#include "ServiceBroker.h"
#include "guilib/LocalizeStrings.h"
#include "music/MusicDatabase.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"

using namespace XFILE::MUSICDATABASEDIRECTORY;

CDirectoryNodeAlbum::CDirectoryNodeAlbum(const std::string& strName, CDirectoryNode* pParent)
  : CDirectoryNode(NODE_TYPE_ALBUM, strName, pParent)
{

}

NODE_TYPE CDirectoryNodeAlbum::GetChildType() const
{
  if (GetName() == "-1")
    return NODE_TYPE_SONG;

  //! @todo: This makes all multi-disc albums album > disc > song, make disc level optional
  CMusicDatabase musicdatabase;
  if (!CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(CSettings::SETTING_MUSICLIBRARY_SHOWDISCS))
    return NODE_TYPE_SONG;
  if (musicdatabase.Open())
  {
    int iDiscTotal = -1;
    iDiscTotal = musicdatabase.GetAlbumDiscCount(GetID());
    musicdatabase.Close();
    if (iDiscTotal > 1)
      return NODE_TYPE_DISC;
  }

  return NODE_TYPE_SONG;
}

std::string CDirectoryNodeAlbum::GetLocalizedName() const
{
  if (GetID() == -1)
    return g_localizeStrings.Get(15102); // All Albums
                                         
  CMusicDatabase db;
  if (db.Open())
    return db.GetAlbumById(GetID());
  return "";
}

bool CDirectoryNodeAlbum::GetContent(CFileItemList& items) const
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return false;

  CQueryParams params;
  CollectQueryParams(params);

  bool bSuccess=musicdatabase.GetAlbumsNav(BuildPath(), items, params.GetGenreId(), params.GetArtistId());

  musicdatabase.Close();

  return bSuccess;
}
