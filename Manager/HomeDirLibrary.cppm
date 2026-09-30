/*
MIT License

Copyright (c) 2024 Evgeny Kislov,
Copyright (c) 2026 FUZE.page

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
module;
// Detect platform
#if defined(linux) || defined(__linux) || defined(__linux__)
#define LINUX_PLATFORM
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define WINDOWS_PLATFORM
#elif defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__)
#define MACOS_PLATFORM
#else
#error Unknown platform
#endif

#if defined(LINUX_PLATFORM) || defined(MACOS_PLATFORM)

#include <string>

#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#elifdef WINDOWS_PLATFORM

#include <windows.h>
#include <shlobj.h>
#endif
export module HomeDirLibrary;

namespace HomeDirLibrary {
#if defined(LINUX_PLATFORM) || defined(MACOS_PLATFORM)
// --------------------------
// Linux and MacOS implementation
// --------------------------
const char kHomeEnvVar[] = "HOME";
const char kDataDir[] = "/.local/share";


export std::string getHomeDir() {
  try {
    auto ev = getenv(kHomeEnvVar);
    if (ev) {
      return std::string(ev);
    }
    auto pw = getpwuid(getuid());
    if (pw) {
      return std::string(pw->pw_dir);
    }
  } catch (std::exception) {
  }

  return std::string();
}


export std::string getDataDir() {
  return getHomeDir() + kDataDir;
}


#elifdef WINDOWS_PLATFORM
// --------------------------
// Windows implementation
// --------------------------

std::string getCsidlPathA(int csidl) {
  try {
    CHAR s[MAX_PATH];
    if (SHGetFolderPathA(nullptr, csidl, nullptr, 0, s) == S_OK) {
      return std::string(s);
    }
  } catch (std::exception) {}
  return std::string();
}

std::wstring getCsidlPathW(int csidl) {
  try {
    WCHAR s[MAX_PATH];
    if (SHGetFolderPathW(nullptr, csidl, nullptr, 0, s) == S_OK) {
      return std::wstring(s);
    }
  } catch (std::exception) {}
  return std::wstring();
}


export std::string getHomeDir() {
  return getCsidlPathA(CSIDL_PROFILE);
}


export std::wstring getHomeDirW() {
  return getCsidlPathW(CSIDL_PROFILE);
}


export std::string getDataDir() {
  return getCsidlPathA(CSIDL_LOCAL_APPDATA);
}


export std::wstring getDataDirW() {
  return getCsidlPathW(CSIDL_LOCAL_APPDATA);
}
#endif
} // namespace HomeDirLibrary
