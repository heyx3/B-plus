#pragma once

#include <variant>

#include "../Data.h"

namespace Bplus::GL::Textures
{
    BETTER_ENUM(ColorChannels, uint8_t,
        Red = 0, Green = 1, Blue = 2, Alpha = 3);

    BETTER_ENUM(AllChannels, uint8_t,
        Red = 0, Green = 1, Blue = 2, Alpha = 3,
        Depth = 4, Stencil = 5);


    #pragma region SimpleFormat

    //The type of data representing each color channel in a texture's pixels.
    BETTER_ENUM(FormatTypes, uint8_t,
        //A floating-point number (i.e. theoretically-unlimited range).
        Float,
        //A value between 0 - 1, stored as an unsigned integer between 0 and its maximum value.
        NormalizedUInt,
        //A value between -1 and +1, stored as a signed integer between its minimum value and its maximum value.
        NormalizedInt,
        
        //An unsigned integer. Sampling from this texture yields integer values, not floats.
        UInt,
        //A signed integer. Sampling from this texture yields integer values, not floats.
        Int
    );

    //The sets of components that can be stored in various texture formats.
    BETTER_ENUM(FormatComponents, uint8_t,
        R, RG, RGB, RGBA
    );
    //The sets of bit-depths that components can have in various texture formats.
    BETTER_ENUM(BitDepths, uint8_t,
        B2 = 2,
        B4 = 4,
        B5 = 5,
        B8 = 8,
        B10 = 10,
        B12 = 12,
        B16 = 16
    );


    //A straight-forward texture format.
    struct BP_API SimpleFormat
    {
    public:

        FormatTypes Type;
        FormatComponents Components;
        BitDepths ChannelBitSize;
    };

    #pragma endregion

    #pragma region SpecialFormats

    //Special one-off texture formats.
    BETTER_ENUM(SpecialFormats, GLenum,
        //NormalizedUInt texture packing each pixel into 2 bytes:
        //    5 bits for Red, 6 for Green, and 5 for Blue (no alpha).
        R5_G6_B5 = GL_RGB565,

        //NormalizedUInt texture packing each pixel into 4 bytes:
        //    10 bits each for Red, Green, and Blue, and 2 bits for Alpha.
        RGB10_A2 = GL_RGB10_A2,

        //UInt texture (meaning it outputs integer values, not floats!)
        //    that packs each pixel into 4 bytes:
        //    10 bits each for Red, Green, and Blue, and 2 bits for Alpha.
        RGB10_A2_UInt = GL_RGB10_A2UI,

        //Floating-point texture using special unsigned 11-bit floats for Red and Green,
        //    and unsigned 10-bit float for Blue. No Alpha.
        //Floats of this size can represent values from .0000610 to 65500,
        //    with ~2 digits of precision.
        RGB_TinyFloats = GL_R11F_G11F_B10F,

        //Floating-point texture using special unsigned 14-bit floats
        //    each for Red, Green, and Blue (no alpha), but with a catch:
        //They share the same 5-bit exponent, to fit into 32 bits per pixel.
        RGB_SharedExpFloats = GL_RGB9_E5,


        //Each pixel is a 24-bit sRGB colorspace image (no alpha).
        //Each channel is 8 bytes, and the texture data is treated as non-linear,
        //    which means it's converted into linear values on the fly when sampled.
        sRGB = GL_SRGB8,

        //Same as sRGB, but with the addition of a linear (meaning non-sRGB) 8-bit Alpha value.
        sRGB_LinearAlpha = GL_SRGB8_ALPHA8,


        //NormalizedUInt texture packing each pixel into a single byte:
        //    3 bits for Red, 3 for Green, and 2 for Blue (no alpha).
        //Note that, from reading on the Internet,
        //    it seems most hardware just converts to R5_G6_B5 under the hood.
        R3_G3_B2 = GL_R3_G3_B2,

        //NormalizedUInt texture packing each pixel into 2 bytes:
        //    5 bits each for Red, Green, and Blue, and 1 bit for Alpha.
        //It is highly recommended to use a compressed format instead of this one.
        RGB5_A1 = GL_RGB5_A1
    );

    #pragma endregion

    #pragma region CompressedFormats

    //Compressed texture formats.
    //All are based on "block compression", where 4x4 blocks of pixels
    //    are intelligently compressed together.
    BETTER_ENUM(CompressedFormats, GLenum,
        //BC4 compression, with one color channel and a value range from 0 - 1.
        Greyscale_NormalizedUInt = GL_COMPRESSED_RED_RGTC1,
        //BC4 compression, with one color channel and a value range from -1 to 1.
        Greyscale_NormalizedInt = GL_COMPRESSED_SIGNED_RED_RGTC1,
                
        //BC5 compression, with two color channels and values range from 0 - 1.
        RG_NormalizedUInt = GL_COMPRESSED_RG_RGTC2,
        //BC5 compression, with two color channels and values range from 0 - 1.
        RG_NormalizedInt = GL_COMPRESSED_SIGNED_RG_RGTC2,

        //BC6 compression, with RGB color channels and floating-point values.
        RGB_Float = GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT,
        //BC6 compression, with RGB color channels and *unsigned* floating-point values.
        RGB_UFloat = GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT,

        //BC7 compression, with RGBA channels and values range from 0 - 1.
        RGBA_NormalizedUInt = GL_COMPRESSED_RGBA_BPTC_UNORM,
        //BC7 compression, with RGBA channels and sRGB values ranging from 0 - 1.
        //"sRGB" meaning that the values get converted from sRGB space to linear space when sampled.
        RGBA_sRGB_NormalizedUInt = GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM
    );

    #pragma endregion
    
    #pragma region DepthStencilFormats

    //Formats for depth and/or stencil textures.
    BETTER_ENUM(DepthStencilFormats, GLenum,
        //Depth texture with unsigned 16-bit data.
        Depth_16U = GL_DEPTH_COMPONENT16,
        //Depth texture with unsigned 24-bit data.
        Depth_24U = GL_DEPTH_COMPONENT24,
        //Depth texture with unsigned 32-bit data.
        Depth_32U = GL_DEPTH_COMPONENT32,
        //Depth texture with floating-point 32-bit data.
        Depth_32F = GL_DEPTH_COMPONENT32F,

        //Stencil texture with unsigned 8-bit data.
        //Note that other sizes exist for stencil textures,
        //    but the OpenGL wiki strongly advises against using them.
        Stencil_8 = GL_STENCIL_INDEX8,

        //Hybrid Depth/Stencil texture with unsigned 24-bit depth
        //    and unsigned 8-bit stencil.
        Depth24U_Stencil8 = GL_DEPTH24_STENCIL8,
        //Hybrid Depth/Stencil texture with floating-point 32-bit depth
        //    and unsigned 8-bit stencil.
        Depth32F_Stencil8 = GL_DEPTH32F_STENCIL8
    );
    
    #pragma endregion


    //The pixel format a texture can be stored in.
    struct BP_API Format
    {
    public:

        Format(SimpleFormat format) : data(format) { }
        Format(SpecialFormats format) : data(format) { };
        Format(CompressedFormats format) : data(format) { };
        Format(DepthStencilFormats format) : data(format) { };


        //Gets whether this is a "simple" format (i.e. uniform channel size, uncompressed, etc).
        bool IsSimple() const { return std::holds_alternative<SimpleFormat>(data); }
        //Gets whether this is a block-compressed format.
        bool IsCompressed() const { return std::holds_alternative<CompressedFormats>(data); }
        //Gets whether this format represents any kind of depth/stencil type.
        bool IsDepthStencil() const { return std::holds_alternative<DepthStencilFormats>(data); }

        //Gets whether this format represents a depth/stencil hybrid type.
        bool IsDepthAndStencil() const;
        //Gets whether this format represents a depth type (with NO stencil).
        bool IsDepthOnly() const;
        //Gets whether this format represents a stencil type (no depth).
        bool IsStencilOnly() const;


        //Gets whether this format stores the given channel.
        bool StoresChannel(AllChannels c) const;

        //Gets the number of bits for each channel in this format.
        //If a channel isn't given, assumes the channels are all the same bit-size.
        //If a channel is given and it isn't stored in this texture, returns 0.
        //If the format is compressed, it'll return a vague-but-precise value
        //    based on the compression scheme.
        uint_fast8_t GetChannelBitSize(std::optional<AllChannels> channel = {}) const;

        //Gets the number of bits for each pixel in this format.
        //If the format is compressed, it'll return a vague-but-precise value
        //    based on the compression scheme.
        uint_fast8_t GetPixelBitSize() const;
        

        //Gets the OpenGL enum value representing this format.
        GLenum GetOglEnum() const;


    private:

        using TypeUnion = std::variant<SimpleFormat, SpecialFormats,
                                       CompressedFormats, DepthStencilFormats>;
        TypeUnion data;
    };
}