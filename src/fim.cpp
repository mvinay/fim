#include <assert.h>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#define DEFAULT_PERM S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH

using namespace std;

// This is the root directory where all the details are stored under.
static string root = "./.fim/";

/* ------------- Logging helper functions -------------------*/

enum ErrorLevel { INFO, WARN, ERROR };

static void print(string errorStr, ErrorLevel level) {

  switch (level) {
  case WARN:
    errorStr = "WARNING: " + errorStr;
  case INFO:
    cout << errorStr << endl;
    return;
  case ERROR:
    cerr << "ERROR: " << errorStr << endl;
    return;
  default:
    assert(false);
  }
}

static void printUsage() {
  cout << "\nUsage: " << endl;
  cout << "\tfim init           -  Init the fim repository" << endl;
  cout
      << "\tfim add <pathname> -  Add the pathname to the list of files tracked"
      << endl;
  cout << "\tfim status         -  Get the status of the files currently "
          "being tracked"
       << endl
       << endl;
}

/* -----------------MD5 utility functions --------------------*/

static void getMD5(string src, char *md5) {
  unsigned char result[MD5_DIGEST_LENGTH];
  MD5((unsigned char *)src.data(), src.size(), result);

  for (int i = 0; i < 16; i++)
    sprintf(&md5[i * 2], "%02x", (unsigned int)result[i]);
}

static void getMD5ForFile(const char *path, char *md5) {
  std::ifstream ifs(path);
  std::string content((std::istreambuf_iterator<char>(ifs)),
                      (std::istreambuf_iterator<char>()));
  getMD5(content, md5);
}

/* ------------------ File System level functions ---------- */

static void getAbsPath(const char *path, char *fullPath) {
  realpath(path, fullPath);
}

static void createDirectory(const char *path) { mkdir(path, DEFAULT_PERM); }

bool directoryExists(const char *path) {
  if (path == NULL)
    return false;

  DIR *dir;
  bool exists = false;

  dir = opendir(path);

  if (dir != NULL) {
    exists = true;
    (void)closedir(dir);
  }

  return exists;
}

static void createFileWithContent(string fileName, string content) {
  /*  cout << "creating file with name : " << fileName
         << " and content: " << content; */
  ofstream file;
  file.open(fileName);
  file << content;
  file.close();
}

// Creates the MD5 hash for both the filename and file contents and
// stores the corresponding hash values as filename and file content
// under @root directory.

static void trackFile(const char *path, struct stat s) {
  char md5[33];
  getMD5ForFile(path, md5);

  char md5ForPath[33];
  getMD5(path, md5ForPath);

  string fileName = root + md5ForPath;

  // Create the file and its contents.
  ofstream file;
  file.open(fileName);
  file << s.st_mtime;
  file << " ";
  file << s.st_size;
  file << " ";
  file << md5;
  file.close();
}

// Recursively traverse all the files and directories
// for the the given @path.
static void traverseDirAndAdd(const char *path) {
  if (path == NULL)
    return;

  struct stat s;
  // if the path is a file, add it to the list of ignored files.
  if (stat(path, &s) == 0 && s.st_mode & S_IFREG) {
    trackFile(path, s);
    return;
  }

  DIR *dir = opendir(path);
  if (dir == NULL) {
    print("Not a valid path: " + string(path), ERROR);
    return;
  }

  struct dirent *entry = readdir(dir);

  vector<string> pathList;

  while (entry != NULL) {
    if (entry->d_type != DT_DIR && entry->d_type != DT_REG) {
      entry = readdir(dir);
      continue;
    }

    if (entry->d_type == DT_DIR &&
        (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
         strcmp(entry->d_name, ".fim") == 0)) {
      entry = readdir(dir);
      continue;
    }
    string pathStr = path;
    pathStr.append(string(entry->d_name));
    if (entry->d_type == DT_DIR) {
      pathStr.append("/");
    }
    pathList.push_back(pathStr);
    entry = readdir(dir);
  }
  closedir(dir);

  if (pathList.empty()) {
    return;
  }

  for (int i = 0; i < pathList.size(); ++i) {
    traverseDirAndAdd(pathList[i].c_str());
  }
}

/* Recursively add all the files in @path to the tracking list */
static void addFiles(const char *path) { traverseDirAndAdd(path); }

static int isFileModified(const char *path, struct stat s) {

  char md5ForPath[33];
  getMD5(path, md5ForPath);

  string filePath = root + string(md5ForPath);

  // If the md5 file doesn't exist, return as new file.
  struct stat fileStat;
  if (lstat(filePath.c_str(), &fileStat) != 0) {
    return 1;
  }

  // if the md5 file exists, read the file data.

  ifstream read(filePath);

  long mtime, size;
  string contentmd5;
  read >> mtime;
  read >> size;
  read >> contentmd5;

  // File has not been modified.
  if (mtime == s.st_mtime && size == s.st_size) {
    return 0;
  }

  // If size is not same, file has been modified for sure.
  if (size != s.st_size) {
    return 2;
  }

  // now check the contents md5;
  char md5[33];
  getMD5ForFile(path, md5);

  if (strcmp(md5, contentmd5.c_str()) == 0) {
    return 0;
  }

  return 2;
}

static void checkFiles(const char *path, vector<string> &modifiedFiles,
                       vector<string> &untrackedFiles) {
  if (path == NULL)
    return;

  struct stat s;
  // if the path is a file, add it to the list of ignored files.
  if (stat(path, &s) == 0 && s.st_mode & S_IFREG) {
    int value = isFileModified(path, s);

    if (value == 2) {
      modifiedFiles.push_back(path);
    } else if (value == 1) {
      untrackedFiles.push_back(path);
    }
    return;
  }

  DIR *dir = opendir(path);
  if (dir == NULL) {
    return;
  }
  struct dirent *entry = readdir(dir);

  vector<string> pathList;

  while (entry != NULL) {

    // Only tracks regular files and directories.
    if (entry->d_type != DT_DIR && entry->d_type != DT_REG) {
      entry = readdir(dir);
      continue;
    }

    if (entry->d_type == DT_DIR &&
        (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 ||
         strcmp(entry->d_name, ".fim") == 0)) {
      entry = readdir(dir);
      continue;
    }

    string pathStr = path;
    if (entry->d_type == DT_DIR) {
      pathStr.append("/");
    }
    pathStr.append(string(entry->d_name));
    pathList.push_back(pathStr);
    entry = readdir(dir);
  }
  closedir(dir);

  if (pathList.empty()) {
    return;
  }

  for (int i = 0; i < pathList.size(); ++i) {
    checkFiles(pathList[i].c_str(), modifiedFiles, untrackedFiles);
  }
}

static void checkStatus(const char *path) {
  vector<string> modifiedFiles;
  vector<string> untrackedFiles;
  string final(path);
  final.append("/");
  checkFiles(final.c_str(), modifiedFiles, untrackedFiles);
  const std::string red("\033[0;31m");
  if (modifiedFiles.size() != 0) {
    cout << "\nModified files are:";
    for (string file : modifiedFiles) {
      cout << "\n\t\033[1;31m" << file << "\033[0m";
    }
  } else {
    cout << "\nNo modified files in the working directory";
  }

  if (untrackedFiles.size() != 0) {
    cout << "\nNew files to be added are:";
    for (string file : untrackedFiles) {
      cout << "\n\t\033[1;32m" << file << "\033[0m";
    }
  } else {
    cout << "\nNo untracked files in the working directory";
  }
  cout << endl;
}

static void init() {
  char fullPath[PATH_MAX];
  realpath(root.c_str(), fullPath);
  createDirectory(fullPath);
  root = fullPath;
  print("Initialized empty fim repository!\n", INFO);
}

int main(int argc, char **argv) {

  if (argc == 1) {
    print("Invalid command", ERROR);
    printUsage();
    exit(1);
  }

  bool exists = directoryExists(root.c_str());

  if (strcmp(argv[1], "init") == 0) {

    if (exists) {
      print("fim repo found!! Failed to create the new one!", ERROR);
      exit(1);
    }
    if (argc > 2) {
      print("fim init does not take any arguments", ERROR);
      printUsage();
      exit(4);
    }
    init();
    return 0;
  }

  if (!exists) {
    print("No fim repo found!!! Use 'fim init' to create the empty repo",
          ERROR);
    exit(5);
  }

  // Add the path to the tracking list.
  if (strcmp(argv[1], "add") == 0) {

    // Invalid number of arguments.
    if (argc < 3) {
      print("Expected path as argument", ERROR);
      printUsage();
      exit(2);
    }

    char fullPath[PATH_MAX];
    getAbsPath(argv[2], fullPath);
    addFiles(fullPath);
    return 0;
  }

  // Check the status of the currently tracked files.
  if (strcmp(argv[1], "status") == 0) {

    // Invalid number of arguments.
    if (argc > 2) {
      print("fim status does not take any arguments", ERROR);
      printUsage();
      exit(3);
    }

    char fullPath[PATH_MAX];
    getAbsPath(".", fullPath);
    checkStatus(fullPath);
    return 0;
  }

  return 0;
}
