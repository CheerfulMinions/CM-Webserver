#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include <bit>
#include <ranges>
#include <algorithm>
#include <span>
#include <concepts>
#include <random>
#include <array>
#include <deque>
#include <list>
#include <ranges>
#include <set>
#include <valarray>

// WebSocket操作码
enum class WebSocketOpcode : std::uint8_t {
	Continuation = 0x0,
	Text = 0x1,
	Binary = 0x2,
	Close = 0x8,
	Ping = 0x9,
	Pong = 0xA
};

// 构造WebSocket数据帧
template <std::ranges::contiguous_range Buffer>
requires std::same_as<std::ranges::range_value_t<Buffer>, std::uint8_t>
std::vector<std::uint8_t> constructWebSocketFrame(WebSocketOpcode opcode, Buffer&& data, bool fin = true, bool mask = false) {
	std::vector<std::uint8_t> frame;
	
	// 构造帧头
	std::uint8_t firstByte = static_cast<std::uint8_t>(opcode);
	if (fin) {
		firstByte |= 0x80;
	}
	frame.push_back(firstByte);
	
	// 构造载荷长度
	std::size_t payloadLength = std::ranges::size(data);
	if (payloadLength <= 125) {
		std::uint8_t secondByte = static_cast<std::uint8_t>(payloadLength);
		if (mask) {
			secondByte |= 0x80;
		}
		frame.push_back(secondByte);
	} else if (payloadLength <= 65535) {
		std::uint8_t secondByte = 126;
		if (mask) {
			secondByte |= 0x80;
		}
		frame.push_back(secondByte);
		std::uint16_t len = std::byteswap(static_cast<std::uint16_t>(payloadLength));
		auto lenSpan = std::as_bytes(std::span(&len, 1));
		frame.insert(frame.end(), lenSpan.begin(), lenSpan.end());
	} else {
		std::uint8_t secondByte = 127;
		if (mask) {
			secondByte |= 0x80;
		}
		frame.push_back(secondByte);
		std::uint64_t len = std::byteswap(static_cast<std::uint64_t>(payloadLength));
		auto lenSpan = std::as_bytes(std::span(&len, 1));
		frame.insert(frame.end(), lenSpan.begin(), lenSpan.end());
	}
	
	// 构造掩码键
	if (mask) {
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<std::uint32_t> dist(0, std::numeric_limits<std::uint32_t>::max());
		std::uint32_t maskingKey = dist(gen);
		auto maskSpan = std::as_bytes(std::span(&maskingKey, 1));
		frame.insert(frame.end(), maskSpan.begin(), maskSpan.end());
		
		// 掩码处理数据
		std::vector<std::uint8_t> maskedData;
		std::ranges::transform(data, std::back_inserter(maskedData),
			[&, i = 0](std::uint8_t byte) mutable {
				return byte ^ std::bit_cast<std::uint8_t*>(&maskingKey)[i++ % 4];
			});
		frame.insert(frame.end(), maskedData.begin(), maskedData.end());
	} else {
		frame.insert(frame.end(), data.begin(), data.end());
	}
	
	return frame;
}
