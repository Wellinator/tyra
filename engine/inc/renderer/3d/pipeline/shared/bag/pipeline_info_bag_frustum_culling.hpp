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

namespace Tyra {

enum PipelineInfoBagFrustumCulling {
  /** No frustum culling  */
  PipelineInfoBagFrustumCulling_None = 0,
  /** Frustum culling of parts of an object imprecise */
  PipelineInfoBagFrustumCulling_Simple = 1,
  /** Frustum culling of parts of an object more accurate */
  PipelineInfoBagFrustumCulling_Precise = 2,
};

}
