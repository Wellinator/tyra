#include "states/game/renderer/post_fx_manager.hpp"
#include <gs_gp.h>
#include <gs_psm.h>
#include <dma_tags.h>
#include <time.h>
#include <screenshot.h>
#include <malloc.h>
#include <string.h>

using Tyra::Color;
using Tyra::Renderer;
using Tyra::RendererCoreTextureBuffers;
using Tyra::Texture;
using Tyra::TextureBuilderData;

namespace Demo {

void PostFxManager::dumpGsData(char* prefix, bool trap) {
  dma_channel_wait(DMA_CHANNEL_GIF, 0);

  RendererCoreTextureBuffers depthTexBuffer =
      pRenderer->core.texture.useTexture(pDepthBufferTexture);
  // ps2_screenshot_file(
  //     Tyra::FileUtils::fromCwd(std::string("gs_debug/") + prefix +
  //                              "_depth_buffer_tex.tga")
  //     .c_str(),
  // depthTexBuffer.clut->address >> 6, pDepthBufferTexture->core->width,
  // pDepthBufferTexture->core->height, depthTexBuffer.clut->psm);

  // RendererCoreTextureBuffers fogTexBuffer =
  //     pRenderer->core.texture.useTexture(pFogTexture);
  // ps2_screenshot_file((std::string(prefix) +
  // "_color_buffer_tex.tga").c_str(),
  //                     fogTexBuffer.clut->address >> 6,
  //                     pFogTexture->core->width, pFogTexture->core->height,
  //                     fogTexBuffer.clut->psm);

  // uint32_t width = settings.getWidth(), height = settings.getHeight();
  const zbuffer_t zbuffer = pRenderer->core.gs.zBuffer;
  // printf("zbuffer.address: %X\n", zbuffer.address);
  // ps2_screenshot_file((std::string(prefix) + "_depth_buffer.tga").c_str(),
  //                     zbuffer.address >> 6, width, height, zbuffer.zsm);

  const framebuffer_t front_buffer = pRenderer->core.gs.getFrameBuffer(0);
  printf("front_buffer.address: %X\n", front_buffer.address);
  ps2_screenshot_file((std::string(prefix) + "_front_buffer.tga").c_str(),
                      front_buffer.address >> 6, front_buffer.width,
                      front_buffer.height, front_buffer.psm);

  const framebuffer_t back_buffer = pRenderer->core.gs.getFrameBuffer(1);
  printf("back_buffer.address: %X\n", back_buffer.address);
  ps2_screenshot_file((std::string(prefix) + "_back_buffer.tga").c_str(),
                      back_buffer.address >> 6, back_buffer.width,
                      back_buffer.height, back_buffer.psm);

  if (trap) {
    TYRA_TRAP("GS data dumped to gs_debug folder!", "Addresses:\n",
              "front_buffer.address: ", front_buffer.address >> 6, "\n",
              "back_buffer.address: ", back_buffer.address >> 6, "\n",
              "zbuffer.address: ", zbuffer.address >> 6);
  }
}

PostFxManager::PostFxManager(Renderer* renderer)
    : settings(renderer->core.getSettings()) {
  pRenderer = renderer;
  init();
};

PostFxManager::~PostFxManager(){};

void PostFxManager::applyFogColorToPalette(Color fogColor) {
  // Update the fog color in all 256 palette entries
  // The palette works as a lookup table: depth value (from zbuffer green
  // channel) indexes into this palette to get fog color + intensity (alpha) for
  // that depth
  u8* pal_data = pFogTexture->core->data;

  // Update RGB channels with fog color, preserve alpha (set by scaleDepthMask)
  for (int i = 0; i < 256; i++) {
    pal_data[i * 4 + 0] = fogColor.r;  // R
    pal_data[i * 4 + 1] = fogColor.g;  // G
    pal_data[i * 4 + 2] = fogColor.b;  // B
    // Alpha (i * 4 + 3) is preserved from scaleDepthMask - contains fog
    // intensity curve
  }

  // Note: Updated data will be uploaded to VRAM when copyDepthBuffer calls
  // useTexture
  currentFogColor = fogColor;
}

void PostFxManager::render(Color fogColor) {
#ifdef DEBUG_MODE
  if (g_debug_menu.enablePostFx == false) return;
#endif  // DEBUG_MODE

  // Update palette color if fog color changed
  if (fogColor.r != currentFogColor.r || fogColor.g != currentFogColor.g ||
      fogColor.b != currentFogColor.b) {
    applyFogColorToPalette(fogColor);
  }

  // Set GS settings
  qword_t packets[20] ALIGNED(64);
  qword_t* q = packets;

  q = draw_disable_tests(q, 0, &pRenderer->core.gs.zBuffer);

  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
  dma_channel_wait(DMA_CHANNEL_GIF, 500);

  // Apply fog: Extract green channel from zbuffer, use it to index the fog
  // palette, and blend the result directly onto the framebuffer This implements
  // the classic PS2 fog post-processing technique
  copyDepthBuffer(CHANNEL_GREEN, pFogTexture);

  // Reset GS old settings
  q = packets;

  PACK_GIFTAG(q, GIF_SET_TAG(2, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
  q++;

  // Alpha Blending
  PACK_GIFTAG(q,
              GS_SET_ALPHA(BLEND_COLOR_SOURCE, BLEND_COLOR_DEST,
                           BLEND_ALPHA_SOURCE, BLEND_COLOR_DEST, 0x80),
              GS_REG_ALPHA_1);
  q++;

  PACK_GIFTAG(q, GS_SET_CLAMP(WRAP_CLAMP, WRAP_CLAMP, 0, 0, 0, 0),
              GS_REG_CLAMP_1);
  q++;

  q = draw_enable_tests(q, 0, &pRenderer->core.gs.zBuffer);

  q = draw_texture_expand_alpha(q, 0x80, ALPHA_EXPAND_NORMAL, 0x80);

  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
  dma_channel_wait(DMA_CHANNEL_GIF, 500);
}

void PostFxManager::init() {
  // Initialize post-processing fog effect using the classic PS2 technique:
  // 1. Extract green channel from 24-bit Z-buffer (provides good depth
  // distribution)
  // 2. Use depth value to index into a 256-entry RGBA palette texture
  // 3. Palette contains fog color with alpha gradient (near=transparent,
  // far=opaque)
  // 4. Blend palette lookup onto framebuffer with depth test filtering

  // Create temp depth buffer texture (64x32 blocks for efficient GS transfers)
  TextureBuilderData pDepthTempBuffer;
  pDepthTempBuffer.width = 64;
  pDepthTempBuffer.height = 32;
  pDepthTempBuffer.bpp = Tyra::TextureBpp::bpp32;
  pDepthTempBuffer.data = new u8[64 * 32 * 4]{0};
  pDepthTempBuffer.gsComponents = TEXTURE_COMPONENTS_RGBA;

  pDepthBufferTexture = new Texture(&pDepthTempBuffer);
  pRenderer->core.texture.repository.add(pDepthBufferTexture);
  pRenderer->core.texture.useTexture(pDepthBufferTexture);

  // Create FOG color palette - 256 entries (16x16)
  TextureBuilderData pColorPaletteRaster;
  pColorPaletteRaster.width = 16;
  pColorPaletteRaster.height = 16;
  pColorPaletteRaster.bpp = Tyra::TextureBpp::bpp32;
  pColorPaletteRaster.gsComponents = TEXTURE_COMPONENTS_RGBA;
  pColorPaletteRaster.data = new u8[16 * 16 * 4]{0};

  // Initialize palette with transparent values
  for (int i = 0; i < 256 * 4; i += 4) {
    pColorPaletteRaster.data[i + 0] = 128;  // R
    pColorPaletteRaster.data[i + 1] = 128;  // G
    pColorPaletteRaster.data[i + 2] = 128;  // B
    pColorPaletteRaster.data[i + 3] = 0;    // A - will be set by scaleDepthMask
  }

  pFogTexture = new Texture(&pColorPaletteRaster);
  pRenderer->core.texture.repository.add(pFogTexture);
  pRenderer->core.texture.useTexture(pFogTexture);

  // Setup fog depth scale - controls fog density at different depths
  // The scale controls how fast fog increases with distance
  // Lower values = closer to camera (less fog), higher values = farther (more
  // fog) This creates a smooth gradient from transparent (near) to opaque (far)
  uint8_t fog_scale[16] = {0, 1, 1,  2,  2,  3,  4,  5,
                           6, 8, 10, 12, 16, 20, 24, 32};
  scaleDepthMask(pFogTexture, 0, fog_scale);
};

void PostFxManager::updateDebugPallet() {
  uint32_t* pallet = reinterpret_cast<uint32_t*>(pFogTexture->core->data);
  uint32_t i = 1;

  pallet[0] = (i) << 24;
  pallet[1] = (i += 4) << 24;
  pallet[2] = (i += 4) << 24;
  pallet[3] = (i += 4) << 24;
  pallet[4] = (i += 4) << 24;
  pallet[5] = (i += 4) << 24;
  pallet[6] = (i += 4) << 24;
  pallet[7] = (i += 4) << 24;

  pallet[16] = (i += 3) << 24;
  pallet[17] = (i += 3) << 24;
  pallet[18] = (i += 3) << 24;
  pallet[19] = (i += 3) << 24;
  pallet[20] = (i += 3) << 24;
  pallet[21] = (i += 3) << 24;
  pallet[22] = (i += 3) << 24;
  pallet[23] = (i += 3) << 24;

  pallet[8] = (i += 2) << 24;
  pallet[9] = (i += 2) << 24;
  pallet[10] = (i += 2) << 24;
  pallet[11] = (i += 2) << 24;
  pallet[12] = (i += 2) << 24;
  pallet[13] = (i += 2) << 24;
  pallet[14] = (i += 2) << 24;
  pallet[15] = (i += 2) << 24;

  pallet[24] = (i += 1) << 24;
  pallet[25] = (i += 1) << 24;
  pallet[26] = (i += 1) << 24;
  pallet[27] = (i += 1) << 24;
  pallet[28] = (i += 1) << 24;
  pallet[29] = (i += 1) << 24;
  pallet[30] = (i += 1) << 24;
  pallet[31] = (i += 1) << 24;

  printf("debugPalletIndex: %i, old value: %li\n", debugPalletIndex,
         pallet[debugPalletIndex] >> 24);
  pallet[debugPalletIndex] = 1 << 24;

  // Note: Updated palette will be uploaded when useTexture is called

  debugPalletIndex++;
  debugPalletIndex %= 24;
}

void PostFxManager::copyDepthBuffer(ColourChannels channelIn,
                                    Texture* palette) {
  uint32_t width = settings.getWidth(), height = settings.getHeight();
  uint32_t zbufferAddr = pRenderer->core.gs.zBuffer.address;
  RendererCoreTextureBuffers texBuffer =
      pRenderer->core.texture.useTexture(palette);

  uint32_t pal_addr = texBuffer.core->address;

  uint32_t page = 0;
  uint32_t x, y;

  for (y = 0; y < height; y += 32) {
    for (x = 0; x < width; x += 64) {
      uint32_t buf_addr =
          pRenderer->core.texture.useTexture(pDepthBufferTexture).core->address;

      qword_t packets[5] ALIGNED(64);
      qword_t* q = packets;

      PACK_GIFTAG(q, GIF_SET_TAG(4, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
      q++;

      PACK_GIFTAG(q,
                  GS_SET_BITBLTBUF((zbufferAddr >> 6) + page, 1, GS_PSMZ_32,
                                   buf_addr >> 6, 1, GS_PSM_32),
                  GS_REG_BITBLTBUF);
      q++;

      PACK_GIFTAG(q, GS_SET_TRXPOS(0, 0, 0, 0, 0),
                  GS_REG_TRXPOS);  // ...then set the offset in the buffer, and
                                   // pixel transmission order...
      q++;

      PACK_GIFTAG(q, (uint64_t)(64) | ((uint64_t)(32) << 32),
                  GS_REG_TRXREG);  // ...the width and height of the
                                   // transmission for the dest buffer...
      q++;

      PACK_GIFTAG(q, 2L, GS_REG_TRXDIR);  // ...and finally the direction of
                                          // transmission, local-to-local...
      q++;

      pal_addr = pRenderer->core.texture.useTexture(palette).core->address;

      dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
      dma_channel_fast_waits(DMA_CHANNEL_GIF);
      dma_channel_wait(DMA_CHANNEL_GIF, 500);

      // For fog, blend the palette colors onto RGB channels using alpha
      // blending For other effects, copy to a specific channel
      performChannelCopy(channelIn, CHANNEL_ALPHA, x, y, buf_addr, width,
                         height, pal_addr,
                         true);  // true = use blending for fog

      page += 32;
    }
  }
};

void PostFxManager::performChannelCopy(ColourChannels channelIn,
                                       ColourChannels channelOut,
                                       uint32_t blockX, uint32_t blockY,
                                       uint32_t source_addr, uint32_t width,
                                       uint32_t height, uint32_t pal_addr,
                                       bool useBlending) {
  const framebuffer_t buf_frame = pRenderer->core.gs.getCurrentFrameData();

  // For the BLUE and ALPHA channels, we need to offset our 'U's by 8 texels
  const uint32_t horz_block_offset =
      (channelIn == CHANNEL_BLUE || channelIn == CHANNEL_ALPHA);
  // For the GREEN and ALPHA channels, we need to offset our 'T's by 2 texels
  const uint32_t vert_block_offset =
      (channelIn == CHANNEL_GREEN || channelIn == CHANNEL_ALPHA);

  const uint32_t clamp_horz = horz_block_offset ? 8 : 0;
  const uint32_t clamp_vert = vert_block_offset ? 2 : 0;

  qword_t packets[500] ALIGNED(64);
  qword_t* q = packets;

  PACK_GIFTAG(q, GIF_SET_TAG(5, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
  q++;

  PACK_GIFTAG(q, GS_SET_XYOFFSET(0, 0), GS_REG_XYOFFSET_1);
  q++;

  int tw, th;
  setTwTh(width, height, &tw, &th);
  // Use point sampling (filter=0) for palette lookup to avoid interpolation
  PACK_GIFTAG(q,
              GS_SET_TEX0(source_addr >> 6, 1, GS_PSM_8, tw, th, 1, 1,
                          pal_addr >> 6, GS_PSM_32, 0, 0, 0),
              GS_REG_TEX0_1);
  q++;

  PACK_GIFTAG(q,
              GS_SET_CLAMP(WRAP_REGION_REPEAT, WRAP_REGION_REPEAT, 0xF7,
                           clamp_horz, 0xFD, clamp_vert),
              GIF_REG_CLAMP_1);
  q++;

  PACK_GIFTAG(q, GS_SET_TEXFLUSH(1), GS_REG_TEXFLUSH);
  q++;

  uint32_t frame_mask;
  if (useBlending) {
    // For fog blending, write to all RGB channels (preserve alpha)
    frame_mask = 0xFF000000;  // Preserve alpha, write RGB
  } else {
    // For channel copy, write only to the target channel
    switch (channelOut) {
      case CHANNEL_RED:
        frame_mask = ~0x000000FF;
        break;
      case CHANNEL_GREEN:
        frame_mask = ~0x0000FF00;
        break;
      case CHANNEL_BLUE:
        frame_mask = ~0x00FF0000;
        break;
      case CHANNEL_ALPHA:
        frame_mask = ~0xFF000000;
        break;
      default:
        frame_mask = ~0x00FF0000;
        break;
    }
  }

  PACK_GIFTAG(q,
              GS_SET_FRAME(buf_frame.address >> 11, buf_frame.width >> 6,
                           buf_frame.psm, frame_mask),
              GS_REG_FRAME_1);
  q++;

  // For fog blending, enable alpha blending; for channel copy, disable it
  uint32_t prim_flags = useBlending ? 1 : 0;  // ABE (alpha blend enable)

  PACK_GIFTAG(
      q,
      GIF_SET_TAG(96, 1, 1,
                  GS_SET_PRIM(GS_PRIM_SPRITE, 0, 1, 0, prim_flags, 0, 1, 0, 0),
                  GIF_FLG_PACKED, 4),
      (GIF_REG_UV) | (GIF_REG_XYZ2 << 4) | (GIF_REG_UV << 8) |
          (GIF_REG_XYZ2 << 12));
  q++;

  int y;
  for (y = 0; y < 32; y += 2) {
    if (((y % 4) == 0) ^ (vert_block_offset == 1))  // Even (4 16x2 sprites)
    {
      int x;
      for (x = 0; x < 64; x += 16) {
        // UV
        PACK_GIFTAG(q, GIF_SET_ST(8 + ((8 + x * 2) << 4), 8 + ((y * 2) << 4)),
                    0);
        q++;

        // XYZ2
        PACK_GIFTAG(q,
                    (uint64_t)((x + blockX) << 4) |
                        ((uint64_t)((y + blockY) << 4) << 32),
                    1);
        q++;

        // UV
        PACK_GIFTAG(
            q, GIF_SET_ST(8 + ((24 + x * 2) << 4), 8 + ((2 + y * 2) << 4)), 0);
        q++;

        // XYZ2
        PACK_GIFTAG(q,
                    (uint64_t)((x + 16 + blockX) << 4) |
                        ((uint64_t)((y + 2 + blockY) << 4) << 32),
                    1);
        q++;
      }
    } else  // Odd (Eight 8x2 sprites)
    {
      int x;
      for (x = 0; x < 64; x += 8) {
        // UV
        PACK_GIFTAG(q, GIF_SET_ST(8 + ((4 + x * 2) << 4), 8 + ((y * 2) << 4)),
                    0);
        q++;

        // XYZ2
        PACK_GIFTAG(q,
                    (uint64_t)((x + blockX) << 4) |
                        ((uint64_t)((y + blockY) << 4) << 32),
                    1);
        q++;

        // UV
        PACK_GIFTAG(
            q, GIF_SET_ST(8 + ((12 + x * 2) << 4), 8 + ((2 + y * 2) << 4)), 0);
        q++;

        // XYZ2
        PACK_GIFTAG(q,
                    (uint64_t)((x + 8 + blockX) << 4) |
                        ((uint64_t)((y + 2 + blockY) << 4) << 32),
                    1);
        q++;
      }
    }
  }

  FlushCache(0);
  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
  dma_channel_wait(DMA_CHANNEL_GIF, 500);
  q = packets;

  PACK_GIFTAG(q, GIF_SET_TAG(3, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
  q++;

  PACK_GIFTAG(q, GS_SET_CLAMP(WRAP_CLAMP, WRAP_CLAMP, 0, 0, 0, 0),
              GS_REG_CLAMP_1);
  q++;

  PACK_GIFTAG(q,
              GS_SET_FRAME(buf_frame.address >> 11, buf_frame.width >> 6,
                           buf_frame.psm, buf_frame.mask),
              GS_REG_FRAME_1);
  q++;

  PACK_GIFTAG(q,
              GS_SET_XYOFFSET(
                  (int)(screenCenter - (settings.getWidth() / 2.0F) * 16.0f),
                  (int)(screenCenter - (settings.getHeight() / 2.0F) * 16.0f)),
              GS_REG_XYOFFSET_1);
  q++;

  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
  dma_channel_wait(DMA_CHANNEL_GIF, 500);
};

void PostFxManager::setTwTh(int w, int h, int* tw, int* th) {
  *tw = 31 - (lzw(w) + 1);
  if (w > (1 << *tw)) (*tw)++;

  *th = 31 - (lzw(h) + 1);
  if (h > (1 << *th)) (*th)++;
}

void PostFxManager::scaleDepthMask(Texture* palette, uint8_t initial_value,
                                   uint8_t factors[16]) {
  // Setup fog intensity curve across the 256-entry palette
  // Each palette entry corresponds to a depth value (0-255 from green channel)
  // Lower indices = closer to camera (less fog)
  // Higher indices = farther from camera (more fog)
  //
  // The factor_order follows GS swizzled memory layout for optimal access
  // patterns
  static const uint8_t factor_order[16][2] = {
      {0, 7},    {8, 15},    {16, 23},   {24, 31},  {32, 39}, {40, 47},
      {48, 55},  {56, 63},   {64, 71},   {72, 79},  {80, 87}, {88, 95},
      {96, 103}, {104, 111}, {112, 119}, {120, 127}};

  u8* pal_data = palette->core->data;
  uint8_t alpha_value = initial_value;

  // Apply fog intensity gradients across the 256-entry palette
  // Alpha value increases with distance, creating depth-based fog
  for (int j = 0; j < 16; j++) {
    for (int i = factor_order[j][0]; i <= factor_order[j][1]; i++) {
      // Set alpha channel to control fog intensity (0 = transparent, 128 =
      // opaque) RGB channels are set by applyFogColorToPalette() based on fog
      // color
      pal_data[i * 4 + 3] = alpha_value;  // Alpha channel

      // Increment alpha for next group (non-linear fog falloff)
      if (alpha_value < 128) {
        alpha_value += factors[j];
        if (alpha_value > 128) alpha_value = 128;
      }
    }
  }

  // Fill remaining entries (128-255) with maximum fog
  // These represent very far distances or depths beyond 0x00ffff range
  for (int i = 128; i < 256; i++) {
    pal_data[i * 4 + 3] = 128;
  }

  // Note: Palette data will be uploaded to VRAM when first used via
  // useTexture()
}

}  // namespace Demo
