/*
# ______       ____   ___
#   |     \/   ____| |___|    
#   |     |   |   \  |   |       
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Wellington Carvalho <wellcoj@gmail.com> and André Guilherme <andregui17@outlook.com>
*/

#include "../include/modules/file_manager.hpp"

FileManager::FileManager()
{}

FileManager::~FileManager()
{}

FILE *FileManager::openFile(const char *t_subfolder, const char *t_name, const char *t_extension)
{
    char *path_part1 = String::createConcatenated(t_subfolder, t_name);
    char *path_part2 = String::createConcatenated(HOST_SOURCE, path_part1);
    char *path = String::createConcatenated(path_part2, t_extension);
    delete[] path_part1;
    delete[] path_part2;

    printf("Opening file: %s\n", path);
    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        char *errorMsg = String::createConcatenated("Failed to load the file: ", path);
        assertMsg(file != NULL, errorMsg);
    }

    return file;
}

FILE *FileManager::openFile(const char *t_dir, const char *t_file)
{
    char *path_part1 = String::createConcatenated(t_dir, t_file);
    char *path = String::createConcatenated(HOST_SOURCE, path_part1);
    delete[] path_part1;
    
    printf("Opening file: %s\n", path);
    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        char *errorMsg = String::createConcatenated("Failed to load the file: ", path);
        assertMsg(file != NULL, errorMsg);
    }

    return file;
}

FILE *FileManager::openFile(const char *t_path)
{
    char *path = String::createConcatenated(HOST_SOURCE, t_path);

    printf("Opening file: %s\n", path);
    
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        char *errorMsg = String::createConcatenated("Failed to load the file: ", path);
        assertMsg(file != NULL, errorMsg);
    }

    return file;
}
