/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicInfoTag.h"
#include "MusicInfoTagLoaderFFmpeg.h"
#include "cores/FFmpeg.h"
#include "filesystem/File.h"
#include "music/MusicEmbeddedCoverLoaderFFmpeg.h"
#include "settings/AdvancedSettings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"

using namespace MUSIC_INFO;
using namespace XFILE;

static int vfs_file_read(void* h, uint8_t* buf, int size)
{
  CFile* pFile = static_cast<CFile*>(h);
  return pFile->Read(buf, size);
}

static int64_t vfs_file_seek(void* h, int64_t pos, int whence)
{
  CFile* pFile = static_cast<CFile*>(h);
  if (whence == AVSEEK_SIZE)
    return pFile->GetLength();
  else
    return pFile->Seek(pos, whence & ~AVSEEK_FORCE);
}

CMusicInfoTagLoaderFFmpeg::CMusicInfoTagLoaderFFmpeg(void) = default;

CMusicInfoTagLoaderFFmpeg::~CMusicInfoTagLoaderFFmpeg() = default;

bool CMusicInfoTagLoaderFFmpeg::Load(const std::string& strFileName,
                                     CMusicInfoTag& tag,
                                     EmbeddedArt* art)
{
  tag.SetLoaded(false);

  CFile file;
  if (!file.Open(strFileName))
    return false;

  int bufferSize = 4096;
  int blockSize = file.GetChunkSize();
  if (blockSize > 1)
    bufferSize = blockSize;
  uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
  AVIOContext* ioctx =
      avio_alloc_context(buffer, bufferSize, 0, &file, vfs_file_read, NULL, vfs_file_seek);

  AVFormatContext* fctx = avformat_alloc_context();
  fctx->pb = ioctx;

  if (file.IoControl(IOControl::SEEK_POSSIBLE, NULL) != 1)
    ioctx->seekable = 0;

  const AVInputFormat* iformat = nullptr;
  av_probe_input_buffer(ioctx, &iformat, strFileName.c_str(), NULL, 0, 0);

  if (avformat_open_input(&fctx, strFileName.c_str(), iformat, NULL) < 0)
  {
    if (fctx)
      avformat_close_input(&fctx);
    av_free(ioctx->buffer);
    av_free(ioctx);
    return false;
  }

  /* ffmpeg supports the return of ID3v2 metadata but has its own naming system
     for some, but not all, of the keys. In particular the key for the conductor
     tag TPE3 is called "performer".
     See https://github.com/xbmc/FFmpeg/blob/master/libavformat/id3v2.c#L43
     Other keys are retuened using their 4 char name.
     Only single frame values are returned even for v2.4 fomart tags e.g. while
     tagged with multiple TPE1 frames "artist1", "artist2", "artist3" only
     "artist1" is returned by ffmpeg.
     Hence, like with v2.3 format tags, multiple values for artist, genre etc.
     need to be combined when tagging into a single value using a known item
     separator e.g. "artist1 / artist2 / artist3"

     Any changes to ID3v2 tag processing in CTagLoaderTagLib need to be
     repeated here
  */
  std::vector<std::string> separators{" feat. ", " ft. ", " Feat. ", " Ft. ",  ";", ":",
                                      "|",       "#",     "/",       " with ", "&"};
  const std::string musicsep =
      CServiceBroker::GetSettingsComponent()->GetAdvancedSettings()->m_musicItemSeparator;
  if (musicsep.find_first_of(";/,&|#") == std::string::npos)
    separators.push_back(musicsep);
  std::vector<std::string> tagdata;

  auto&& ParseTag = [&](AVDictionaryEntry* avtag)
  {
    if (StringUtils::CompareNoCase(avtag->key, "album") == 0)
      tag.SetAlbum(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "artist") == 0)
      tag.SetArtist(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "album_artist") == 0 ||
             StringUtils::CompareNoCase(avtag->key, "album artist") == 0)
      tag.SetAlbumArtist(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "title") == 0)
      tag.SetTitle(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "genre") == 0)
      tag.SetGenre(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "part_number") == 0 ||
             StringUtils::CompareNoCase(avtag->key, "track") == 0)
      tag.SetTrackNumber(static_cast<int>(strtol(avtag->value, nullptr, 10)));
    else if (StringUtils::CompareNoCase(avtag->key, "disc") == 0)
      tag.SetDiscNumber(static_cast<int>(strtol(avtag->value, nullptr, 10)));
    else if (StringUtils::CompareNoCase(avtag->key, "date") == 0)
      tag.SetReleaseDate(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "compilation") == 0)
      tag.SetCompilation((strtol(avtag->value, nullptr, 10) == 0) ? false : true);
    else if (StringUtils::CompareNoCase(avtag->key, "encoded_by") == 0)
    {
    }
    else if (StringUtils::CompareNoCase(avtag->key, "composer") == 0)
      tag.AddArtistRole("Composer", avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "performer") == 0) // Conductor or TPE3 tag
      tag.AddArtistRole("Conductor", avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "TEXT") == 0)
      tag.AddArtistRole("Lyricist", avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "TPE4") == 0)
      tag.AddArtistRole("Remixer", avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "LABEL") == 0 ||
             StringUtils::CompareNoCase(avtag->key, "TPUB") == 0)
      tag.SetRecordLabel(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "copyright") == 0 ||
             StringUtils::CompareNoCase(avtag->key, "TCOP") == 0)
    {
    } // Copyright message
    else if (StringUtils::CompareNoCase(avtag->key, "TDRC") == 0)
      tag.SetReleaseDate(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "TDOR") == 0 ||
             StringUtils::CompareNoCase(avtag->key, "TORY") == 0)
      tag.SetOriginalDate(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "TDAT") == 0)
      tag.AddReleaseDate(avtag->value, true); // MMDD part
    else if (StringUtils::CompareNoCase(avtag->key, "TYER") == 0)
      tag.AddReleaseDate(avtag->value); // YYYY part
    else if (StringUtils::CompareNoCase(avtag->key, "TBPM") == 0)
      tag.SetBPM(static_cast<int>(strtol(avtag->value, nullptr, 10)));
    else if (StringUtils::CompareNoCase(avtag->key, "TDTG") == 0)
    {
    } // Tagging time
    else if (StringUtils::CompareNoCase(avtag->key, "language") == 0 ||
             StringUtils::CompareNoCase(avtag->key, "TLAN") == 0)
    {
    } // Languages
    else if (StringUtils::CompareNoCase(avtag->key, "mood") == 0 ||
             StringUtils::CompareNoCase(avtag->key, "TMOO") == 0)
      tag.SetMood(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "artist-sort") == 0 ||
             StringUtils::CompareNoCase(avtag->key, "TSOP") == 0)
    {
    }
    else if (StringUtils::CompareNoCase(avtag->key, "TSO2") == 0)
    {
    } // Album artist sort
    else if (StringUtils::CompareNoCase(avtag->key, "TSOC") == 0)
    {
    } // composer sort
    else if (StringUtils::CompareNoCase(avtag->key, "TSST") == 0)
      tag.SetDiscSubtitle(avtag->value);
    // the above values are all id3v2.3/4 frames, we could also have text frames
    else if (StringUtils::CompareNoCase(avtag->key, "MUSICBRAINZ ARTIST ID") == 0)
      tag.SetMusicBrainzArtistID(StringUtils::Split(avtag->value, separators));
    else if (StringUtils::CompareNoCase(avtag->key, "MUSICBRAINZ ALBUM ID") == 0)
      tag.SetMusicBrainzAlbumID(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "MUSICBRAINZ RELEASEGROUP ID") == 0)
      tag.SetMusicBrainzReleaseGroupID(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "MUSICBRAINZ ALBUM ARTIST ID") == 0)
      tag.SetMusicBrainzAlbumArtistID(StringUtils::Split(avtag->value, separators));
    else if (StringUtils::CompareNoCase(avtag->key, "MUSICBRAINZ ALBUM ARTIST") == 0)
      tag.SetAlbumArtist(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "MUSICBRAINZ ALBUM TYPE") == 0)
      tag.SetMusicBrainzReleaseType(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "MUSICBRAINZ ALBUM STATUS") == 0)
      tag.SetAlbumReleaseStatus(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "ALBUM ARTIST") == 0)
      tag.SetAlbumArtist(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "ALBUMARTIST") == 0)
      tag.SetAlbumArtist(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "ALBUM ARTIST SORT") == 0)
      tag.SetAlbumArtistSort(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "ALBUMARTISTSORT") == 0)
      tag.SetAlbumArtistSort(avtag->value);
    else if (StringUtils::CompareNoCase(avtag->key, "ARTISTS") == 0)
      tag.SetMusicBrainzArtistHints(StringUtils::Split(avtag->value, separators));
    else if (StringUtils::CompareNoCase(avtag->key, "ALBUMARTISTS") == 0)
      tag.SetMusicBrainzAlbumArtistHints(StringUtils::Split(avtag->value, separators));
    else if (StringUtils::CompareNoCase(avtag->key, "ALBUM ARTISTS") == 0)
      tag.SetMusicBrainzAlbumArtistHints(StringUtils::Split(avtag->value, separators));
    else if (StringUtils::CompareNoCase(avtag->key, "WRITER") == 0)
      tag.AddArtistRole("Writer", StringUtils::Split(avtag->value, separators));
    else if (StringUtils::CompareNoCase(avtag->key, "PERFORMER") == 0)
    {
      tagdata = StringUtils::Split(avtag->key, separators);
      AddRole(tagdata, separators, tag);
    }
    else if (StringUtils::CompareNoCase(avtag->key, "ARRANGER") == 0)
    {
      tagdata = StringUtils::Split(avtag->key, separators);
      AddRole(tagdata, separators, tag);
    }
    else if (StringUtils::CompareNoCase(avtag->key, "LYRICIST") == 0)
      tag.AddArtistRole("Lyricist", StringUtils::Split(avtag->value, separators));
    else if (StringUtils::CompareNoCase(avtag->key, "COMPOSER") == 0)
      tag.AddArtistRole("Composer", StringUtils::Split(avtag->value, separators));
    else if (StringUtils::CompareNoCase(avtag->key, "CONDUCTOR") == 0)
      tag.AddArtistRole("Conductor", StringUtils::Split(avtag->value, separators));
    else if (StringUtils::CompareNoCase(avtag->key, "ENGINEER") == 0)
      tag.AddArtistRole("Engineer", StringUtils::Split(avtag->value, separators));
  };

  AVDictionaryEntry* avtag = nullptr;
  while ((avtag = av_dict_get(fctx->metadata, "", avtag, AV_DICT_IGNORE_SUFFIX)))
    ParseTag(avtag);

  const AVStream* st = fctx->streams[0];
  if (st)
    while ((avtag = av_dict_get(st->metadata, "", avtag, AV_DICT_IGNORE_SUFFIX)))
      ParseTag(avtag);

  // Look for any embedded cover art
  CMusicEmbeddedCoverLoaderFFmpeg::GetEmbeddedCover(fctx, tag, art);
  bool haveFFmpegInfo = false;
  musicCodecInfo codec_info;
  haveFFmpegInfo = CMusicCodecInfoFFmpeg::GetMusicCodecInfo(strFileName, codec_info);
  if (haveFFmpegInfo) // use data from FFmpeg if taglib data missing or not accurate
  {
    tag.SetBitRate(codec_info.bitRate);
    tag.SetSampleRate(codec_info.sampleRate);
    tag.SetBitsPerSample(codec_info.bitsPerSample);
    tag.SetCodec(codec_info.codecName);
  }

  if (!tag.GetTitle().empty())
    tag.SetLoaded(true);

  avformat_close_input(&fctx);
  av_free(ioctx->buffer);
  av_free(ioctx);

  return true;
}

void CMusicInfoTagLoaderFFmpeg::AddRole(const std::vector<std::string>& data,
                                        const std::vector<std::string>& separators,
                                        MUSIC_INFO::CMusicInfoTag& musictag)
{
  if (!data.empty())
  {
    for (size_t i = 0; i + 1 < data.size(); i += 2)
    {
      std::vector<std::string> roles = StringUtils::Split(data[i], separators);
      for (auto& role : roles)
      {
        StringUtils::Trim(role);
        StringUtils::ToCapitalize(role);
        musictag.AddArtistRole(role, StringUtils::Split(data[i + 1], separators));
      }
    }
  }
}