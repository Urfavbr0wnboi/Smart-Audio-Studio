#pragma once

#include "Project.h"

namespace sas
{

/** Reads and writes .sasproj files.

    A project file is readable JSON that describes the tracks, mixer settings
    and clips. Audio is not copied into it: each clip refers to its audio file,
    by a path relative to the project file when the audio is in the project's
    folder (so the folder can be moved or shared) and by an absolute path otherwise.
*/
namespace ProjectFile
{
    inline const juce::String fileExtension { ".sasproj" };
    inline const juce::String fileWildcard { "*.sasproj" };
    constexpr int currentVersion = 1;

    /** Describes the project as JSON. `projectFile` is where it will be saved,
        which decides how audio paths are written.
    */
    juce::String toJson (const Project&, const ChannelParams& master, const juce::File& projectFile);

    /** Saves the project. The existing file is only replaced once the new one is complete. */
    juce::Result save (const Project&, const ChannelParams& master, const juce::File& projectFile);

    struct LoadReport
    {
        juce::StringArray missingFiles;   // audio files that could not be found or opened
    };

    /** Replaces the project's contents with a saved project. On failure the
        project is left as it was.
    */
    juce::Result load (Project&, ChannelParams& master, const juce::File& projectFile, LoadReport&);
}

} // namespace sas
