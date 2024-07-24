/*
# _____        ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licensed under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#pragma once

#include "renderer/3d/pipeline/shared/bag/pipeline_info_bag.hpp"

namespace Tyra {

class DynPipInfoBag : public PipelineInfoBag {
 public:
  DynPipInfoBag() {}
  ~DynPipInfoBag() {}

  /**
   * @brief Experimental! Value used by clipping algorithm.
   * Default: -10.0
   * When objects are very clese to the camera eyes you can you this margin to
   * control the clipping efficiency
   */
  float clipMargin = -10.0F;
};

}  // namespace Tyra
