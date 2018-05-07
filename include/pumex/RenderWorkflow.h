//
// Copyright(c) 2017-2018 Paweł Księżopolski ( pumexx )
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#pragma once
#include <unordered_map>
#include <vector>
#include <set>
#include <map>
#include <memory>
#include <mutex>
#include <vulkan/vulkan.h>
#include <gli/texture.hpp>
#include <pumex/Export.h>
#include <pumex/Command.h>

namespace pumex
{

class  Device;
struct QueueTraits;
struct SubpassDefinition;
class  DeviceMemoryAllocator;
class  RenderPass;
class  FrameBufferImages;
struct FrameBufferImageDefinition;
class  Node;
class  ValidateGPUVisitor;
class  BuildCommandBufferVisitor;
class  Resource;
class RenderCommand;

struct PUMEX_EXPORT LoadOp
{
  enum Type { Load, Clear, DontCare };
  LoadOp()
    : loadType{ DontCare }, clearColor{}
  {
  }
  LoadOp(Type lType, const glm::vec4& color)
    : loadType{ lType }, clearColor{ color }
  {
  }

  Type loadType;
  glm::vec4 clearColor;
};

inline LoadOp loadOpLoad();
inline LoadOp loadOpClear(const glm::vec2& color = glm::vec2(1.0f, 0.0f));
inline LoadOp loadOpClear(const glm::vec4& color = glm::vec4(0.0f));
inline LoadOp loadOpDontCare();

struct PUMEX_EXPORT StoreOp
{
  enum Type { Store, DontCare };
  StoreOp()
    : storeType{ DontCare }
  {
  }
  StoreOp(Type sType)
    : storeType{ sType }
  {
  }
  Type storeType;
};

inline StoreOp storeOpStore();
inline StoreOp storeOpDontCare();

enum AttachmentType { atUndefined, atSurface, atColor, atDepth, atDepthStencil, atStencil };

inline VkImageAspectFlags getAspectMask(AttachmentType at);
inline VkImageUsageFlags  getAttachmentUsage(VkImageLayout imageLayout);

struct PUMEX_EXPORT AttachmentSize
{
  enum Type { Undefined, Absolute, SurfaceDependent };

  AttachmentSize()
    : attachmentSize{ Undefined }, imageSize{ 0.0f, 0.0f, 0.0f }
  {
  }
  AttachmentSize(Type aSize, const glm::vec3& imSize)
    : attachmentSize{ aSize }, imageSize{ imSize }
  {
  }
  AttachmentSize(Type aSize, const glm::vec2& imSize)
    : attachmentSize{ aSize }, imageSize{ imSize.x, imSize.y, 1.0f }
  {
  }

  Type      attachmentSize;
  glm::vec3 imageSize;
};

inline bool operator==(const AttachmentSize& lhs, const AttachmentSize& rhs)
{
  return lhs.attachmentSize == rhs.attachmentSize && lhs.imageSize == rhs.imageSize;
}

inline bool operator!=(const AttachmentSize& lhs, const AttachmentSize& rhs)
{
  return lhs.attachmentSize != rhs.attachmentSize || lhs.imageSize != rhs.imageSize;
}

class PUMEX_EXPORT RenderWorkflowResourceType
{
public:
  enum MetaType { Undefined, Attachment, Image, Buffer };
  enum BufferType { UniformBuffer=1, StorageBuffer=2 };
  enum ImageType { CombinedImageSampler=1, SampledImage=2, StorageImage=4  };
  typedef VkFlags ImageTypeFlags;

  RenderWorkflowResourceType(const std::string& typeName, bool persistent, VkFormat format, VkSampleCountFlagBits samples, AttachmentType attachmentType, const AttachmentSize& attachmentSize);
  RenderWorkflowResourceType(const std::string& typeName, bool persistent, const BufferType& bufferType);
  RenderWorkflowResourceType(const std::string& typeName, bool persistent, const ImageTypeFlags& imageType);

  bool isEqual(const RenderWorkflowResourceType& rhs) const;

  MetaType              metaType;
  std::string           typeName;
  bool                  persistent;

  struct AttachmentData
  {
    AttachmentData(VkFormat f, VkSampleCountFlagBits s, AttachmentType at, const AttachmentSize& as, const gli::swizzles& sw = gli::swizzles(gli::swizzle::SWIZZLE_RED, gli::swizzle::SWIZZLE_GREEN, gli::swizzle::SWIZZLE_BLUE, gli::swizzle::SWIZZLE_ALPHA))
      : format{ f }, samples{ s }, attachmentType{ at }, attachmentSize{ as }, swizzles{ sw }
    {
    }

    VkFormat              format;
    VkSampleCountFlagBits samples;
    AttachmentType        attachmentType;
    AttachmentSize        attachmentSize;
    gli::swizzles         swizzles;

    bool isEqual(const AttachmentData& rhs) const;
  };

  struct BufferData
  {
    BufferData(const BufferType& bt)
      : bufferType{ bt }
    {
    }
    bool isEqual(const BufferData& rhs) const;
    BufferType bufferType;
  };

  struct ImageData
  {
    ImageData(const ImageTypeFlags& itf)
      : imageType{ itf }
    {
    }
    bool isEqual(const ImageData& rhs) const;
    ImageTypeFlags imageType;
  };

  union
  {
    AttachmentData attachment;
    BufferData     buffer;
    ImageData      image;
  };
};

class PUMEX_EXPORT WorkflowResource
{
public:
  WorkflowResource(const std::string& name, std::shared_ptr<RenderWorkflowResourceType> resourceType);

  std::string                                 name;
  std::shared_ptr<RenderWorkflowResourceType> resourceType;
};

class RenderWorkflow;

class PUMEX_EXPORT RenderOperation
{
public:
  enum Type { Graphics, Compute };

  RenderOperation(const std::string& name, Type operationType, AttachmentSize attachmentSize = AttachmentSize(AttachmentSize::SurfaceDependent, glm::vec2(1.0f,1.0f)), VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE);
  virtual ~RenderOperation();

  void setRenderWorkflow ( std::shared_ptr<RenderWorkflow> renderWorkflow );
  void setSceneNode(std::shared_ptr<Node> node);


  std::string                   name;
  Type                          operationType;
  AttachmentSize                attachmentSize;

  VkSubpassContents             subpassContents;

  std::weak_ptr<RenderWorkflow> renderWorkflow;
  std::shared_ptr<Node>         sceneNode;

  bool                          enabled; // not implemented
};

enum ResourceTransitionType 
{
  rttAttachmentInput         = 1, 
  rttAttachmentOutput        = 2,
  rttAttachmentResolveOutput = 4,
  rttAttachmentDepthOutput   = 8,
  rttBufferInput             = 16,
  rttBufferOutput            = 32
};

typedef VkFlags ResourceTransitionTypeFlags;

const ResourceTransitionTypeFlags rttAllAttachments       = rttAttachmentInput | rttAttachmentOutput | rttAttachmentResolveOutput | rttAttachmentDepthOutput;
const ResourceTransitionTypeFlags rttAllAttachmentInputs =  rttAttachmentInput;
const ResourceTransitionTypeFlags rttAllAttachmentOutputs = rttAttachmentOutput | rttAttachmentResolveOutput | rttAttachmentDepthOutput;
const ResourceTransitionTypeFlags rttAllInputs            = rttAttachmentInput | rttBufferInput;
const ResourceTransitionTypeFlags rttAllOutputs           = rttAttachmentOutput | rttAttachmentResolveOutput | rttAttachmentDepthOutput | rttBufferOutput;
const ResourceTransitionTypeFlags rttAllInputsOutputs     = rttAllInputs | rttAllOutputs;

class PUMEX_EXPORT ResourceTransition
{
public:
  ResourceTransition(std::shared_ptr<RenderOperation> operation, std::shared_ptr<WorkflowResource> resource, ResourceTransitionType transitionType, VkImageLayout layout, const LoadOp& load);
  ResourceTransition(std::shared_ptr<RenderOperation> operation, std::shared_ptr<WorkflowResource> resource, ResourceTransitionType transitionType, VkPipelineStageFlags pipelineStage, VkAccessFlags accessFlags);
  ~ResourceTransition();
  std::shared_ptr<RenderOperation>  operation;
  std::shared_ptr<WorkflowResource> resource;
  ResourceTransitionType            transitionType;

  struct AttachmentData
  {
    AttachmentData(VkImageLayout l, const LoadOp& ld)
      : resolveResource{}, layout{ l }, load{ ld }
    {
    }
    std::shared_ptr<WorkflowResource> resolveResource;
    VkImageLayout                     layout;
    LoadOp                            load;
  };
  struct BufferData
  {
    BufferData(VkPipelineStageFlags ps, VkAccessFlags af)
      : pipelineStage{ ps }, accessFlags{ af }
    {
    }
    VkPipelineStageFlags           pipelineStage;
    VkAccessFlags                  accessFlags;
  };

  union
  {
    AttachmentData attachment;
    BufferData     buffer;
  };
};

inline void getPipelineStageMasks(std::shared_ptr<ResourceTransition> generatingTransition, std::shared_ptr<ResourceTransition> consumingTransition, VkPipelineStageFlags& srcStageMask, VkPipelineStageFlags& dstStageMask);
inline void getAccessMasks(std::shared_ptr<ResourceTransition> generatingTransition, std::shared_ptr<ResourceTransition> consumingTransition, VkAccessFlags& srcAccessMask, VkAccessFlags& dstAccessMask);

class PUMEX_EXPORT RenderWorkflowSequences
{
public:
  RenderWorkflowSequences(const std::vector<QueueTraits>& queueTraits, const std::vector<std::vector<std::shared_ptr<RenderCommand>>>& commands, std::shared_ptr<FrameBuffer> frameBuffer, const std::vector<VkImageLayout>& initialImageLayouts, std::shared_ptr<RenderPass> outputRenderPass, uint32_t presentationQueueIndex);

  std::vector<QueueTraits>                                 queueTraits;
  std::vector<std::vector<std::shared_ptr<RenderCommand>>> commands;
  std::shared_ptr<FrameBuffer>                             frameBuffer;
  std::vector<VkImageLayout>                               initialImageLayouts;
  std::shared_ptr<RenderPass>                              outputRenderPass;
  uint32_t                                                 presentationQueueIndex = 0;

  QueueTraits getPresentationQueue() const;
};

class PUMEX_EXPORT RenderWorkflowCompiler
{
public:
  virtual std::shared_ptr<RenderWorkflowSequences> compile(RenderWorkflow& workflow) = 0;
};

class PUMEX_EXPORT RenderWorkflow : public std::enable_shared_from_this<RenderWorkflow>
{
public:
  RenderWorkflow()                                 = delete;
  explicit RenderWorkflow(const std::string& name, std::shared_ptr<DeviceMemoryAllocator> frameBufferAllocator, const std::vector<QueueTraits>& queueTraits);
  ~RenderWorkflow();

  void                                        addResourceType(std::shared_ptr<RenderWorkflowResourceType> tp);
  std::shared_ptr<RenderWorkflowResourceType> getResourceType(const std::string& typeName) const;

  inline const std::vector<QueueTraits>&      getQueueTraits() const;

  void                                        addRenderOperation(std::shared_ptr<RenderOperation> op);
  std::vector<std::string>                    getRenderOperationNames() const;
  std::shared_ptr<RenderOperation>            getRenderOperation(const std::string& opName) const;

  void                                        setSceneNode(const std::string& opName, std::shared_ptr<Node> node);
  std::shared_ptr<Node>                       getSceneNode(const std::string& opName);

  void addAttachmentInput(const std::string& opName, const std::string& resourceType, const std::string& resourceName, VkImageLayout layout);
  void addAttachmentOutput(const std::string& opName, const std::string& resourceType, const std::string& resourceName, VkImageLayout layout, const LoadOp& loadOp);
  void addAttachmentResolveOutput(const std::string& opName, const std::string& resourceType, const std::string& resourceName, const std::string& resourceSource, VkImageLayout layout, const LoadOp& loadOp);
  void addAttachmentDepthOutput(const std::string& opName, const std::string& resourceType, const std::string& resourceName, VkImageLayout layout, const LoadOp& loadOp);

  void addBufferInput(const std::string& opName, const std::string& resourceType, const std::string& resourceName, VkPipelineStageFlagBits pipelineStage, VkAccessFlagBits accessFlags);
  void addBufferOutput(const std::string& opName, const std::string& resourceType, const std::string& resourceName, VkPipelineStageFlagBits pipelineStage, VkAccessFlagBits accessFlags);

  std::vector<std::string>                    getResourceNames() const;
  std::shared_ptr<WorkflowResource>           getResource(const std::string& resourceName) const;

  void associateResource(const std::string& resourceName, std::shared_ptr<Resource> resource);
  std::shared_ptr<Resource> getAssociatedResource(const std::string& resourceName) const;

  std::vector<std::shared_ptr<ResourceTransition>> getOperationIO(const std::string& opName, ResourceTransitionTypeFlags transitionTypes) const;
  std::vector<std::shared_ptr<ResourceTransition>> getResourceIO(const std::string& resourceName, ResourceTransitionTypeFlags transitionTypes) const;

  std::set<std::shared_ptr<RenderOperation>> getInitialOperations() const;
  std::set<std::shared_ptr<RenderOperation>> getFinalOperations() const;
  std::set<std::shared_ptr<RenderOperation>> getPreviousOperations(const std::string& opName) const;
  std::set<std::shared_ptr<RenderOperation>> getNextOperations(const std::string& opName) const;

  bool compile(std::shared_ptr<RenderWorkflowCompiler> compiler);

  // data created during workflow compilation - may be used in many surfaces at once
  std::shared_ptr<DeviceMemoryAllocator>                                       frameBufferAllocator;
  std::shared_ptr<RenderWorkflowSequences>                                     workflowSequences;

protected:
  // data provided by user during workflow setup
  std::string                                                                  name;
  std::unordered_map<std::string, std::shared_ptr<RenderWorkflowResourceType>> resourceTypes;
  std::unordered_map<std::string, std::shared_ptr<RenderOperation>>            renderOperations;
  std::unordered_map<std::string, std::shared_ptr<WorkflowResource>>           resources;
  std::unordered_map<std::string, std::shared_ptr<Resource>>                   associatedResources;
  std::vector<std::shared_ptr<ResourceTransition>>                             transitions;
  std::vector<QueueTraits>                                                     queueTraits;
  bool                                                                         valid                = false;
  mutable std::mutex                                                           compileMutex;
};

// This is the first implementation of workflow compiler
// It only uses one queue to do the job

struct PUMEX_EXPORT StandardRenderWorkflowCostCalculator
{
  void  tagOperationByAttachmentType(const RenderWorkflow& workflow);
  float calculateWorkflowCost(const RenderWorkflow& workflow, const std::vector<std::shared_ptr<RenderOperation>>& operationSchedule) const;

  std::unordered_map<std::string, int> attachmentTag;
};

class PUMEX_EXPORT SingleQueueWorkflowCompiler : public RenderWorkflowCompiler
{
public:
  std::shared_ptr<RenderWorkflowSequences>      compile(RenderWorkflow& workflow) override;
private:
  void                                          verifyOperations(const RenderWorkflow& workflow);
  std::vector<std::shared_ptr<RenderOperation>> calculatePartialOrdering(const RenderWorkflow& workflow);
  void                                          collectResources(const RenderWorkflow& workflow, const std::vector<std::vector<std::shared_ptr<RenderOperation>>>& operationSequences, std::map<std::string, std::string>& resourceAlias, std::vector<FrameBufferImageDefinition>& frameBufferDefinitions, std::unordered_map<std::string, uint32_t>& attachmentIndex);
  std::vector<std::shared_ptr<RenderCommand>>   createCommandSequence(const std::vector<std::shared_ptr<RenderOperation>>& operationSequence, std::vector<FrameBufferImageDefinition>& frameBufferDefinitions, const std::unordered_map<std::string, uint32_t>& attachmentIndex);
  std::vector<VkImageLayout>                    calculateInitialLayouts(const RenderWorkflow& workflow, std::vector<FrameBufferImageDefinition>& frameBufferDefinitions, const std::unordered_map<std::string, uint32_t>& attachmentIndex);
  void                                          finalizeRenderPasses(const RenderWorkflow& workflow, const std::vector<std::vector<std::shared_ptr<RenderCommand>>>& commands, const std::vector<std::shared_ptr<RenderOperation>>& partialOrdering, std::vector<FrameBufferImageDefinition>& frameBufferDefinitions, const std::unordered_map<std::string, uint32_t>& attachmentIndex);
  void                                          createPipelineBarriers(const RenderWorkflow& workflow, std::vector<std::vector<std::shared_ptr<RenderCommand>>>& commandSequences);
  void                                          createSubpassDependency(std::shared_ptr<ResourceTransition> generatingTransition, std::shared_ptr<RenderCommand> generatingCommand, std::shared_ptr<ResourceTransition> consumingTransition, std::shared_ptr<RenderCommand> consumingCommand, uint32_t generatingQueueIndex, uint32_t consumingQueueIndex);
  void                                          createPipelineBarrier(std::shared_ptr<ResourceTransition> generatingTransition, std::shared_ptr<RenderCommand> generatingCommand, std::shared_ptr<ResourceTransition> consumingTransition, std::shared_ptr<RenderCommand> consumingCommand, uint32_t generatingQueueIndex, uint32_t consumingQueueIndex);
  std::shared_ptr<RenderPass>                   findOutputRenderPass(const RenderWorkflow& workflow, const std::vector<std::vector<std::shared_ptr<RenderCommand>>>& commands, uint32_t& presentationQueueIndex);


  StandardRenderWorkflowCostCalculator          costCalculator;
};

LoadOp             loadOpLoad()                        { return LoadOp(LoadOp::Load, glm::vec4(0.0f)); }
LoadOp             loadOpClear(const glm::vec2& color) { return LoadOp(LoadOp::Clear, glm::vec4(color.x, color.y, 0.0f, 0.0f)); }
LoadOp             loadOpClear(const glm::vec4& color) { return LoadOp(LoadOp::Clear, color); }
LoadOp             loadOpDontCare()                    { return LoadOp(LoadOp::DontCare, glm::vec4(0.0f));}
StoreOp            storeOpStore()                      { return StoreOp(StoreOp::Store); }
StoreOp            storeOpDontCare()                   { return StoreOp(StoreOp::DontCare); }

const std::vector<QueueTraits>& RenderWorkflow::getQueueTraits() const { return queueTraits; }


VkImageAspectFlags getAspectMask(AttachmentType at)
{
  switch (at)
  {
  case atColor:
  case atSurface:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  case atDepth:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  case atDepthStencil:
    return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
  case atStencil:
    return VK_IMAGE_ASPECT_STENCIL_BIT;
  default:
    return (VkImageAspectFlags)0;
  }
  return (VkImageAspectFlags)0;
}

VkImageUsageFlags  getAttachmentUsage(VkImageLayout il)
{
  switch (il)
  {
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:         return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL: return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:         return VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:             return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:             return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
  case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:               return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  case VK_IMAGE_LAYOUT_UNDEFINED:
  case VK_IMAGE_LAYOUT_GENERAL:
  case VK_IMAGE_LAYOUT_PREINITIALIZED:
  default:                                               return (VkImageUsageFlags)0;
  }
  return (VkImageUsageFlags)0;
}

void getPipelineStageMasks(std::shared_ptr<ResourceTransition> generatingTransition, std::shared_ptr<ResourceTransition> consumingTransition, VkPipelineStageFlags& srcStageMask, VkPipelineStageFlags& dstStageMask)
{
  switch (generatingTransition->transitionType)
  {
  case rttAttachmentOutput:
  case rttAttachmentResolveOutput:
  case rttAttachmentDepthOutput:
    switch (generatingTransition->attachment.layout)
    {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      srcStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; break;
      break;
    }
    break;
  case rttBufferOutput:
    srcStageMask = generatingTransition->buffer.pipelineStage;
    break;
  }

  switch (consumingTransition->transitionType)
  {
  case rttAttachmentInput:
    switch (consumingTransition->attachment.layout)
    {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; break;
    }
    break;
  case rttBufferInput:
    dstStageMask = consumingTransition->buffer.pipelineStage;
    break;
  }

}

void getAccessMasks(std::shared_ptr<ResourceTransition> generatingTransition, std::shared_ptr<ResourceTransition> consumingTransition, VkAccessFlags& srcAccessMask, VkAccessFlags& dstAccessMask)
{
  switch (generatingTransition->transitionType)
  {
  case rttAttachmentOutput:
  case rttAttachmentResolveOutput:
  case rttAttachmentDepthOutput:
    switch (generatingTransition->attachment.layout)
    {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      srcAccessMask = VK_ACCESS_SHADER_READ_BIT; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT; break;
    }
    break;
  case rttBufferOutput:
    srcAccessMask = generatingTransition->buffer.accessFlags;
    break;
  }

  switch (consumingTransition->transitionType)
  {
  case rttAttachmentInput:
    switch (consumingTransition->attachment.layout)
    {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT; break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
      dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT; break;
    }
    break;
  case rttBufferInput:
    dstAccessMask = consumingTransition->buffer.accessFlags;
    break;
  }
}
	
}