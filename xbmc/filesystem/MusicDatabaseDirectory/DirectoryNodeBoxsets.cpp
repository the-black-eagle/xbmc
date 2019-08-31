/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DirectoryNodeBoxsets.h"

#include "QueryParams.h"
#include "guilib/LocalizeStrings.h"
#include "music/MusicDatabase.h"

using namespace XFILE::MUSICDATABASEDIRECTORY;

CDirectoryNodeBoxsets::CDirectoryNodeBoxsets(const std::string& strName, CDirectoryNode* pParent)
  : CDirectoryNode(NODE_TYPE_BOXSETS, strName, pParent)
{

}

NODE_TYPE CDirectoryNodeBoxsets::GetChildType() const
{
  return NODE_TYPE_BOXSET_DISCS;
}
std::string CDirectoryNodeBoxsets::GetLocalizedName() const
{
  if (GetID() == -1)
    return g_localizeStrings.Get(15102); // All Albums
  CMusicDatabase db;
  if (db.Open())
   return db.GetAlbumById(GetID());
  return "";
}

bool CDirectoryNodeBoxsets::GetContent(CFileItemList& items) const
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return false;

  CQueryParams params;
  CollectQueryParams(params);

  bool bSuccess=musicdatabase.GetBoxsetsAlbums(BuildPath(), items);

  musicdatabase.Close();

  return bSuccess;
}
