#pragma once

#include "../Platform.h"
#include "../Math/Math.h"

#include "BPAssert.h"
#include "Functions.h"

#include <string>

namespace Bplus::Strings
{
    //Converts a number into a string in base-2.
    template<typename UInt_t>
    std::string ToBinaryString(UInt_t value,
                               bool removeLeadingZeroes = true,
                               std::string prefix = "")
    {
        //Allocate the result, and initialize it with the prefix.
        std::string result = prefix;
        result.reserve(result.size() + (sizeof(UInt_t) * 8));

        //Get each bit from each byte and append its value into the string.
        const std::byte* valueBytes = (const std::byte*)&value;
        constexpr size_t nBytes = sizeof(UInt_t);
        for (size_t byteI = 0; byteI < nBytes; ++byteI)
        {
            auto byte = (uint_fast8_t)valueBytes[IsPlatformLittleEndian() ?
                                                     (nBytes - 1 - byteI) :
                                                     byteI];

            for (int_fast8_t bitI = 7; bitI >= 0; --bitI)
            {
                auto bit = (byte >> bitI) & 1;
                BPAssert(bit == 0 || bit == 1, "Bit is not a bit");
                result += (bit == 0) ? '0' : '1';
            }
        }

        if (removeLeadingZeroes)
        {
            //Make sure not to remove the very last digit,
            //    in case the total value is 0!
            const size_t charI = prefix.size();
            while (charI < result.size() - 1 && result[charI] == '0')
                result.erase(charI, 1);
        }

        return result;
    }


    enum class NumberBases { Decimal = 10, Octal = 8, Hex = 16, Binary = 2 };

    template<typename Int_t>
    std::string ToBaseString(Int_t value, NumberBases base,
                             std::string prefix = "")
    {
        //Handle some easy cases first:
        if (base == NumberBases::Decimal)
            return prefix + std::to_string(value);
        if (base == NumberBases::Binary)
            return ToBinaryString(value, true, prefix.c_str());

        //Otherwise, we're going to use snprintf to write it into the string.
        const char* formatStr = "ERROR";
        switch (base)
        {
            case NumberBases::Hex: formatStr = "%X"; break;
            case NumberBases::Octal: formatStr = "%o"; break;
            default: BPAssert(false, "Unknown NumberBases"); break;
        }

        //Start with the prefix.
        std::string result = prefix;

        //Estimate the number of digits needed to contain the number.
        auto nDigits = 1 + (uint_fast8_t)Math::Log((double)value, (double)base);
        //To be safe in the face of floating-point error, add 1 to the "correct" result.
        nDigits += 1;

        //Insert the rest of the data.
        size_t startI = result.size();
        result.resize(result.size() + nDigits);
        size_t nActualDigits = snprintf(&result[startI], nDigits + 1,
                                        formatStr, value);
        BPAssert(nActualDigits <= nDigits, "nDigits estimate is too small in ToBaseString()");
        BPAssert(nActualDigits > 0, "error using snprintf() in ToBaseString()");

        //Remove the empty characters at the end of the buffer.
        size_t nWastedDigits = nDigits - nActualDigits;
        result.resize(result.size() - nWastedDigits);
        
        return result;
    }

    bool BP_API StartsWith(const std::string& str, const std::string& snippet);
    bool BP_API EndsWith(const std::string& str, const std::string& snippet);

    void BP_API Replace(std::string& str,
                        const std::string& snippet, const std::string& replacedWith);
}