/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DirectoryNodeAlbumRecentlyAdded.h"
#include "FileItem.h"
#include "FileItemList.h"
#include "guilib/LocalizeStrings.h"
#include "music/MusicDatabase.h"
#include "music/tags/MusicInfoTag.h"
#include "utils/StringUtils.h"

using namespace XFILE::MUSICDATABASEDIRECTORY;
using namespace MUSIC_INFO;

CDirectoryNodeAlbumRecentlyAdded::CDirectoryNodeAlbumRecentlyAdded(const std::string& strName,
                                                                   CDirectoryNode* pParent)
  : CDirectoryNode(NodeType::ALBUM_RECENTLY_ADDED, strName, pParent)
{

}

NodeType CDirectoryNodeAlbumRecentlyAdded::GetChildType() const
{
  if (GetName() == "-1")
    return NodeType::ALBUM_RECENTLY_ADDED_SONGS;

  return NodeType::DISC;
}

std::string CDirectoryNodeAlbumRecentlyAdded::GetLocalizedName() const
{
  if (GetID() == -1)
    return g_localizeStrings.Get(15102); // All Albums
  CMusicDatabase db;
  if (db.Open())
    return db.GetAlbumById(GetID());
  return "";
}

bool CDirectoryNodeAlbumRecentlyAdded::GetContent(CFileItemList& items) const
{
  CMusicDatabase musicdatabase;
  if (!musicdatabase.Open())
    return false;

  VECALBUMS albums;
  if (!musicdatabase.GetRecentlyAddedAlbums(albums))
  {
    musicdatabase.Close();
    return false;
  }

  for (int i=0; i<(int)albums.size(); ++i)
  {
    CAlbum& album=albums[i];
    std::string strDir = StringUtils::Format("{}{}/", BuildPath(), album.idAlbum);
    std::string albumPath = musicdatabase.GetPathForAlbum(album.idAlbum);
    CFileItemPtr pItem(new CFileItem(strDir, album));
    pItem->GetMusicInfoTag()->SetURL(albumPath);
    items.Add(pItem);
  }

  musicdatabase.Close();
  return true;
}
