/*
 *  Copyright (C) 2005-2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AudioBookFileDirectory.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "IFileTypes.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "Util.h"
#include "dbwrappers/Database.h"
#include "filesystem/File.h"
#include "imagefiles/ImageFileURL.h"
#include "music/MusicEmbeddedCoverLoaderFFmpeg.h"
#include "music/tags/MusicCodecInfoFFmpeg.h"
#include "music/tags/MusicInfoTag.h"
#include "music/tags/MusicInfoTagLoaderMatroska.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/Mp4ChplReader.h"
#include "utils/log.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include <commons/ilog.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/dict.h>
#include <libavutil/mem.h>
#include <libavutil/rational.h>

using namespace XFILE;
using namespace MUSIC_INFO;

static int cfile_file_read(void* h, uint8_t* buf, int size)
{
  CFile* pFile = static_cast<CFile*>(h);
  return pFile->Read(buf, size);
}

static int64_t cfile_file_seek(void* h, int64_t pos, int whence)
{
  CFile* pFile = static_cast<CFile*>(h);
  if (whence == AVSEEK_SIZE)
    return pFile->GetLength();
  else
    return pFile->Seek(pos, whence & ~AVSEEK_FORCE);
}

CAudioBookFileDirectory::~CAudioBookFileDirectory(void)
{
  if (m_fctx)
    avformat_close_input(&m_fctx);
  if (m_ioctx)
  {
    av_free(m_ioctx->buffer);
    av_free(m_ioctx);
  }
}

bool CAudioBookFileDirectory::GetDirectory(const CURL& url, CFileItemList& items)
{
  if (!m_fctx && !ContainsFiles(url))
    return true;

  std::string title;
  std::string author;
  std::string album;
  std::string desc;

  std::vector<std::string> separators{" feat. ", " ft. ", " Feat. ", " Ft. ",  ";", ":",
                                      "|",       "#",     "/",       " with ", "&"};
  const std::string musicsep =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator;
  if (musicsep.find_first_of(";/,&|#") == std::string::npos)
    separators.push_back(musicsep); // add custom music separator from as.xml

  const bool isAudioBook = url.IsFileType("m4b");
  // Some tags are relevant to the whole album - these are read first
  CMusicInfoTag albumtag;

  AVDictionaryEntry* tag = nullptr;
  if (isAudioBook)
  {
    while ((tag = av_dict_get(m_fctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
    {
      if (StringUtils::CompareNoCase(tag->key, "title") == 0)
        title = tag->value;
      else if (StringUtils::CompareNoCase(tag->key, "album") == 0)
        album = tag->value;
      else if (StringUtils::CompareNoCase(tag->key, "artist") == 0)
        author = tag->value;
      else if (StringUtils::CompareNoCase(tag->key, "description") == 0)
        desc = tag->value;
    }
    else
    {
      std::string key = StringUtils::ToUpper(tag->key);
      // track is matroska's discnumber when at level 50 (these tags) as set by mp3tag
      // part_number is the matroska spec key
      if (key == "TRACK" || key == "PART_NUMBER")
        albumtag.SetDiscNumber(std::stoi(tag->value));
      else if (key == "SUBTITLE" || key == "SETSUBTITLE")
        albumtag.SetDiscSubtitle(tag->value);
      else if (key == "TITLE")
        albumtag.SetAlbum(tag->value);
      else if (key == "ALBUM")
        albumtag.SetAlbum(tag->value);
      else if (key == "ARTIST")
        albumtag.SetArtist(tag->value);
      else if (key == "ARTISTSORT" || key == "ARTIST SORT")
        albumtag.SetArtistSort(
            StringUtils::Join(StringUtils::Split(tag->value, separators), musicsep));
      else if (key == "ALBUMARTIST" || key == "ALBUM ARTIST" || key == "ALBUM_ARTIST")
        albumtag.SetAlbumArtist(
            StringUtils::Join(StringUtils::Split(tag->value, separators), musicsep));
      else if (key == "ALBUMARTSTS" || key == "ALBUM ARTISTS")
        albumtag.SetAlbumArtist(StringUtils::Split(tag->value, separators));
      else if (key == "ALBUMARTISTSORT" || key == "ALBUM ARTIST SORT" || key == "SORT_ALBUM_ARTIST")
        albumtag.SetAlbumArtistSort(
            StringUtils::Join(StringUtils::Split(tag->value, separators), musicsep));
      else if (key == "MUSICBRAINZ_ARTISTID")
        albumtag.SetMusicBrainzArtistID(StringUtils::Split(tag->value, separators));
      else if (key == "MUSICBRAINZ_ALBUMARTISTID" || key == "MUSICBRAINZ ALBUM ARTIST ID")
        albumtag.SetMusicBrainzAlbumArtistID(StringUtils::Split(tag->value, separators));
      else if (key == "MUSICBRAINZ_ALBUMARTIST")
        albumtag.SetAlbumArtist(tag->value);
      else if (key == "MUSICBRAINZ_ALBUMID" || key == "MUSICBRAINZ ALBUM ID")
        albumtag.SetMusicBrainzAlbumID(tag->value);
      else if (key == "MUSICBRAINZ_RELEASEGROUPID" || key == "MUSICBRAINZ RELEASE GROUP ID")
        albumtag.SetMusicBrainzReleaseGroupID(tag->value);
      //else if (key == "MUSICBRAINZ_ALBUMRELEASECOUNTRY" || key == "MUSICBRAINZ ALBUM RELEASE COUNTRY")
      // albumtag.Set
      else if (key == "MUSICBRAINZ_ALBUMSTATUS")
        albumtag.SetAlbumReleaseStatus(tag->value);
      else if (key == "MUSICBRAINZ_ALBUMTYPE")
        albumtag.SetMusicBrainzReleaseType(tag->value);
      else if (key == "PUBLISHER")
        albumtag.SetRecordLabel(tag->value);
      // mp3tag info shows year but the value is stored in date_recorded
      // equates to TDRC in id3v2.4 ISO 8601 yyyy-mm-dd or part thereof
      else if (key == "YEAR" || key == "DATE_RECORDED")
        albumtag.SetReleaseDate(tag->value);
      else if (key == "ORIGYEAR") // ISO 8601 as above. Equates to TDOR in id3v2.4 (set by mp3tag)
        albumtag.SetOriginalDate(tag->value);
      else if (key == "MOOD")
        albumtag.SetMood(tag->value);
      // genre could be comma delimited or not. Temporarily add the comma just in case.  true trims
      // any whitespace around the genre(s)
      else if (key == "GENRE")
      {
        separators.emplace_back(",");
        albumtag.SetGenre(StringUtils::Split(tag->value, separators), true);
        separators.pop_back();
      }
      // comma separated list of role, person
      else if (key == "INVOLVEDPEOPLE")
      {
        tagdata = StringUtils::Split(tag->value, ",");
        AddCommaDelimitedString(tagdata, separators, albumtag);
      }
      else if (key == "SUBTITLE" || key == "SETSUBTITLE")
        albumtag.SetDiscSubtitle(tag->value);
      else if (key == "REMIXED_BY")
        albumtag.AddArtistRole("Remixer", tag->value);
      else if (key == "MIXED_BY" || key == "MIXER")
        albumtag.AddArtistRole("Mixer", tag->value);
      else if (key == "COMMENT")
        albumtag.SetComment(tag->value);
    }
  }

  std::string thumb;

  if (m_fctx->nb_chapters > 1)
    thumb = IMAGE_FILES::URLFromFile(url.Get(), "music");

  // now get the AudioCodec -------------------------------------
  bool haveFFmpegInfo = false;
  musicCodecInfo codec_info;
  haveFFmpegInfo = CMusicCodecInfoFFmpeg::GetMusicCodecInfo(url.Get(), codec_info);
  if (haveFFmpegInfo) // use data from FFmpeg (taglib 2.3 does not support some codecs)
  {
    albumtag.SetBitRate(codec_info.bitRate);
    albumtag.SetSampleRate(codec_info.sampleRate);
    /*!
    * Additional Music properties (next PR - Add Album Codec Support to Music)
    * albumtag.SetBitsPerSample(codec_info.bitsPerSample);
    * albumtag.SetCodec(codec_info.codecName); // e.g. 'truehd_atmos', 'dts_ma', 'dts_hd', etc
    */
    albumtag.SetNoOfChannels(codec_info.channels);
    albumtag.SetDuration(codec_info.duration);
  }

  float chapter_size = 0;

  bool chapter_error = false;
  for (size_t i=0;i<m_fctx->nb_chapters;++i)
  {
    if (m_fctx->chapters[i]->start < 0) // negative start time, ignore it
      continue;
    chapter_size = m_fctx->chapters[i]->end * av_q2d(m_fctx->chapters[i]->time_base);
    if (chapter_size < 1)
    {
      CLog::Log(LOGWARNING,
                "CAudioBookFileDirectory: Tiny chapter of size {}s detected when scanning {} Most "
                "likely this file needs the chapters correcting",
                chapter_size, url.GetRedacted());
      chapter_error = true;
      continue;
    }
    tag=nullptr;
    std::string chaptitle = StringUtils::Format(
        CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(25010), i + 1);
    std::string chapauthor;
    std::string chapalbum;

    std::shared_ptr<CFileItem> item(new CFileItem(url.Get(), false));
    *item->GetMusicInfoTag() = albumtag;

    if (isAudioBook)
    {
      while ((tag = av_dict_get(m_fctx->chapters[i]->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
      {
        if (StringUtils::CompareNoCase(tag->key, "title") == 0)
          chaptitle = tag->value;
        else if (StringUtils::CompareNoCase(tag->key, "artist") == 0)
          chapauthor = tag->value;
        else if (StringUtils::CompareNoCase(tag->key, "album") == 0)
          chapalbum = tag->value;
      }
      else
      {
        std::string key = StringUtils::ToUpper(tag->key);
        if (key == "TITLE")
          item->GetMusicInfoTag()->SetTitle(tag->value);
        else if (key == "ARTIST")
          item->GetMusicInfoTag()->SetArtist(tag->value);
        else if (key == "MUSICBRAINZ_TRACKID")
          item->GetMusicInfoTag()->SetMusicBrainzTrackID(tag->value);
        else if (key == "COMPOSER")
          addRole("Composer", tag->value);
        else if (key == "LYRICIST")
          addRole("Lyricist", tag->value);
        else if (key == "CONDUCTOR")
          addRole("Conductor", tag->value);
        else if (key == "WRITER")
          addRole("Writer", tag->value);
        else if (key == "ARRANGER")
          addRole("Arranger", tag->value);
        else if (key == "BAND")
          addRole("Band", tag->value);
        else if (key == "ENGINEER")
          addRole("Engineer", tag->value);
        else if (key == "PRODUCER")
          addRole("Producer", tag->value);
        else if (key == "REMIXED_BY")
          addRole("Remixer", tag->value);
        else if (key == "YEAR" || key == "DATE_RECORDED")
          item->GetMusicInfoTag()->SetReleaseDate(tag->value);
        else if (key == "ORIGYEAR")
          item->GetMusicInfoTag()->SetOriginalDate(tag->value);
        else if (key == "MIXED_BY" || key == "MIXER"  )
          addRole("Mixer", tag->value);
        else if (key == "SUBTITLE" || key == "SETSUBTITLE")
          item->GetMusicInfoTag()->SetDiscSubtitle(tag->value);
        else if (key == "COMMENT")
          item->GetMusicInfoTag()->SetComment(tag->value);
        else if (key == "MOOD")
          item->GetMusicInfoTag()->SetMood(tag->value);
        else if (key == "GENRE")
        {
          separators.emplace_back(",");
          item->GetMusicInfoTag()->SetGenre(StringUtils::Split(tag->value, separators), true);
          separators.pop_back();
        }
        // comma separated list of instrument, person
        else if (key == "INSTRUMENTS")
        {
          tagdata = StringUtils::Split(tag->value, ",");
          AddCommaDelimitedString(tagdata, separators, *item->GetMusicInfoTag());
        }
        // comma separated list of role, person
        else if (key == "INVOLVEDPEOPLE")
        {
          tagdata = StringUtils::Split(tag->value, ",");
          AddCommaDelimitedString(tagdata, separators, *item->GetMusicInfoTag());
        }
      }
      /* The comma separated lists are outside the Matroska spec
         (see https://www.matroska.org/technical/tagging.html) as it states to use multiple simple
         tags for eg 2 or more composers.  However, ffmpeg returns just the last tag and drops the
         rest (https://trac.ffmpeg.org/ticket/9641).  Therefore until (if) it gets fixed, this is
         the best solution.
       */
    }
    if (isAudioBook)
    {
      item->GetMusicInfoTag()->SetTitle(chaptitle);
      item->GetMusicInfoTag()->SetAlbum(chapalbum.empty() ? album.empty() ? title : album
                                                          : chapalbum);
      item->GetMusicInfoTag()->SetArtist(chapauthor.empty() ? author : chapauthor);
      if (!desc.empty())
        item->GetMusicInfoTag()->SetComment(desc);

      item->SetStartOffset(CUtil::ConvertSecsToMilliSecs(m_fctx->chapters[i]->start *
                                                         av_q2d(m_fctx->chapters[i]->time_base)));
      item->SetEndOffset(CUtil::ConvertSecsToMilliSecs(m_fctx->chapters[i]->end *
                                                       av_q2d(m_fctx->chapters[i]->time_base)));
      item->GetMusicInfoTag()->SetDuration(
          CUtil::ConvertMilliSecsToSecsInt(item->GetEndOffset() - item->GetStartOffset()));
    }
    else
    {
      // process chapter tags for this track using file-order chapter UID
      if (i < chapterOrder.size())
      {
        auto it = chapterTags.find(std::get<0>(chapterOrder[i]));
        if (it != chapterTags.end())
        {
          for (const auto& Tracktag : it->second)
            CMusicInfoTagLoaderMatroska::ParseTag(Tracktag.first, Tracktag.second, separators,
                                                  musicsep, *item->GetMusicInfoTag());

          item->SetStartOffset(CUtil::ConvertSecsToMilliSecs(std::get<2>(chapterOrder[i])));
          item->SetEndOffset(CUtil::ConvertSecsToMilliSecs(std::get<3>(chapterOrder[i])));
          item->GetMusicInfoTag()->SetDuration(
              CUtil::ConvertMilliSecsToSecsInt(item->GetEndOffset() - item->GetStartOffset()));
        }
      }
    }

    item->GetMusicInfoTag()->SetTrackNumber(i + 1);
    item->GetMusicInfoTag()->SetLoaded(true);

    item->SetLabel(StringUtils::Format("{0:02}. {1} - {2}", i + 1,
                                       item->GetMusicInfoTag()->GetAlbum(),
                                       item->GetMusicInfoTag()->GetTitle()));

    item->SetProperty("item_start", item->GetStartOffset());
    item->SetProperty("audio_bookmark", item->GetStartOffset());
    if (!thumb.empty() && !chapter_error)
      item->SetArt("thumb", thumb);
    items.Add(item);
  }
  return true;
}

bool CAudioBookFileDirectory::Exists(const CURL& url)
{
  return CFile::Exists(url);
}

bool CAudioBookFileDirectory::ContainsFiles(const CURL& url)
{
  CFile file;
  if (!file.Open(url))
    return false;

  uint8_t* buffer = static_cast<uint8_t*>(av_malloc(32768));
  if (!buffer)
    return false;

  m_ioctx = avio_alloc_context(buffer, 32768, 0, &file, cfile_file_read, nullptr, cfile_file_seek);
  if (!m_ioctx)
  {
    av_free(buffer);
    return false;
  }

  m_fctx = avformat_alloc_context();
  if (!m_fctx)
  {
    av_free(m_ioctx->buffer);
    av_free(m_ioctx);
    m_ioctx = nullptr;
    return false;
  }
  m_fctx->pb = m_ioctx;
  m_fctx->flags |= AVFMT_FLAG_CUSTOM_IO;

  if (file.IoControl(IOControl::SEEK_POSSIBLE, nullptr) == 0)
    m_ioctx->seekable = 0;

  m_ioctx->max_packet_size = 32768;

  const AVInputFormat* iformat = nullptr;
  av_probe_input_buffer(m_ioctx, &iformat, url.Get().c_str(), nullptr, 0, 0);

  bool contains = false;

  if (avformat_open_input(&m_fctx, url.Get().c_str(), iformat, nullptr) < 0)
  {
    if (m_fctx)
      avformat_close_input(&m_fctx);
    av_free(m_ioctx->buffer);
    av_free(m_ioctx);
    m_ioctx = nullptr;
    return false;
  }
  m_fctx->flags |= AVFMT_FLAG_NOPARSE;
  int err = avformat_find_stream_info(m_fctx, NULL);
  if (err < 0)
    CLog::Log(LOGERROR, "Can't detect codec info in file {}", url.GetRedacted());

  contains = m_fctx->nb_chapters > 1;

  return contains;
}
