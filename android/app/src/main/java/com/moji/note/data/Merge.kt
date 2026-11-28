package com.moji.note.data

data class ImportReport(
    val added: Int = 0,
    val skipped: Int = 0,
    val total: Int = 0,
) {
    fun summary(): String = "导入完成：新增 $added 条，跳过 $skipped 条"
}

data class SyncResult(
    val events: List<MojiEvent>,
    val pulled: Int,
    val keptLocal: Int,
)

object Merger {

    /**
     * 单次导入，语义对齐 PC Repository::import_events：
     * merge 时按 id 或 (title, created_at) 去重；id 冲突换新 id。
     */
    fun importEvents(
        existing: List<MojiEvent>,
        incoming: List<MojiEvent>,
        merge: Boolean,
        now: Long = MojiTime.nowSeconds(),
    ): Pair<List<MojiEvent>, ImportReport> {
        val base = if (merge) existing.toMutableList() else mutableListOf()
        val ids = base.mapTo(mutableSetOf()) { it.id }
        val fingerprints = base.mapTo(mutableSetOf()) { it.fingerprint() }
        var added = 0
        var skipped = 0
        for (raw in incoming) {
            if (raw.title.isBlank()) {
                skipped++
                continue
            }
            val fp = raw.fingerprint()
            if (merge && (raw.id in ids || fp in fingerprints)) {
                skipped++
                continue
            }
            var event = raw
            if (event.id.isEmpty() || event.id in ids) {
                var newId = randomHex8()
                while (newId in ids) newId = randomHex8()
                event = event.copy(id = newId)
            }
            event = event.copy(
                doneAt = if (event.done) event.doneAt ?: now else null,
                updatedAt = event.updatedAt ?: event.createdAt ?: now,
            )
            base.add(event)
            ids.add(event.id)
            fingerprints.add(fp)
            added++
        }
        return reindexed(base) to ImportReport(added, skipped, incoming.size)
    }

    /**
     * 双向同步：同 id 取 updated_at 较新的一方；不同 id 但标题+创建时间相同视为同一条。
     * 本地独有的记录保留。
     */
    fun syncMerge(local: List<MojiEvent>, remote: List<MojiEvent>): SyncResult {
        val merged = LinkedHashMap<String, MojiEvent>()
        val fingerprints = HashSet<String>()
        local.forEach {
            merged[it.id] = it
            fingerprints.add(it.fingerprint())
        }
        var pulled = 0
        var keptLocal = 0
        for (remoteEvent in remote) {
            val current = merged[remoteEvent.id]
            if (current != null) {
                val localTime = current.updatedAt ?: current.createdAt ?: 0L
                val remoteTime = remoteEvent.updatedAt ?: remoteEvent.createdAt ?: 0L
                if (remoteTime > localTime) {
                    merged[remoteEvent.id] = remoteEvent
                    fingerprints.add(remoteEvent.fingerprint())
                    pulled++
                } else {
                    keptLocal++
                }
                continue
            }
            if (remoteEvent.fingerprint() in fingerprints) continue
            merged[remoteEvent.id] = remoteEvent
            fingerprints.add(remoteEvent.fingerprint())
            pulled++
        }
        return SyncResult(reindexed(merged.values.toList()), pulled, keptLocal)
    }
}
