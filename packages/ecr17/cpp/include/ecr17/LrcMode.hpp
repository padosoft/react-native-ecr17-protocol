#pragma once

namespace padosoft::ecr17 {

/// Which framing bytes the LRC folds in (terminals differ: see Lrc::compute).
/// The React Native binding maps the JS union `"stx" | "std" | "noext" | "stx_noext"`
/// (nitrogen's generated `LrcMode`) onto this enum; member names match it.
enum class LrcMode {
    STX,
    STD,
    NOEXT,
    STX_NOEXT,
};

}  // namespace padosoft::ecr17
