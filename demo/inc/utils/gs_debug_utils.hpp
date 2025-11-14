#pragma once

#include <tamtypes.h>
#include <dma.h>

namespace Demo {

class GsDebugUtils {
 public:
  /**
   * Debug e parseia pacotes GIF/GS
   * @param packets Array de qwords para inspecionar
   * @param count Número de qwords no array
   * @param label Label descritivo para o output
   */
  static void debugPackets(qword_t* packets, int count,
                           const char* label = "GIF Packets");

 private:
  static const char* getRegisterName(u64 addr);
  static void parseAlphaRegister(u64 data);
  static void parseFrameRegister(u64 data);
  static void parseZbufRegister(u64 data);
  static void parseTestRegister(u64 data);
  static void parseXyoffsetRegister(u64 data);
  static void parseTex0Register(u64 data);
};

}  // namespace Demo
