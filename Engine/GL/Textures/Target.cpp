#include "Target.h"

#include "../Device.h"
#include "../Context.h"

#include <glm/gtc/type_ptr.hpp>


using namespace Bplus;
using namespace Bplus::GL;
using namespace Bplus::GL::Textures;


TargetOutput::TargetOutput(Texture1D* tex, uint_mipLevel_t mipLevel)
    : data(tex), MipLevel(mipLevel) { }
TargetOutput::TargetOutput(Texture2D* tex, uint_mipLevel_t mipLevel)
    : data(tex), MipLevel(mipLevel) { }
TargetOutput::TargetOutput(Texture3D* tex, uint_mipLevel_t mipLevel)
    : data(tex), MipLevel(mipLevel) { }
TargetOutput::TargetOutput(TextureCube* tex, uint_mipLevel_t mipLevel)
    : data(tex), MipLevel(mipLevel) { }
TargetOutput::TargetOutput(std::tuple<Texture3D*, uint32_t> tex, uint_mipLevel_t mipLevel)
    : data(tex), MipLevel(mipLevel) { }
TargetOutput::TargetOutput(std::tuple<TextureCube*, CubeFaces> data, uint_mipLevel_t mipLevel)
    : data(data), MipLevel(mipLevel) { }

bool TargetOutput::IsLayered() const
{
    if (IsTex3D() || IsTexCube())
        return true;
    else if (IsTex1D() || IsTex2D() || IsTex3DSlice() || IsTexCubeFace())
        return false;
    else
    {
        BP_ASSERT(false, "Unknown TargetOutput type");
        return false;
    }
}
bool TargetOutput::IsFlat() const
{
    if (IsTex1D() || IsTex2D())
        return true;
    else if (IsTex3D() || IsTexCube() || IsTex3DSlice() || IsTexCubeFace())
        return false;
    else
    {
        BP_ASSERT(false, "Unknown TargetOutput type");
        return false;
    }
}
Texture* TargetOutput::GetTex() const
{
    if (IsTex1D())
        return GetTex1D();
    else if (IsTex2D())
        return GetTex2D();
    else if (IsTex3D())
        return GetTex3D();
    else if (IsTex3DSlice())
        return std::get<Texture3D*>(GetTex3DSlice());
    else if (IsTexCubeFace())
        return std::get<TextureCube*>(GetTexCubeFace());
    else
    {
        BP_ASSERT(false, "Unknown type for TargetOutput");
        return nullptr;
    }
}
glm::uvec2 TargetOutput::GetSize() const
{
    if (IsTex1D())
        return glm::uvec2(GetTex1D()->GetSize().x, 1);
    else if (IsTex2D())
        return GetTex2D()->GetSize();
    else if (IsTex3D())
        return GetTex3D()->GetSize();
    else if (IsTexCube())
        return GetTexCube()->GetSize2D();
    else if (IsTexCubeFace())
        return std::get<0>(GetTexCubeFace())->GetSize2D();
    else if (IsTex3DSlice())
        return std::get<0>(GetTex3DSlice())->GetSize();
    else
    {
        BP_ASSERT(false, "Unknown type for TargetOutput");
        return glm::uvec2{ 0 };
    }
}
uint32_t TargetOutput::GetLayer() const
{
    BP_ASSERT(!IsLayered(),
             "Trying to get the specific layer from a multi-layered output");

    if (IsTex1D() || IsTex2D())
        return 0;
    else if (IsTex3DSlice())
        return std::get<1>(GetTex3DSlice());
    else if (IsTexCubeFace())
        return (uint32_t)std::get<1>(GetTexCubeFace())._to_index();
    else if (IsTex3D() || IsTexCube())
        { BP_ASSERT(false, "Shouldn't ever get here"); return -1; }
    else
    {
        BP_ASSERT(false, "Unknown TargetOutput type");
        return -1;
    }
}
uint32_t TargetOutput::GetLayerCount() const
{
    if (!IsLayered())
        return 1;
    else if (IsTex3D())
        return (uint32_t)GetTex3D()->GetSize().z;
    else if (IsTexCube())
        return 6;
    else
    {
        BP_ASSERT(false, "Unknown TargetOutput type");
        return 1;
    }
}


namespace
{
    thread_local struct ThreadTargetData {
        bool initializedYet = false;
        std::unordered_map<OglPtr::Target, const Target*> targetsByOglPtr;
    } threadData;

    void CheckInit()
    {
        if (!threadData.initializedYet)
        {
            threadData.initializedYet = true;

            std::function<void()> refreshContext = []()
            {
                //Nothing needs to be done right now,
                //    but this is kept here just in case it becomes useful.
                //TODO: Assert all Target instances still exist in OpenGL.
            };
            refreshContext();
            Context::RegisterCallback_RefreshState(refreshContext);

            Context::RegisterCallback_Destroyed([]()
            {
                //Make sure all targets have been cleaned up.
                //TODO: Use OpenGL's debug utilities to give the targets names and provide more info here.
                BP_ASSERT(threadData.targetsByOglPtr.size() == 0,
                         "Target memory leaks!");

                threadData.targetsByOglPtr.clear();
            });
        }
    }
}

const Target* Target::Find(OglPtr::Target ptr)
{
    CheckInit();
    
    auto found = threadData.targetsByOglPtr.find(ptr);
    return (found == threadData.targetsByOglPtr.end()) ?
               nullptr :
               found->second;
}


Target::Target(TargetStates& outStatus,
               const glm::uvec2& _size, uint32_t nLayers)
    : size(_size), glPtr(GlCreate(glCreateFramebuffers)),
      //According to the OpenGL 4.5 standard, framebuffers start with
      //    just the first attachment enabled.
      activeColorAttachments(1, 0),
      internalActiveColorAttachments({ GL_COLOR_ATTACHMENT0 })
{
    CheckInit();
    threadData.targetsByOglPtr[glPtr] = this;

    BP_ASSERT(size.x > 0, "Target's width can't be 0");
    BP_ASSERT(size.y > 0, "Target's height can't be 0");

    glNamedFramebufferParameteri(glPtr.Get(), GL_FRAMEBUFFER_DEFAULT_WIDTH, (GLint)size.x);
    glNamedFramebufferParameteri(glPtr.Get(), GL_FRAMEBUFFER_DEFAULT_HEIGHT, (GLint)size.y);
    if (nLayers > 1)
        glNamedFramebufferParameteri(glPtr.Get(), GL_FRAMEBUFFER_DEFAULT_LAYERS, (GLint)nLayers);

    outStatus = GetStatus();
}
Target::Target(TargetStates& outStatus,
               const glm::uvec2& _size,
               const Format& colorFormat, DepthStencilFormats depthFormat,
               bool depthIsRenderBuffer, uint_mipLevel_t nMips)
    : Target(outStatus, _size)
{
    auto colorTex = new Texture2D(size, colorFormat, nMips);
    managedTextures.insert(colorTex);
    tex_colors.push_back(colorTex);
    AttachTexture(GL_COLOR_ATTACHMENT0, colorTex);

    //Generate a depth/stencil texture, or depth/stencil RenderBuffer.
    if (depthIsRenderBuffer)
    {
        AttachBuffer(depthFormat);
    }
    else
    {
        auto tex = new Texture2D(size, depthFormat, nMips);
        managedTextures.insert(tex);

        auto format = tex->GetFormat().AsDepthStencil();
        AttachTexture(GetAttachmentType(format), tex);
        tex_depth = tex;
        if (IsStencilOnly(format) || IsDepthAndStencil(format))
            tex_stencil = tex;
    }

    outStatus = GetStatus();
}

Target::Target(TargetStates& outStatus,
               const TargetOutput& color, const TargetOutput& depthStencil)
    : Target(outStatus,
             glm::min(color.GetSize(), depthStencil.GetSize()),
             std::min(color.GetLayerCount(), depthStencil.GetLayerCount()))
{
    BP_ASSERT(depthStencil.GetSize() == color.GetSize(),
             "Color and depth aren't same size");
    BP_ASSERT(depthStencil.GetTex()->GetFormat().IsDepthStencil(),
             "Depth/stencil texture isn't a depth or stencil format");

    AttachTexture(GL_COLOR_ATTACHMENT0, color);
    tex_colors.push_back(color);

    auto depthFormat = depthStencil.GetTex()->GetFormat().AsDepthStencil();
    AttachTexture(GetAttachmentType(depthFormat),
                  depthStencil);
    tex_depth = depthStencil;
    if (IsStencilOnly(depthFormat) || IsDepthAndStencil(depthFormat))
        tex_stencil = depthStencil;

    outStatus = GetStatus();
}
Target::Target(TargetStates& outStatus,
               const TargetOutput& color, DepthStencilFormats depthFormat)
    : Target(outStatus, color.GetSize(), color.GetLayerCount())
{
    AttachTexture(GL_COLOR_ATTACHMENT0, color);
    tex_colors.push_back(color);

    AttachBuffer(depthFormat);

    outStatus = GetStatus();
}

#pragma region Helper functions for the last Target() constructor
namespace
{
    //Used to find the min size and layer count for
    //    one of the Target constructors.
    template<typename T,
             T(*ValueGetter)(const TargetOutput&),
             T(*MinFunc)(const T&, const T&)>
    T ComputeMin(const TargetOutput* outputsList, size_t nOutputsInList,
                 std::optional<TargetOutput> optionalOutput,
                 T maxVal, T defaultVal)
    {
        if (nOutputsInList == 0 && !optionalOutput.has_value())
            return defaultVal;

        T output = maxVal;
        if (optionalOutput.has_value())
            output = MinFunc(output, ValueGetter(optionalOutput.value()));

        for (uint32_t i = 0; i < nOutputsInList; ++i)
            output = MinFunc(output, ValueGetter(outputsList[i]));

        return output;
    }

    glm::uvec2 GetTargOutSize(const TargetOutput& to) { return to.GetSize(); }
    uint32_t GetTargOutLayerCount(const TargetOutput& to) { return to.GetLayerCount(); }

    uint32_t MinU32(const uint32_t& a, const uint32_t& b) { return std::min(a, b); }
}
#pragma endregion
Target::Target(TargetStates& outStatus,
               const TargetOutput* colorOutputs, uint32_t nColorOutputs,
               std::optional<TargetOutput> depthOutput)
    : Target(outStatus,
             ComputeMin<glm::uvec2, GetTargOutSize, glm::min>(
                colorOutputs, nColorOutputs, depthOutput,
                glm::uvec2{ std::numeric_limits<glm::u32>().max() },
                glm::uvec2{ 1 }),
             ComputeMin<uint32_t, GetTargOutLayerCount, MinU32>(
                colorOutputs, nColorOutputs, depthOutput,
                std::numeric_limits<uint32_t>().max(),
                (uint32_t)1))
{
    //Set up the color attachments.
    std::vector<std::optional<uint_fast32_t>> colorAttachmentIndices;
    for (uint_fast32_t i = 0; i < nColorOutputs; ++i)
    {
        AttachTexture((GLenum)(GL_COLOR_ATTACHMENT0 + i), colorOutputs[i]);
        tex_colors.push_back(colorOutputs[i]);
        colorAttachmentIndices.push_back(i);
    }
    //By default, make sure all attachments are used for rendering.
    this->SetDrawBuffers(colorAttachmentIndices.data(), colorAttachmentIndices.size());

    //Set up the depth attachment.
    if (depthOutput.has_value())
    {
        auto format = depthOutput.value().GetTex()->GetFormat();
        BP_ASSERT(format.IsDepthStencil(), "Depth attachment isn't a depth/stencil format");

        AttachTexture(GetAttachmentType(format.AsDepthStencil()), depthOutput.value());
        tex_depth = depthOutput.value();

        if (IsStencilOnly(format.AsDepthStencil()) || IsDepthAndStencil(format.AsDepthStencil()))
            tex_stencil = depthOutput.value();
    }
    else
    {
        AttachBuffer(DepthStencilFormats::Depth_24U);
    }

    outStatus = GetStatus();
}

Target::~Target()
{
    //First clean up any managed textures.
    for (auto* texPtr : managedTextures)
        delete texPtr;

    //Next, clean up the depth renderbuffer.
    if (depthBuffer.has_value())
        depthBuffer.reset();

    //Finally, clean up the FBO itself.
    if (!glPtr.IsNull())
    {
        threadData.targetsByOglPtr.erase(glPtr);
        glDeleteFramebuffers(1, &glPtr.Get());
    }
}


namespace
{
    TargetStates _dummy_error_code;
    uint32_t GetNDefaultLayers(const Target& target)
    {
        GLint result;
        glGetNamedFramebufferParameteriv(target.GetGlPtr().Get(),
                                         GL_FRAMEBUFFER_DEFAULT_LAYERS,
                                         &result);
        return (uint32_t)result;
    }
}
Target::Target(Target&& from)
    : Target(_dummy_error_code, from.size, GetNDefaultLayers(from))
{
    glPtr = from.glPtr;
    from.glPtr = OglPtr::Target::Null();

    tex_colors = std::move(from.tex_colors);
    tex_depth = from.tex_depth;
    tex_stencil = from.tex_stencil;

    depthBuffer = std::move(from.depthBuffer);
    isDepthRBBound = from.isDepthRBBound;
    isStencilRBBound = from.isStencilRBBound;

    managedTextures = std::move(from.managedTextures);

    //Update the static reference to this buffer.
    auto found = threadData.targetsByOglPtr.find(glPtr);
    BP_ASSERT(found != threadData.targetsByOglPtr.end(),
             "Un-indexed Target detected");
    found->second = this;
}

TargetStates Target::GetStatus() const
{
    //Make sure none of the formats are compressed.
    if ((tex_depth.has_value() &&
         tex_depth.value().GetTex()->GetFormat().IsCompressed()) ||
        (tex_stencil.has_value() &&
         tex_stencil.value().GetTex()->GetFormat().IsCompressed()) ||
        std::any_of(tex_colors.begin(), tex_colors.end(),
                    [](const TargetOutput& o)
                        { return o.GetTex()->GetFormat().IsCompressed(); }))
    {
        return TargetStates::CompressedFormat;
    }

    //Ask OpenGL if any other errors were detected.
    auto status = glCheckNamedFramebufferStatus(glPtr.Get(), GL_DRAW_FRAMEBUFFER);
    if (status == GL_FRAMEBUFFER_COMPLETE)
        return TargetStates::Ready;
    else if (status == GL_FRAMEBUFFER_UNSUPPORTED)
        return TargetStates::DriverUnsupported;
    else if (status == GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)
        return TargetStates::LayerMixup;
    //All other errors should have been handled by the logic in this class.
    else
    {
        BP_ASSERT_STR(false, "Unexpected glCheckFramebufferStatus code: " + IO::ToHex(status));
        return TargetStates::Unknown;
    }
}

void Target::RecomputeSize()
{
    //Edge-case: there are no attachments, so fall back to the default size.
    if (tex_colors.size() == 0 && !tex_depth.has_value() && !tex_stencil.has_value())
    {
        GLint x, y;
        glGetNamedFramebufferParameteriv(glPtr.Get(), GL_FRAMEBUFFER_DEFAULT_WIDTH, &x);
        glGetNamedFramebufferParameteriv(glPtr.Get(), GL_FRAMEBUFFER_DEFAULT_HEIGHT, &y);
        size = { (glm::u32)x, (glm::u32)y };
    }
    else
    {
        size = glm::uvec2{ std::numeric_limits<glm::u32>().max() };
        size = ComputeMin<glm::uvec2, GetTargOutSize, glm::min>(
                   tex_colors.data(), tex_colors.size(), tex_depth,
                   glm::uvec2{ std::numeric_limits<glm::u32>().max() },
                   glm::uvec2{ 0 });
        if (tex_stencil.has_value())
            size = glm::min(size, tex_stencil.value().GetSize());
    }
}

void Target::Activate(bool resetViewport, bool resetScissor) const
{
    auto& context = *Context::GetCurrentContext();
    context.SetActiveTarget(GetGlPtr());
    if (resetViewport)
        context.SetViewport(0, 0, (int)GetSize().x, (int)GetSize().y);
    if (resetScissor)
        context.DisableScissor();
}

void Target::AttachTexture(GLenum attachment, const TargetOutput& output)
{
    if (output.IsLayered() || output.IsFlat())
    {
        glNamedFramebufferTexture(glPtr.Get(), attachment,
            output.GetTex()->GetOglPtr().Get(),
            output.MipLevel);
    }
    else
    {
        glNamedFramebufferTextureLayer(glPtr.Get(), attachment,
            output.GetTex()->GetOglPtr().Get(),
            output.MipLevel, (GLint)output.GetLayer());
    }
}
void Target::AttachBuffer(DepthStencilFormats format)
{
    BP_ASSERT(!tex_stencil.has_value(),
             "Can't use a stencil texture and a depth buffer separately; they must be the same texture");

    //Create a new renderbuffer if we need it.
    if (!depthBuffer.has_value() || depthBuffer.value().GetSize() != size)
        depthBuffer = TargetBuffer{ Format{format}, size };

    //Attach the renderbuffer.
    GLenum attachment;
    if (IsDepthOnly(format))
        attachment = GL_DEPTH_ATTACHMENT;
    else if (IsDepthAndStencil(format))
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
    else
        BP_ASSERT(false, "Attaching Renderbuffer for FBO, but format isn't supported");
    glNamedFramebufferRenderbuffer(glPtr.Get(), attachment,
                                   GL_RENDERBUFFER, depthBuffer.value().GetOglPtr().Get());
}

void Target::SetDrawBuffers(const std::optional<uint32_t>* attachments, size_t nAttachments)
{
    //Update the field that remembers this data.
    activeColorAttachments.clear();
    internalActiveColorAttachments.clear();
    activeColorAttachments.reserve(nAttachments);
    internalActiveColorAttachments.reserve(nAttachments);
    for (size_t i = 0; i < nAttachments; ++i)
    {
        auto attachment = attachments[i];
        activeColorAttachments.push_back(attachment);
        internalActiveColorAttachments.push_back(attachment.has_value() ?
                                                   (GL_COLOR_ATTACHMENT0 + attachment.value()) :
                                                   GL_NONE);
    }

    glNamedFramebufferDrawBuffers(glPtr.Get(), (GLsizei)nAttachments,
                                  internalActiveColorAttachments.data());
}

const TargetOutput* Target::GetOutput_Color(uint32_t index) const
{
    return (index < GetNColorOutputs()) ?
               &tex_colors[activeColorAttachments[index].value()] :
               nullptr;
}


//Guide to clearing FBO's in OpenGL: https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glClearBuffer.xhtml

void Target::ClearColor(const glm::fvec4& rgba, uint32_t index)
{
    BP_ASSERT(index < GetNColorOutputs(), "Not enough color outputs to reach this index");
    BP_ASSERT(GetOutput_Color(index) != nullptr, "No color output at this index");
    BP_ASSERT(!GetOutput_Color(index)->GetTex()->GetFormat().IsInteger(),
             "Trying to clear an int/uint texture with a float value");

    //Currently there's a bug in GLEW where the rgba array in the below OpenGL call
    //    isn't marked 'const', even though it should be.
    //For now we're making a copy of the data and passing it as non-const,
    //    but there's already a fix in GLEW and we should probably integrate
    //    GLEW 2.2.0 once it comes out.
    //TODO: Update to GLEW 2.2.0 when available: https://github.com/nigels-com/glew
    auto rgbaCopy = rgba;

    glClearNamedFramebufferfv(glPtr.Get(), GL_COLOR, index, glm::value_ptr(rgbaCopy));
}
void Target::ClearColor(const glm::uvec4& rgba, uint32_t index)
{
    BP_ASSERT(index < GetNColorOutputs(), "Not enough color outputs to clear");
    BP_ASSERT(GetOutput_Color(index)->GetTex()->GetFormat().GetComponentType() ==
                 +FormatTypes::UInt,
             "Trying to clear a non-UInt texture with a uint value");

    glClearNamedFramebufferuiv(glPtr.Get(), GL_COLOR, index, glm::value_ptr(rgba));
}
void Target::ClearColor(const glm::ivec4& rgba, uint32_t index)
{
    BP_ASSERT(index < GetNColorOutputs(), "Not enough color outputs to clear");
    BP_ASSERT(GetOutput_Color(index)->GetTex()->GetFormat().GetComponentType() ==
                 +FormatTypes::Int,
             "Trying to clear a non-Int texture with an int value");

    glClearNamedFramebufferiv(glPtr.Get(), GL_COLOR, index, glm::value_ptr(rgba));
}

void Target::ClearDepth(float depth)
{
    auto context = Context::GetCurrentContext();
    auto oldState = context->GetDepthWrites();

    context->SetDepthWrites(true);
    glClearNamedFramebufferfv(glPtr.Get(), GL_DEPTH, 0, &depth);

    context->SetDepthWrites(oldState);
}
void Target::ClearStencil(uint32_t value)
{
    GLint valueI = Reinterpret<uint32_t, GLint>(value);
    glClearNamedFramebufferiv(glPtr.Get(), GL_STENCIL, 0, &valueI);
}
void Target::ClearDepthStencil(float depth, uint32_t stencil)
{
    auto stencilI = Reinterpret<uint32_t, GLint>(stencil);
    glClearNamedFramebufferfi(glPtr.Get(), GL_DEPTH_STENCIL, 0,
                              depth, stencilI);
}

GLenum Target::GetAttachmentType(DepthStencilFormats format)
{
    if (IsDepthOnly(format))
        return GL_DEPTH_ATTACHMENT;
    else if (IsStencilOnly(format))
        return GL_STENCIL_ATTACHMENT;
    else
    {
        BP_ASSERT(IsDepthAndStencil(format),
                 "Format is not depth, stencil, or both. How is that possible?");
        return GL_DEPTH_STENCIL_ATTACHMENT;
    }
}