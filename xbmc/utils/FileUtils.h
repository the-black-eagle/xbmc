/*
 *  Copyright (C) 2010-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItem.h"

#include <string>

// If statx is available we will use that to get the file creation (birth) date and time on Posix
// If we are running on a kernel that doesn't support statx the wrapper will use stat but fill the
// statx buffer with the results - in that case we won't get btime, just ctime and mtime
#ifdef STATX_TYPE
#define __statx_defined 1
#endif

class CFileUtils
{
public:
  static bool CheckFileAccessAllowed(const std::string &filePath);
  static bool DeleteItem(const CFileItemPtr &item);
  static bool DeleteItem(const std::string &strPath);
  static bool RenameFile(const std::string &strFile);
  static bool RemoteAccessAllowed(const std::string &strPath);
  static unsigned int LoadFile(const std::string &filename, void* &outputBuffer);
  /*! \brief Get the modified date of a file if its invalid it returns the creation date - this behavior changes when you set bUseLatestDate
  \param strFileNameAndPath path to the file
  \param bUseLatestDate use the newer datetime of the files mtime and ctime
  \return Returns the file date, can return a invalid date if problems occur
  */
  static CDateTime GetModificationDate(const std::string& strFileNameAndPath, const bool& bUseLatestDate);
  static CDateTime GetModificationDate(const int& code, const std::string& strFileNameAndPath);
  /*! \brief Get the creation date of a file if possible
  \param strFileNameAndPath path to the file
  \return Returns the creation date, else returns the earlier of the files ctime and mtime if valid
  */
  static CDateTime GetCreationDate(const std::string& strFileNameAndPath);
};
