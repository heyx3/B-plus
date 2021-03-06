#pragma once

#include <random>

#define TEST_NO_MAIN
#include <acutest.h>

#include <B+/GL/Textures/Target.h>
#include "../SimpleApp.hpp"

using namespace Bplus::GL::Textures;


void TestTargetBasic()
{
    Simple::RunTest([&]()
    {
        TEST_CASE("Creating textures");
        Texture2D tColor(glm::uvec2{ 25, 455 },
                         SimpleFormat{ FormatTypes::NormalizedUInt,
                                       SimpleFormatComponents::RGBA,
                                       SimpleFormatBitDepths::B8 });
        Texture2D tDepth(glm::uvec2{ 25, 455 },
                         Format{ DepthStencilFormats::Depth_32F });

        TargetStates targetStatus;
        TEST_CASE("Creating target");
        Target target(targetStatus, &tColor, &tDepth);
        TEST_CHECK_(targetStatus == +TargetStates::Ready,
                    "Target isn't usable: %s", targetStatus._to_string());
        
        TEST_CASE("Clearing target");
        const glm::vec4 clearColor{ 0.45, 0.8f, 1.0f, 0.25f };
        const float clearDepth = 0.5f;
        target.ClearColor(clearColor);
        target.ClearDepth(clearDepth);

        TEST_CASE("Reading cleared color value");
        glm::vec4 colorPixel{ -9999.0f };
        tColor.Get_Color(&colorPixel, false,
                         Bplus::GL::Textures::GetData2DParams(
                             Bplus::Math::Box2Du::MakeSize(glm::uvec2{ 1 })));
        const float colorEpsilon = 0.001f;
        TEST_CHECK_(glm::all(glm::equal(colorPixel, clearColor, colorEpsilon)),
                    "Actual color %s doesn't match expected color %s within epsilon %f",
                    glm::to_string(colorPixel).c_str(),
                    glm::to_string(clearColor).c_str(),
                    colorEpsilon);

        TEST_CASE("Reading cleared depth value");
        float depthPixel = -999.0f;
        tDepth.Get_Depth(&depthPixel,
                         Bplus::GL::Textures::GetData2DParams(
                             Bplus::Math::Box2Du::MakeSize(glm::uvec2{ 1 })));
        TEST_CHECK_(depthPixel == clearDepth,
                    "Actual depth %f doesn't exactly match expected depth %f",
                    depthPixel, clearDepth);
    });
}

//TODO: Test layered Targets
//TODO: Test cubemap Targets
//TODO: Test 3D texture Targets

//TODO: Test rendering to targets