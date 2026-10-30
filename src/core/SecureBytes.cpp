#include "SecureBytes.h"

#include <algorithm>

namespace usnip {

void secureWipe(void *data, std::size_t size)
{
    // Writes through a volatile pointer are not removed by the optimizer.
    volatile auto *p = static_cast<volatile std::uint8_t *>(data);
    for (std::size_t i = 0; i < size; ++i) {
        p[i] = 0;
    }
}

SecureBytes::SecureBytes(std::size_t size) : m_bytes(size, 0)
{
}

SecureBytes::SecureBytes(const std::uint8_t *data, std::size_t size) : m_bytes(data, data + size)
{
}

SecureBytes::SecureBytes(std::span<const std::uint8_t> bytes) : m_bytes(bytes.begin(), bytes.end())
{
}

SecureBytes SecureBytes::fromUtf8(std::string_view text)
{
    return SecureBytes(reinterpret_cast<const std::uint8_t *>(text.data()), text.size());
}

SecureBytes::SecureBytes(const SecureBytes &other) : m_bytes(other.m_bytes)
{
}

SecureBytes &SecureBytes::operator=(const SecureBytes &other)
{
    if (this != &other) {
        clear();
        m_bytes = other.m_bytes;
    }
    return *this;
}

SecureBytes::SecureBytes(SecureBytes &&other) noexcept : m_bytes(std::move(other.m_bytes))
{
    other.m_bytes.clear();
}

SecureBytes &SecureBytes::operator=(SecureBytes &&other) noexcept
{
    if (this != &other) {
        clear();
        m_bytes = std::move(other.m_bytes);
        other.m_bytes.clear();
    }
    return *this;
}

SecureBytes::~SecureBytes()
{
    clear();
}

void SecureBytes::truncate(std::size_t newSize)
{
    if (newSize >= m_bytes.size()) {
        return;
    }
    secureWipe(m_bytes.data() + newSize, m_bytes.size() - newSize);
    m_bytes.resize(newSize);
}

void SecureBytes::clear()
{
    secureWipe(m_bytes.data(), m_bytes.size());
    m_bytes.clear();
    m_bytes.shrink_to_fit();
}

} // namespace usnip
