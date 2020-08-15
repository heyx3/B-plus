#include "Context.h"

#include "Buffers/MeshData.h"
#include "Materials/CompiledShader.h"

using namespace Bplus;
using namespace Bplus::GL;
using namespace Bplus::GL::Buffers;


DrawMeshMode_Basic::DrawMeshMode_Basic(const Buffers::MeshData& mesh,
                                       std::optional<uint32_t> _nElements)
    : Data(mesh), Primitive(mesh.PrimitiveType)
{
    uint32_t nElements;
    if (_nElements.has_value())
        nElements = _nElements.value();
    else
    {
        if (mesh.HasIndexData())
        {
            auto indexData = mesh.GetIndexData().value();
            BPAssert(indexData.DataStructSize ==
                         GetByteSize(mesh.GetIndexDataType().value()),
                     "Listed byte-size of the data in the index buffer doesn't match the size expected by the mesh");

            BPAssert(indexData.Buf->GetByteSize() % indexData.DataStructSize == 0,
                     "Index buffer's size isn't divisible by the byte size of one element");
            nElements = indexData.Buf->GetByteSize() / indexData.DataStructSize;
        }
        else
        {
            BPAssert(false,
                     "Can't deduce the Count from a non-indexed MeshData automatically!\
 This can be done, but it's complicated when per-instance data is involved so I didn't bother.");
        }
    }

    Elements = Math::IntervalU::MakeSize(glm::uvec1{ nElements });
}


namespace
{
    struct ContextThreadData
    {
        Context* Instance = nullptr;

        std::vector<std::function<void()>> RefreshCallbacks, DestroyCallbacks;
    };
    thread_local ContextThreadData contextThreadData = ContextThreadData();
}

Context* Context::GetCurrentContext() { return contextThreadData.Instance; }

void Context::RegisterCallback_Destroyed(std::function<void()> func)
{
    contextThreadData.DestroyCallbacks.push_back(func);
}
void Context::RegisterCallback_RefreshState(std::function<void()> func)
{
    contextThreadData.RefreshCallbacks.push_back(func);
}


Context::Context(SDL_Window* _owner, std::string& errMsg, VsyncModes _vsync)
    : owner(_owner), vsync(vsync)
{
    if (contextThreadData.Instance != nullptr)
    {
        if (contextThreadData.Instance == this)
            errMsg = "This context has already been constructed!";
        else
            errMsg = "A context already exists on this thread that hasn't been cleaned up.";

        return;
    }

    //Configure/create the OpenGL context.
    if (!TrySDL(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,
                                    GLVersion_Major()),
                errMsg, "Error setting OpenGL context major") ||
        !TrySDL(SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,
                                    GLVersion_Minor()),
                errMsg, "Error setting OpenGL context minor") ||
        !TrySDL(sdlContext = SDL_GL_CreateContext(owner),
                errMsg, "Error initializing OpenGL context"))
    {
        return;
    }

    //We started OpenGL successfully!
    contextThreadData.Instance = this;
    isInitialized = true;

    //Initialize GLEW.
    glewExperimental = GL_TRUE;
    auto glewError = glewInit();
    if (glewError != GLEW_OK)
    {
        errMsg = std::string("Error setting up GLEW: ") +
            (const char*)glewGetErrorString(glewError);
        return;
    }

    RefreshState();
}
Context::~Context()
{
    if (isInitialized)
    {
        SDL_GL_DeleteContext(sdlContext);

        BPAssert(contextThreadData.Instance == this,
                 "More than one initialized Context in this thread");
        contextThreadData.Instance = nullptr;
    }

    for (const auto& callback : contextThreadData.DestroyCallbacks)
        callback();
}


void Context::RefreshState()
{
    //A handful of features will be left enabled permanently for simplicity;
    //    many can still be effectively disabled via object-specific parameters.
    glEnable(GL_BLEND);
    glEnable(GL_STENCIL_TEST);
    //Depth-testing is particularly important to keep on, because disabling it
    //    has a side effect of disabling any depth writes.
    glEnable(GL_DEPTH_TEST);
    //Point meshes must always specify their pixel size in their shaders;
    //    we don't bother with the global setting.
    //See https://www.khronos.org/opengl/wiki/Primitive#Point_primitives
    glEnable(GL_PROGRAM_POINT_SIZE);
    //Don't force a "fixed index" for primitive restart;
    //    this would only be useful for OpenGL ES compatibility.
    glDisable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
    //Keep point sprite coordinates at their default origin: upper-left.
    glPointParameteri(GL_POINT_SPRITE_COORD_ORIGIN, GL_UPPER_LEFT);

    //Containers for various OpenGL settings.
    GLint tempI;
    glm::ivec4 tempI4;
    glm::fvec4 tempF4;

    if (glIsEnabled(GL_SCISSOR_TEST))
    {
        glGetIntegerv(GL_SCISSOR_BOX, glm::value_ptr(tempI4));
        scissor = tempI4;
    }
    else
    {
        scissor.Clear();
    }

    state.EnableDepthWrite = glIsEnabled(GL_DEPTH_WRITEMASK);
    vsync = VsyncModes::_from_integral(SDL_GL_GetSwapInterval());
    glGetBooleanv(GL_COLOR_WRITEMASK, (GLboolean*)(&state.ColorWriteMask[0]));

    if (glIsEnabled(GL_CULL_FACE))
    {
        glGetIntegerv(GL_CULL_FACE_MODE, &tempI);
        state.CullMode = FaceCullModes::_from_integral(tempI);
    }
    else
    {
        state.CullMode = FaceCullModes::Off;
    }

    glGetIntegerv(GL_VIEWPORT, glm::value_ptr(tempI4));

    glGetIntegerv(GL_DEPTH_FUNC, &tempI);
    state.DepthTest = ValueTests::_from_integral(tempI);

    //Get color blending settings.
    glGetIntegerv(GL_BLEND_SRC_RGB, &tempI);
    state.ColorBlending.Src = BlendFactors::_from_integral(tempI);
    glGetIntegerv(GL_BLEND_DST_RGB, &tempI);
    state.ColorBlending.Dest = BlendFactors::_from_integral(tempI);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &tempI);
    state.ColorBlending.Op = BlendOps::_from_integral(tempI);

    //Get alpha blending settings.
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &tempI);
    state.AlphaBlending.Src = BlendFactors::_from_integral(tempI);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &tempI);
    state.AlphaBlending.Dest = BlendFactors::_from_integral(tempI);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &tempI);
    state.AlphaBlending.Op = BlendOps::_from_integral(tempI);

    //Get the blend constant.
    glGetFloatv(GL_BLEND_COLOR, glm::value_ptr(tempF4));
    state.ColorBlending.Constant = { tempF4.r, tempF4.g, tempF4.b };
    state.AlphaBlending.Constant = glm::vec1(tempF4.a);

    //Get the stencil tests and write ops.
    for (int faceI = 0; faceI < 2; ++faceI)
    {
        StencilTest& testData = (faceI == 0) ? state.StencilTestFront : state.StencilTestBack;
        StencilResult& resultData = (faceI == 0) ? state.StencilResultFront : state.StencilResultBack;
        GLuint& writeMask = (faceI == 0) ? state.StencilMaskFront : state.StencilMaskBack;

        GLenum side = (faceI == 0) ? GL_FRONT : GL_BACK,
               key_test = (faceI == 0) ? GL_STENCIL_FUNC : GL_STENCIL_BACK_FUNC,
               key_ref = (faceI == 0) ? GL_STENCIL_REF : GL_STENCIL_BACK_REF,
               key_valueMask = (faceI == 0) ? GL_STENCIL_VALUE_MASK : GL_STENCIL_BACK_VALUE_MASK,
               key_onFail = (faceI == 0) ? GL_STENCIL_FAIL : GL_STENCIL_BACK_FAIL,
               key_onFailDepth = (faceI == 0) ? GL_STENCIL_PASS_DEPTH_FAIL : GL_STENCIL_BACK_PASS_DEPTH_FAIL,
               key_onPass = (faceI == 0) ? GL_STENCIL_PASS_DEPTH_PASS : GL_STENCIL_BACK_PASS_DEPTH_PASS,
               key_writeMask = (faceI == 0) ? GL_STENCIL_WRITEMASK : GL_STENCIL_BACK_WRITEMASK;

        glGetIntegerv(key_test, &tempI);
        testData.Test = ValueTests::_from_integral(tempI);
        glGetIntegerv(key_ref, &tempI);
        testData.RefValue = tempI;
        glGetIntegerv(key_valueMask, &tempI);
        testData.Mask = (GLuint)tempI;

        glGetIntegerv(key_onFail, &tempI);
        resultData.OnFailStencil = StencilOps::_from_integral(tempI);
        glGetIntegerv(key_onFailDepth, &tempI);
        resultData.OnPassStencilFailDepth = StencilOps::_from_integral(tempI);
        glGetIntegerv(key_onPass, &tempI);
        resultData.OnPassStencilDepth = StencilOps::_from_integral(tempI);

        glGetIntegerv(key_writeMask, &tempI);
        writeMask = (GLuint)tempI;
    }

    //Update other systems that want to refresh.
    for (auto& f : contextThreadData.RefreshCallbacks)
        f();
}

void Context::SetState(const RenderState& newState)
{
    SetFaceCulling(newState.CullMode);

    //Depth/color:
    SetDepthTest(newState.DepthTest);
    SetDepthWrites(newState.EnableDepthWrite);
    SetColorWriteMask(newState.ColorWriteMask);

    //Blending:
    SetColorBlending(newState.ColorBlending);
    SetAlphaBlending(newState.AlphaBlending);

    //Stencil:
    SetStencilTestFrontFaces(newState.StencilTestFront);
    SetStencilTestBackFaces(newState.StencilTestBack);
    SetStencilResultFrontFaces(newState.StencilResultFront);
    SetStencilResultBackFaces(newState.StencilResultBack);
    SetStencilMaskFrontFaces(newState.StencilMaskFront);
    SetStencilMaskBackFaces(newState.StencilMaskBack);
}


void Context::SetActiveTarget(OglPtr::Target t)
{
    if (activeRT != t)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, t.Get());
        activeRT = t;
    }
}

void Context::Clear(float r, float g, float b, float a)
{
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}
void Context::Clear(float depth)
{
    glClearDepth(depth);
    glClear(GL_DEPTH_BUFFER_BIT);
}
void Context::Clear(float r, float g, float b, float a, float depth)
{
    glClearColor(r, g, b, a);
    glClearDepth(depth);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
}


void Context::Draw(const DrawMeshMode_Basic& mesh, const CompiledShader& shader) const
{
    shader.Activate();
    mesh.Data.Activate();
    glDrawArrays((GLenum)mesh.Primitive,
                 mesh.Elements.MinCorner.x, mesh.Elements.Size.x);
}
void Context::Draw(const DrawMeshMode_Basic& mesh, DrawMeshMode_Indexed indices,
                   const CompiledShader& shader) const
{
    BPAssert(mesh.Data.HasIndexData(),
             "Using indexed drawing on a mesh with no index data");

    shader.Activate();
    mesh.Data.Activate();

    //Configure the "primitive restart" special index, if desired.
    if (indices.ResetValue.has_value())
    {
        glEnable(GL_PRIMITIVE_RESTART);
        glPrimitiveRestartIndex(indices.ResetValue.value());
    }
    else
    {
        glDisable(GL_PRIMITIVE_RESTART);
    }

    //Make the correct draw call.
    auto indexType = mesh.Data.GetIndexDataType().value();
    auto firstByte = GetByteSize(indexType) * mesh.Elements.MinCorner.x;
    if (indices.ValueOffset == 0)
    {
        glDrawElements((GLenum)mesh.Primitive, mesh.Elements.Size.x,
                        (GLenum)indexType, (const void*)firstByte);
    }
    else
    {
        glDrawElementsBaseVertex((GLenum)mesh.Primitive,
                                    mesh.Elements.Size.x,
                                    (GLenum)indexType,
                                    (void*)firstByte, //TODO: File another GLEW issue; this should be const
                                    indices.ValueOffset);
    }
}


void Context::SetViewport(int minX, int minY, int width, int height)
{
    glm::ivec4 newViewport(minX, minY, width, height);
    if (newViewport == viewport)
        return;

    viewport = newViewport;
    glViewport(minX, minY, width, height);
}
void Context::GetViewport(int& outMinX, int& outMinY, int& outWidth, int& outHeight)
{
    outMinX = viewport.x;
    outMinY = viewport.y;
    outWidth = viewport.z;
    outHeight = viewport.w;
}


void Context::SetScissor(int minX, int minY, int width, int height)
{
    if (!scissor.IsCreated())
    {
        glEnable(GL_SCISSOR_TEST);
        scissor = { -1, -1, -1, -1 };
    }

    glm::ivec4 newScissor{ minX, minY, width, height };
    if (scissor.Get() != newScissor)
    {
        glScissor(minX, minY, width, height);
        scissor = newScissor;
    }
}
void Context::DisableScissor()
{
    if (scissor.IsCreated())
    {
        glDisable(GL_SCISSOR_TEST);
        scissor.Clear();
    }
}
bool Context::GetScissor(int& outMinX, int& outMinY, int& outWidth, int& outHeight) const
{
    if (scissor.IsCreated())
    {
        outMinX = scissor.Get().x;
        outMinY = scissor.Get().y;
        outWidth = scissor.Get().z;
        outHeight = scissor.Get().w;
        return true;
    }
    else
    {
        return false;
    }
}


bool Context::SetVsyncMode(VsyncModes mode)
{
    int err = SDL_GL_SetSwapInterval((int)mode);

    //If it failed, maybe the hardware just doesn't support G-sync/FreeSync.
    if (err != 0 && mode == +VsyncModes::Adaptive)
        err = SDL_GL_SetSwapInterval((int)VsyncModes::On);

    return err == 0;
}

void Context::SetFaceCulling(FaceCullModes mode)
{
    if (mode == +FaceCullModes::Off)
    {
        if (state.CullMode != +FaceCullModes::Off)
        {
            glDisable(GL_CULL_FACE);
            state.CullMode = +FaceCullModes::Off;
        }
    }
    else
    {
        if (state.CullMode == +FaceCullModes::Off)
            glEnable(GL_CULL_FACE);

        if (state.CullMode != mode)
        {
            state.CullMode = mode;
            glCullFace((GLenum)mode);
        }
    }
}

void Context::SetDepthTest(ValueTests newTest)
{
    //If we haven't initialized depth-testing yet, turn it on permanently.
    //Disabling depth-testing also disables depth writes,
    //    but we expose a separate mechanism for handling that.
    if ((GLenum)state.DepthTest == GL_INVALID_ENUM)
        glEnable(GL_DEPTH_TEST);
    
    if (state.DepthTest != newTest)
    {
        glDepthFunc((GLenum)newTest);
        state.DepthTest = newTest;
    }
}

void Context::SetDepthWrites(bool canWriteDepth)
{
    if (canWriteDepth != state.EnableDepthWrite)
    {
        state.EnableDepthWrite = canWriteDepth;
        if (state.EnableDepthWrite)
            glEnable(GL_DEPTH_WRITEMASK);
        else
            glDisable(GL_DEPTH_WRITEMASK);
    }
}

void Context::SetColorWriteMask(glm::bvec4 canWrite)
{
    if (canWrite == state.ColorWriteMask)
        return;

    state.ColorWriteMask = canWrite;
    glColorMask(canWrite.r, canWrite.g, canWrite.b, canWrite.a);
}


BlendStateRGBA Context::GetBlending() const
{
    //Make sure the same blend settings are being used for both RGB and Alpha.
    BlendStateAlpha colorBlendTest{ state.ColorBlending.Src, state.ColorBlending.Dest,
                                    state.ColorBlending.Op,
                                    state.AlphaBlending.Constant };
    BPAssert(state.AlphaBlending == colorBlendTest,
             "Alpha blend state and color blend state do not match up");

    return BlendStateRGBA{ state.ColorBlending.Src, state.ColorBlending.Dest,
                           state.ColorBlending.Op,
                           { state.ColorBlending.Constant, state.AlphaBlending.Constant } };
}

void Context::SetBlending(const BlendStateRGBA& blendState)
{
    //Don't waste time in the GPU driver if we're already in this blend state.
    BlendStateRGB newColorBlending{ blendState.Src, blendState.Dest, blendState.Op,
                                    { blendState.Constant.r, blendState.Constant.g, blendState.Constant.b } };
    BlendStateAlpha newAlphaBlending{ blendState.Src, blendState.Dest, blendState.Op,
                                      glm::vec1(blendState.Constant.a) };
    if ((newColorBlending == state.ColorBlending) &
        (newAlphaBlending == state.AlphaBlending))
    {
        return;
    }

    state.ColorBlending = newColorBlending;
    state.AlphaBlending = newAlphaBlending;

    glBlendFunc((GLenum)blendState.Src, (GLenum)blendState.Dest);
    glBlendEquation((GLenum)blendState.Op);
    glBlendColor(blendState.Constant.r, blendState.Constant.g, blendState.Constant.b, blendState.Constant.a);
}
void Context::SetColorBlending(const BlendStateRGB& blendState)
{
    if (blendState == state.ColorBlending)
        return;

    state.ColorBlending = blendState;
    glBlendFuncSeparate((GLenum)state.ColorBlending.Src, (GLenum)state.ColorBlending.Dest,
                        (GLenum)state.AlphaBlending.Src, (GLenum)state.AlphaBlending.Dest);
    glBlendEquationSeparate((GLenum)state.ColorBlending.Op,
                            (GLenum)state.AlphaBlending.Op);
    glBlendColor(state.ColorBlending.Constant.r,
                 state.ColorBlending.Constant.g,
                 state.ColorBlending.Constant.b,
                 state.AlphaBlending.Constant.x);
}
void Context::SetAlphaBlending(const BlendStateAlpha& blendState)
{
    if (blendState == state.AlphaBlending)
        return;

    state.AlphaBlending = blendState;
    glBlendFuncSeparate((GLenum)state.ColorBlending.Src, (GLenum)state.ColorBlending.Dest,
                        (GLenum)state.AlphaBlending.Src, (GLenum)state.AlphaBlending.Dest);
    glBlendEquationSeparate((GLenum)state.ColorBlending.Op,
                            (GLenum)state.AlphaBlending.Op);
    glBlendColor(state.ColorBlending.Constant.r,
                 state.ColorBlending.Constant.g,
                 state.ColorBlending.Constant.b,
                 state.AlphaBlending.Constant.x);
}


const StencilTest& Context::GetStencilTest() const
{
    //Make sure the same settings are being used for both front- and back-faces.
    BPAssert(state.StencilTestFront == state.StencilTestBack,
             "Front-face stencil test and back-face stencil test don't match");
    return state.StencilTestFront;
}

void Context::SetStencilTest(const StencilTest& test)
{
    if ((state.StencilTestFront == test) & (state.StencilTestBack == test))
        return;

    state.StencilTestFront = test;
    state.StencilTestBack = test;

    glStencilFunc((GLenum)test.Test, test.RefValue, test.Mask);
}
void Context::SetStencilTestFrontFaces(const StencilTest& test)
{
    if (test == state.StencilTestFront)
        return;

    state.StencilTestFront = test;
    glStencilFuncSeparate(GL_FRONT, (GLenum)test.Test, test.RefValue, test.Mask);
}
void Context::SetStencilTestBackFaces(const StencilTest& test)
{
    if (test == state.StencilTestBack)
        return;

    state.StencilTestBack = test;
    glStencilFuncSeparate(GL_BACK, (GLenum)test.Test, test.RefValue, test.Mask);
}


const StencilResult& Context::GetStencilResult() const
{
    //Make sure the same settings are being used for both front- and back-faces.
    BPAssert(state.StencilResultFront == state.StencilResultBack,
             "Front-face stencil result and back-face stencil result don't match");
    return state.StencilResultFront;
}

void Context::SetStencilResult(const StencilResult& result)
{
    if ((state.StencilResultFront == result) & (state.StencilResultBack == result))
        return;

    state.StencilResultFront = result;
    state.StencilResultBack = result;
    glStencilOp((GLenum)result.OnFailStencil,
                (GLenum)result.OnPassStencilFailDepth,
                (GLenum)result.OnPassStencilDepth);
}
void Context::SetStencilResultFrontFaces(const StencilResult& result)
{
    if (result == state.StencilResultFront)
        return;

    state.StencilResultFront = result;
    glStencilOpSeparate(GL_FRONT,
                        (GLenum)result.OnFailStencil,
                        (GLenum)result.OnPassStencilFailDepth,
                        (GLenum)result.OnPassStencilDepth);
}
void Context::SetStencilResultBackFaces(const StencilResult& result)
{
    if (result == state.StencilResultBack)
        return;

    state.StencilResultBack = result;
    glStencilOpSeparate(GL_BACK,
                        (GLenum)result.OnFailStencil,
                        (GLenum)result.OnPassStencilFailDepth,
                        (GLenum)result.OnPassStencilDepth);
}


GLuint Context::GetStencilMask() const
{
    //Make sure the same settings are being used for both front- and back-faces.
    BPAssert(state.StencilMaskFront == state.StencilMaskBack,
             "Front-face stencil mask and back-face stencil mask don't match up");
    return state.StencilMaskFront;
}

void Context::SetStencilMask(GLuint mask)
{
    if ((state.StencilMaskFront == mask) & (state.StencilMaskBack == mask))
        return;

    state.StencilMaskFront = mask;
    state.StencilMaskBack = mask;
    glStencilMask(mask);
}
void Context::SetStencilMaskFrontFaces(GLuint mask)
{
    if (mask == state.StencilMaskFront)
        return;

    state.StencilMaskFront = mask;
    glStencilMaskSeparate(GL_FRONT, mask);
}
void Context::SetStencilMaskBackFaces(GLuint mask)
{
    if (mask == state.StencilMaskBack)
        return;

    state.StencilMaskBack = mask;
    glStencilMaskSeparate(GL_BACK, mask);
}