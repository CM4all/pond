/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Record.hxx"
#include "net/log/Parser.hxx"

#include <algorithm>

Record::Record(uint64_t _id, ConstBuffer<uint8_t> _raw)
	:id(_id), raw_size(_raw.size)
{
	memcpy(this + 1, _raw.data, raw_size);

	auto raw = ConstBuffer<uint8_t>::FromVoid({this + 1, raw_size});
	parsed = Net::Log::ParseDatagram(raw.begin(), raw.end());
}
