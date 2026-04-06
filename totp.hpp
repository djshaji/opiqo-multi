/**
 * totp.hpp — TOTP / HOTP serial-key verification for the opiqo PC version.
 *
 * The Android app generates a 6-digit TOTP code with:
 *   secret  = BASE32_SECRET  (shared, embedded in both apps)
 *   step    = 30 s           (standard RFC 6238 — same as Microsoft/Google Authenticator)
 *   digits  = 6
 *
 * The PC app calls verifyTotp(userCode) to accept or reject the code.
 * A ±1-step skew window (±30 s) is accepted to tolerate clock drift.
 *
 * Requires Windows BCrypt (bcrypt.lib / bcrypt.dll — ships with Windows 7+).
 */

#pragma once

#include <windows.h>
#include <bcrypt.h>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

// ── Shared secret ────────────────────────────────────────────────────────────
// Must match the Base32 key used in SettingsActivity.getSerialKey().
static constexpr const char* BASE32_SECRET = "JBSWY3DPEHPK3PXP";

// ── TOTP parameters ──────────────────────────────────────────────────────────
static constexpr int TOTP_STEP   = 30;    // standard RFC 6238 step (seconds)
static constexpr int TOTP_DIGITS = 6;     // output code length

namespace totp {

/**
 * Decode a Base32-encoded secret (RFC 4648, uppercase, padding optional).
 */
inline std::vector<uint8_t> base32Decode(const char* s) {
    static const char kAlpha[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::vector<uint8_t> out;
    int buf = 0, bits = 0;
    for (; *s; ++s) {
        if (*s == '=') break;
        const char* p = strchr(kAlpha, toupper(static_cast<unsigned char>(*s)));
        if (!p) continue;
        buf = (buf << 5) | static_cast<int>(p - kAlpha);
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

/**
 * Compute one HOTP code for the given key and 8-byte big-endian counter.
 * Uses Windows BCrypt for HMAC-SHA1 (RFC 4226).
 */
inline uint32_t hotp(const uint8_t* key, size_t keyLen,
                     uint64_t counter, int digits = TOTP_DIGITS) {
    // Pack counter as big-endian 8 bytes.
    uint8_t msg[8];
    uint64_t tmp = counter;
    for (int i = 7; i >= 0; --i) {
        msg[i] = static_cast<uint8_t>(tmp & 0xFF);
        tmp >>= 8;
    }

    // HMAC-SHA1 via Windows BCrypt.
    BCRYPT_ALG_HANDLE  alg  = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    uint8_t digest[20] = {};

    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr,
                                BCRYPT_ALG_HANDLE_HMAC_FLAG);
    BCryptCreateHash(alg, &hash, nullptr, 0,
                     const_cast<PUCHAR>(key), static_cast<ULONG>(keyLen), 0);
    BCryptHashData(hash, msg, 8, 0);
    BCryptFinishHash(hash, digest, 20, 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);

    // RFC 4226 dynamic truncation.
    int offset = digest[19] & 0x0F;
    uint32_t code = ((digest[offset]     & 0x7Fu) << 24)
                  | ((digest[offset + 1] & 0xFFu) << 16)
                  | ((digest[offset + 2] & 0xFFu) <<  8)
                  |  (digest[offset + 3] & 0xFFu);

    static const uint32_t kPow10[] = {
        1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u
    };
    return code % kPow10[digits];
}

/**
 * Compute a TOTP code from a Base32 secret.
 *
 * @param base32Secret  RFC 4648 Base32 encoded shared secret.
 * @param step          Time-step in seconds (default: TOTP_STEP = 600).
 * @param digits        Output code length  (default: TOTP_DIGITS = 6).
 * @param skew          Counter offset      (0 = now, ±1 = adjacent windows).
 */
inline uint32_t compute(const char* base32Secret,
                        int step   = TOTP_STEP,
                        int digits = TOTP_DIGITS,
                        int skew   = 0) {
    // Current Unix time via Windows FILETIME.
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t unixSec = (((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime)
                       / 10000000ULL - 11644473600ULL;

    uint64_t counter = static_cast<uint64_t>(
        static_cast<int64_t>(unixSec / step) + skew);

    auto key = base32Decode(base32Secret);
    return hotp(key.data(), key.size(), counter, digits);
}

/**
 * Verify a user-supplied code against the current step and ±1 adjacent
 * windows to tolerate clock skew of up to one full step (±10 minutes).
 *
 * @param base32Secret  RFC 4648 Base32 encoded shared secret.
 * @param userCode      6-digit numeric code entered by the user.
 * @param step          Time-step in seconds (default: TOTP_STEP = 600).
 * @param digits        Expected code length (default: TOTP_DIGITS = 6).
 * @return true if the code matches any of the three accepted windows.
 */
inline bool verify(const char* base32Secret, uint32_t userCode,
                   int step   = TOTP_STEP,
                   int digits = TOTP_DIGITS) {
    for (int skew = -1; skew <= 1; ++skew) {
        if (compute(base32Secret, step, digits, skew) == userCode)
            return true;
    }
    return false;
}

} // namespace totp

/**
 * Top-level helper — call this from your license-check code.
 *
 * Usage:
 *   uint32_t code = readCodeFromUser();   // e.g. atoi(inputField.text)
 *   if (verifyTotp(code)) activateLicense();
 */
inline bool verifyTotp(uint32_t userCode) {
    return totp::verify(BASE32_SECRET, userCode);
}

