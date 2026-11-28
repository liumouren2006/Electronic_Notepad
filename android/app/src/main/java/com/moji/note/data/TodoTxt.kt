package com.moji.note.data

/** todo.txt 导入导出，规则对齐 PC 端 importer.cpp。 */
object TodoTxt {

    private fun priorityLetter(priority: Int): Char {
        val index = (priority - 1).coerceIn(0, 25)
        return ('A' + index)
    }

    private fun oneLine(text: String): String =
        text.replace('\n', ' ').replace('\r', ' ')

    fun export(events: List<MojiEvent>, now: Long = MojiTime.nowSeconds()): String {
        val sb = StringBuilder()
        for (e in events) {
            val parts = mutableListOf<String>()
            if (e.done) {
                parts.add("x")
                parts.add(MojiTime.date(e.doneAt ?: now))
            }
            parts.add("(${priorityLetter(e.priority)})")
            e.createdAt?.let { parts.add(MojiTime.date(it)) }
            parts.add(oneLine(e.title))
            e.due?.let {
                val fmt = java.text.SimpleDateFormat("yyyy-MM-dd'T'HH:mm", java.util.Locale.US)
                parts.add("due:" + fmt.format(java.util.Date(it * 1000L)))
            }
            for (tag in e.tags) parts.add("+" + tag.replace(' ', '_'))
            sb.append(parts.joinToString(" ")).append('\n')
        }
        return sb.toString()
    }

    fun import(text: String, now: Long = MojiTime.nowSeconds()): List<MojiEvent> {
        val out = mutableListOf<MojiEvent>()
        for (rawLine in text.split('\n')) {
            var line = rawLine.trim()
            if (line.isEmpty()) continue
            var done = false
            if (line.startsWith("x ")) {
                done = true
                line = line.substring(2).trim()
            }
            val tokens = line.split(Regex("[ \\t]+")).filter { it.isNotEmpty() }
            var priority = 1
            var title = ""
            val tags = mutableListOf<String>()
            var due: Long? = null
            var created: Long? = null
            var completed: Long? = null
            for (token in tokens) {
                if (token.length == 3 && token.first() == '(' && token.last() == ')') {
                    val c = token[1].uppercaseChar()
                    if (c in 'A'..'Z') {
                        priority = c - 'A' + 1
                        continue
                    }
                }
                if (token.startsWith("due:")) {
                    due = MojiTime.parse(token.substring(4))
                    continue
                }
                if (token.length > 2 && token.startsWith("t:")) continue
                if (token.length > 1 && token.first() == '+') {
                    tags.add(token.substring(1).replace('_', ' '))
                    continue
                }
                if (isDateToken(token)) {
                    val d = MojiTime.parse(token)
                    if (created == null) created = d else if (completed == null) completed = d
                    continue
                }
                if (title.isNotEmpty()) title += " "
                title += token
            }
            if (title.isEmpty()) continue
            val createdAt = created ?: now
            out.add(
                MojiEvent(
                    id = randomHex8(),
                    title = title,
                    due = due,
                    priority = priority,
                    done = done,
                    doneAt = if (done) completed ?: now else null,
                    tags = tags.distinct(),
                    createdAt = createdAt,
                    updatedAt = createdAt,
                ),
            )
        }
        return out
    }

    private fun isDateToken(token: String): Boolean =
        token.length == 10 && token[4] == '-' && token[7] == '-' &&
            MojiTime.parse(token) != null
}
