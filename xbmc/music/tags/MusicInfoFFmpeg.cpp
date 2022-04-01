/*
 *  Copyright (C) 2005-2022 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicInfoCodecFFmpeg.h"
#include "cores/FFmpeg.h"
#include "filesystem/File.h"


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

bool CMusicInfoCodecFFmpeg::GetMusicCodecInfo(const std::string& strFileName, musicCodecInfo& codec_info)
{
  AVCodec* decoder = nullptr;
  std::string codec = "";
  CFile file;
  bool res = false;
  if (!file.Open(strFileName))
    return res;

  int bufferSize = 4096;
  int blockSize = file.GetChunkSize();
  if (blockSize > 1)
    bufferSize = blockSize;
  uint8_t* buffer = (uint8_t*)av_malloc(bufferSize);
  AVIOContext* ioctx =
      avio_alloc_context(buffer, bufferSize, 0, &file, vfs_file_read, NULL, vfs_file_seek);

  AVFormatContext* fctx = avformat_alloc_context();
  fctx->pb = ioctx;

  if (file.IoControl(IOCTRL_SEEK_POSSIBLE, NULL) != 1)
    ioctx->seekable = 0;

  AVInputFormat* iformat = NULL;
  av_probe_input_buffer(ioctx, &iformat, strFileName.c_str(), NULL, 0, 0);

  if (!ioctx)
  {
    avformat_close_input(&fctx);
    if (ioctx)
    {
      av_freep(&ioctx->buffer);
      av_freep(&ioctx);
    }
    return res;
  }

  AVStream* st = nullptr;
  if (avformat_open_input(&fctx, strFileName.c_str(), nullptr, nullptr) == 0)
  {
    fctx->flags |= AVFMT_FLAG_NOPARSE;
    if (avformat_find_stream_info(fctx, nullptr) >= 0)
    {
      st = fctx->streams[0];
      if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
      {
        decoder = avcodec_find_decoder(st->codecpar->codec_id);
      }
    }
    if (decoder)
    {
      codec_info.codecName = decoder->name;
      codec_info.bitRate = static_cast<int>(st->codecpar->bit_rate / 1000);
      codec_info.channels = st->codecpar->channels;
      codec_info.bitsPerSample = (st->codecpar->bits_per_coded_sample != 0)
                                     ? st->codecpar->bits_per_coded_sample
                                     : st->codecpar->bits_per_raw_sample;
      codec_info.sampleRate = st->codecpar->sample_rate;
      res = true;
    }
  }

  if (fctx)
    avformat_close_input(&fctx);
  if (ioctx)
  {
    av_free(ioctx->buffer);
    av_free(ioctx);
  }
  return res;
}
