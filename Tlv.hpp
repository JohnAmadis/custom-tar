#ifndef TLV_HPP
#define TLV_HPP

#include <string>
#include <cstdint>
#include "compiler.h"

/**
 * @brief Class representing a Type-Length-Value (TLV) structure.
 */
class Tlv
{
public:

    /**
     * @brief Type representing a tag (4-byte identifier).
     */
    using Tag_t = uint32_t;

    /**
     * @brief Structure representing a TLV (Type-Length-Value) entry.
     */
    struct Tag
    {
        Tag_t id;        //!< The tag value

        Tag() : id(0) {}
        Tag( uint32_t v ) : id(v) {}
        Tag( const char v[4] )
        {
            id = (static_cast<uint32_t>(v[0]) << 24) |
                    (static_cast<uint32_t>(v[1]) << 16) |
                    (static_cast<uint32_t>(v[2]) << 8)  |
                    (static_cast<uint32_t>(v[3]));
        }

        bool operator==( const Tag& other ) const { return id == other.id; }
        bool operator!=( const Tag& other ) const { return id != other.id; }
        bool operator<( const Tag& other ) const { return id < other.id; }
        bool operator<=( const Tag& other ) const { return id <= other.id; }
        bool operator>( const Tag& other ) const { return id > other.id; }
        bool operator>=( const Tag& other ) const { return id >= other.id; }
    } PACKED;

    Tlv() = default;
    Tlv( const Tag& tag, const void* data = nullptr, size_t size = 0 );

private:
    Tag_t       m_tag{0};
    const void* m_data{nullptr};
    size_t      m_size{0};
};

#endif // TLV_HPP