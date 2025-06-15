#pragma once

enum class BinaryEncoderType : uint8_t
{
    /**
     * Named_Binary_Encoder (0x01)
     *
     * Binary layout:
     * --------------------------------------------------------
     * | Offset | Field        | Size    | Description         |
     * |--------|--------------|---------|---------------------|
     * | 0      | message_type | 1 byte  | Always 0x01         |
     * | 1–2    | name_length  | 2 bytes | Big-endian uint16_t |
     * | 3–N    | signal_name  | N bytes | UTF-8, not null-term|
     * | N+1+   | payload      | varies  | Signal value data   |
     * 
     * Notes:
     * - Byte order: All multi-byte fields use **big-endian**.
     * - signal_name: Encoded as raw UTF-8, no null terminator.
     * - payload: Binary blob of the signal's value, e.g., RGB matrix.
     * - Extensible by using new values for `BinaryEncoderType`.
     */
    Named_Binary_Encoder = 1,
    
    /**
     * Timestamped_Int_Vector_Encoder (0x02)
     *
     * Binary layout:
     * ------------------------------------------------------------
     * | Offset | Field        | Size         | Description        |
     * |--------|--------------|--------------|--------------------|
     * | 0      | message_type | 1 byte       | Always 0x02        |
     * | 1–2    | name_length  | 2 bytes      | Big-endian uint16_t|
     * | 3–N    | signal_name  | N bytes      | UTF-8              |
     * | N+1+   | timestamp    | 8 bytes      | Big-endian uint64_t|
     * | N+9+   | vector_len   | 4 bytes      | Big-endian uint32_t|
     * | N+13+  | vector_data  | 4 * len bytes| int32_t values     |
     *
     * Notes:
     * - Timestamp is in microseconds since epoch.
     * - All integers are big-endian.
     */
    Timestamped_Int_Vector_Encoder = 2,
};

template<typename T>
using BinaryEncoder = std::function<std::vector<uint8_t>(const std::string&, const T&)>;
