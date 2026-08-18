#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <random>

namespace ExportPackageCrypto {
	inline const std::array<uint8_t, 16> kBaseKey = {
		0x4B, 0x1D, 0x9A, 0x77, 0xE2, 0x03, 0xBC, 0x5F,
		0x88, 0x2C, 0x71, 0xF4, 0x0E, 0x9B, 0x36, 0xA5
	};

	using Salt = std::array<uint8_t, 16>;

	inline Salt GenerateRandomSalt() {
		Salt salt{};
		std::random_device rd;
		for (auto& b : salt) {
			b = static_cast<uint8_t>(rd() & 0xFF);
		}
		return salt;
	}

	inline std::array<uint8_t, 16> DeriveKey(const Salt& salt) {
		std::array<uint8_t, 16> key{};
		for (size_t i = 0; i < key.size(); i++) {
			key[i] = kBaseKey[i] ^ salt[i];
		}
		return key;
	}

	inline void XorBuffer(std::vector<uint8_t>& data, const std::array<uint8_t, 16>& key) {
		for (size_t i = 0; i < data.size(); i++) {
			data[i] ^= key[i % key.size()];
		}
	}
}