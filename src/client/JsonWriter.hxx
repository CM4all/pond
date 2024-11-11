// SPDX-License-Identifier: BSD-2-Clause
// Copyright CM4all GmbH
// author: Max Kellermann <mk@cm4all.com>

#pragma once

#include <fmt/core.h>

#include <algorithm> // for std::copy()
#include <utility>
#include <cinttypes>
#include <string_view>

/**
 * A very simple JSON writing library.  Everything is written to a
 * FILE* in one single line, which makes it easy to generate JSONL
 * (JSON Lines).
 */
namespace JsonWriter {

struct BufferFull {};

/**
 * The sink which will receive JSON data and writes them into a
 * buffer.
 */
class Sink {
	char *p, *const end;

public:
	[[nodiscard]]
	explicit constexpr Sink(char *_p, char *_end) noexcept
		:p(_p), end(_end) {}

	[[nodiscard]]
	constexpr char *GetPosition() const noexcept {
		return p;
	}

	constexpr void WriteRaw(char ch) {
		if (p >= end) [[unlikely]]
			throw BufferFull{};

		*p++ = ch;
	}

	constexpr void WriteRaw(std::string_view s) {
		if (s.size() > static_cast<std::size_t>(end - p)) [[unlikely]]
			throw BufferFull{};

		p = std::copy(s.begin(), s.end(), p);
	}

	void VFmtRaw(fmt::string_view format_str, fmt::format_args args) noexcept {
		auto [q, _] = fmt::vformat_to_n(p, end - p,
						format_str, args);
		// TOOD throw on truncation?
		p = q;
	}

	template<typename S, typename... Args>
	void FmtRaw(const S &format_str, Args&&... args) noexcept {
		return VFmtRaw(format_str, fmt::make_format_args(args...));
	}

private:
	constexpr void WriteStringChar(char ch) {
		switch (ch) {
		case '\\':
		case '"':
			WriteRaw('\\');
			WriteRaw(ch);
			break;

		case '\n':
			WriteRaw('\\');
			WriteRaw('n');
			break;

		case '\r':
			WriteRaw('\\');
			WriteRaw('r');
			break;

		default:
			if ((unsigned char)ch < 0x20)
				/* escape non-printable control characters */
				FmtRaw("\\u{:04x}", (unsigned char)ch);
			else
				WriteRaw(ch);
			break;
		}
	}

public:
	constexpr void WriteValue(std::string_view value) {
		WriteRaw('"');

		for (const char ch : value)
			WriteStringChar(ch);

		WriteRaw('"');
	}

	constexpr void WriteValue(const char *value) {
		WriteRaw('"');

		while (*value != 0)
			WriteStringChar(*value++);

		WriteRaw('"');
	}

	constexpr void WriteValue(std::nullptr_t) {
		WriteRaw("null");
	}

	constexpr void WriteValue(bool value) {
		WriteRaw(value ? "true" : "false");
	}

	void WriteValue(std::integral auto value) {
		FmtRaw("{}", value);
	}

	void WriteValue(std::floating_point auto value) {
		FmtRaw("{}", value);
	}
};

/**
 * Write an object (dictionary, map).  Call AddMember() for each
 * member, and call Flush() once to finish this object.
 */
class Object {
	Sink sink;

	bool pending_comma = false;

public:
	[[nodiscard]]
	explicit Object(Sink _sink):sink(_sink) {
		sink.WriteRaw('{');
	}

	Object(const Object &) = delete;
	Object &operator=(const Object &) = delete;

	[[nodiscard]]
	char *GetPosition() const noexcept {
		return sink.GetPosition();
	}

	[[nodiscard]]
	Sink &AddMember(const char *name) {
		if (pending_comma) {
			sink.WriteRaw(',');
			pending_comma = false;
		}

		sink.WriteValue(name);
		sink.WriteRaw(':');
		pending_comma = true;

		return sink;
	}

	template<typename T>
	void AddMember(const char *name, T &&value) {
		AddMember(name).WriteValue(std::forward<T>(value));
	}

	void Flush() {
		sink.WriteRaw('}');
	}
};

} // namespace JsonWriter
