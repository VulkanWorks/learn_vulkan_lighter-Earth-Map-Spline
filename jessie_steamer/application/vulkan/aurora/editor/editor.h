//
//  editor.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H

#include <array>
#include <memory>
#include <vector>

#include "jessie_steamer/application/vulkan/aurora/editor/button.h"
#include "jessie_steamer/application/vulkan/aurora/editor/celestial.h"
#include "jessie_steamer/application/vulkan/aurora/editor/path.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/rotation.h"
#include "jessie_steamer/common/timer.h"
#include "jessie_steamer/common/window.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

class Editor {
 public:
  enum ButtonIndex {
    kPath1ButtonIndex,
    kPath2ButtonIndex,
    kPath3ButtonIndex,
    kEditingButtonIndex,
    kDaylightButtonIndex,
    kAuroraButtonIndex,
    kNumButtons,
    kNumAuroraPaths = kEditingButtonIndex,
  };

  Editor(const wrapper::vulkan::WindowContext& window_context,
         int num_frames_in_flight);

  // This class is neither copyable nor movable.
  Editor(const Editor&) = delete;
  Editor& operator=(const Editor&) = delete;

  // Registers callbacks.
  void OnEnter(common::Window* mutable_window);

  // Unregisters callbacks.
  void OnExit(common::Window* mutable_window);

  void Recreate(const wrapper::vulkan::WindowContext& window_context);

  void UpdateData(const wrapper::vulkan::WindowContext& window_context,
                  int frame);

  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame);

 private:
  class StateManager {
   public:
    explicit StateManager();

    // This class is neither copyable nor movable.
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    void Update(absl::optional<ButtonIndex> clicked_button);

    int GetEditingPathIndex() const;

    bool IsSelected(ButtonIndex index) const {
      return button_states_[index] == Button::State::kSelected;
    }
    bool IsUnselected(ButtonIndex index) const {
      return button_states_[index] == Button::State::kUnselected;
    }
    bool IsEditing() const {
      return IsSelected(ButtonIndex::kEditingButtonIndex);
    }

    // Accessors.
    const std::vector<Button::State>& button_states() const {
      return button_states_;
    }

   private:
    struct ClickInfo {
      ButtonIndex button_index;
      float start_time;
    };

    void SetPathButtonStates(Button::State state);

    void FlipButtonState(ButtonIndex index);

    common::BasicTimer timer_;
    std::vector<Button::State> button_states_;
    absl::optional<ClickInfo> click_info_;
    ButtonIndex last_edited_path_ = kPath1ButtonIndex;
  };

  bool did_press_left_ = false;
  bool did_release_right_ = false;
  common::Sphere earth_;
  common::Sphere aurora_layer_;
  StateManager state_manager_;
  std::unique_ptr<Celestial> celestial_;
  std::unique_ptr<AuroraPath> aurora_path_;
  std::unique_ptr<Button> button_;
  std::unique_ptr<common::UserControlledCamera> general_camera_;
  std::unique_ptr<common::UserControlledCamera> skybox_camera_;
  std::unique_ptr<wrapper::vulkan::NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
  std::unique_ptr<wrapper::vulkan::Image> depth_stencil_image_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_EDITOR_H */
