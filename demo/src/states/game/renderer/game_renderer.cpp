/*
# _____        ____   ___
#   |     \/   ____| |___|
#   |     |   |   \  |   |
#-----------------------------------------------------------------------
# Copyright 2022, tyra - https://github.com/h4570/tyra
# Licensed under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#include "states/game/renderer/game_renderer.hpp"

using Tyra::Threading;

namespace Demo {

GameRenderer::GameRenderer(Renderer* t_renderer) : postFx(t_renderer) {
  renderer = t_renderer;

  stpip.setRenderer(&renderer->core);
  dypip.setRenderer(&renderer->core);

  postFxSprite.position.x = 0;
  postFxSprite.position.y = 0;
  postFxSprite.size.x = 192.0f;
  postFxSprite.size.y = 144.0f;

  postFxSprite.color.r = 128.0f;
  postFxSprite.color.g = 128.0f;
  postFxSprite.color.b = 128.0f;
  postFxSprite.color.a = 128.0f;

  postFxSprite.mode = Tyra::SpriteMode::MODE_STRETCH;
  postFx.pFogTexture->addLink(postFxSprite.id);

  postFxDepthSprite.position.x = 192;
  postFxDepthSprite.position.y = 0;
  postFxDepthSprite.size.x = 192.0f;
  postFxDepthSprite.size.y = 144.0f;

  postFxDepthSprite.color.r = 128.0f;
  postFxDepthSprite.color.g = 128.0f;
  postFxDepthSprite.color.b = 128.0f;
  postFxDepthSprite.color.a = 128.0f;

  postFxDepthSprite.mode = Tyra::SpriteMode::MODE_STRETCH;
  postFx.pDepthBufferTexture->addLink(postFxDepthSprite.id);
}

GameRenderer::~GameRenderer() {}

void GameRenderer::add(std::vector<RendererStaticPair*> t_staticPairs) {
  staticPairs.insert(staticPairs.end(), t_staticPairs.begin(),
                     t_staticPairs.end());
}

void GameRenderer::add(std::vector<RendererDynamicPair*> t_dynamicPairs) {
  dynamicPairs.insert(dynamicPairs.end(), t_dynamicPairs.begin(),
                      t_dynamicPairs.end());
}

void GameRenderer::add(Sprite* sprite) { sprites.push_back(sprite); }

void GameRenderer::add(const CoreBBox& bbox) { bboxes.push_back(bbox); }

void GameRenderer::add(RendererStaticPair* staticPair) {
  staticPairs.push_back(staticPair);
}

void GameRenderer::add(RendererDynamicPair* dynamicPair) {
  dynamicPairs.push_back(dynamicPair);
}

void GameRenderer::clear() {
  staticPairs.clear();
  dynamicPairs.clear();
  sprites.clear();
  bboxes.clear();
}

void GameRenderer::render() {
  // Render static stuff
  if (staticPairs.size()) {
    renderer->renderer3D.usePipeline(&stpip);

    for (auto& pair : staticPairs) {
      stpip.render(pair->mesh, pair->options);
    }
  }

  Threading::switchThread();  // give some time for audio thread

  // Render animated stuff
  // if (dynamicPairs.size()) {
  //   renderer->renderer3D.usePipeline(&dypip);

  //   for (auto& pair : dynamicPairs) {
  //     dypip.render(pair->mesh, pair->options);
  //   }
  // }

  Threading::switchThread();

  // if (downloaded_frame == 0 && frame_count == 100) {
  //   postFx.dumpGsData("pre_fog_", false);
  // } else if (frame_count == 0) {
  //   postFx.dumpGsData("frame_0_pre_fog_", false);
  // } else if (frame_count == 1) {
  //   postFx.dumpGsData("frame_1_pre_fog_", false);
  // }

  postFx.render(Color(0, 0, 200, 128));

  // renderer->renderer2D.render(postFxSprite);
  // renderer->renderer2D.render(postFxDepthSprite);

  // if (downloaded_frame == 0 && frame_count == 100) {
  //   downloaded_frame = 1;
  //   frame_count = 0;
  //   return postFx.dumpGsData("post_fog_", false);
  // } else if (frame_count == 0) {
  //   postFx.dumpGsData("frame_0_post_fog_", false);
  // } else if (frame_count == 1) {
  //   postFx.dumpGsData("frame_1_post_fog_", false);
  // }

  // Render 2D
  for (auto& sprite : sprites) {
    renderer->renderer2D.render(sprite);
  }

  // Render debug stuff after stapip/dynpip, otherwise it will not be visible
  // for (auto& bbox : bboxes) {
  //   renderer->renderer3D.utility.drawBBox(bbox);
  // }

  frame_count++;
}

}  // namespace Demo
