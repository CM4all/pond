/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Database.hxx"
#include "util/DeleteDisposer.hxx"

#include <assert.h>

Database::~Database()
{
	all_records.clear_and_dispose(DeleteDisposer());
}

const Record &
Database::Emplace(ConstBuffer<uint8_t> raw)
{
	if (all_records.size() >= max_records)
		Dispose(&all_records.front());

	auto *record = new Record(raw);
	all_records.push_back(*record);

	return *record;
}

void
Database::Dispose(Record *record)
{
	record->DisplaceCursors();
	all_records.remove(*record);
	delete record;
}
