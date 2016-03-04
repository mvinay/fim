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
static void trackFile(const char *path) {
  char md5[33];
  getMD5ForFile(path, md5);

  char md5ForPath[33];
  getMD5(path, md5ForPath);

  // cout << "\nMD5 is : " << md5 << endl;
  createFileWithContent(root + string(md5ForPath), md5);
}

// Recursively traverse all the files and directories
// for the the given @path.
static void traverseDirAndAdd(const char *path) {
  if (path == NULL)
    return;

  struct stat s;
  // if the path is a file, add it to the list of ignored files.
  if (stat(path, &s) == 0 && s.st_mode & S_IFREG) {
    trackFile(path);
    return;
  }

  DIR *dir = opendir(path);
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

static bool isFileModified(const char *path) { return true; }

static void checkFiles(const char *path, vector<string> &modifiedFiles) {
  if (path == NULL)
    return;

  struct stat s;
  // if the path is a file, add it to the list of ignored files.
  if (stat(path, &s) == 0 && s.st_mode & S_IFREG) {
    if (isFileModified(path)) {
      modifiedFiles.push_back(path);
    }
    return;
  }

  DIR *dir = opendir(path);
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
    checkFiles(pathList[i].c_str(), modifiedFiles);
  }
}

static void checkStatus(const char *path) {
  vector<string> modifiedFiles;
  checkFiles(path, modifiedFiles);
  if (modifiedFiles.size() == 0) {
    cout << "\nNo modified files found!\n";
    return;
  }
  cout << "\nModified files are: \n";
  for (string file : modifiedFiles) {
    cout << "\n\t" << file;
  }
  cout << endl;
}

static void init() { 
    createDirectory(root.c_str()); 
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

    string path = argv[2];
    if (path == ".") {
      path += "/";
    }
    addFiles(path.c_str());
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

    checkStatus("./");
    return 0;
  }

  return 0;
}
