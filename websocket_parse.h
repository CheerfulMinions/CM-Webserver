#include <iostream>
#include <vector>
#include <bitset>
#include <cstdint>
#include <string>
#include <span>
#include <stdexcept>

class WebSocketFrameParser {
public:
	std::vector<uint8_t> parseFrame(std::span<const uint8_t> data) {
		if (data.size() < 2) {
			throw std::runtime_error("Incomplete frame header");
		}
		
		bool fin = data[0] & 0x80;
		uint8_t opcode = data[0] & 0x0F;
		bool mask = data[1] & 0x80;
		uint64_t payload_len = data[1] & 0x7F;
		
		size_t header_len = 2;
		if (payload_len == 126) {
			if (data.size() < 4) {
				throw std::runtime_error("Incomplete extended payload length");
			}
			payload_len = static_cast<uint64_t>(data[2]) << 8 | data[3];
			header_len += 2;
		} else if (payload_len == 127) {
			if (data.size() < 10) {
				throw std::runtime_error("Incomplete extended payload length");
			}
			payload_len = static_cast<uint64_t>(data[2]) << 56 |
			static_cast<uint64_t>(data[3]) << 48 |
			static_cast<uint64_t>(data[4]) << 40 |
			static_cast<uint64_t>(data[5]) << 32 |
			static_cast<uint64_t>(data[6]) << 24 |
			static_cast<uint64_t>(data[7]) << 16 |
			static_cast<uint64_t>(data[8]) << 8 |
			data[9];
			header_len += 8;
		}
		
		if (mask) {
			if (data.size() < header_len + 4) {
				throw std::runtime_error("Incomplete masking key");
			}
			std::array<uint8_t, 4> masking_key{data[header_len], data[header_len + 1], data[header_len + 2], data[header_len + 3]};
			header_len += 4;
			
			if (data.size() < header_len + payload_len) {
				throw std::runtime_error("Incomplete payload data");
			}
			
			std::vector<uint8_t> payload(data.begin() + header_len, data.begin() + header_len + payload_len);
			for (size_t i = 0; i < payload_len; ++i) {
				payload[i] ^= masking_key[i % 4];
			}
			
			return payload;
		} else {
			if (data.size() < header_len + payload_len) {
				throw std::runtime_error("Incomplete payload data");
			}
			
			return std::vector<uint8_t>(data.begin() + header_len, data.begin() + header_len + payload_len);
		}
	}
};
