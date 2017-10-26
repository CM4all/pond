/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Database.hxx"
#include "net/log/Parser.hxx"
#include "util/DeleteDisposer.hxx"

#include <algorithm>

#include <assert.h>

template<typename T>
static std::unique_ptr<T[]>
Duplicate(ConstBuffer<T> src)
{
	if (src.empty())
		return nullptr;

	auto dest = std::make_unique<T[]>(src.size);
	std::copy_n(src.data, src.size, dest.get());
	return dest;
}

Record::Record(ConstBuffer<uint8_t> _raw)
	:raw(Duplicate(_raw)),
	 raw_size(_raw.size),
	 parsed(Net::Log::ParseDatagram(raw.get(), raw.get() + _raw.size))
{
}

Database::~Database()
{
	assert(follow_cursors.empty());

	records.clear_and_dispose(DeleteDisposer());
}

const Record &
Database::Emplace(ConstBuffer<uint8_t> raw)
{
	auto *record = new Record(raw);
	records.push_back(*record);

	follow_cursors.clear_and_dispose([record](Cursor *cursor){
			cursor->OnAppend(*record);
		});

	return *record;
}
