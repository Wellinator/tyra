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

class StaPipInfoBag : public PipelineInfoBag {
 public:
  StaPipInfoBag() { fullClipChecks = false; }
  ~StaPipInfoBag() {}

  /**
   * @brief Experimental! False -> disables "clip against each plane" algorithm.
   * Default: True.
   *
   * Full clip checks are slow, but they are
   * preventing visual artifacts, which can happen
   * for big 3D objects (or objects near camera eyes)
   * Force enabled in dynamic pipe, because of efficiency.
   */
  bool fullClipChecks;

  /**
   * @brief Experimental! Value used by clipping algorithm.
   * Default: -10.0
   * When objects are very clese to the camera eyes you can you this margin to
   * control the clipping efficiency
   */
  float clipMargin = -10.0F;
};

}  // namespace Tyra
