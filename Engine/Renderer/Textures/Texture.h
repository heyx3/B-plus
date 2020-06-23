#pragma once

#include <numeric>
#include <array>

#include "Data.h"


namespace Bplus::GL::Textures
{
    class Texture;
    struct TexView;
    struct ImgView;


    #pragma region TexHandle and ImgHandle

    //A helper class for Texture, representing a bindless handle.
    //Should not and cannot be used outside of that class,
    //    but it can't be nested in Texture due to forward-declaration problems.
    struct BP_API TexHandle
    {
        const OglPtr::View ViewGlPtr;
        const OglPtr::Sampler SamplerGlPtr = OglPtr::Sampler::Null();

        //No copying, but moves are fine.
        TexHandle(const TexHandle& cpy) = delete;
        TexHandle& operator=(const TexHandle& cpy) = delete;
        TexHandle(TexHandle&& src);
        TexHandle& operator=(TexHandle&& src)
        {
            //Call deconstructor, then move constructor.
            this->~TexHandle();
            new (this) TexHandle(std::move(src));

            return *this;
        }

        void Activate();
        void Deactivate();

        bool IsActive() const { return activeCount > 0; }


    private:

        friend class Texture;
        friend struct TexView;
        friend struct std::default_delete<TexHandle>;
        friend class std::unique_ptr<TexHandle>;

        TexHandle(const Texture* src);
        TexHandle(const Texture* src, const Sampler<3>& sampler3D);
        ~TexHandle();

        uint_fast32_t activeCount = 0;
        bool skipDestructor = false;
    };


    //Represents the parameters that come with an "ImgView".
    struct BP_API ImgHandleData
    {
        uint_mipLevel_t MipLevel;
        std::optional<uint_fast32_t> SingleLayer;
        ImageAccessModes Access;

        ImgHandleData(ImageAccessModes access = ImageAccessModes::ReadWrite,
                      std::optional<uint_fast32_t> singleLayer = std::nullopt,
                      uint_mipLevel_t mipLevel = 0)
            : MipLevel(mipLevel), SingleLayer(singleLayer), Access(access) { }

        bool operator==(const ImgHandleData& other) const
        {
            return (SingleLayer == other.SingleLayer) &
                (Access == other.Access) &
                (MipLevel == other.MipLevel);
        }
        bool operator!=(const ImgHandleData& other) const { return !operator==(other); }
    };

    //A helper class for Texture, representing a bindless handle.
    //Should not and cannot be used outside of that class,
    //    but it can't be nested in Texture due to forward-declaration problems.
    struct BP_API ImgHandle
    {
        const OglPtr::View ViewGlPtr;
        const ImgHandleData Params;

        //No copying, but moves are fine.
        ImgHandle(const ImgHandle& cpy) = delete;
        ImgHandle& operator=(const ImgHandle& cpy) = delete;
        ImgHandle(ImgHandle&& src);
        ImgHandle& operator=(ImgHandle&& src)
        {
            //Call deconstructor, then move constructor.
            this->~ImgHandle();
            new (this) ImgHandle(std::move(src));

            return *this;
        }

        void Activate();
        void Deactivate();

        bool IsActive() const { return activeCount > 0; }


    private:

        friend class Texture;
        friend struct ImgView;
        friend struct std::default_delete<ImgHandle>;
        friend class std::unique_ptr<ImgHandle>;

        ImgHandle(const Texture* src, const ImgHandleData& params);
        ~ImgHandle();


        uint_fast32_t activeCount = 0;
        bool skipDestructor = false;
    };

    #pragma endregion


    //A texture combined with a custom sampler.
    struct BP_API TexView
    {
        const OglPtr::View GlPtr;

        const Texture& Owner;
        TexHandle& Handle;

        TexView(const Texture& owner, TexHandle& handle);
        ~TexView();

        TexView(const TexView& cpy) : TexView(cpy.Owner, cpy.Handle) { }
        TexView& operator=(const TexView& cpy);
        TexView(TexView&& from) : TexView(from) { }
        TexView& operator=(TexView&& from) { return operator=(from); }
    };

    //A specific mip-level of a texture,
    //    for direct reading and writing (no sampling).
    struct BP_API ImgView
    {
        const OglPtr::View GlPtr;

        const Texture& Owner;
        ImgHandle& Handle;

        ImgView(const Texture& owner, ImgHandle& handle);
        ~ImgView();

        ImgView(const ImgView& cpy) : ImgView(cpy.Owner, cpy.Handle) { }
        ImgView& operator=(const ImgView& cpy);
        ImgView(ImgView&& from) : ImgView(from) { }
        ImgView& operator=(ImgView&& from) { return operator=(from); }
    };
}
//Provide a hash for Textures::ImgHandleData:
namespace std {
    template<> struct hash<Bplus::GL::Textures::ImgHandleData> {
        size_t operator()(const Bplus::GL::Textures::ImgHandleData& value) const {
            return MultiHash(value.Access, value.MipLevel, value.SingleLayer);
        }
    };
}

namespace Bplus::GL::Textures
{
    //The base class for all OpenGL textures.
    //Designed to be used with OpenGL's Bindless Textures extension.
    class BP_API Texture
    {
    public:

        Texture(Types type, Format format, uint_mipLevel_t nMipLevels,
                const Sampler<3>& sampler3D);

        virtual ~Texture();
        
        //No copying.
        Texture(const Texture& cpy) = delete;
        Texture& operator=(const Texture& cpy) = delete;


        const Format& GetFormat() const { return format; }

        //Gets a 3D version of this texture's sampler,
        //    which may contain some garbage data if the texture is smaller than 3D.
        const Sampler<3>& GetSamplerFull() const { return sampler3D; }

        Types GetType() const { return type; }
        uint_mipLevel_t GetNMipLevels() const { return nMipLevels; }
        OglPtr::Texture GetOglPtr() const { return glPtr; }


        //Gets the number of bytes needed to store one mip leve of this texture
        //    in its native format.
        virtual size_t GetByteSize(uint_mipLevel_t mipLevel = 0) const = 0;
        //Gets the total byte-size of this texture's data, across all mip levels.
        virtual size_t GetTotalByteSize() const;


        //Updates mipmaps for this texture.
        //Not allowed for compressed-format textures.
        void RecomputeMips();
        
        //Gets (or creates) an "image" view of this texture,
        //    allowing simple reads/writes but no sampling.
        ImgView GetView(ImgHandleData params) const;

        //Gets (or creates) a view of this texture with the given sampler.
        //Child classes should provide a public "GeteView() with
        //    the correct-dimensional sampler.
        TexView GetViewFull(std::optional<Sampler<3>> customSampler = std::nullopt) const;


    protected:

        //Child classes have access to the move constructor.
        Texture(Texture&& src);
        Texture& operator=(Texture&& src) = delete;
        

        //Given a set of components for texture uploading/downloading,
        //    and the data type of this texture's pixels,
        //    finds the corresponding OpenGL enum value.
        GLenum GetOglChannels(ComponentData components) const
        {
            //If the pixel format isn't integer (i.e. it's float or normalized integer),
            //    we can directly use the enum values.
            //Otherwise, we should be sending the "integer" enum values.
            if (!format.IsInteger())
                return (GLenum)components;
            else switch (components)
            {
                case ComponentData::Red: return GL_RED_INTEGER;
                case ComponentData::Green: return GL_GREEN_INTEGER;
                case ComponentData::Blue: return GL_BLUE_INTEGER;
                case ComponentData::RG: return GL_RG_INTEGER;
                case ComponentData::RGB: return GL_RGB_INTEGER;
                case ComponentData::BGR: return GL_BGR_INTEGER;
                case ComponentData::RGBA: return GL_RGBA_INTEGER;
                case ComponentData::BGRA: return GL_BGRA_INTEGER;

                default:
                    std::string msg = "Unexpected data component type: ";
                    msg += components._to_string();
                    BPAssert(false, msg.c_str());
                    return GL_NONE;
            }
        }

        //Given a type T, finds the corresponding GLenum for that type of data.
        //    bool types are interpreted as unsigned integers of the same size.
        template<typename T>
        GLenum GetOglInputFormat() const
        {
            //Deduce the "type" argument for OpenGL, representing
            //    the size of each channel being sent in.
            GLenum type = GL_NONE;
            if constexpr (std::is_same_v<T, bool>) {
                if constexpr (sizeof(bool) == 1) {
                    type = GL_UNSIGNED_BYTE;
                } else if constexpr (sizeof(bool) == 2) {
                    type = GL_UNSIGNED_SHORT;
                } else if constexpr (sizeof(bool) == 4) {
                    type = GL_UNSIGNED_INT;
                } else {
                    static_assert(false, "Unexpected value for sizeof(bool)");
                }
            } else if constexpr (std::is_same_v<T, glm::u8>) {
                type = GL_UNSIGNED_BYTE;
            } else if constexpr (std::is_same_v<T, glm::u16>) {
                type = GL_UNSIGNED_SHORT;
            } else if constexpr (std::is_same_v<T, glm::u32>) {
                type = GL_UNSIGNED_INT;
            } else if constexpr (std::is_same_v<T, glm::i8>) {
                type = GL_BYTE;
            } else if constexpr (std::is_same_v<T, glm::i16>) {
                type = GL_SHORT;
            } else if constexpr (std::is_same_v<T, glm::i32>) {
                type = GL_INT;
            } else if constexpr (std::is_same_v<T, glm::f32>) {
                type = GL_FLOAT;
            } else {
                T ta;
                ta.aslflskjflsjf4444 = 4;
                static_assert(false, "T is an unexpected type");
            }

            return type;
        }

        //Given a number of dimensions, and a switch for reversed (BGR) ordering,
        //    finds the corresponding enum value representing component ordering
        //    for texture upload/download.
        template<glm::length_t L>
        ComponentData GetComponents(bool bgrOrdering) const
        {
            if constexpr (L == 1) {
                return ComponentData::Greyscale;
            } else if constexpr (L == 2) {
                return ComponentData::RG;
            } else if constexpr (L == 3) {
                return (bgrOrdering ?
                            ComponentData::BGR :
                            ComponentData::RGB);
            } else if constexpr (L == 4) {
                return (bgrOrdering ?
                            ComponentData::BGRA :
                            ComponentData::RGBA);
            } else {
                static_assert(false, "L should be between 1 and 4");
                return ComponentData::Greyscale;
            }
        }


    private:

        OglPtr::Texture glPtr;
        Types type;
        uint_mipLevel_t nMipLevels;

        Format format;
        Sampler<3> sampler3D;


        //Texture views represent different ways of sampling from this texture in a shader.
        //This field is a cache of the views that have already been created.
        //They are stored as unique_ptr so that their pointer doesn't change.
        mutable std::unordered_map<Sampler<3>, std::unique_ptr<TexHandle>> texHandles;

        //Image views represent different parts of this texture for shaders to read/write.
        //This field is a cache of the views that have already been created.
        //They are stored as unique_ptr so that their pointer doesn't change.
        mutable std::unordered_map<ImgHandleData, std::unique_ptr<ImgHandle>> imgHandles;
    };
}