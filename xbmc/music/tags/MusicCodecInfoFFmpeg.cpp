/*
 *  Copyright (C) 2005-2022 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "MusicCodecInfoFFmpeg.h"
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

bool CMusicCodecInfoFFmpeg::GetMusicCodecInfo(const std::string& strFileName,
                                              musicCodecInfo& codec_info)
{
  const AVCodec* decoder = nullptr;
  std::string codec;
  CFile file;
  bool haveInfo = false;
  if (!file.Open(strFileName))
    return haveInfo;

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

  if (!ioctx)
  {
    avformat_close_input(&fctx);
    if (ioctx)
    {
      av_freep(&ioctx->buffer);
      av_freep(&ioctx);
    }
    return haveInfo;
  }

  AVStream* st = nullptr;
  if (avformat_open_input(&fctx, strFileName.c_str(), nullptr, nullptr) == 0)
  {
    fctx->flags |= AVFMT_FLAG_NOPARSE;
    if (avformat_find_stream_info(fctx, nullptr) >= 0)
    {
      bool gotStream = false;
      for (unsigned int i = 0; i < fctx->nb_streams; ++i)
      {
        st = fctx->streams[i];
        if (st->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
          gotStream = true;
          break;
        }
        else
          continue;
      }
      if (!gotStream)
        return haveInfo;
      {
        decoder = avcodec_find_decoder(st->codecpar->codec_id);
      }
    }
    if (decoder)
    {
      std::string codec_name = "unknown";

      bool isMasterAudio = false;
      bool hasAtmos = false;
      AVDictionaryEntry* tag=nullptr;
      // Codec test only detects dts core - see if we can get a hint from the stream's metadata
      // ffmpeg 6.0.1 can't detect HD audio profiles, HDMA can be figured from the codec priv_data
      while ((tag = av_dict_get(st->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
      {
        std::string codecDesc = StringUtils::ToUpper(tag->value);
        if (StringUtils::Contains(codecDesc, "HDMA") ||
            StringUtils::Contains(codecDesc, "DTS-HD") ||
            StringUtils::Contains(codecDesc, "Master Audio"))
        {
          isMasterAudio = true;
          break;
        }
        else if (StringUtils::Contains(codecDesc, "ATMOS") ||
                 StringUtils::Contains(codecDesc, "JOC"))
        {
          hasAtmos = true;
          break;
        }
      }
      const AVCodec* dec = nullptr;
      AVCodecContext* codec_ctx = nullptr;

      codec_name = avcodec_get_name(st->codecpar->codec_id);
      dec = avcodec_find_decoder(st->codecpar->codec_id);
      codec_ctx = avcodec_alloc_context3(dec);
      avcodec_parameters_to_context(codec_ctx, st->codecpar);

      if (nullptr != codec_ctx->priv_data)
      {
        uint8_t* priv_bytes = (uint8_t*)codec_ctx->priv_data;
        if (priv_bytes[0] == 0x40) //0x40 - dtshd_ma 0x80 - truehd
          isMasterAudio = true;
      }
      avcodec_free_context(&codec_ctx);
      av_free(codec_ctx);

      if (dec->id == AV_CODEC_ID_DTS)
      {
        if ((dec->profiles->profile == FF_PROFILE_DTS_HD_MA) || isMasterAudio)
          codec_name = "dtshd_ma";
        else if (dec->profiles->profile == FF_PROFILE_DTS_HD_MA_X)
          codec_name = "dts_x";
        else if (dec->profiles->profile == FF_PROFILE_DTS_HD_MA_X_IMAX)
          codec_name = "dts_x_imax";
        else
          codec_name = "dca";
      }
      if (dec->id == AV_CODEC_ID_EAC3 &&
          (hasAtmos || (dec->profiles->profile == FF_PROFILE_EAC3_DDP_ATMOS)))
        codec_name = "eac3_ddp_atmos";

      if (dec->id == AV_CODEC_ID_TRUEHD &&
          (hasAtmos || (dec->profiles->profile == FF_PROFILE_TRUEHD_ATMOS)))
        codec_name = "truehd_atmos";
      codec_info.codecName = codec_name;
      codec_info.bitRate = static_cast<int>(st->codecpar->bit_rate / 1000);
      codec_info.channels = st->codecpar->ch_layout.nb_channels;
      codec_info.bitsPerSample = (st->codecpar->bits_per_coded_sample != 0)
                                     ? st->codecpar->bits_per_coded_sample
                                     : st->codecpar->bits_per_raw_sample;
      codec_info.sampleRate = st->codecpar->sample_rate;
      codec_info.duration = st->duration / AV_TIME_BASE;
      if (st->codecpar->codec_id == AV_CODEC_ID_DTS)
      {
        if (st->codecpar->profile == FF_PROFILE_DTS_HD_MA)
          codec_name = "dtshd_ma";
        else if (st->codecpar->profile == FF_PROFILE_DTS_HD_MA_X)
          codec_name = "dtshd_ma_x";
        else if (st->codecpar->profile == FF_PROFILE_DTS_HD_MA_X_IMAX)
          codec_name = "dtshd_ma_x_imax";
        else if (st->codecpar->profile == FF_PROFILE_DTS_HD_HRA)
          codec_name = "dtshd_hra";
        else
          codec_name = "dca";
      }
      if (st->codecpar->codec_id == AV_CODEC_ID_EAC3 &&
          st->codecpar->profile == AV_PROFILE_EAC3_DDP_ATMOS)
        codec_name = "eac3_ddp_atmos";

      if (st->codecpar->codec_id == AV_CODEC_ID_TRUEHD &&
          st->codecpar->profile == AV_PROFILE_TRUEHD_ATMOS)
        codec_name = "truehd_atmos";
      codec_info.codecName = codec_name;
      haveInfo= true;
    }
  }

  if (fctx)
    avformat_close_input(&fctx);
  if (ioctx)
  {
    av_free(ioctx->buffer);
    av_free(ioctx);
  }
  return haveInfo;
}
