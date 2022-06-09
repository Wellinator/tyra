/*
# ______       ____   ___
#   |     \/   ____| |___|    
#   |     |   |   \  |   |       
#-----------------------------------------------------------------------
# Copyright 2020 - 2022, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
# Wellington Carvalho <wellcoj@gmail.com>
*/

#ifndef _TYRA_BMP_LOADER_
#define _TYRA_BMP_LOADER_

#include <stdio.h>
#include <tamtypes.h>
#include "../models/texture.hpp"
#include "../modules/file_manager.hpp"

/** Class responsible for loading images in bmp format */
class BmpLoader
{

public:
    BmpLoader();
    ~BmpLoader();

    void load(Texture &o_texture, const char *t_subfolder, const char *t_name, const char *t_extension);

    FileManager fileManager;
};

#endif
