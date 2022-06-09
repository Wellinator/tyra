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

#include "../include/loaders/bmp_loader.hpp"

#include "../include/utils/debug.hpp"
#include "../include/utils/string.hpp"
#include <cstdlib>

// ----
// Constructors/Destructors
// ----

BmpLoader::BmpLoader() {}

BmpLoader::~BmpLoader() {}

// ----
// Methods
// ----

/**
 * @param t_name Without extension. Example "skyfall2"
 * @param t_extension With dot and extension. Example ".BMP"
 */
void BmpLoader::load(Texture &o_texture, const char *t_subfolder, const char *t_name, const char *t_extension)
{
    FILE *file = fileManager.openFile(t_subfolder, t_name, t_extension);
    assertMsg(file != NULL, "Failed to load .bmp file!");

    unsigned char header[54];
    fread(header, sizeof(unsigned char), 54, file);

    u32 width = (u32)header[18];
    u32 height = (u32)header[22];
    u32 bits = (u32)header[28];
    u32 dataOffset = (u32)header[10];

    assertMsg(bits == 24, "Invalid bits per pixel in .bmp file - expected 24!");

    o_texture.setSize(width, height, TEX_TYPE_RGB);
    printf("BMPLoader - width: %d | height: %d | bits: %d\n", width, height, bits);

    u64 rowPadded = (width * 3 + 3) & (~3);

    unsigned char row[rowPadded];

    //Offset file stream to raster data
    fseek(file, dataOffset, SEEK_SET);

    u32 x = 0;
    for (u32 i = 0; i < height; i++)
    {
        fread(&row, sizeof(unsigned char), rowPadded, file);
        for (u32 j = 0; j < width * 3; j += 3)
        {
            // Convert (B, G, R) to (R, G, B)
            o_texture.setData(x, row[j + 2]);
            o_texture.setData(x + 1, row[j + 1]);
            o_texture.setData(x + 2, row[j]);
            x += 3;
        }
    }
    fclose(file);
}
