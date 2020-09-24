/*
 *  Copyright (C) 2005-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */


// FileShoutcast.cpp: implementation of the CShoutcastFile class.
//
//////////////////////////////////////////////////////////////////////

#include "ShoutcastFile.h"

#include "FileCache.h"
#include "FileItem.h"
#include "URL.h"
#include "guilib/GUIWindowManager.h"
#include "messaging/ApplicationMessenger.h"
#include "utils/CharsetConverter.h"
#include "utils/HTMLUtil.h"
#include "utils/JSONVariantParser.h"
#include "utils/RegExp.h"
#include "utils/StringUtils.h"

#include <climits>

using namespace XFILE;
using namespace MUSIC_INFO;
using namespace KODI::MESSAGING;

CShoutcastFile::CShoutcastFile() :
  IFile(), CThread("ShoutcastFile")
{
  m_discarded = 0;
  m_currint = 0;
  m_buffer = NULL;
  m_cacheReader = NULL;
  m_tagPos = 0;
  m_metaint = 0;
}

CShoutcastFile::~CShoutcastFile()
{
  Close();
}

int64_t CShoutcastFile::GetPosition()
{
  return m_file.GetPosition()-m_discarded;
}

int64_t CShoutcastFile::GetLength()
{
  return 0;
}

bool CShoutcastFile::Open(const CURL& url)
{
  CURL url2(url);
  url2.SetProtocolOptions(url2.GetProtocolOptions()+"&noshout=true&Icy-MetaData=1");
  if (url.GetProtocol() == "shouts")
    url2.SetProtocol("https");
  else if (url.GetProtocol() == "shout")
    url2.SetProtocol("http");

  bool result = m_file.Open(url2);
  if (result)
  {
    std::string icyTitle;
    icyTitle = m_file.GetHttpHeader().GetValue("icy-name");
    if (icyTitle.empty())
      icyTitle = m_file.GetHttpHeader().GetValue("ice-name"); // icecast
    if (icyTitle == "This is my server name") // Handle badly set up servers
      icyTitle.clear();

    m_tag.SetGenre(m_file.GetHttpHeader().GetValue("icy-genre"));
    if (m_tag.GetGenre().empty())
      m_tag.SetGenre(m_file.GetHttpHeader().GetValue("ice-genre")); // icecast

    m_tag.SetStreamType(CMusicInfoTag::StreamType::SHOUTCAST);
    m_tag.SetStationName(icyTitle);
    m_tag.SetLoaded(true);
  }
  m_fileCharset = m_file.GetProperty(XFILE::FILE_PROPERTY_CONTENT_CHARSET);
  m_metaint = atoi(m_file.GetHttpHeader().GetValue("icy-metaint").c_str());
  if (!m_metaint)
    m_metaint = -1;
  m_buffer = new char[16*255];
  m_tagPos = 1;

  return result;
}

ssize_t CShoutcastFile::Read(void* lpBuf, size_t uiBufSize)
{
  if (uiBufSize > SSIZE_MAX)
    uiBufSize = SSIZE_MAX;

  if (m_currint >= m_metaint && m_metaint > 0)
  {
    unsigned char header;
    m_file.Read(&header,1);
    ReadTruncated(m_buffer, header*16);
    if ((ExtractTagInfo(m_buffer)
        // this is here to workaround issues caused by application posting callbacks to itself (3cf882d9)
        // the callback will set an empty tag in the info manager item, while we think we have ours set
        || m_file.GetPosition() < 10*m_metaint) && !m_tagPos)
    {
      m_tagPos = m_file.GetPosition();
      m_tagChange.Set();
    }
    m_discarded += header*16+1;
    m_currint = 0;
  }

  ssize_t toRead;
  if (m_metaint > 0)
    toRead = std::min<size_t>(uiBufSize,m_metaint-m_currint);
  else
    toRead = std::min<size_t>(uiBufSize,16*255);
  toRead = m_file.Read(lpBuf,toRead);
  if (toRead > 0)
    m_currint += toRead;
  return toRead;
}

int64_t CShoutcastFile::Seek(int64_t iFilePosition, int iWhence)
{
  return -1;
}

void CShoutcastFile::Close()
{
  StopThread();
  delete[] m_buffer;
  m_buffer = NULL;
  m_file.Close();
  m_tag.Clear();
  m_title.clear();
  m_haveExtraData = false;
}

bool CShoutcastFile::ExtractTagInfo(const char* buf)
{
  std::string strBuffer = buf;

  if (!m_fileCharset.empty())
  {
    std::string converted;
    g_charsetConverter.ToUtf8(m_fileCharset, strBuffer, converted);
    strBuffer = converted;
  }
  else
    g_charsetConverter.unknownToUTF8(strBuffer);

  bool result=false;

  std::wstring wBuffer, wConverted;
  g_charsetConverter.utf8ToW(strBuffer, wBuffer, false);
  HTML::CHTMLUtil::ConvertHTMLToW(wBuffer, wConverted);
  g_charsetConverter.wToUTF8(wConverted, strBuffer);

  CRegExp reTitle(true);
  reTitle.RegComp("StreamTitle=\'(.*?)\';");

  if (reTitle.RegFind(strBuffer.c_str()) != -1)
  {
    const std::string newTitle = reTitle.GetMatch(1);

    /* Example of data contained in the metadata buffer (v1 streams only contain the StreamTitle)
       StreamTitle='Tuesday's Gone - Lynyrd Skynyrd';
       StreamUrl='https://listenapi.planetradio.co.uk/api9/eventdata/58431417';
    */

    CRegExp reURL(true);
    reURL.RegComp("StreamUrl=\'(.*?)\';");
    m_haveExtraData = (reURL.RegFind(strBuffer.c_str()) != -1) && !reURL.GetMatch(1).empty();

    CSingleLock lock(m_tagSection);
    result = (m_title != newTitle);
    if (result) // track has changed
    {
      m_title = newTitle;

      std::string artistInfo;
      std::string title;
      std::string coverURL;

      if (m_haveExtraData) // track has changed and extra metadata might be available
      {
        const std::string serverUrl = reURL.GetMatch(1);
        if (!serverUrl.empty() && serverUrl.substr(0,4) == "http")
        {
          XFILE::CCurlFile http;
          std::string extData;
          CURL dataURL(serverUrl);
          if (http.Get(dataURL.Get(), extData))
          { // No webpages thanks - Many US stations link back to their websites rather than data
            if (extData.find("<html") == std::string::npos &&
                extData.find("<HTML") == std::string::npos)
            {
              CVariant variant;
              if (CJSONVariantParser::Parse(extData, variant))
              {
                /* Example of data returned from the server
                   {"eventId":58431417,"eventStart":"2020-09-15 10:03:23","eventFinish":"2020-09-15 10:10:43","eventDuration":438,"eventType":"Song","eventSongTitle":"Tuesday's Gone",
                   "eventSongArtist":"Lynyrd Skynyrd",
                   "eventImageUrl":"https://assets.planetradio.co.uk/artist/1-1/320x320/753.jpg?ver=1465083598",
                   "eventImageUrlSmall":"https://assets.planetradio.co.uk/artist/1-1/160x160/753.jpg?ver=1465083598",
                   "eventAppleMusicUrl":"https://geo.itunes.apple.com/dk/album/287661543?i=287661795"}
                */

                artistInfo = variant["eventSongArtist"].asString();
                title = variant["eventSongTitle"].asString();
                coverURL = variant["eventImageUrl"].asString();
              }
            }
            else // we got a webpage so use what data we have, else artist and title will be blank
            {
              const std::vector<std::string> tokens = StringUtils::Split(newTitle, " - ");
              if (tokens.size() == 2)
              {
                artistInfo = tokens[0];
                title = tokens[1];
              }
              else
              {
                title = newTitle;
              }
            }
          }
        }
        else if (serverUrl.find("&artist=") != std::string::npos) // data is url encoded in serverUrl
        {
          /*Examples of data from server - Note the picture data is not a full URL so we can't
          fetch it for the second example - just grab the artist and track info
          &artist=Steely%20Dan&title=Deacon%20Blues&album=The%20Best%20of%20Steely%20Dan%20Then%20And%20Now

          &artist=Jimi%20Hendrix&title=Hey%20Joe&album=Cornerstones%201967-1970
          &duration=206211&songtype=S&overlay=NO&buycd=&website=
          &picture=az_1810_Experience%20Hendrix%20The%20Best%20Of%20Jimi%20Hendrix_Jimi%20Hendrix.jpg
         */
          std::map<std::string, std::string> params;
          std::stringstream paramlist(CURL::Decode(serverUrl));
          std::string keyval, key, val;

          while (std::getline(paramlist, keyval, '&')) // split each term
          {
            std::istringstream iss(keyval);
            // split key/value pairs
            if (std::getline(std::getline(iss, key, '='), val))
              params[key] = val;
          }

          auto option = params.find("artist");
          if (option != params.end())
            artistInfo = option->second;
          option = params.find("title");
          if (option != params.end())
            title = option->second;

        }
      }
      else // track has changed and no extra metadata are available
      {
        const std::vector<std::string> tokens = StringUtils::Split(newTitle, " - ");
        if (tokens.size() == 2)
        {
          artistInfo = tokens[0];
          title = tokens[1];
        }
        else
        {
          title = newTitle;
        }
      }

      m_tag.SetArtist(artistInfo);
      m_tag.SetTitle(title);
      m_tag.SetShoutcastCover(coverURL);

      m_tagChange.Set();
    }
  }

  return result;
}

void CShoutcastFile::ReadTruncated(char* buf2, int size)
{
  char* buf = buf2;
  while (size > 0)
  {
    int read = m_file.Read(buf,size);
    size -= read;
    buf += read;
  }
}

int CShoutcastFile::IoControl(EIoControl control, void* payload)
{
  if (control == IOCTRL_SET_CACHE && m_cacheReader == nullptr)
  {
    m_cacheReader = (CFileCache*)payload;
    Create();
  }

  return IFile::IoControl(control, payload);
}

void CShoutcastFile::Process()
{
  while (!m_bStop)
  {
    if (m_tagChange.WaitMSec(500))
    {
      while (!m_bStop && m_cacheReader->GetPosition() < m_tagPos)
        CThread::Sleep(20);
      CSingleLock lock(m_tagSection);
      CApplicationMessenger::GetInstance().PostMsg(TMSG_UPDATE_CURRENT_ITEM, 1,-1, static_cast<void*>(new CFileItem(m_tag)));
      m_tagPos = 0;
    }
  }
}
