/* Copyright (c) 2015-2022 The Khronos Group Inc.
 * Copyright (c) 2015-2022 Valve Corporation
 * Copyright (c) 2015-2022 LunarG, Inc.
 * Copyright (C) 2015-2022 Google Inc.
 * Modifications Copyright (C) 2020 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Courtney Goeltzenleuchter <courtneygo@google.com>
 * Author: Tobin Ehlis <tobine@google.com>
 * Author: Chris Forbes <chrisf@ijw.co.nz>
 * Author: Mark Lobodzinski <mark@lunarg.com>
 * Author: Dave Houlton <daveh@lunarg.com>
 * Author: John Zulauf <jzulauf@lunarg.com>
 * Author: Tobias Hector <tobias.hector@amd.com>
 */
#pragma once
#include "base_node.h"
#include "query_state.h"
#include "command_validation.h"
#include "hash_vk_types.h"
#include "subresource_adapter.h"
#include "image_layout_map.h"
#include "pipeline_state.h"
#include "device_state.h"
#include "descriptor_sets.h"
#include "qfo_transfer.h"

struct SUBPASS_INFO;
class FRAMEBUFFER_STATE;
class RENDER_PASS_STATE;
class CoreChecks;
class ValidationStateTracker;

#ifdef VK_USE_PLATFORM_METAL_EXT
static bool GetMetalExport(const VkEventCreateInfo *info) {
    bool retval = false;
    auto export_metal_object_info = LvlFindInChain<VkExportMetalObjectCreateInfoEXT>(info->pNext);
    while (export_metal_object_info) {
        if (export_metal_object_info->exportObjectType == VK_EXPORT_METAL_OBJECT_TYPE_METAL_SHARED_EVENT_BIT_EXT) {
            retval = true;
            break;
        }
        export_metal_object_info = LvlFindInChain<VkExportMetalObjectCreateInfoEXT>(export_metal_object_info->pNext);
    }
    return retval;
}
#endif  // VK_USE_PLATFORM_METAL_EXT

class EVENT_STATE : public BASE_NODE {
  public:
    int write_in_use;
#ifdef VK_USE_PLATFORM_METAL_EXT
    const bool metal_event_export;
#endif  // VK_USE_PLATFORM_METAL_EXT
    VkPipelineStageFlags2KHR stageMask = VkPipelineStageFlags2KHR(0);
    VkEventCreateFlags flags;

    EVENT_STATE(VkEvent event_, const VkEventCreateInfo *pCreateInfo)
        : BASE_NODE(event_, kVulkanObjectTypeEvent),
          write_in_use(0),
#ifdef VK_USE_PLATFORM_METAL_EXT
          metal_event_export(GetMetalExport(pCreateInfo)),
#endif  // VK_USE_PLATFORM_METAL_EXT
          flags(pCreateInfo->flags) {
    }

    VkEvent event() const { return handle_.Cast<VkEvent>(); }
};

// Only CoreChecks uses this, but the state tracker stores it.
constexpr static auto kInvalidLayout = image_layout_map::kInvalidLayout;
using ImageSubresourceLayoutMap = image_layout_map::ImageSubresourceLayoutMap;
typedef layer_data::unordered_map<VkEvent, VkPipelineStageFlags2KHR> EventToStageMap;

// Track command pools and their command buffers
class COMMAND_POOL_STATE : public BASE_NODE {
  public:
    ValidationStateTracker *dev_data;
    const VkCommandPoolCreateFlags createFlags;
    const uint32_t queueFamilyIndex;
    const VkQueueFlags queue_flags;
    const bool unprotected;  // can't be used for protected memory
    // Cmd buffers allocated from this pool
    layer_data::unordered_map<VkCommandBuffer, CMD_BUFFER_STATE *> commandBuffers;

    COMMAND_POOL_STATE(ValidationStateTracker *dev, VkCommandPool cp, const VkCommandPoolCreateInfo *pCreateInfo,
                       VkQueueFlags flags);
    virtual ~COMMAND_POOL_STATE() { Destroy(); }

    VkCommandPool commandPool() const { return handle_.Cast<VkCommandPool>(); }

    void Allocate(const VkCommandBufferAllocateInfo *create_info, const VkCommandBuffer *command_buffers);
    void Free(uint32_t count, const VkCommandBuffer *command_buffers);
    void Reset();

    void Destroy() override;
};

// Autogenerated as part of the command_validation.h codegen
const char *CommandTypeString(CMD_TYPE type);

enum CB_STATE {
    CB_NEW,                 // Newly created CB w/o any cmds
    CB_RECORDING,           // BeginCB has been called on this CB
    CB_RECORDED,            // EndCB has been called on this CB
    CB_INVALID_COMPLETE,    // had a complete recording, but was since invalidated
    CB_INVALID_INCOMPLETE,  // fouled before recording was completed
};

// CB Status -- used to track status of various bindings on cmd buffer objects
typedef uint64_t CBStatusFlags;
enum CBStatusFlagBits : uint64_t {
    // clang-format off
    CBSTATUS_NONE                            = 0x00000000,   // No status is set
    CBSTATUS_LINE_WIDTH_SET                  = 0x00000001,   // Line width has been set
    CBSTATUS_DEPTH_BIAS_SET                  = 0x00000002,   // Depth bias has been set
    CBSTATUS_BLEND_CONSTANTS_SET             = 0x00000004,   // Blend constants state has been set
    CBSTATUS_DEPTH_BOUNDS_SET                = 0x00000008,   // Depth bounds state object has been set
    CBSTATUS_STENCIL_READ_MASK_SET           = 0x00000010,   // Stencil read mask has been set
    CBSTATUS_STENCIL_WRITE_MASK_SET          = 0x00000020,   // Stencil write mask has been set
    CBSTATUS_STENCIL_REFERENCE_SET           = 0x00000040,   // Stencil reference has been set
    CBSTATUS_VIEWPORT_SET                    = 0x00000080,
    CBSTATUS_SCISSOR_SET                     = 0x00000100,
    CBSTATUS_INDEX_BUFFER_BOUND              = 0x00000200,   // Index buffer has been set
    CBSTATUS_EXCLUSIVE_SCISSOR_SET           = 0x00000400,
    CBSTATUS_SHADING_RATE_PALETTE_SET        = 0x00000800,
    CBSTATUS_LINE_STIPPLE_SET                = 0x00001000,
    CBSTATUS_VIEWPORT_W_SCALING_SET          = 0x00002000,
    CBSTATUS_CULL_MODE_SET                   = 0x00004000,
    CBSTATUS_FRONT_FACE_SET                  = 0x00008000,
    CBSTATUS_PRIMITIVE_TOPOLOGY_SET          = 0x00010000,
    CBSTATUS_VIEWPORT_WITH_COUNT_SET         = 0x00020000,
    CBSTATUS_SCISSOR_WITH_COUNT_SET          = 0x00040000,
    CBSTATUS_VERTEX_INPUT_BINDING_STRIDE_SET = 0x00080000,
    CBSTATUS_DEPTH_TEST_ENABLE_SET           = 0x00100000,
    CBSTATUS_DEPTH_WRITE_ENABLE_SET          = 0x00200000,
    CBSTATUS_DEPTH_COMPARE_OP_SET            = 0x00400000,
    CBSTATUS_DEPTH_BOUNDS_TEST_ENABLE_SET    = 0x00800000,
    CBSTATUS_STENCIL_TEST_ENABLE_SET         = 0x01000000,
    CBSTATUS_STENCIL_OP_SET                  = 0x02000000,
    CBSTATUS_DISCARD_RECTANGLE_SET           = 0x04000000,
    CBSTATUS_SAMPLE_LOCATIONS_SET            = 0x08000000,
    CBSTATUS_COARSE_SAMPLE_ORDER_SET         = 0x10000000,
    CBSTATUS_PATCH_CONTROL_POINTS_SET        = 0x20000000,
    CBSTATUS_RASTERIZER_DISCARD_ENABLE_SET   = 0x40000000,
    CBSTATUS_DEPTH_BIAS_ENABLE_SET           = 0x80000000,
    CBSTATUS_LOGIC_OP_SET                    = 0x100000000,
    CBSTATUS_PRIMITIVE_RESTART_ENABLE_SET    = 0x200000000,
    CBSTATUS_VERTEX_INPUT_SET                = 0x400000000,
    CBSTATUS_COLOR_WRITE_ENABLE_SET          = 0x800000000,
    CBSTATUS_ALL_STATE_SET                   = 0xFFFFFFDFF,   // All state set (intentionally exclude index buffer)
    // clang-format on
};

VkDynamicState ConvertToDynamicState(CBStatusFlagBits flag);
CBStatusFlagBits ConvertToCBStatusFlagBits(VkDynamicState state);
std::string DynamicStateString(CBStatusFlags input_value);

struct BufferBinding {
    std::shared_ptr<BUFFER_STATE> buffer_state;
    VkDeviceSize size;
    VkDeviceSize offset;
    VkDeviceSize stride;

    BufferBinding() : buffer_state(), size(0), offset(0), stride(0) {}
    virtual ~BufferBinding() {}

    virtual void reset() { *this = BufferBinding(); }
};

struct IndexBufferBinding : BufferBinding {
    VkIndexType index_type;

    IndexBufferBinding() : BufferBinding(), index_type(static_cast<VkIndexType>(0)) {}
    virtual ~IndexBufferBinding() {}

    virtual void reset() override { *this = IndexBufferBinding(); }
};

struct CBVertexBufferBindingInfo {
    std::vector<BufferBinding> vertex_buffer_bindings;
};

typedef layer_data::unordered_map<const IMAGE_STATE *, std::shared_ptr<ImageSubresourceLayoutMap>> CommandBufferImageLayoutMap;

typedef layer_data::unordered_map<const GlobalImageLayoutRangeMap *, std::shared_ptr<ImageSubresourceLayoutMap>>
    CommandBufferAliasedLayoutMap;

class CMD_BUFFER_STATE : public REFCOUNTED_NODE {
  public:
    VkCommandBufferAllocateInfo createInfo = {};
    VkCommandBufferBeginInfo beginInfo;
    VkCommandBufferInheritanceInfo inheritanceInfo;
    // since command buffers can only be destroyed by their command pool, this does not need to be a shared_ptr
    const COMMAND_POOL_STATE *command_pool;
    ValidationStateTracker *dev_data;
    bool unprotected;  // can't be used for protected memory
    bool hasRenderPassInstance;
    bool suspendsRenderPassInstance;
    bool resumesRenderPassInstance;

    // Track if certain commands have been called at least once in lifetime of the command buffer
    // primary command buffers values are set true if a secondary command buffer has a command
    bool has_draw_cmd;
    bool has_dispatch_cmd;
    bool has_trace_rays_cmd;
    bool has_build_as_cmd;

    CB_STATE state;         // Track cmd buffer update state
    uint64_t commandCount;  // Number of commands recorded. Currently only used with VK_KHR_performance_query
    uint64_t submitCount;   // Number of times CB has been submitted
    bool pipeline_bound = false;                  // True if CmdBindPipeline has been called on this command buffer, false otherwise
    uint64_t commands_since_begin_rendering = 0;  // Number of commands since the last CmdBeginRenderingCommand
    typedef uint64_t ImageLayoutUpdateCount;
    ImageLayoutUpdateCount image_layout_change_count;  // The sequence number for changes to image layout (for cached validation)
    CBStatusFlags status;                              // Track status of various bindings on cmd buffer
    CBStatusFlags static_status;                       // All state bits provided by current graphics pipeline
                                                       // rather than dynamic state
    CBStatusFlags dynamic_status;                      // dynamic state set up in pipeline
    std::string begin_rendering_func_name;
    // Currently storing "lastBound" objects on per-CB basis
    //  long-term may want to create caches of "lastBound" states and could have
    //  each individual CMD_NODE referencing its own "lastBound" state
    // Store last bound state for Gfx & Compute pipeline bind points
    std::array<LAST_BOUND_STATE, BindPoint_Count> lastBound;  // index is LvlBindPoint.

    // Use the casting boilerplate from BASE_NODE to implement the derived shared_from_this
    std::shared_ptr<const CMD_BUFFER_STATE> shared_from_this() const { return SharedFromThisImpl(this); }
    std::shared_ptr<CMD_BUFFER_STATE> shared_from_this() { return SharedFromThisImpl(this); }

    struct CmdDrawDispatchInfo {
        CMD_TYPE cmd_type;
        std::vector<std::pair<const uint32_t, DescriptorRequirement>> binding_infos;
        VkFramebuffer framebuffer;
        std::shared_ptr<std::vector<SUBPASS_INFO>> subpasses;
        std::shared_ptr<std::vector<IMAGE_VIEW_STATE *>> attachments;
    };
    layer_data::unordered_map<VkDescriptorSet, std::vector<CmdDrawDispatchInfo>> validate_descriptorsets_in_queuesubmit;

    // If VK_NV_inherited_viewport_scissor is enabled and VkCommandBufferInheritanceViewportScissorInfoNV::viewportScissor2D is
    // true, then is the nonempty list of viewports passed in pViewportDepths. Otherwise, this is empty.
    std::vector<VkViewport> inheritedViewportDepths;

    // For each draw command D recorded to this command buffer, let
    //  * g_D be the graphics pipeline used
    //  * v_G be the viewportCount of g_D (0 if g_D disables rasterization or enables VK_DYNAMIC_STATE_VIEWPORT_WITH_COUNT_EXT)
    //  * s_G be the scissorCount  of g_D (0 if g_D disables rasterization or enables VK_DYNAMIC_STATE_SCISSOR_WITH_COUNT_EXT)
    // Then this value is max(0, max(v_G for all D in cb), max(s_G for all D in cb))
    uint32_t usedViewportScissorCount;
    uint32_t pipelineStaticViewportCount;  // v_G for currently-bound graphics pipeline.
    uint32_t pipelineStaticScissorCount;   // s_G for currently-bound graphics pipeline.

    uint32_t viewportMask;
    uint32_t viewportWithCountMask;
    uint32_t viewportWithCountCount;
    uint32_t scissorMask;
    uint32_t scissorWithCountMask;
    uint32_t scissorWithCountCount;

    // Dynamic viewports set in this command buffer; if bit j of viewportMask is set then dynamicViewports[j] is valid, but the
    // converse need not be true.
    std::vector<VkViewport> dynamicViewports;

    // Bits set when binding graphics pipeline defining corresponding static state, or executing any secondary command buffer.
    // Bits unset by calling a corresponding vkCmdSet[State] cmd.
    uint32_t trashedViewportMask;
    uint32_t trashedScissorMask;
    bool trashedViewportCount;
    bool trashedScissorCount;

    // True iff any draw command recorded to this command buffer consumes dynamic viewport/scissor with count state.
    bool usedDynamicViewportCount;
    bool usedDynamicScissorCount;

    uint32_t initial_device_mask;
    VkPrimitiveTopology primitiveTopology;

    bool rasterization_disabled = false;

    safe_VkRenderPassBeginInfo activeRenderPassBeginInfo;
    std::shared_ptr<RENDER_PASS_STATE> activeRenderPass;
    std::shared_ptr<std::vector<SUBPASS_INFO>> active_subpasses;
    std::shared_ptr<std::vector<IMAGE_VIEW_STATE *>> active_attachments;
    std::set<std::shared_ptr<IMAGE_VIEW_STATE>> attachments_view_states;

    VkSubpassContents activeSubpassContents;
    uint32_t active_render_pass_device_mask;
    uint32_t activeSubpass;
    std::shared_ptr<FRAMEBUFFER_STATE> activeFramebuffer;
    layer_data::unordered_set<std::shared_ptr<FRAMEBUFFER_STATE>> framebuffers;
    // Unified data structs to track objects bound to this command buffer as well as object
    //  dependencies that have been broken : either destroyed objects, or updated descriptor sets
    layer_data::unordered_set<std::shared_ptr<BASE_NODE>> object_bindings;
    layer_data::unordered_map<VulkanTypedHandle, LogObjectList> broken_bindings;

    QFOTransferBarrierSets<QFOBufferTransferBarrier> qfo_transfer_buffer_barriers;
    QFOTransferBarrierSets<QFOImageTransferBarrier> qfo_transfer_image_barriers;

    layer_data::unordered_set<VkEvent> waitedEvents;
    std::vector<VkEvent> writeEventsBeforeWait;
    std::vector<VkEvent> events;
    layer_data::unordered_set<QueryObject> activeQueries;
    layer_data::unordered_set<QueryObject> startedQueries;
    layer_data::unordered_set<QueryObject> resetQueries;
    layer_data::unordered_set<QueryObject> updatedQueries;
    CommandBufferImageLayoutMap image_layout_map;
    CommandBufferAliasedLayoutMap aliased_image_layout_map;  // storage for potentially aliased images

    CBVertexBufferBindingInfo current_vertex_buffer_binding_info;
    bool vertex_buffer_used;  // Track for perf warning to make sure any bound vtx buffer used
    VkCommandBuffer primaryCommandBuffer;
    // If primary, the secondary command buffers we will call.
    // If secondary, the primary command buffers we will be called by.
    layer_data::unordered_set<CMD_BUFFER_STATE *> linkedCommandBuffers;
    // Validation functions run at primary CB queue submit time
    using QueueCallback = std::function<bool(const ValidationStateTracker &device_data, const class QUEUE_STATE &queue_state,
                                             const CMD_BUFFER_STATE &cb_state)>;
    std::vector<QueueCallback> queue_submit_functions;
    // Used by some layers to defer actions until vkCmdEndRenderPass time.
    // Layers using this are responsible for inserting the callbacks into queue_submit_functions.
    std::vector<QueueCallback> queue_submit_functions_after_render_pass;
    // Validation functions run when secondary CB is executed in primary
    std::vector<std::function<bool(const CMD_BUFFER_STATE &secondary, const CMD_BUFFER_STATE *primary, const FRAMEBUFFER_STATE *)>>
        cmd_execute_commands_functions;
    std::vector<std::function<bool(CMD_BUFFER_STATE &cb, bool do_validate, EventToStageMap *localEventToStageMap)>> eventUpdates;
    std::vector<std::function<bool(const ValidationStateTracker *device_data, bool do_validate, VkQueryPool &firstPerfQueryPool,
                                   uint32_t perfQueryPass, QueryMap *localQueryToStateMap)>>
        queryUpdates;
    layer_data::unordered_set<const cvdescriptorset::DescriptorSet *> validated_descriptor_sets;
    layer_data::unordered_map<const cvdescriptorset::DescriptorSet *, cvdescriptorset::DescriptorSet::CachedValidation>
        descriptorset_cache;
    // Contents valid only after an index buffer is bound (CBSTATUS_INDEX_BUFFER_BOUND set)
    IndexBufferBinding index_buffer_binding;
    bool performance_lock_acquired = false;
    bool performance_lock_released = false;

    // Cache of current insert label...
    LoggingLabel debug_label;

    std::vector<uint8_t> push_constant_data;
    PushConstantRangesId push_constant_data_ranges;

    std::map<VkShaderStageFlagBits, std::vector<uint8_t>>
        push_constant_data_update;  // vector's value is enum PushConstantByteState.
    VkPipelineLayout push_constant_pipeline_layout_set;

    // Used for Best Practices tracking
    uint32_t small_indexed_draw_call_count;

    bool transform_feedback_active{false};
    bool conditional_rendering_active{false};
    bool conditional_rendering_inside_render_pass{false};
    uint32_t conditional_rendering_subpass{0};
    uint32_t dynamicColorWriteEnableAttachmentCount{0};
    mutable ReadWriteLock lock;
    ReadLockGuard ReadLock() const { return ReadLockGuard(lock); }
    WriteLockGuard WriteLock() { return WriteLockGuard(lock); }

    CMD_BUFFER_STATE(ValidationStateTracker *, VkCommandBuffer cb, const VkCommandBufferAllocateInfo *pCreateInfo,
                     const COMMAND_POOL_STATE *cmd_pool);

    virtual ~CMD_BUFFER_STATE() { Destroy(); }

    void Destroy() override;

    VkCommandBuffer commandBuffer() const { return handle_.Cast<VkCommandBuffer>(); }

    IMAGE_VIEW_STATE *GetActiveAttachmentImageViewState(uint32_t index);
    const IMAGE_VIEW_STATE *GetActiveAttachmentImageViewState(uint32_t index) const;

    void AddChild(std::shared_ptr<BASE_NODE> &base_node);
    template <typename StateObject>
    void AddChild(std::shared_ptr<StateObject> &child_node) {
        auto base = std::static_pointer_cast<BASE_NODE>(child_node);
        AddChild(base);
    }

    void RemoveChild(std::shared_ptr<BASE_NODE> &base_node);
    template <typename StateObject>
    void RemoveChild(std::shared_ptr<StateObject> &child_node) {
        auto base = std::static_pointer_cast<BASE_NODE>(child_node);
        RemoveChild(base);
    }

    virtual void Reset();

    void IncrementResources();

    void ResetPushConstantDataIfIncompatible(const PIPELINE_LAYOUT_STATE *pipeline_layout_state);

    const ImageSubresourceLayoutMap *GetImageSubresourceLayoutMap(const IMAGE_STATE &image_state) const;
    ImageSubresourceLayoutMap *GetImageSubresourceLayoutMap(const IMAGE_STATE &image_state);
    const CommandBufferImageLayoutMap& GetImageSubresourceLayoutMap() const;

    const QFOTransferBarrierSets<QFOImageTransferBarrier> &GetQFOBarrierSets(const QFOImageTransferBarrier &type_tag) const {
        return qfo_transfer_image_barriers;
    }

    const QFOTransferBarrierSets<QFOBufferTransferBarrier> &GetQFOBarrierSets(const QFOBufferTransferBarrier &type_tag) const {
        return qfo_transfer_buffer_barriers;
    }

    QFOTransferBarrierSets<QFOImageTransferBarrier> &GetQFOBarrierSets(const QFOImageTransferBarrier &type_tag) {
        return qfo_transfer_image_barriers;
    }

    QFOTransferBarrierSets<QFOBufferTransferBarrier> &GetQFOBarrierSets(const QFOBufferTransferBarrier &type_tag) {
        return qfo_transfer_buffer_barriers;
    }

    PIPELINE_STATE *GetCurrentPipeline(VkPipelineBindPoint pipelineBindPoint) const {
        const auto lv_bind_point = ConvertToLvlBindPoint(pipelineBindPoint);
        return lastBound[lv_bind_point].pipeline_state;
    }

    void GetCurrentPipelineAndDesriptorSets(VkPipelineBindPoint pipelineBindPoint, const PIPELINE_STATE **rtn_pipe,
                                            const std::vector<LAST_BOUND_STATE::PER_SET> **rtn_sets) const {
        const auto lv_bind_point = ConvertToLvlBindPoint(pipelineBindPoint);
        const auto &last_bound_it = lastBound[lv_bind_point];
        if (!last_bound_it.IsUsing()) {
            return;
        }
        *rtn_pipe = last_bound_it.pipeline_state;
        *rtn_sets = &(last_bound_it.per_set);
    }

    VkQueueFlags GetQueueFlags() const {
	    return command_pool->queue_flags;
    }

    template <typename Barrier>
    inline bool IsReleaseOp(const Barrier &barrier) const {
        return (IsTransferOp(barrier)) && (command_pool->queueFamilyIndex == barrier.srcQueueFamilyIndex);
    }
    template <typename Barrier>
    inline bool IsAcquireOp(const Barrier &barrier) const {
        return (IsTransferOp(barrier)) && (command_pool->queueFamilyIndex == barrier.dstQueueFamilyIndex);
    }

    void Begin(const VkCommandBufferBeginInfo *pBeginInfo);
    void End(VkResult result);

    void BeginQuery(const QueryObject &query_obj);
    void EndQuery(const QueryObject &query_obj);
    void EndQueries(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);
    void ResetQueryPool(VkQueryPool queryPool, uint32_t firstQuery, uint32_t queryCount);

    void BeginRenderPass(CMD_TYPE cmd_type, const VkRenderPassBeginInfo *pRenderPassBegin, VkSubpassContents contents);
    void NextSubpass(CMD_TYPE cmd_type, VkSubpassContents contents);
    void EndRenderPass(CMD_TYPE cmd_type);

    void BeginRendering(CMD_TYPE cmd_type, const VkRenderingInfo *pRenderingInfo);

    void ExecuteCommands(uint32_t commandBuffersCount, const VkCommandBuffer *pCommandBuffers);

    void UpdateLastBoundDescriptorSets(VkPipelineBindPoint pipeline_bind_point, const PIPELINE_LAYOUT_STATE *pipeline_layout,
                                       uint32_t first_set, uint32_t set_count, const VkDescriptorSet *pDescriptorSets,
                                       std::shared_ptr<cvdescriptorset::DescriptorSet> &push_descriptor_set,
                                       uint32_t dynamic_offset_count, const uint32_t *p_dynamic_offsets);

    void PushDescriptorSetState(VkPipelineBindPoint pipelineBindPoint, PIPELINE_LAYOUT_STATE *pipeline_layout, uint32_t set,
                                uint32_t descriptorWriteCount, const VkWriteDescriptorSet *pDescriptorWrites);

    void UpdateDrawCmd(CMD_TYPE cmd_type);
    void UpdateDispatchCmd(CMD_TYPE cmd_type);
    void UpdateTraceRayCmd(CMD_TYPE cmd_type);
    void UpdatePipelineState(CMD_TYPE cmd_type, const VkPipelineBindPoint bind_point);

    virtual void RecordCmd(CMD_TYPE cmd_type);
    void RecordStateCmd(CMD_TYPE cmd_type, CBStatusFlags state_bits);
    void RecordColorWriteEnableStateCmd(CMD_TYPE cmd_type, CBStatusFlags state_bits, uint32_t attachment_count);
    void RecordTransferCmd(CMD_TYPE cmd_type, std::shared_ptr<BINDABLE> &&buf1, std::shared_ptr<BINDABLE> &&buf2 = nullptr);
    void RecordSetEvent(CMD_TYPE cmd_type, VkEvent event, VkPipelineStageFlags2KHR stageMask);
    void RecordResetEvent(CMD_TYPE cmd_type, VkEvent event, VkPipelineStageFlags2KHR stageMask);
    virtual void RecordWaitEvents(CMD_TYPE cmd_type, uint32_t eventCount, const VkEvent *pEvents,
                                  VkPipelineStageFlags2KHR src_stage_mask);
    void RecordWriteTimestamp(CMD_TYPE cmd_type, VkPipelineStageFlags2KHR pipelineStage, VkQueryPool queryPool, uint32_t slot);

    void RecordBarriers(uint32_t memoryBarrierCount, const VkMemoryBarrier *pMemoryBarriers, uint32_t bufferMemoryBarrierCount,
                        const VkBufferMemoryBarrier *pBufferMemoryBarriers, uint32_t imageMemoryBarrierCount,
                        const VkImageMemoryBarrier *pImageMemoryBarriers);
    void RecordBarriers(const VkDependencyInfoKHR &dep_info);

    void SetImageViewLayout(const IMAGE_VIEW_STATE &view_state, VkImageLayout layout, VkImageLayout layoutStencil);
    void SetImageViewInitialLayout(const IMAGE_VIEW_STATE &view_state, VkImageLayout layout);

    void SetImageLayout(const IMAGE_STATE &image_state, const VkImageSubresourceRange &image_subresource_range,
                        VkImageLayout layout, VkImageLayout expected_layout = kInvalidLayout);
    void SetImageLayout(const IMAGE_STATE &image_state, const VkImageSubresourceLayers &image_subresource_layers,
                        VkImageLayout layout);
    void SetImageInitialLayout(VkImage image, const VkImageSubresourceRange &range, VkImageLayout layout);
    void SetImageInitialLayout(const IMAGE_STATE &image_state, const VkImageSubresourceRange &range, VkImageLayout layout);
    void SetImageInitialLayout(const IMAGE_STATE &image_state, const VkImageSubresourceLayers &layers, VkImageLayout layout);

    void Submit(uint32_t perf_submit_pass);
    void Retire(uint32_t perf_submit_pass, const std::function<bool(const QueryObject &)> &is_query_updated_after);

    uint32_t GetDynamicColorAttachmentCount() {
        if (activeRenderPass) {
            if (activeRenderPass->use_dynamic_rendering_inherited) {
                return activeRenderPass->inheritance_rendering_info.colorAttachmentCount;
            }
            if (activeRenderPass->use_dynamic_rendering) {
                return activeRenderPass->dynamic_rendering_begin_rendering_info.colorAttachmentCount;
            }
        }
        return 0;
    }
    uint32_t GetDynamicColorAttachmentImageIndex(uint32_t index) { return index; }
    uint32_t GetDynamicColorResolveAttachmentImageIndex(uint32_t index) { return index + GetDynamicColorAttachmentCount(); }
    uint32_t GetDynamicDepthAttachmentImageIndex() { return 2 * GetDynamicColorAttachmentCount(); }
    uint32_t GetDynamicDepthResolveAttachmentImageIndex() { return 2 * GetDynamicColorAttachmentCount() + 1; }
    uint32_t GetDynamicStencilAttachmentImageIndex() { return 2 * GetDynamicColorAttachmentCount() + 2; }
    uint32_t GetDynamicStencilResolveAttachmentImageIndex() { return 2 * GetDynamicColorAttachmentCount() + 3; }

    bool RasterizationDisabled() const;
    inline void BindPipeline(LvlBindPoint bind_point, PIPELINE_STATE *pipe_state) {
        lastBound[bind_point].pipeline_state = pipe_state;
        pipeline_bound = true;
    }

  protected:
    void NotifyInvalidate(const BASE_NODE::NodeList &invalid_nodes, bool unlink) override;
    void UpdateAttachmentsView(const VkRenderPassBeginInfo *pRenderPassBegin);
    void UnbindResources();
};

// specializations for barriers that cannot do queue family ownership transfers
template <>
inline bool CMD_BUFFER_STATE::IsReleaseOp(const VkMemoryBarrier &barrier) const {
    return false;
}
template <>
inline bool CMD_BUFFER_STATE::IsReleaseOp(const VkMemoryBarrier2KHR &barrier) const {
    return false;
}
template <>
inline bool CMD_BUFFER_STATE::IsReleaseOp(const VkSubpassDependency2 &barrier) const {
    return false;
}
template <>
inline bool CMD_BUFFER_STATE::IsAcquireOp(const VkMemoryBarrier &barrier) const {
    return false;
}
template <>
inline bool CMD_BUFFER_STATE::IsAcquireOp(const VkMemoryBarrier2KHR &barrier) const {
    return false;
}
template <>
inline bool CMD_BUFFER_STATE::IsAcquireOp(const VkSubpassDependency2 &barrier) const {
    return false;
}
