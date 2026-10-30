#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace usnip {

// Byte buffer for secret material (keys, decrypted snippet bodies).
// The storage is wiped on destruction, on clear() and before being overwritten.
// It never grows after construction, so no stale copies are left behind by reallocation.
class SecureBytes {
public:
    SecureBytes() = default;
    explicit SecureBytes(std::size_t size);
    SecureBytes(const std::uint8_t *data, std::size_t size);
    explicit SecureBytes(std::span<const std::uint8_t> bytes);
    static SecureBytes fromUtf8(std::string_view text);

    SecureBytes(const SecureBytes &other);
    SecureBytes &operator=(const SecureBytes &other);
    SecureBytes(SecureBytes &&other) noexcept;
    SecureBytes &operator=(SecureBytes &&other) noexcept;
    ~SecureBytes();

    std::uint8_t *data()
    {
        return m_bytes.data();
    }
    const std::uint8_t *data() const
    {
        return m_bytes.data();
    }
    std::size_t size() const
    {
        return m_bytes.size();
    }
    bool empty() const
    {
        return m_bytes.empty();
    }
    std::span<const std::uint8_t> view() const
    {
        return m_bytes;
    }

    // Shrinks to newSize, wiping the dropped tail. Never reallocates.
    void truncate(std::size_t newSize);
    void clear();

    friend bool operator==(const SecureBytes &a, const SecureBytes &b)
    {
        return a.m_bytes == b.m_bytes;
    }

private:
    std::vector<std::uint8_t> m_bytes;
};

void secureWipe(void *data, std::size_t size);

} // namespace usnip
