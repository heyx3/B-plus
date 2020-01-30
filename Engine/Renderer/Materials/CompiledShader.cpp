#include "CompiledShader.h"

using namespace Bplus::GL;

//Hook into the thread rendering context's "RefreshState()" function
//    to refresh the currently-active shader.
namespace
{
    thread_local struct
    {
        CompiledShader* currentShader = nullptr;
        bool initializedYet = false;

        std::unordered_map<Ptr::ShaderProgram, CompiledShader*> shadersByHandle =
            std::unordered_map<Ptr::ShaderProgram, CompiledShader*>();
    } threadData;


    //Returns the error message, or an empty string if everything is fine.
    std::string TryCompile(GLuint shaderObject)
    {
        glCompileShader(shaderObject);

        GLint isCompiled = 0;
        glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &isCompiled);
        if (isCompiled == GL_FALSE)
        {
            GLint msgLength = 0;
            glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &msgLength);

            std::vector<char> msgBuffer(msgLength);
            glGetShaderInfoLog(shaderObject, msgLength, &msgLength, msgBuffer.data());

            return std::string(msgBuffer.data());
        }

        return "";
    }
}



Ptr::ShaderProgram CompiledShader::Compile(const char* vertShader, const char* fragShader,
                                           std::string& outErrMsg)
{
    GLuint vertShaderObj = glCreateShader(GL_VERTEX_SHADER),
           fragShaderObj = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertShaderObj, 1, &vertShader, nullptr);
    glShaderSource(fragShaderObj, 1, &fragShader, nullptr);

    auto errorMsg = TryCompile(vertShaderObj);
    if (errorMsg.size() > 0)
    {
        outErrMsg = "Error compiling vertex shader: " + errorMsg;
        glDeleteShader(vertShaderObj);
        glDeleteShader(fragShaderObj);
        return Ptr::ShaderProgram::Null;
    }

    errorMsg = TryCompile(fragShaderObj);
    if (errorMsg.size() > 0)
    {
        outErrMsg = "Error compiling fragment shader: " + errorMsg;
        glDeleteShader(vertShaderObj);
        glDeleteShader(fragShaderObj);
        return Ptr::ShaderProgram::Null;
    }

    //Now that everything is compiled, try linking it all together.
    GLuint programObj = glCreateProgram();
    glAttachShader(programObj, vertShaderObj);
    glAttachShader(programObj, fragShaderObj);
    glLinkProgram(programObj);
    //We can immediately mark the individual shader objects for cleanup.
    glDeleteShader(vertShaderObj);
    glDeleteShader(fragShaderObj);
    //Check the link result.
    GLint isLinked = 0;
    glGetProgramiv(programObj, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint msgLength = 0;
        glGetProgramiv(programObj, GL_INFO_LOG_LENGTH, &msgLength);

        std::vector<char> msgBuffer(msgLength);
        glGetProgramInfoLog(programObj, msgLength, &msgLength, msgBuffer.data());

        glDeleteProgram(programObj);
        errorMsg = "Error linking shaders: " + std::string(msgBuffer.data());
        return Ptr::ShaderProgram::Null;
    }

    //If the link is successful, we need to "detach" the shader objects
    //    from the program object so that they can be cleaned up.
    glDetachShader(programObj, vertShaderObj);
    glDetachShader(programObj, fragShaderObj);

    return Ptr::ShaderProgram(programObj);
}

Ptr::ShaderProgram CompiledShader::Compile(const char* vertShader,
                                           const char* geomShader,
                                           const char* fragShader,
                                           std::string& outErrMsg)
{
    GLuint vertShaderObj = glCreateShader(GL_VERTEX_SHADER),
           geomShaderObj = glCreateShader(GL_GEOMETRY_SHADER),
           fragShaderObj = glCreateShader(GL_FRAGMENT_SHADER);

    glShaderSource(vertShaderObj, 1, &vertShader, nullptr);
    glShaderSource(geomShaderObj, 1, &geomShader, nullptr);
    glShaderSource(fragShaderObj, 1, &fragShader, nullptr);

    auto errorMsg = TryCompile(vertShaderObj);
    if (errorMsg.size() > 0)
    {
        outErrMsg = "Error compiling vertex shader: " + errorMsg;
        glDeleteShader(vertShaderObj);
        glDeleteShader(geomShaderObj);
        glDeleteShader(fragShaderObj);
        return Ptr::ShaderProgram::Null;
    }

    errorMsg = TryCompile(geomShaderObj);
    if (errorMsg.size() > 0)
    {
        outErrMsg = "Error compiling geometry shader: " + errorMsg;
        glDeleteShader(vertShaderObj);
        glDeleteShader(geomShaderObj);
        glDeleteShader(fragShaderObj);
        return Ptr::ShaderProgram::Null;
    }

    errorMsg = TryCompile(fragShaderObj);
    if (errorMsg.size() > 0)
    {
        outErrMsg = "Error compiling fragment shader: " + errorMsg;
        glDeleteShader(vertShaderObj);
        glDeleteShader(geomShaderObj);
        glDeleteShader(fragShaderObj);
        return Ptr::ShaderProgram::Null;
    }

    //Now that everything is compiled, try linking it all together.
    GLuint programObj = glCreateProgram();
    glAttachShader(programObj, vertShaderObj);
    glAttachShader(programObj, geomShaderObj);
    glAttachShader(programObj, fragShaderObj);
    glLinkProgram(programObj);
    //We can immediately mark the individual shader objects for cleanup.
    glDeleteShader(vertShaderObj);
    glDeleteShader(geomShaderObj);
    glDeleteShader(fragShaderObj);
    //Check the link result.
    GLint isLinked = 0;
    glGetProgramiv(programObj, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint msgLength = 0;
        glGetProgramiv(programObj, GL_INFO_LOG_LENGTH, &msgLength);

        std::vector<char> msgBuffer(msgLength);
        glGetProgramInfoLog(programObj, msgLength, &msgLength, msgBuffer.data());

        glDeleteProgram(programObj);
        errorMsg = "Error linking shaders: " + std::string(msgBuffer.data());
        return Ptr::ShaderProgram::Null;
    }

    //If the link is successful, we need to "detach" the shader objects
    //    from the program object so that they can be cleaned up.
    glDetachShader(programObj, vertShaderObj);
    glDetachShader(programObj, geomShaderObj);
    glDetachShader(programObj, fragShaderObj);

    return Ptr::ShaderProgram(programObj);
}

//TODO: The set of available uniforms should be locked in stone by the end of this constructor.
CompiledShader::CompiledShader(Ptr::ShaderProgram compiledProgramHandle,
                               const std::vector<std::string>& uniformNames)
    : programHandle(compiledProgramHandle)
{
    if (!threadData.initializedYet)
    {
        threadData.initializedYet = true;

        std::function<void()> refreshContext = []()
        {
            //Get the handle of the currently-bound shader.
            GLint _currentProgram;
            glGetIntegerv(GL_CURRENT_PROGRAM, &_currentProgram);
            Ptr::ShaderProgram currentProgram((GLuint)_currentProgram);

            //Look that shader up in the thread-local dictionary
            //    of all compiled shaders.
            auto found = threadData.shadersByHandle.find(currentProgram);
            threadData.currentShader = (found == threadData.shadersByHandle.end()) ?
                                           nullptr :
                                           found->second;
        };
        refreshContext();
        Context::RegisterCallback_RefreshState(refreshContext);

        Context::RegisterCallback_Destroyed([]()
        {
            //If any CompiledShaders haven't been cleaned up yet,
            //    it's a memory leak.
            BPAssert(threadData.shadersByHandle.size() == 0,
                     "Some CompiledShader instances haven't been cleaned up");
            threadData.shadersByHandle.clear();
        });
    }


    BPAssert(threadData.shadersByHandle.find(programHandle) == threadData.shadersByHandle.end(),
             "A CompiledShader already exists with this program");
    threadData.shadersByHandle[programHandle] = this;

    //Store all uniform pointers, ignoring ones that don't exist/
    //    have been optimized out.
    for (const auto& uniformName : uniformNames)
    {
        Ptr::ShaderUniform loc{ glGetUniformLocation(programHandle.Get(),
                                                      uniformName.c_str()) };
        if (loc != Ptr::ShaderUniform::Null)
            uniformPtrs[uniformName] = loc;
    }
}
CompiledShader::~CompiledShader()
{
    if (programHandle != Ptr::ShaderProgram::Null)
    {
        glDeleteProgram(programHandle.Get());
        threadData.shadersByHandle.erase(programHandle);
    }
}

CompiledShader::CompiledShader(CompiledShader&& src)
    : uniformPtrs(std::move(src.uniformPtrs)), programHandle(src.programHandle)
{
    src.programHandle = Ptr::ShaderProgram::Null;

    BPAssert(threadData.shadersByHandle.find(programHandle) != threadData.shadersByHandle.end(),
             "CompiledShader is missing from 'shadersByHandle'");
    BPAssert(threadData.shadersByHandle[programHandle] == &src,
             "Some other CompiledShader owns this program handle");
    threadData.shadersByHandle[programHandle] = this;
}
CompiledShader& CompiledShader::operator=(CompiledShader&& src)
{
    uniformPtrs = std::move(src.uniformPtrs);
    programHandle = src.programHandle;

    src.programHandle = Ptr::ShaderProgram::Null;
    
    BPAssert(threadData.shadersByHandle.find(programHandle) != threadData.shadersByHandle.end(),
             "CompiledShader is missing from 'shadersByHandle'");
    BPAssert(threadData.shadersByHandle[programHandle] == &src,
             "Some other CompiledShader owns this program handle");
    threadData.shadersByHandle[programHandle] = this;

    return *this;
}

CompiledShader* CompiledShader::GetCurrentActive()
{
    return threadData.currentShader;
}
void CompiledShader::Activate()
{
    if (threadData.currentShader == this)
        return;

    glUseProgram(programHandle.Get());
    threadData.currentShader = this;
}