package com.moji.note.data

/** Markdown 导出，版式对齐 PC 端 export_markdown。 */
object MarkdownExport {

    fun export(events: List<MojiEvent>, now: Long = MojiTime.nowSeconds()): String {
        val sb = StringBuilder()
        sb.append("# 墨记 · 记事本导出\n\n")
        sb.append("> 导出时间：").append(MojiTime.iso(now))
            .append("　共 ").append(events.size).append(" 条\n\n")
        dumpGroup(sb, "进行中", events.filter { !it.done }, now)
        dumpGroup(sb, "已完成", events.filter { it.done }, now)
        return sb.toString()
    }

    private fun dumpGroup(
        sb: StringBuilder,
        title: String,
        list: List<MojiEvent>,
        now: Long,
    ) {
        sb.append("## ").append(title).append("（").append(list.size).append("）\n\n")
        if (list.isEmpty()) {
            sb.append("_暂无_\n\n")
            return
        }
        for (e in list) {
            sb.append("### ").append(e.priority).append(". [")
                .append(if (e.done) "x" else " ").append("] ").append(e.title).append('\n')
            sb.append("- 截止：")
                .append(if (e.due == null) "无期限" else MojiTime.humanize(e.due, now))
                .append('\n')
            if (e.tags.isNotEmpty()) {
                sb.append("- 标签：")
                for (tag in e.tags) sb.append("`#").append(tag).append("` ")
                sb.append('\n')
            }
            if (e.attachments.isNotEmpty()) {
                sb.append("- 附件：")
                sb.append(e.attachments.joinToString("、") { "${it.name}（${it.mime}）" })
                sb.append('\n')
            }
            if (e.note.isNotEmpty()) {
                sb.append('\n')
                for (line in e.note.split('\n')) sb.append("> ").append(line).append('\n')
            }
            sb.append('\n')
        }
    }
}
