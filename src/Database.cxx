/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Database.hxx"
#include "util/DeleteDisposer.hxx"

#include <assert.h>

Database::~Database()
{
	assert(follow_cursors.empty());

	records.clear_and_dispose(DeleteDisposer());
}

const Record &
Database::Emplace(ConstBuffer<uint8_t> raw)
{
	if (records.size() >= max_records)
		Dispose(&records.front());

	auto *record = new Record(raw);
	records.push_back(*record);

	follow_cursors.clear_and_dispose([record](Cursor *cursor){
			cursor->OnAppend(*record);
		});

	return *record;
}

void
Database::Dispose(Record *record)
{
	record->DisplaceCursors();
	records.erase(records.iterator_to(*record));
	delete record;
}
