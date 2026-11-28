package com.moji.note.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckBox
import androidx.compose.material.icons.outlined.CheckBoxOutlineBlank
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private sealed interface MdBlock {
    data class Heading(val level: Int, val text: String) : MdBlock
    data class Paragraph(val text: String) : MdBlock
    data class Bullet(val text: String, val ordered: Int?) : MdBlock
    data class Task(val checked: Boolean, val text: String) : MdBlock
    data class Quote(val text: String) : MdBlock
    data class Code(val text: String) : MdBlock
    data class Table(val rows: List<List<String>>, val header: Boolean) : MdBlock
    data object Divider : MdBlock
}

private fun parseBlocks(markdown: String): List<MdBlock> {
    val lines = markdown.replace("\r\n", "\n").split('\n')
    val blocks = mutableListOf<MdBlock>()
    var i = 0
    val paragraph = StringBuilder()

    fun flushParagraph() {
        if (paragraph.isNotBlank()) blocks.add(MdBlock.Paragraph(paragraph.toString().trim()))
        paragraph.clear()
    }

    while (i < lines.size) {
        val line = lines[i]
        val trimmed = line.trim()
        when {
            trimmed.startsWith("```") -> {
                flushParagraph()
                val code = StringBuilder()
                i++
                while (i < lines.size && !lines[i].trim().startsWith("```")) {
                    code.append(lines[i]).append('\n')
                    i++
                }
                blocks.add(MdBlock.Code(code.toString().trimEnd()))
            }
            trimmed.isEmpty() -> flushParagraph()
            trimmed.matches(Regex("^#{1,6}\\s+.*")) -> {
                flushParagraph()
                val level = trimmed.takeWhile { it == '#' }.length
                blocks.add(MdBlock.Heading(level, trimmed.drop(level).trim()))
            }
            trimmed.matches(Regex("^(-{3,}|\\*{3,}|_{3,})$")) -> {
                flushParagraph()
                blocks.add(MdBlock.Divider)
            }
            trimmed.startsWith("|") && i + 1 < lines.size &&
                lines[i + 1].trim().matches(Regex("^\\|?[\\s:|-]+\\|?$")) -> {
                flushParagraph()
                val rows = mutableListOf<List<String>>()
                rows.add(parseRow(trimmed))
                i += 2
                while (i < lines.size && lines[i].trim().startsWith("|")) {
                    rows.add(parseRow(lines[i].trim()))
                    i++
                }
                blocks.add(MdBlock.Table(rows, header = true))
                continue
            }
            trimmed.startsWith("> ") -> {
                flushParagraph()
                val quote = StringBuilder(trimmed.removePrefix("> ").trim())
                var j = i + 1
                while (j < lines.size && lines[j].trim().startsWith("> ")) {
                    quote.append('\n').append(lines[j].trim().removePrefix("> ").trim())
                    j++
                }
                blocks.add(MdBlock.Quote(quote.toString()))
                i = j
                continue
            }
            Regex("^[-*+]\\s+\\[[ xX]]\\s+.*").matches(trimmed) -> {
                flushParagraph()
                val checked = trimmed[3].lowercaseChar() == 'x'
                blocks.add(MdBlock.Task(checked, trimmed.substring(6).trim()))
            }
            Regex("^[-*+]\\s+.*").matches(trimmed) -> {
                flushParagraph()
                blocks.add(MdBlock.Bullet(trimmed.substring(2).trim(), null))
            }
            Regex("^\\d+[.)]\\s+.*").matches(trimmed) -> {
                flushParagraph()
                val number = trimmed.takeWhile { it.isDigit() }.toIntOrNull() ?: 1
                blocks.add(MdBlock.Bullet(trimmed.substringAfter(' ').trim(), number))
            }
            else -> {
                if (paragraph.isNotEmpty()) paragraph.append(' ')
                paragraph.append(trimmed)
            }
        }
        i++
    }
    flushParagraph()
    return blocks
}

private fun parseRow(line: String): List<String> =
    line.trim().trim('|').split('|').map { it.trim() }

private fun isAsciiAlnum(c: Char?): Boolean =
    c != null && (c in 'a'..'z' || c in 'A'..'Z' || c in '0'..'9')

private fun inlineMarkdown(raw: String, linkColor: Color): AnnotatedString = buildAnnotatedString {
    var i = 0
    val text = raw
    while (i < text.length) {
        val two = if (i + 1 < text.length) text.substring(i, i + 2) else ""
        val three = if (i + 2 < text.length) text.substring(i, i + 3) else ""
        when {
            three == "***" -> {
                val end = text.indexOf("***", i + 3)
                if (end > i + 3 && !text[i + 3].isWhitespace()) {
                    pushStyle(
                        SpanStyle(
                            fontWeight = FontWeight.Bold,
                            fontStyle = FontStyle.Italic,
                        ),
                    )
                    append(text.substring(i + 3, end))
                    pop()
                    i = end + 3
                } else {
                    append("***")
                    i += 3
                }
            }
            two == "**" || two == "__" -> {
                val end = text.indexOf(two, i + 2)
                if (end > i + 1) {
                    pushStyle(SpanStyle(fontWeight = FontWeight.Bold))
                    append(text.substring(i + 2, end))
                    pop()
                    i = end + 2
                } else {
                    append(two)
                    i += 2
                }
            }
            two == "++" && !isAsciiAlnum(text.getOrNull(i - 1)) &&
                i + 2 < text.length && !text[i + 2].isWhitespace() -> {
                val end = text.indexOf("++", i + 2)
                if (end > i + 2 && !text[end - 1].isWhitespace()) {
                    pushStyle(SpanStyle(textDecoration = TextDecoration.Underline))
                    append(text.substring(i + 2, end))
                    pop()
                    i = end + 2
                } else {
                    append(two)
                    i += 2
                }
            }
            two == "==" && i + 2 < text.length && !text[i + 2].isWhitespace() -> {
                val end = text.indexOf("==", i + 2)
                if (end > i + 2 && !text[end - 1].isWhitespace()) {
                    pushStyle(SpanStyle(background = linkColor.copy(alpha = 0.20f)))
                    append(text.substring(i + 2, end))
                    pop()
                    i = end + 2
                } else {
                    append(two)
                    i += 2
                }
            }
            two == "~~" -> {
                val end = text.indexOf("~~", i + 2)
                if (end > i + 1) {
                    pushStyle(SpanStyle(textDecoration = TextDecoration.LineThrough))
                    append(text.substring(i + 2, end))
                    pop()
                    i = end + 2
                } else {
                    append(two)
                    i += 2
                }
            }
            text[i] == '`' -> {
                val end = text.indexOf('`', i + 1)
                if (end > i) {
                    pushStyle(
                        SpanStyle(
                            fontFamily = FontFamily.Monospace,
                            background = linkColor.copy(alpha = 0.10f),
                        ),
                    )
                    append(text.substring(i + 1, end))
                    pop()
                    i = end + 1
                } else {
                    append('`')
                    i++
                }
            }
            text[i] == '*' || text[i] == '_' -> {
                val marker = text[i]
                val end = text.indexOf(marker, i + 1)
                if (end > i + 1 && !text[i + 1].isWhitespace()) {
                    pushStyle(SpanStyle(fontStyle = FontStyle.Italic))
                    append(text.substring(i + 1, end))
                    pop()
                    i = end + 1
                } else {
                    append(marker)
                    i++
                }
            }
            text[i] == '[' -> {
                val close = text.indexOf(']', i + 1)
                val openParen = if (close >= 0) text.indexOf('(', close + 1) else -1
                val end = if (openParen == close + 1) text.indexOf(')', openParen + 1) else -1
                if (close > i && openParen == close + 1 && end > openParen) {
                    pushStyle(
                        SpanStyle(
                            color = linkColor,
                            textDecoration = TextDecoration.Underline,
                        ),
                    )
                    append(text.substring(i + 1, close))
                    pop()
                    i = end + 1
                } else {
                    append('[')
                    i++
                }
            }
            else -> {
                append(text[i])
                i++
            }
        }
    }
}

@Composable
fun MarkdownView(
    markdown: String,
    modifier: Modifier = Modifier,
    compact: Boolean = false,
) {
    val blocks = remember(markdown) { parseBlocks(markdown) }
    val linkColor = MaterialTheme.colorScheme.primary
    val bodySize = if (compact) 13.sp else 15.sp
    Column(modifier = modifier.fillMaxWidth()) {
        for (block in blocks) {
            when (block) {
                is MdBlock.Heading -> {
                    val size = when (block.level) {
                        1 -> 24.sp
                        2 -> 20.sp
                        3 -> 17.sp
                        else -> 15.sp
                    }
                    Text(
                        text = inlineMarkdown(block.text, linkColor),
                        fontSize = size,
                        fontWeight = FontWeight.Bold,
                        lineHeight = size * 1.3f,
                        modifier = Modifier.padding(top = if (compact) 6.dp else 12.dp, bottom = 4.dp),
                    )
                }
                is MdBlock.Paragraph -> Text(
                    text = inlineMarkdown(block.text, linkColor),
                    fontSize = bodySize,
                    lineHeight = bodySize * 1.55f,
                    modifier = Modifier.padding(vertical = if (compact) 2.dp else 4.dp),
                )
                is MdBlock.Bullet -> Row(
                    modifier = Modifier.padding(vertical = 2.dp),
                    verticalAlignment = Alignment.Top,
                ) {
                    Text(
                        text = block.ordered?.let { "$it." } ?: "•",
                        fontSize = bodySize,
                        color = MaterialTheme.colorScheme.primary,
                        modifier = Modifier.width(if (block.ordered != null) 24.dp else 16.dp),
                    )
                    Text(
                        text = inlineMarkdown(block.text, linkColor),
                        fontSize = bodySize,
                        lineHeight = bodySize * 1.5f,
                    )
                }
                is MdBlock.Task -> Row(
                    modifier = Modifier.padding(vertical = 2.dp),
                    verticalAlignment = Alignment.Top,
                ) {
                    Icon(
                        imageVector = if (block.checked) Icons.Filled.CheckBox
                        else Icons.Outlined.CheckBoxOutlineBlank,
                        contentDescription = null,
                        tint = if (block.checked) MaterialTheme.colorScheme.primary
                        else MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.size(17.dp).padding(top = 2.dp),
                    )
                    Spacer(Modifier.width(8.dp))
                    Text(
                        text = inlineMarkdown(block.text, linkColor),
                        fontSize = bodySize,
                        lineHeight = bodySize * 1.5f,
                        textDecoration = if (block.checked) TextDecoration.LineThrough else null,
                        color = if (block.checked) MaterialTheme.colorScheme.onSurfaceVariant
                        else MaterialTheme.colorScheme.onSurface,
                    )
                }
                is MdBlock.Quote -> Row(
                    modifier = Modifier.padding(vertical = 6.dp),
                    verticalAlignment = Alignment.Top,
                ) {
                    Box(
                        Modifier
                            .width(3.dp)
                            .height((bodySize.value * 1.5f * (block.text.count { it == '\n' } + 1)).dp)
                            .clip(RoundedCornerShape(2.dp))
                            .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.55f)),
                    )
                    Spacer(Modifier.width(10.dp))
                    Text(
                        text = inlineMarkdown(block.text, linkColor),
                        fontSize = bodySize,
                        lineHeight = bodySize * 1.5f,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                is MdBlock.Code -> Box(
                    Modifier
                        .fillMaxWidth()
                        .padding(vertical = 6.dp)
                        .clip(RoundedCornerShape(12.dp))
                        .background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.65f))
                        .padding(12.dp),
                ) {
                    Text(
                        text = block.text,
                        fontFamily = FontFamily.Monospace,
                        fontSize = 12.5.sp,
                        lineHeight = 19.sp,
                        modifier = Modifier.horizontalScroll(rememberScrollState()),
                    )
                }
                is MdBlock.Table -> {
                    val scroll = rememberScrollState()
                    Column(
                        Modifier
                            .padding(vertical = 8.dp)
                            .clip(RoundedCornerShape(12.dp))
                            .background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.4f))
                            .horizontalScroll(scroll),
                    ) {
                        block.rows.forEachIndexed { rowIndex, row ->
                            Row(Modifier.fillMaxWidth()) {
                                row.forEachIndexed { cellIndex, cell ->
                                    Text(
                                        text = inlineMarkdown(cell, linkColor),
                                        fontSize = 12.5.sp,
                                        fontWeight = if (rowIndex == 0 && block.header) FontWeight.Bold
                                        else FontWeight.Normal,
                                        modifier = Modifier
                                            .width(150.dp)
                                            .padding(horizontal = 10.dp, vertical = 8.dp),
                                    )
                                    if (cellIndex < row.size - 1) {
                                        Box(
                                            Modifier
                                                .width(1.dp)
                                                .height(34.dp)
                                                .background(MaterialTheme.colorScheme.outlineVariant),
                                        )
                                    }
                                }
                            }
                            if (rowIndex < block.rows.size - 1) {
                                HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
                            }
                        }
                    }
                }
                MdBlock.Divider -> HorizontalDivider(
                    modifier = Modifier.padding(vertical = 10.dp),
                    color = MaterialTheme.colorScheme.outlineVariant,
                )
            }
        }
        if (blocks.isEmpty()) {
            Text(
                "暂无内容",
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = bodySize,
            )
        }
    }
}
