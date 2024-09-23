#include "msg/message.hpp"

void msg::write_header(int len, int msg_type, std::vector<unsigned char> &vec)
{
    if (vec.size() < 8)
    {
        throw std::invalid_argument("Vec size for header not enough.");
    }

    util::int32_to_uchar_big_endian(len, vec.data());
    util::int32_to_uchar_big_endian(msg_type, vec.data() + 4);
}

std::pair<int, MessageType> msg::parse_header(unsigned char *header)
{

    return std::make_pair<int, MessageType>(util::chars_to_int32_big_endian(header),
                                            static_cast<MessageType>(util::chars_to_int32_big_endian(header + 4)));
}