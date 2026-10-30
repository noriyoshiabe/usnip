#include "Keychain.h"

#include "core/Crypto.h"

#import <Foundation/Foundation.h>
#import <Security/Security.h>

namespace usnip::platform {

namespace {

NSString *const kService = @"io.github.noriyoshiabe.usnip";
NSString *const kAccount = @"master-key";

NSMutableDictionary *baseQuery()
{
    return [@{
        (__bridge id)kSecClass : (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService : kService,
        (__bridge id)kSecAttrAccount : kAccount,
    } mutableCopy];
}

} // namespace

std::optional<SecureBytes> loadOrCreateMasterKey()
{
    @autoreleasepool {
        NSMutableDictionary *query = baseQuery();
        query[(__bridge id)kSecReturnData] = @YES;
        query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;

        CFTypeRef result = nullptr;
        const OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);
        if (status == errSecSuccess) {
            NSData *data = CFBridgingRelease(result);
            if (data.length != crypto::kKeySize) {
                return std::nullopt;
            }
            return SecureBytes(static_cast<const std::uint8_t *>(data.bytes), data.length);
        }
        if (status != errSecItemNotFound) {
            return std::nullopt;
        }

        // Items in the login keychain are not synced to iCloud. The default access list
        // trusts only the app that created the item, identified by its code signature.
        SecureBytes key = crypto::randomBytes(crypto::kKeySize);
        NSMutableDictionary *attributes = baseQuery();
        attributes[(__bridge id)kSecAttrLabel] = @"μSnip master key";
        attributes[(__bridge id)kSecValueData] = [NSData dataWithBytesNoCopy:key.data()
                                                                      length:key.size()
                                                                freeWhenDone:NO];
        if (SecItemAdd((__bridge CFDictionaryRef)attributes, nullptr) != errSecSuccess) {
            return std::nullopt;
        }
        return key;
    }
}

} // namespace usnip::platform
