#include "codec/frame_parser.hpp"

namespace slipstream::codec {

std::size_t FrameParser::buffered() const noexcept { return end_ - begin_; }

}  // namespace slipstream::codec
