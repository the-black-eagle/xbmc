/*
 *  Copyright (C) 2016-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ContextMenus.h"

#include "PlayListPlayer.h"
#include "ServiceBroker.h"
#include "filesystem/MusicDatabaseDirectory.h"
#include "filesystem/VirtualDirectory.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/GUIComponent.h"
#include "music/dialogs/GUIDialogMusicInfo.h"
#include "playlists/PlayList.h"
#include "tags/MusicInfoTag.h"

#include <utility>

namespace CONTEXTMENU
{

CMusicInfo::CMusicInfo(MediaType mediaType)
  : CStaticContextMenuAction(19033), m_mediaType(std::move(mediaType))
{
}

bool CMusicInfo::IsVisible(const CFileItem& item) const
{
  return (item.HasMusicInfoTag() && item.GetMusicInfoTag()->GetType() == m_mediaType) ||
         (m_mediaType == MediaTypeArtist && item.IsVideoDb() && item.HasProperty("artist_musicid")) ||
         (m_mediaType == MediaTypeAlbum && item.IsVideoDb() && item.HasProperty("album_musicid"));
}

bool CMusicInfo::Execute(const CFileItemPtr& item) const
{
  CGUIDialogMusicInfo::ShowFor(item.get());
  return true;
}

std::string CPlayMusic::GetLabel(const CFileItem& itemIn) const
{
  CFileItem item(itemIn.GetItemToPlay());
  return g_localizeStrings.Get(208); // Play
}

bool CPlayMusic::IsVisible(const CFileItem& itemIn) const
{
  // restrict this to just the home window
  // (we have code in CGUIMusicBase that adds a play button in other windows)
  const int currentWindowID = CServiceBroker::GetGUI()->GetWindowManager().GetActiveWindow();
  return (itemIn.HasMusicInfoTag() && currentWindowID == WINDOW_HOME);
}

bool CPlayMusic::Execute(const CFileItemPtr& itemIn) const
{
  CMusicDatabase db;
  db.Open();
  if (itemIn->m_bIsFolder) // build a playlist and play it
  {
    CFileItemList queuedItems;
    AddItemToPlayList(itemIn, queuedItems, db);
    CServiceBroker::GetPlaylistPlayer().ClearPlaylist(PLAYLIST_MUSIC);
    CServiceBroker::GetPlaylistPlayer().Reset();
    CServiceBroker::GetPlaylistPlayer().Add(PLAYLIST_MUSIC, queuedItems);
    CServiceBroker::GetPlaylistPlayer().SetCurrentPlaylist(PLAYLIST_MUSIC);
    CServiceBroker::GetPlaylistPlayer().Play();
  }
  else // song, so just play it
  {
    CFileItem item(itemIn->GetItemToPlay());
    PlaySong(item);
  }
  db.Close();
  return true;
}
}

void PlaySong(CFileItem& item)
{
  if (item.IsMusicDb()) // should always be true as we are only called on the home screen
    CServiceBroker::GetPlaylistPlayer().Play(std::make_shared<CFileItem>(item), "");
}

void AddItemToPlayList(const CFileItemPtr& pItem, CFileItemList& queuedItems, CMusicDatabase& db)
{
  XFILE::CMusicDatabaseDirectory dir;
  if (!dir.ContainsSongs(pItem->GetPath()))
  {
    CMusicDbUrl musicUrl;
    if (musicUrl.FromString(pItem->GetPath()))
    {
      musicUrl.AppendPath("-1/");
      CFileItemPtr item(new CFileItem(musicUrl.ToString(), true));
      item->SetCanQueue(true);
      AddItemToPlayList(item, queuedItems, db);
    }
    return;
  }
  if (pItem->m_bIsFolder) // artist or album
  {
    CFileItemList items;
    XFILE::CVirtualDirectory dir2;
    CURL pathToUrl(pItem->GetPath());
    dir2.GetDirectory(pathToUrl, items);
    for (int i = 0; i < items.Size(); ++i)
    {
      CFileItemPtr itemCheck = queuedItems.Get(items[i]->GetPath());
      if (!itemCheck || itemCheck->m_lStartOffset != items[i]->m_lStartOffset)
      { // add new item
        CFileItemPtr item(new CFileItem(*items[i]));
        db.SetPropertiesForFileItem(*item);
        queuedItems.Add(item);
      }
    }
  }
}
