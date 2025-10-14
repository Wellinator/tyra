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

PostFxManager::PostFxManager(Renderer* t_renderer)
    : settings(t_renderer->core.getSettings()) {
  this->t_renderer = t_renderer;
  init();
};

PostFxManager::~PostFxManager(){};

void PostFxManager::init() {
  // Create temp depth buffer texture
  TextureBuilderData pDepthTempBuffer;
  pDepthTempBuffer.width = 64;
  pDepthTempBuffer.height = 32;
  pDepthTempBuffer.bpp = Tyra::TextureBpp::bpp8;
  pDepthTempBuffer.data = new u8[64 * 32 * 4]{0};
  pDepthTempBuffer.gsComponents = TEXTURE_COMPONENTS_RGBA;

  pDepthBufferTexture = new Texture(&pDepthTempBuffer);
  t_renderer->core.texture.repository.add(pDepthBufferTexture);
  t_renderer->core.texture.useTexture(pDepthBufferTexture);

  // Create FOG palette texture (CLUT)
  // Must be 8-bit indexed with a 256-entry palette for depth mapping
  // Dimensions MUST be power of 2: using 16x16 = 256 pixels total
  TextureBuilderData pColorPaletteRaster;
  pColorPaletteRaster.width = 16;   // 16 (2^4) - power of 2 requirement
  pColorPaletteRaster.height = 16;  // 16 (2^4) - 16x16 = 256 pixels
  pColorPaletteRaster.bpp = Tyra::TextureBpp::bpp8;  // 8-bit indexed texture
  pColorPaletteRaster.gsComponents = TEXTURE_COMPONENTS_RGBA;

  // Create 8-bit texture data (256 indices in 16x16 layout)
  pColorPaletteRaster.data = new u8[256];
  for (int i = 0; i < 256; i++) {
    pColorPaletteRaster.data[i] =
        i;  // Each pixel points to its own palette entry
  }

  // Create 256-entry RGBA CLUT (Color Look-Up Table)
  // CLUT must be configured separately from texture data
  u32* clutData = new u32[256];
  for (int i = 0; i < 256; i++) {
    clutData[i] = 0x00000000;  // Initialize to transparent black
  }

  // Configure CLUT properties
  pColorPaletteRaster.clut = reinterpret_cast<u8*>(clutData);
  pColorPaletteRaster.clutWidth = 16;  // 16x16 = 256 entries
  pColorPaletteRaster.clutHeight = 16;
  pColorPaletteRaster.clutBpp = Tyra::TextureBpp::bpp32;  // CLUT is 32-bit RGBA
  pColorPaletteRaster.clutGsComponents = TEXTURE_COMPONENTS_RGBA;

  // for (int i = 0; i < 1024; i++) {
  //   pColorPaletteRaster.data[i] = 128;
  // }

  // u8* pallet = pColorPaletteRaster.data;
  // for (int i = 0; i < 256; i++) {
  //   const int targetIndex = getPatternValue(i) * 4;
  //   const u8 value = std::min(128, int((i * 3) + 1));

  //   pallet[targetIndex + 0] = value;
  //   pallet[targetIndex + 1] = value;
  //   pallet[targetIndex + 2] = value;
  //   pallet[targetIndex + 3] = value;

  //   printf("[%lu] = %i\n", getPatternValue(i), value);
  // }

  // const u8 blockLength = 8;
  // const uint32_t maxValue = 128;
  // uint32_t* pallet = reinterpret_cast<uint32_t*>(pColorPaletteRaster.data);

  // for (uint32_t i = 0; i <= 5; i++) {
  //   const uint32_t offset = getPatternValue(i) * blockLength;
  //   TYRA_LOG("offset: ", offset);

  //   for (u8 j = 0; j < 8; j++) {
  //     const uint32_t index = offset + j;

  //     TYRA_LOG("index: ", index);

  //     // const uint32_t value = std::round(easeInOut(i, 255, 0.08f, 40));
  //     // const uint32_t value = std::floor(std::exp2(std::pow(i, 0.5f)
  //     * 1.5f));
  //     // const uint32_t value = std::floor(std::exp2(index * 0.35f));

  //     const uint32_t value = index * 2;

  //     if (index < 256) pallet[index] = std::min(value, maxValue) << 24;
  //   }
  // }

  pFogTexture = new Texture(&pColorPaletteRaster);
  t_renderer->core.texture.repository.add(pFogTexture);

  TYRA_LOG("Fog texture created - checking CLUT...");
  if (pFogTexture->clut == nullptr) {
    TYRA_WARN("WARNING: pFogTexture->clut is NULL after creation!");
  } else {
    TYRA_LOG("CLUT exists:");
    TYRA_LOG("  - Data pointer: ", (void*)pFogTexture->clut->data);
    TYRA_LOG("  - Width: ", pFogTexture->clut->width,
             ", Height: ", pFogTexture->clut->height);
    TYRA_LOG("  - Size: ", pFogTexture->clut->width * pFogTexture->clut->height,
             " entries");
  }

  // IMPORTANT: useTexture() must be called to allocate texture in VRAM
  // before we can modify CLUT data
  RendererCoreTextureBuffers fogBuffers =
      t_renderer->core.texture.useTexture(pFogTexture);

  TYRA_LOG("Fog texture allocated in VRAM:");
  TYRA_LOG("  - Texture address: ", fogBuffers.core->address);
  TYRA_LOG("  - CLUT address: ", fogBuffers.clut->address);

  // uint8_t fog_scale[18] = {1, 1, 0, 0, 0, 0, 0, 0, 0,
  //                          0, 0, 0, 0, 0, 0, 0, 0, 0};

  // Configure fog depth curve
  // Near objects (low depth) = low alpha (less fog)
  // Far objects (high depth) = high alpha (more fog)
  uint8_t fog_scale[18] = {1, 1, 2, 3, 10, 7, 3, 2, 1,
                           1, 0, 0, 0, 0,  0, 0, 0, 0};

  // First set alpha values in CLUT (depth-based fog intensity)
  scaleDepthMask(pFogTexture, 1, fog_scale);

  // Then apply initial fog color to palette (default gray fog)
  // This sets RGB values in the palette, alpha comes from scaleDepthMask
  // NOTE: Must be called AFTER scaleDepthMask to preserve alpha values
  applyFogColorToPalette(Color(128, 128, 128));

  TYRA_LOG("Fog palette initialized with ", 256, " entries");
};

void PostFxManager::dumpGsData(char* prefix, bool trap) {
  dma_channel_wait(DMA_CHANNEL_GIF, 0);

  RendererCoreTextureBuffers depthTexBuffer =
      t_renderer->core.texture.useTexture(pDepthBufferTexture);
  // ps2_screenshot_file(
  //     Tyra::FileUtils::fromCwd(std::string("gs_debug/") + prefix +
  //                              "_depth_buffer_tex.tga")
  //     .c_str(),
  // depthTexBuffer.clut->address >> 6, pDepthBufferTexture->core->width,
  // pDepthBufferTexture->core->height, depthTexBuffer.clut->psm);

  // RendererCoreTextureBuffers fogTexBuffer =
  //     t_renderer->core.texture.useTexture(pFogTexture);
  // ps2_screenshot_file((std::string(prefix) +
  // "_color_buffer_tex.tga").c_str(),
  //                     fogTexBuffer.clut->address >> 6,
  //                     pFogTexture->core->width, pFogTexture->core->height,
  //                     fogTexBuffer.clut->psm);

  // uint32_t width = settings.getWidth(), height = settings.getHeight();
  const zbuffer_t zbuffer = t_renderer->core.gs.zBuffer;
  // printf("zbuffer.address: %X\n", zbuffer.address);
  // ps2_screenshot_file((std::string(prefix) + "_depth_buffer.tga").c_str(),
  //                     zbuffer.address >> 6, width, height, zbuffer.zsm);

  const framebuffer_t front_buffer = t_renderer->core.gs.getFrameBuffer(0);
  printf("front_buffer.address: %X\n", front_buffer.address);
  ps2_screenshot_file((std::string(prefix) + "_front_buffer.tga").c_str(),
                      front_buffer.address >> 6, front_buffer.width,
                      front_buffer.height, front_buffer.psm);

  const framebuffer_t back_buffer = t_renderer->core.gs.getFrameBuffer(1);
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

void PostFxManager::render(Color fogColor) {
  // Update fog color in palette only if it changed (optimization)
  if (fogColor.r != currentFogColor.r || fogColor.g != currentFogColor.g ||
      fogColor.b != currentFogColor.b) {
    applyFogColorToPalette(fogColor);
    currentFogColor = fogColor;
    TYRA_LOG("Fog color changed, palette updated");
  }

  // Apply the post effects
  renderFog(fogColor);

  // Reset GS settings after fog
  const u8 context = t_renderer->core.gs.getDrawContext();
  qword_t packets[20] ALIGNED(64);
  qword_t* q = packets;

  PACK_GIFTAG(q, GIF_SET_TAG(2, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
  q++;

  // Restore default alpha blending
  PACK_GIFTAG(q,
              GS_SET_ALPHA(BLEND_COLOR_SOURCE, BLEND_COLOR_DEST,
                           BLEND_ALPHA_SOURCE, BLEND_COLOR_DEST, 0x80),
              GS_REG_ALPHA_1 + context);
  q++;

  // Restore default clamp mode
  PACK_GIFTAG(q, GS_SET_CLAMP(WRAP_CLAMP, WRAP_CLAMP, 0, 0, 0, 0),
              GS_REG_CLAMP_1 + context);
  q++;

  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
  dma_wait_fast();
}

uint32_t getPatternValue(uint32_t N) {
  // uint32_t mod = N % 3;
  // if (mod == 1) {
  //   return N + 1;
  // } else if (mod == 2) {
  //   return N - 1;
  // } else {
  //   return N;
  // }
  const int r = N % 3;
  return N + (r == 1 ? 1 : (r == 2 ? -1 : 0));
}

double easeInOut(double x, double maxInput, double offset = 0.1,
                 double steepness = 15.0) {
  // Optional: clamp x to the range [0, maxInput]
  x = std::max(std::min(x, maxInput), 0.0);

  // Normalize input to [0,1]
  double t = x / maxInput;

  // Compute the logistic (sigmoid) function value
  double logistic = 1.0 / (1.0 + std::exp(-steepness * (t - offset)));

  // Determine the logistic values at the endpoints for normalization:
  // at t = 0:
  double logistic0 = 1.0 / (1.0 + std::exp(steepness * offset));
  // at t = 1:
  double logistic1 = 1.0 / (1.0 + std::exp(-steepness * (1.0 - offset)));

  // Normalize so that logistic0 maps to 0 and logistic1 maps to 1
  double normalized = (logistic - logistic0) / (logistic1 - logistic0);

  return normalized * 128.0;
}

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

  t_renderer->core.texture.updateTextureInfo(pFogTexture);

  debugPalletIndex++;
  debugPalletIndex %= 24;
}

void PostFxManager::copyDepthBuffer(ColourChannels channelIn,
                                    Texture* palette) {
  // TYRA_LOG("=== copyDepthBuffer START ===");

  uint32_t width = settings.getWidth(), height = settings.getHeight();
  uint32_t zbufferAddr = t_renderer->core.gs.zBuffer.address;
  const zbuffer_t zbuffer = t_renderer->core.gs.zBuffer;

  RendererCoreTextureBuffers texBuffer =
      t_renderer->core.texture.useTexture(palette);

  uint32_t pal_addr = texBuffer.core->address;

  // Detect Z-buffer format and configure appropriately
  uint32_t zbuffer_psm = (zbuffer.zsm == GS_ZBUF_32)   ? GS_PSMZ_32
                         : (zbuffer.zsm == GS_ZBUF_24) ? GS_PSMZ_24
                         : (zbuffer.zsm == GS_ZBUF_16) ? GS_PSMZ_16
                                                       : GS_PSMZ_16S;

  // For PSMZ_24, the page width is different (2 instead of 1)
  uint32_t zbuffer_width = (zbuffer_psm == GS_PSMZ_24) ? 2 : 1;

  // TYRA_LOG("Width: ", width, ", Height: ", height);
  // TYRA_LOG("Z-buffer address: ", zbufferAddr);
  // TYRA_LOG("Z-buffer format: ", (zbuffer_psm == GS_PSMZ_32) ? "32-bit" :
  //                                (zbuffer_psm == GS_PSMZ_24) ? "24-bit" :
  //                                (zbuffer_psm == GS_PSMZ_16) ? "16-bit" :
  //                                "16S-bit");
  // TYRA_LOG("Palette address: ", pal_addr);
  // TYRA_LOG("Channel: ", (channelIn == CHANNEL_RED     ? "RED"
  //                        : channelIn == CHANNEL_GREEN ? "GREEN"
  //                        : channelIn == CHANNEL_BLUE  ? "BLUE"
  //                                                     : "ALPHA"));

  uint32_t page = 0;
  uint32_t x, y;

  for (y = 0; y < height; y += 32) {
    for (x = 0; x < width; x += 64) {
      uint32_t buf_addr =
          t_renderer->core.texture.useTexture(pDepthBufferTexture)
              .core->address;

      qword_t packets[5] ALIGNED(64);
      qword_t* q = packets;

      PACK_GIFTAG(q, GIF_SET_TAG(4, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
      q++;

      PACK_GIFTAG(q,
                  GS_SET_BITBLTBUF((zbufferAddr >> 6) + page, zbuffer_width,
                                   zbuffer_psm, buf_addr >> 6, 1, GS_PSM_32),
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

      pal_addr = t_renderer->core.texture.useTexture(palette).core->address;

      dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
      dma_channel_fast_waits(DMA_CHANNEL_GIF);
      dma_wait_fast();

      performChannelCopy(channelIn, CHANNEL_GREEN, x, y, buf_addr, width,
                         height, pal_addr);

      page += 32;
    }
  }

  // TYRA_LOG("=== copyDepthBuffer END ===");
};

void PostFxManager::performChannelCopy(ColourChannels channelIn,
                                       ColourChannels channelOut,
                                       uint32_t blockX, uint32_t blockY,
                                       uint32_t source_addr, uint32_t width,
                                       uint32_t height, uint32_t pal_addr) {
  const framebuffer_t buf_frame = t_renderer->core.gs.getCurrentFrameData();
  const u8 context = t_renderer->core.gs.getDrawContext();

  // TYRA_LOG("performChannelCopy - Block(", blockX, ",", blockY,
  //          "), src:", source_addr, ", pal:", pal_addr);

  // For the BLUE and ALPHA channels, we need to offset our 'U's by 8 texels
  const uint32_t horz_block_offset =
      (channelIn == CHANNEL_BLUE || channelIn == CHANNEL_ALPHA);
  // For the GREEN and ALPHA channels, we need to offset our 'T's by 2 texels
  const uint32_t vert_block_offset =
      (channelIn == CHANNEL_GREEN || channelIn == CHANNEL_ALPHA);

  const uint32_t clamp_horz = horz_block_offset ? 8 : 0;
  const uint32_t clamp_vert = vert_block_offset ? 2 : 0;

  // TYRA_LOG("Offsets - horz: ", horz_block_offset,
  //          ", vert: ", vert_block_offset);
  // TYRA_LOG("Clamps - horz: ", clamp_horz, ", vert: ", clamp_vert);

  qword_t packets[500] ALIGNED(64);
  qword_t* q = packets;

  PACK_GIFTAG(q, GIF_SET_TAG(5, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
  q++;

  PACK_GIFTAG(q, GS_SET_XYOFFSET(0, 0), GS_REG_XYOFFSET_1 + context);
  q++;

  int tw, th;
  setTwTh(width, height, &tw, &th);

  PACK_GIFTAG(
      q,
      GS_SET_TEX0(source_addr >> 6, 2, GS_PSM_8, tw, th, 1,
                  TEXTURE_FUNCTION_DECAL, pal_addr >> 6, GS_PSM_32, 0, 0, 1),
      GS_REG_TEX0_1 + context);
  q++;

  PACK_GIFTAG(q,
              GS_SET_CLAMP(WRAP_REGION_REPEAT, WRAP_REGION_REPEAT, 0xF7,
                           clamp_horz, 0xFD, clamp_vert),
              GIF_REG_CLAMP_1 + context);
  q++;

  PACK_GIFTAG(q, GS_SET_TEXFLUSH(1), GS_REG_TEXFLUSH);
  q++;

  uint32_t frame_mask;
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

  PACK_GIFTAG(q,
              GS_SET_FRAME(buf_frame.address >> 11, buf_frame.width >> 6,
                           GS_PSM_32, frame_mask),
              GS_REG_FRAME_1 + context);
  q++;

  PACK_GIFTAG(
      q,
      GIF_SET_TAG(96, 1, 1, GS_SET_PRIM(GS_PRIM_SPRITE, 0, 1, 0, 0, 0, 1, 0, 0),
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
  dma_wait_fast();

  q = packets;

  PACK_GIFTAG(q, GIF_SET_TAG(2, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
  q++;

  //   PACK_GIFTAG(q, GS_SET_CLAMP(WRAP_CLAMP, WRAP_CLAMP, 0, 0, 0, 0),
  //               GS_REG_CLAMP_1);
  //   q++;

  PACK_GIFTAG(q,
              GS_SET_FRAME(buf_frame.address >> 11, buf_frame.width >> 6,
                           buf_frame.psm, 0x00),
              GS_REG_FRAME_1 + context);
  q++;

  PACK_GIFTAG(
      q,
      GS_SET_XYOFFSET(ftoi4(screenCenter - (settings.getWidth() / 2.0F)),
                      ftoi4(screenCenter - (settings.getHeight() / 2.0F))),
      GS_REG_XYOFFSET_1);
  q++;

  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
  dma_wait_fast();
};

void PostFxManager::setTwTh(int w, int h, int* tw, int* th) {
  *tw = 31 - (lzw(w) + 1);
  if (w > (1 << *tw)) (*tw)++;

  *th = 31 - (lzw(h) + 1);
  if (h > (1 << *th)) (*th)++;
}

void PostFxManager::uploadClutToVram(Texture* texture) {
  // Upload CLUT directly to VRAM using BITBLTBUF + TRXDIR
  // This bypasses the need for texture to be "linked" to a sprite

  if (texture == nullptr || texture->clut == nullptr) {
    TYRA_WARN("Cannot upload CLUT: texture or CLUT is null!");
    return;
  }

  // Get CLUT info from texture buffers
  RendererCoreTextureBuffers texBuffers =
      t_renderer->core.texture.useTexture(texture);

  uint32_t clut_addr = texBuffers.clut->address;
  uint32_t clut_width = texture->clut->width;
  uint32_t clut_height = texture->clut->height;
  uint32_t clut_size =
      clut_width * clut_height * 4;  // RGBA32 = 4 bytes per pixel

  TYRA_LOG("Uploading CLUT to VRAM:");
  TYRA_LOG("  - Address: ", clut_addr);
  TYRA_LOG("  - Size: ", clut_width, "x", clut_height, " (", clut_size,
           " bytes)");

  // Create DMA packet for CLUT upload
  qword_t* packets = (qword_t*)memalign(64, sizeof(qword_t) * 128);
  qword_t* q = packets;

  // Setup BITBLTBUF for host-to-local transfer
  PACK_GIFTAG(q, GIF_SET_TAG(4, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
  q++;

  PACK_GIFTAG(
      q,
      GS_SET_BITBLTBUF(0, 0, 0,                        // Source: host memory
                       clut_addr >> 6, 0, GS_PSM_32),  // Dest: CLUT in VRAM
      GS_REG_BITBLTBUF);
  q++;

  PACK_GIFTAG(q, GS_SET_TRXPOS(0, 0, 0, 0, 0), GS_REG_TRXPOS);
  q++;

  PACK_GIFTAG(q, (uint64_t)(clut_width) | ((uint64_t)(clut_height) << 32),
              GS_REG_TRXREG);
  q++;

  PACK_GIFTAG(q, 0L, GS_REG_TRXDIR);  // 0 = host-to-local
  q++;

  // Send setup packets
  FlushCache(0);
  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
  dma_wait_fast();

  // Now send actual CLUT data
  uint32_t qword_count = (clut_size + 15) / 16;  // Round up to qwords

  q = packets;
  PACK_GIFTAG(q, GIF_SET_TAG(qword_count, 0, 0, 0, GIF_FLG_IMAGE, 0), 0);
  q++;

  // Copy CLUT data
  uint32_t* clut_data = reinterpret_cast<uint32_t*>(texture->clut->data);
  memcpy(q, clut_data, clut_size);
  q += qword_count;

  // Send CLUT data
  FlushCache(0);
  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
  dma_wait_fast();

  free(packets);

  TYRA_LOG("CLUT uploaded successfully");
}

void PostFxManager::scaleDepthMask(Texture* palette, uint8_t initial_value,
                                   uint8_t factors[16]) {
  int i, j, k = initial_value;

  static const uint8_t factor_order[18][2] = {
      {0, 3},   {4, 7},   {16, 19},   {20, 23},   {8, 15},    {24, 31},
      {40, 47}, {32, 39}, {48, 55},   {34, 71},   {56, 63},   {72, 79},
      {88, 95}, {80, 87}, {112, 119}, {104, 111}, {120, 127}, {136, 143}};

  // Access the CLUT data (palette), not the texture data
  uint32_t* pal_rgba = reinterpret_cast<uint32_t*>(palette->clut->data);

  printf("Fog palette values (alpha): \n");

  for (j = 0; j < 16; j++) {
    for (i = factor_order[j][0]; i <= factor_order[j][1]; i++) {
      // PS2 format: RGBA (each 8 bits)
      // We store fog color (will be set dynamically) with depth-based alpha
      // For now, use white color - will be modulated with primitive color
      // Alpha increases with depth index = more fog at distance
      pal_rgba[i] = (k << 24) |     // Alpha (fog intensity based on depth)
                    (0xFF << 16) |  // R = white (will be modulated)
                    (0xFF << 8) |   // G = white
                    0xFF;           // B = white

      printf("%i,", k);

      if (k < 128 || factors[j] >= 128) {
        k += factors[j];
      } else if (k != 128) {
        k = 128;
      }
    }
  }

  printf("\n");

  // Upload the updated palette to VRAM directly
  uploadClutToVram(palette);

  TYRA_LOG("Palette updated and uploaded to VRAM with ", 256, " fog entries");
}

void PostFxManager::applyFogColorToPalette(Color fogColor) {
  // Apply fog color to all palette entries while preserving alpha values
  // This allows dynamic fog color without rebuilding the entire palette

  // Safety check: ensure CLUT exists and is allocated
  if (pFogTexture == nullptr || pFogTexture->clut == nullptr) {
    TYRA_WARN("Cannot apply fog color: CLUT is not initialized!");
    return;
  }

  if (pFogTexture->clut->data == nullptr) {
    TYRA_WARN("Cannot apply fog color: CLUT data is null!");
    return;
  }

  uint32_t* pal_rgba = reinterpret_cast<uint32_t*>(pFogTexture->clut->data);

  for (int i = 0; i < 256; i++) {
    uint32_t current_alpha = pal_rgba[i] & 0xFF000000;  // Preserve alpha
    pal_rgba[i] = current_alpha |                       // Keep alpha
                  ((uint32_t)fogColor.r << 16) |        // Set R
                  ((uint32_t)fogColor.g << 8) |         // Set G
                  (uint32_t)fogColor.b;                 // Set B
  }

  // Upload updated palette to VRAM directly
  uploadClutToVram(pFogTexture);

  TYRA_LOG("Fog color updated in palette: R=", (int)fogColor.r,
           " G=", (int)fogColor.g, " B=", (int)fogColor.b);
}

void PostFxManager::renderFog(Color fog) {
  TYRA_LOG("=== renderFog START ===");

  uint32_t width = settings.getWidth(), height = settings.getHeight();

  const framebuffer_t buf_frame = t_renderer->core.gs.getCurrentFrameData();
  const zbuffer_t& zbuffer = t_renderer->core.gs.zBuffer;
  const u8 context = t_renderer->core.gs.getDrawContext();

  TYRA_LOG("Applying fog - Context: ", (int)context);
  TYRA_LOG("Color: R=", (int)fog.r, " G=", (int)fog.g, " B=", (int)fog.b);

  qword_t packets[500] ALIGNED(64);
  qword_t* q = packets;

  // Step 1: Setup GS state for post-processing overlay
  // Critical: Set XYOFFSET to 0 BEFORE drawing to use absolute screen
  // coordinates
  PACK_GIFTAG(q, GIF_SET_TAG(6, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
  q++;

  // Set XYOFFSET to (0,0) for absolute screen-space rendering
  PACK_GIFTAG(q, GS_SET_XYOFFSET(0, 0), GS_REG_XYOFFSET_1 + context);
  q++;

  // Disable Z-buffer writes (but not tests - we want to preserve depth)
  PACK_GIFTAG(q, GS_SET_ZBUF(zbuffer.address >> 13, zbuffer.zsm, 1),
              GS_REG_ZBUF_1 + context);
  q++;

  // Explicitly disable texture mapping by setting TEX0 to zero
  PACK_GIFTAG(q, 0L, GS_REG_TEX0_1 + context);
  q++;

  // Disable texture mapping in TEX1 as well
  PACK_GIFTAG(q, 0L, GS_REG_TEX1_1 + context);
  q++;

  // ALPHA register: Simple alpha blending
  // (Cs - Cd) * As + Cd = blend fog over scene
  PACK_GIFTAG(q,
              GS_SET_ALPHA(BLEND_COLOR_SOURCE, BLEND_COLOR_DEST,
                           BLEND_ALPHA_SOURCE, BLEND_COLOR_DEST, 0x0),
              GS_REG_ALPHA_1 + context);
  q++;

  // RGBAQ: Set fog color with alpha
  PACK_GIFTAG(
      q, GS_SET_RGBAQ((int)fog.r, (int)fog.g, (int)fog.b, 0x80, 0x3f800000),
      GS_REG_RGBAQ);
  q++;

  TYRA_LOG("GS state configured for post-FX");

  // Step 2: Draw full-screen quad using TRIANGLE_STRIP (more reliable than
  // SPRITE) Using absolute coordinates (XYOFFSET is set to 0)
  PACK_GIFTAG(
      q,
      GIF_SET_TAG(4, 1, 1,
                  GS_SET_PRIM(GS_PRIM_TRIANGLE_STRIP, 0, 0, 0, 0, 0, 1, 0, 0),
                  GIF_FLG_PACKED, 1),
      GIF_REG_XYZ2);
  q++;

  // Vertex 1: Top-left (0, 0)
  PACK_GIFTAG(q, GIF_SET_XYZ(0 << 4, 0 << 4, 0), 0);
  q++;

  // Vertex 2: Top-right (width, 0)
  PACK_GIFTAG(q, GIF_SET_XYZ(width << 4, 0 << 4, 0), 0);
  q++;

  // Vertex 3: Bottom-left (0, height)
  PACK_GIFTAG(q, GIF_SET_XYZ(0 << 4, height << 4, 0), 0);
  q++;

  // Vertex 4: Bottom-right (width, height)
  PACK_GIFTAG(q, GIF_SET_XYZ(width << 4, height << 4, 0), 0);
  q++;

  // Step 3: Restore original GS state
  PACK_GIFTAG(q, GIF_SET_TAG(2, 1, 0, 0, GIF_FLG_PACKED, 1), GIF_REG_AD);
  q++;

  // Restore original XYOFFSET (centered)
  PACK_GIFTAG(
      q,
      GS_SET_XYOFFSET(ftoi4(screenCenter - (settings.getWidth() / 2.0F)),
                      ftoi4(screenCenter - (settings.getHeight() / 2.0F))),
      GS_REG_XYOFFSET_1 + context);
  q++;

  // Re-enable Z-buffer writes
  PACK_GIFTAG(q, GS_SET_ZBUF(zbuffer.address >> 13, zbuffer.zsm, 0),
              GS_REG_ZBUF_1 + context);
  q++;

  TYRA_LOG("Total packets: ", q - packets);

  // Send to GS
  FlushCache(0);
  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, q - packets, 0, 0);
  dma_wait_fast();

  TYRA_LOG("=== renderFog END ===");
};

}  // namespace Demo
