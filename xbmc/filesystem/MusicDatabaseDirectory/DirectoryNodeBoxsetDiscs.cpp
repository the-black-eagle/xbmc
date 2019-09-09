/*
 *  Copyright (C) 2005-2019 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DirectoryNodeBoxsetDiscs.h"

#include "QueryParams.h"
#include "guilib/LocalizeStrings.h"
#include "music/MusicDatabase.h"

using namespace XFILE::MUSICDATABASEDIRECTORY;

CDirectoryNodeBoxsetDiscs::CDirectoryNodeBoxsetDiscs(const std::string& strName,
                                                     CDirectoryNode* pParent)
  : CDirectoryNode(NODE_TYPE_DISC, strName, pParent)
{
}

NODE_TYPE CDirectoryNodeBoxsetDiscs::GetChildType() const
{
  return NODE_TYPE_SONG;
}

std::string CDirectoryNodeBoxsetDiscs::GetLocalizedName() const
{

  if (GetID() == -1)
    return g_localizeStrings.Get(38075); // "All discs" 

  // ! @todo: Find better approach, use the label of the item we click?
  CQueryParams params;
  CollectQueryParams(params);
  CMusicDatabase db;
  if (db.Open())
    return db.GetBoxsetDiscById(params.GetAlbumId(), params.GetDisc());
  return "";
}

bool CDirectoryNodeBoxsetDiscs::GetContent(CFileItemList& items) const
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return false;

  CQueryParams params;
  CollectQueryParams(params);
  
  bool bSuccess = musicdatabase.GetBoxsetDiscs(BuildPath(), items, params.GetAlbumId());

  musicdatabase.Close();

  return bSuccess;
}