/*
 *  Copyright (C) 2005-2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DirectoryNodeBoxsetDiscSongs.h"

#include "QueryParams.h"
#include "guilib/LocalizeStrings.h"
#include "music/MusicDatabase.h"

using namespace XFILE::MUSICDATABASEDIRECTORY;

CDirectoryNodeBoxsetDiscSongs::CDirectoryNodeBoxsetDiscSongs(const std::string& strName, CDirectoryNode* pParent)
  : CDirectoryNode(NODE_TYPE_BOXSET_DISC_SONGS, strName, pParent)
{

}

NODE_TYPE CDirectoryNodeBoxsetDiscSongs::GetChildType() const
{
  return NODE_TYPE_BOXSET_DISC_SONGS;
}
std::string CDirectoryNodeBoxsetDiscSongs::GetLocalizedName() const
{

  if (GetID() == -1)
    return g_localizeStrings.Get(15102); // All Albums
  return "";
}

bool CDirectoryNodeBoxsetDiscSongs::GetContent(CFileItemList& items) const
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return false;

  CQueryParams params;
  CollectQueryParams(params);

  bool bSuccess=musicdatabase.GetBoxsetDiscSongs(BuildPath(), items);
  musicdatabase.Close();

  return bSuccess;
}