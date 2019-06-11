#include <algorithm>
#include <cassert>
#include <cctype>
#include <ciso646>
#include <cstdint>
#include <cstring>

#include <endianness/endianness.h>

#include <ecl3/keyword.h>

namespace {

void memcpy_bswap32(void* d, const void* s, std::size_t nmemb) noexcept (true) {
    std::uint32_t tmp;
    auto* dst = reinterpret_cast< char* >(d);
    auto* src = reinterpret_cast< const char* >(s);
    for (std::size_t i = 0; i < nmemb; ++i) {
        std::memcpy(&tmp, src, sizeof(tmp));
        tmp = bswap32(tmp);
        std::memcpy(dst, &tmp, sizeof(tmp));
        dst += sizeof(tmp);
        src += sizeof(tmp);
    }
}

void memcpy_bswap64(void* d, const void* s, std::size_t nmemb) noexcept (true) {
    std::uint64_t tmp;
    auto* dst = reinterpret_cast< char* >(d);
    auto* src = reinterpret_cast< const char* >(s);
    for (std::size_t i = 0; i < nmemb; ++i) {
        std::memcpy(&tmp, src, sizeof(tmp));
        tmp = bswap64(tmp);
        std::memcpy(dst, &tmp, sizeof(tmp));
        dst += sizeof(tmp);
        src += sizeof(tmp);
    }
}

#if defined(__BIG_ENDIAN__)
void memcpy_msb32(void* dst, const void* src, std::size_t nmemb) noexcept (true) {
    if (dst == src) return;
    std::memcpy(dst, src, nmemb * sizeof(std::uint32_t));
}

void memcpy_msb64(void* dst, const void* src, std::size_t nmemb) noexcept (true) {
    if (dst == src) return;
    std::memcpy(dst, src, nmemb * sizeof(std::uint64_t));
}

#elif defined(__LITTLE_ENDIAN__)
void memcpy_msb32(void* dst, const void* src, std::size_t nmemb) noexcept (true) {
    memcpy_bswap32(dst, src, nmemb);
}

void memcpy_msb64(void* dst, const void* src, std::size_t nmemb) noexcept (true) {
    memcpy_bswap64(dst, src, nmemb);
}

#else
    #error "__BIG_ENDIAN__ or __LITTLE_ENDIAN__ must be set"
#endif

}

int ecl3_get_native(void* dst, const void* src, int fmt, std::size_t elems) {
    switch (fmt) {
        case ECL3_INTE:
        case ECL3_REAL:
        case ECL3_LOGI:
            memcpy_msb32(dst, src, elems);
            return ECL3_OK;

        case ECL3_DOUB:
            memcpy_msb64(dst, src, elems);
            return ECL3_OK;

        case ECL3_MESS:
            return ECL3_OK;

        default:
            break;
    }

    int size;
    const int err = ecl3_type_size(fmt, &size);
    if (err) return err;

    std::memcpy(dst, src, elems * size);
    return ECL3_OK;
}

int ecl3_put_native(void* dst, const void* src, int fmt, std::size_t elems) {
    /*
     * get/put native are currently the same, because there is no little-endian
     * input support. They both exist for symmetry.
     */
    return ecl3_get_native(dst, src, fmt, elems);
}

int ecl3_array_header_size() {
    /*
     * Described in the manual to be 16 bytes long
     */
    return 16;
}

int ecl3_array_header(const void* source, char* kw, char* type, int* count) {
    const auto* src = reinterpret_cast< const char* >(source);

    std::memcpy(kw, src, 8);
    std::uint32_t tmp;
    ecl3_get_native(&tmp, src + 8, ECL3_INTE, 1);
    std::memcpy(type, src + 12, 4);
    *count = tmp;
    return ECL3_OK;
}

namespace {

constexpr bool isC0NN(int type) noexcept (true) {
    return (type & 0xFFFF0000) == (int('C') << 24 | int('0') << 16);
}

}

int ecl3_block_size(int type, int* size) {
    switch (type) {
        case ECL3_INTE:
        case ECL3_REAL:
        case ECL3_DOUB:
        case ECL3_MESS:
        case ECL3_LOGI:
        case ECL3_X231:
            *size = ECL3_BLOCK_SIZE_NUMERIC;
            return ECL3_OK;

        case ECL3_CHAR:
            *size = ECL3_BLOCK_SIZE_STRING;
            return ECL3_OK;

        default:
            break;
    }

    if (!isC0NN(type))
        return ECL3_INVALID_ARGS;

    *size = ECL3_BLOCK_SIZE_STRING;
    return ECL3_OK;

}

int ecl3_array_body(void* dst,
                    const void* src,
                    int type,
                    int elems,
                    int block_size,
                    int* count) {

    switch (type) {
        case ECL3_MESS:
        case ECL3_X231:
            return ECL3_UNSUPPORTED;

        default:
            break;
    }

    elems = std::min(elems, block_size);
    ecl3_get_native(dst, src, type, elems);
    *count = elems;
    return ECL3_OK;
}

int ecl3_typeid(const char* str, int* type) {
    static_assert(
        sizeof(int) == sizeof(std::int32_t),
        "This function relies on 4-byte int. "
        "Implement a fallback for your architecture"
    );

    std::int32_t kw;
    std::memcpy(&kw, str, sizeof(kw));
    kw = be32toh(kw);
    /*
     * The enum values correspond to the 4 bytes that make up the ascii
     * representation of the type specifier. This means the enum now carries
     * both size information (by masking the lower 2 bytes of C0NN, implicitly
     * otherwise) and covers the total set of accepted types.
     *
     * This makes for a very fast & simple implementation, and makes the
     * knowledge easy to carry around
     */
    switch (kw) {
        case ECL3_INTE:
        case ECL3_REAL:
        case ECL3_DOUB:
        case ECL3_CHAR:
        case ECL3_MESS:
        case ECL3_LOGI:
        case ECL3_X231:

        case ECL3_C001:
        case ECL3_C002:
        case ECL3_C003:
        case ECL3_C004:
        case ECL3_C005:
        case ECL3_C006:
        case ECL3_C007:
        case ECL3_C008:
        case ECL3_C009:
        case ECL3_C010:
        case ECL3_C011:
        case ECL3_C012:
        case ECL3_C013:
        case ECL3_C014:
        case ECL3_C015:
        case ECL3_C016:
        case ECL3_C017:
        case ECL3_C018:
        case ECL3_C019:
        case ECL3_C020:
        case ECL3_C021:
        case ECL3_C022:
        case ECL3_C023:
        case ECL3_C024:
        case ECL3_C025:
        case ECL3_C026:
        case ECL3_C027:
        case ECL3_C028:
        case ECL3_C029:
        case ECL3_C030:
        case ECL3_C031:
        case ECL3_C032:
        case ECL3_C033:
        case ECL3_C034:
        case ECL3_C035:
        case ECL3_C036:
        case ECL3_C037:
        case ECL3_C038:
        case ECL3_C039:
        case ECL3_C040:
        case ECL3_C041:
        case ECL3_C042:
        case ECL3_C043:
        case ECL3_C044:
        case ECL3_C045:
        case ECL3_C046:
        case ECL3_C047:
        case ECL3_C048:
        case ECL3_C049:
        case ECL3_C050:
        case ECL3_C051:
        case ECL3_C052:
        case ECL3_C053:
        case ECL3_C054:
        case ECL3_C055:
        case ECL3_C056:
        case ECL3_C057:
        case ECL3_C058:
        case ECL3_C059:
        case ECL3_C060:
        case ECL3_C061:
        case ECL3_C062:
        case ECL3_C063:
        case ECL3_C064:
        case ECL3_C065:
        case ECL3_C066:
        case ECL3_C067:
        case ECL3_C068:
        case ECL3_C069:
        case ECL3_C070:
        case ECL3_C071:
        case ECL3_C072:
        case ECL3_C073:
        case ECL3_C074:
        case ECL3_C075:
        case ECL3_C076:
        case ECL3_C077:
        case ECL3_C078:
        case ECL3_C079:
        case ECL3_C080:
        case ECL3_C081:
        case ECL3_C082:
        case ECL3_C083:
        case ECL3_C084:
        case ECL3_C085:
        case ECL3_C086:
        case ECL3_C087:
        case ECL3_C088:
        case ECL3_C089:
        case ECL3_C090:
        case ECL3_C091:
        case ECL3_C092:
        case ECL3_C093:
        case ECL3_C094:
        case ECL3_C095:
        case ECL3_C096:
        case ECL3_C097:
        case ECL3_C098:
        case ECL3_C099:
            *type = kw;
            return ECL3_OK;

        default:
            return ECL3_INVALID_ARGS;
    }
}

int ecl3_type_size(int type, int* size) {

    switch (type) {
        case ECL3_INTE:
        case ECL3_REAL:
        case ECL3_LOGI:
            *size = 4;
            return ECL3_OK;

        case ECL3_CHAR:
        case ECL3_DOUB:
            *size = 8;
            return ECL3_OK;

        case ECL3_MESS:
            *size = 0;
            return ECL3_OK;

        case ECL3_X231:
            return ECL3_UNSUPPORTED;

        case ECL3_C001:
        case ECL3_C002:
        case ECL3_C003:
        case ECL3_C004:
        case ECL3_C005:
        case ECL3_C006:
        case ECL3_C007:
        case ECL3_C008:
        case ECL3_C009:
        case ECL3_C010:
        case ECL3_C011:
        case ECL3_C012:
        case ECL3_C013:
        case ECL3_C014:
        case ECL3_C015:
        case ECL3_C016:
        case ECL3_C017:
        case ECL3_C018:
        case ECL3_C019:
        case ECL3_C020:
        case ECL3_C021:
        case ECL3_C022:
        case ECL3_C023:
        case ECL3_C024:
        case ECL3_C025:
        case ECL3_C026:
        case ECL3_C027:
        case ECL3_C028:
        case ECL3_C029:
        case ECL3_C030:
        case ECL3_C031:
        case ECL3_C032:
        case ECL3_C033:
        case ECL3_C034:
        case ECL3_C035:
        case ECL3_C036:
        case ECL3_C037:
        case ECL3_C038:
        case ECL3_C039:
        case ECL3_C040:
        case ECL3_C041:
        case ECL3_C042:
        case ECL3_C043:
        case ECL3_C044:
        case ECL3_C045:
        case ECL3_C046:
        case ECL3_C047:
        case ECL3_C048:
        case ECL3_C049:
        case ECL3_C050:
        case ECL3_C051:
        case ECL3_C052:
        case ECL3_C053:
        case ECL3_C054:
        case ECL3_C055:
        case ECL3_C056:
        case ECL3_C057:
        case ECL3_C058:
        case ECL3_C059:
        case ECL3_C060:
        case ECL3_C061:
        case ECL3_C062:
        case ECL3_C063:
        case ECL3_C064:
        case ECL3_C065:
        case ECL3_C066:
        case ECL3_C067:
        case ECL3_C068:
        case ECL3_C069:
        case ECL3_C070:
        case ECL3_C071:
        case ECL3_C072:
        case ECL3_C073:
        case ECL3_C074:
        case ECL3_C075:
        case ECL3_C076:
        case ECL3_C077:
        case ECL3_C078:
        case ECL3_C079:
        case ECL3_C080:
        case ECL3_C081:
        case ECL3_C082:
        case ECL3_C083:
        case ECL3_C084:
        case ECL3_C085:
        case ECL3_C086:
        case ECL3_C087:
        case ECL3_C088:
        case ECL3_C089:
        case ECL3_C090:
        case ECL3_C091:
        case ECL3_C092:
        case ECL3_C093:
        case ECL3_C094:
        case ECL3_C095:
        case ECL3_C096:
        case ECL3_C097:
        case ECL3_C098:
        case ECL3_C099:
            break;

        default:
            return ECL3_INVALID_ARGS;
    }

    /*
     * C0NN - the length is encoded in the enum at the two lower bytes
     */
    static_assert(
        '0' == 48,
        "this function assumes ASCII-compatible encoding of chars - "
        "address the ecl3_typeids enum to be compatible for your charset"
    );

    /*
     * Example for C001 (on little-endian):
     * On disk representation: 67 48 48 49
     * This is also the byte-representation of the ECL3_C001 enum
     *
     * Pre shift:
     *  high = 00 00 48 00
     *  low  = 00 00 00 49
     *
     * Post shift:
     *  high = 00 00 00 48
     *  low  = 00 00 00 49
     */
    const auto high = ((type & 0x0000FF00) >> 8) - '0';
    const auto low  = ((type & 0x000000FF) >> 0) - '0';

    *size = high * 10 + low;
    return ECL3_OK;
}

const char* ecl3_type_name(int type) {
    switch (type) {
        case ECL3_INTE: return "INTE";
        case ECL3_REAL: return "REAL";
        case ECL3_DOUB: return "DOUB";
        case ECL3_CHAR: return "CHAR";
        case ECL3_MESS: return "MESS";
        case ECL3_LOGI: return "LOGI";
        case ECL3_X231: return "X231";

        case ECL3_C001: return "C001";
        case ECL3_C002: return "C002";
        case ECL3_C003: return "C003";
        case ECL3_C004: return "C004";
        case ECL3_C005: return "C005";
        case ECL3_C006: return "C006";
        case ECL3_C007: return "C007";
        case ECL3_C008: return "C008";
        case ECL3_C009: return "C009";
        case ECL3_C010: return "C010";
        case ECL3_C011: return "C011";
        case ECL3_C012: return "C012";
        case ECL3_C013: return "C013";
        case ECL3_C014: return "C014";
        case ECL3_C015: return "C015";
        case ECL3_C016: return "C016";
        case ECL3_C017: return "C017";
        case ECL3_C018: return "C018";
        case ECL3_C019: return "C019";
        case ECL3_C020: return "C020";
        case ECL3_C021: return "C021";
        case ECL3_C022: return "C022";
        case ECL3_C023: return "C023";
        case ECL3_C024: return "C024";
        case ECL3_C025: return "C025";
        case ECL3_C026: return "C026";
        case ECL3_C027: return "C027";
        case ECL3_C028: return "C028";
        case ECL3_C029: return "C029";
        case ECL3_C030: return "C030";
        case ECL3_C031: return "C031";
        case ECL3_C032: return "C032";
        case ECL3_C033: return "C033";
        case ECL3_C034: return "C034";
        case ECL3_C035: return "C035";
        case ECL3_C036: return "C036";
        case ECL3_C037: return "C037";
        case ECL3_C038: return "C038";
        case ECL3_C039: return "C039";
        case ECL3_C040: return "C040";
        case ECL3_C041: return "C041";
        case ECL3_C042: return "C042";
        case ECL3_C043: return "C043";
        case ECL3_C044: return "C044";
        case ECL3_C045: return "C045";
        case ECL3_C046: return "C046";
        case ECL3_C047: return "C047";
        case ECL3_C048: return "C048";
        case ECL3_C049: return "C049";
        case ECL3_C050: return "C050";
        case ECL3_C051: return "C051";
        case ECL3_C052: return "C052";
        case ECL3_C053: return "C053";
        case ECL3_C054: return "C054";
        case ECL3_C055: return "C055";
        case ECL3_C056: return "C056";
        case ECL3_C057: return "C057";
        case ECL3_C058: return "C058";
        case ECL3_C059: return "C059";
        case ECL3_C060: return "C060";
        case ECL3_C061: return "C061";
        case ECL3_C062: return "C062";
        case ECL3_C063: return "C063";
        case ECL3_C064: return "C064";
        case ECL3_C065: return "C065";
        case ECL3_C066: return "C066";
        case ECL3_C067: return "C067";
        case ECL3_C068: return "C068";
        case ECL3_C069: return "C069";
        case ECL3_C070: return "C070";
        case ECL3_C071: return "C071";
        case ECL3_C072: return "C072";
        case ECL3_C073: return "C073";
        case ECL3_C074: return "C074";
        case ECL3_C075: return "C075";
        case ECL3_C076: return "C076";
        case ECL3_C077: return "C077";
        case ECL3_C078: return "C078";
        case ECL3_C079: return "C079";
        case ECL3_C080: return "C080";
        case ECL3_C081: return "C081";
        case ECL3_C082: return "C082";
        case ECL3_C083: return "C083";
        case ECL3_C084: return "C084";
        case ECL3_C085: return "C085";
        case ECL3_C086: return "C086";
        case ECL3_C087: return "C087";
        case ECL3_C088: return "C088";
        case ECL3_C089: return "C089";
        case ECL3_C090: return "C090";
        case ECL3_C091: return "C091";
        case ECL3_C092: return "C092";
        case ECL3_C093: return "C093";
        case ECL3_C094: return "C094";
        case ECL3_C095: return "C095";
        case ECL3_C096: return "C096";
        case ECL3_C097: return "C097";
        case ECL3_C098: return "C098";
        case ECL3_C099: return "C099";

        default:
            assert(false && "unknown type");
            return nullptr;
    }
}
