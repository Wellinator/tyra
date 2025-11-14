# GsDebugUtils - Utilitário de Debug para Pacotes GS

Classe estática para inspecionar e parsear pacotes GIF/GS do PlayStation 2.

## Uso

```cpp
#include "utils/gs_debug_utils.hpp"

void PostFxManager::render(Color fogColor) {
  qword_t packets[50] ALIGNED(64);
  qword_t* q = packets;

  // ... montar seus pacotes ...

  int packetCount = q - packets;

  // Debug antes de enviar para o GS
  GsDebugUtils::debugPackets(packets, packetCount, "Fog Debug");

  FlushCache(0);
  dma_channel_send_normal(DMA_CHANNEL_GIF, packets, packetCount, 0, 0);
}
```

## Exemplo de Output

```
========== Fog Debug (count=12) ==========
[00] RAW: 0x000000000000000E_0000000000000003
     GIFTAG: NLOOP=3, EOP=0, PRE=0, PRIM=0x000, FLG=0, NREG=1
     Format: PACKED
     Registers: AD
[01] RAW: 0x000000000000004A_0000000000001900
     FRAME_1 (0x4A): DATA=0x00000000_0000000000001900
          FBP=0x0 (addr=0x0), FBW=1 (width=64), PSM=25, FBMSK=0x00000000
[02] RAW: 0x0000000000000040_0000000000000150
     ALPHA_1 (0x40): DATA=0x00000000_0000000000000150
          Formula: (Cd - Cs) * As + Cs, FIX=0
...
```

## Registradores Suportados

- **ALPHA**: Parseia fórmula de blending (A, B, C, D, FIX)
- **FRAME**: FBP, FBW, PSM, FBMSK
- **ZBUF**: ZBP, PSM, ZMSK
- **TEST**: ATE, ATST, AREF, AFAIL, DATE, DATM, ZTE, ZTST
- **XYOFFSET**: OFX, OFY (em pixels)
- **TEX0**: TBP0, TBW, PSM, TW/TH, CLUT info
- E mais de 40 outros registradores GS identificados por nome
