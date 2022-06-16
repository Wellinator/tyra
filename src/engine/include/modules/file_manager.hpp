/*
# ______       ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Wellington Carvalho <wellcoj@gmail.com> and André Guilherme <andregui17@outlook.com>
*/
#ifndef _TYRA_FILE_MANAGER_
#define _TYRA_FILE_MANAGER_

#include <tamtypes.h>
#include <cstdio>
#include <kernel.h>
#include <limits.h>
#include <unistd.h>
#include <cstring>

#include "../utils/debug.hpp"
#include "../utils/string.hpp"

class FileManager
{

public:
    FileManager();
    ~FileManager();

    /**
     * Read file from source. If NDEBUG read from 'mass:' or also it  will read from 'host:'
     * @returns FILE *
     */
    FILE *openFile(const char *t_subfolder, const char *t_name, const char *t_extension);
    FILE *openFile(const char *t_dir, const char *t_file);
    FILE *openFile(const char *t_path);
    char *getBasePath();

private:
    char cwd[NAME_MAX];

    // Argv name+path & just path
    char elfName[NAME_MAX] __attribute__((aligned(16)));
    char elfPath[NAME_MAX - 14]; // It isn't 256 because elfPath will add subpaths

    void setPathInfo(char *path);
};

#endif
