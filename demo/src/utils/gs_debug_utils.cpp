#include "utils/gs_debug_utils.hpp"
#include <stdio.h>

namespace Demo {

void GsDebugUtils::debugPackets(qword_t* packets, int count,
                                const char* label) {
  printf("\n========== %s (count=%d) ==========\n", label, count);

  for (int i = 0; i < count; i++) {
    qword_t* q = &packets[i];
    printf("[%02d] RAW: 0x%016llX_%016llX\n", i, q->dw[1], q->dw[0]);

    // Verificar primeiro se parece formato A+D
    u64 addr = q->dw[1] & 0xFF;
    bool looks_like_ad = (addr <= 0x7F) && ((q->dw[1] >> 8) == 0);

    // Extrair campos potenciais de GIFTAG
    u64 nloop = q->dw[0] & 0x7FFF;
    u64 flg = (q->dw[0] >> 58) & 3;
    u64 nreg = (q->dw[0] >> 60) & 0xF;

    // GIFTAG válido precisa de estrutura coerente
    // Se parece A+D E não tem características fortes de GIFTAG, tratar como A+D
    bool strong_giftag = (nloop > 0) && (nreg > 0) && (flg <= 2);

    if (looks_like_ad && !strong_giftag) {
      // Formato A+D: endereço válido e não parece GIFTAG forte
      u64 data = q->dw[0];

      const char* reg_name = getRegisterName(addr);
      printf("     %s (0x%02llX): DATA=0x%016llX\n", reg_name, addr, data);

      // Parsear registradores específicos
      switch (addr) {
        case 0x42:
        case 0x43:  // ALPHA
          parseAlphaRegister(data);
          break;
        case 0x4C:
        case 0x4D:  // FRAME
          parseFrameRegister(data);
          break;
        case 0x4E:
        case 0x4F:  // ZBUF
          parseZbufRegister(data);
          break;
        case 0x47:
        case 0x48:  // TEST
          parseTestRegister(data);
          break;
        case 0x18:
        case 0x19:  // XYOFFSET
          parseXyoffsetRegister(data);
          break;
        case 0x06:
        case 0x07:  // TEX0
          parseTex0Register(data);
          break;
      }
      continue;
    }

    // Tentar detectar GIFTAG
    if (strong_giftag) {
      u64 eop = (q->dw[0] >> 15) & 1;
      u64 pre = (q->dw[0] >> 46) & 1;
      u64 prim = (q->dw[0] >> 47) & 0x7FF;
      u64 regs = q->dw[1];

      printf(
          "     GIFTAG: NLOOP=%llu, EOP=%llu, PRE=%llu, PRIM=0x%03llX, "
          "FLG=%llu, NREG=%llu\n",
          nloop, eop, pre, prim, flg, nreg);

      const char* flg_names[] = {"PACKED", "REGLIST", "IMAGE", "DISABLE"};
      printf("     Format: %s\n", flg_names[flg]);

      if (flg == 0) {  // PACKED
        printf("     Registers: ");
        for (int r = 0; r < nreg && r < 16; r++) {
          u8 reg_id = (regs >> (r * 4)) & 0xF;
          const char* reg_names[] = {"PRIM",    "RGBAQ",   "ST",     "UV",
                                     "XYZF2",   "XYZ2",    "TEX0_1", "TEX0_2",
                                     "CLAMP_1", "CLAMP_2", "FOG",    "RSVD",
                                     "XYZF3",   "XYZ3",    "AD",     "NOP"};
          printf("%s ", reg_names[reg_id]);
        }
        printf("\n");
      }

      if (pre && prim != 0) {
        u64 prim_type = prim & 7;
        u64 iip = (prim >> 3) & 1;
        u64 tme = (prim >> 4) & 1;
        u64 fge = (prim >> 5) & 1;
        u64 abe = (prim >> 6) & 1;
        u64 aa1 = (prim >> 7) & 1;
        u64 fst = (prim >> 8) & 1;
        u64 ctxt = (prim >> 9) & 1;
        u64 fix = (prim >> 10) & 1;

        const char* prim_names[] = {
            "POINT",          "LINE",         "LINE_STRIP", "TRIANGLE",
            "TRIANGLE_STRIP", "TRIANGLE_FAN", "SPRITE",     "INVALID"};
        printf(
            "     PRIM: %s, IIP=%llu, TME=%llu, FGE=%llu, ABE=%llu, AA1=%llu, "
            "FST=%llu, CTXT=%llu, FIX=%llu\n",
            prim_names[prim_type], iip, tme, fge, abe, aa1, fst, ctxt, fix);
      }
      continue;
    }

    // Se não é nem A+D nem GIFTAG válido, é um pacote desconhecido ou dados
    printf("     UNKNOWN/DATA packet\n");
  }
  printf("========================================\n\n");
}

const char* GsDebugUtils::getRegisterName(u64 addr) {
  switch (addr) {
    case 0x00:
      return "PRIM";
    case 0x01:
      return "RGBAQ";
    case 0x02:
      return "ST";
    case 0x03:
      return "UV";
    case 0x04:
      return "XYZF2";
    case 0x05:
      return "XYZ2";
    case 0x06:
      return "TEX0_1";
    case 0x07:
      return "TEX0_2";
    case 0x08:
      return "CLAMP_1";
    case 0x09:
      return "CLAMP_2";
    case 0x0A:
      return "FOG";
    case 0x0C:
      return "XYZF3";
    case 0x0D:
      return "XYZ3";
    case 0x14:
      return "TEX1_1";
    case 0x15:
      return "TEX1_2";
    case 0x16:
      return "TEX2_1";
    case 0x17:
      return "TEX2_2";
    case 0x18:
      return "XYOFFSET_1";
    case 0x19:
      return "XYOFFSET_2";
    case 0x1A:
      return "PRMODECONT";
    case 0x1B:
      return "PRMODE";
    case 0x1C:
      return "TEXCLUT";
    case 0x22:
      return "SCANMSK";
    case 0x34:
      return "MIPTBP1_1";
    case 0x35:
      return "MIPTBP1_2";
    case 0x36:
      return "MIPTBP2_1";
    case 0x37:
      return "MIPTBP2_2";
    case 0x3B:
      return "TEXA";
    case 0x3D:
      return "FOGCOL";
    case 0x3F:
      return "TEXFLUSH";
    case 0x40:
      return "SCISSOR_1";
    case 0x41:
      return "SCISSOR_2";
    case 0x42:
      return "ALPHA_1";
    case 0x43:
      return "ALPHA_2";
    case 0x44:
      return "DIMX";
    case 0x45:
      return "DTHE";
    case 0x46:
      return "COLCLAMP";
    case 0x47:
      return "TEST_1";
    case 0x48:
      return "TEST_2";
    case 0x49:
      return "PABE";
    case 0x4A:
      return "FBA_1";
    case 0x4B:
      return "FBA_2";
    case 0x4C:
      return "FRAME_1";
    case 0x4D:
      return "FRAME_2";
    case 0x4E:
      return "ZBUF_1";
    case 0x4F:
      return "ZBUF_2";
    case 0x50:
      return "BITBLTBUF";
    case 0x51:
      return "TRXPOS";
    case 0x52:
      return "TRXREG";
    case 0x53:
      return "TRXDIR";
    case 0x54:
      return "HWREG";
    case 0x60:
      return "SIGNAL";
    case 0x61:
      return "FINISH";
    case 0x62:
      return "LABEL";
    case 0x7F:
      return "NOP";
    default:
      return "UNKNOWN";
  }
}

void GsDebugUtils::parseAlphaRegister(u64 data) {
  u8 a = data & 3;
  u8 b = (data >> 2) & 3;
  u8 c = (data >> 4) & 3;
  u8 d = (data >> 6) & 3;
  u8 fix = (data >> 32) & 0xFF;

  const char* blend_src[] = {"Cs", "Cd", "0", "FIX"};
  const char* blend_fac[] = {"As", "Ad", "FIX", "Reserved"};

  printf("          A=%u(%s), B=%u(%s), C=%u(%s), D=%u(%s), FIX=0x%02X\n", a,
         blend_src[a], b, blend_src[b], c, blend_fac[c], d, blend_src[d], fix);
  printf("          Formula: (%s - %s) * %s + %s\n", blend_src[a], blend_src[b],
         blend_fac[c], blend_src[d]);
}

void GsDebugUtils::parseFrameRegister(u64 data) {
  u32 fbp = data & 0x1FF;
  u32 fbw = (data >> 16) & 0x3F;
  u32 psm = (data >> 24) & 0x3F;
  u32 fbmsk = (data >> 32) & 0xFFFFFFFF;

  printf(
      "          FBP=0x%X (addr=0x%X), FBW=%u (width=%u), PSM=%u, "
      "FBMSK=0x%08X\n",
      fbp, fbp * 2048, fbw, fbw * 64, psm, fbmsk);
}

void GsDebugUtils::parseZbufRegister(u64 data) {
  u32 zbp = data & 0x1FF;
  u32 psm = (data >> 24) & 0xF;
  u32 zmsk = (data >> 32) & 1;

  printf("          ZBP=0x%X (addr=0x%X), PSM=%u, ZMSK=%u\n", zbp, zbp * 2048,
         psm, zmsk);
}

void GsDebugUtils::parseTestRegister(u64 data) {
  u8 ate = data & 1;
  u8 atst = (data >> 1) & 7;
  u8 aref = (data >> 4) & 0xFF;
  u8 afail = (data >> 12) & 3;
  u8 date = (data >> 14) & 1;
  u8 datm = (data >> 15) & 1;
  u8 zte = (data >> 16) & 1;
  u8 ztst = (data >> 17) & 3;

  printf(
      "          ATE=%u, ATST=%u, AREF=%u, AFAIL=%u, DATE=%u, DATM=%u, ZTE=%u, "
      "ZTST=%u\n",
      ate, atst, aref, afail, date, datm, zte, ztst);
}

void GsDebugUtils::parseXyoffsetRegister(u64 data) {
  u32 ofx = data & 0xFFFF;
  u32 ofy = (data >> 32) & 0xFFFF;

  printf("          OFX=%u (%.2f), OFY=%u (%.2f)\n", ofx, ofx / 16.0f, ofy,
         ofy / 16.0f);
}

void GsDebugUtils::parseTex0Register(u64 data) {
  u32 tbp0 = data & 0x3FFF;
  u32 tbw = (data >> 14) & 0x3F;
  u32 psm = (data >> 20) & 0x3F;
  u32 tw = (data >> 26) & 0xF;
  u32 th = (data >> 30) & 0xF;
  u32 tcc = (data >> 34) & 1;
  u32 tfx = (data >> 35) & 3;
  u32 cbp = (data >> 37) & 0x3FFF;
  u32 cpsm = (data >> 51) & 0xF;
  u32 csm = (data >> 55) & 1;
  u32 csa = (data >> 56) & 0x1F;
  u32 cld = (data >> 61) & 7;

  printf("          TBP0=0x%X, TBW=%u, PSM=%u, TW=%u (w=%u), TH=%u (h=%u)\n",
         tbp0, tbw, psm, tw, 1 << tw, th, 1 << th);
  printf(
      "          TCC=%u, TFX=%u, CBP=0x%X, CPSM=%u, CSM=%u, CSA=%u, CLD=%u\n",
      tcc, tfx, cbp, cpsm, csm, csa, cld);
}

}  // namespace Demo
