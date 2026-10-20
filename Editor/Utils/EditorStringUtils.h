#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace NoJob::EditorStringUtils
{
    /**
     * @brief Converts ASCII-compatible text to lowercase for editor searches.
     * @param text Text to normalize.
     * @return Lowercase copy of the input string.
     * @note Uses unsigned char to avoid undefined behavior in std::tolower.
     */
    inline std::string ToLower(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return text;
    }

    /**
     * @brief Performs a case-insensitive substring search for editor filtering.
     * @param text Candidate text.
     * @param normalizedFilter Filter already normalized by ToLower().
     * @return True when the filter is empty or found in the candidate.
     */
    inline bool ContainsNormalized(const std::string& text,
        const std::string& normalizedFilter)
    {
        return normalizedFilter.empty() ||
            ToLower(text).find(normalizedFilter) != std::string::npos;
    }
}
