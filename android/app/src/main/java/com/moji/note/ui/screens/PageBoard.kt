package com.moji.note.ui.screens

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.rememberTransformableState
import androidx.compose.foundation.gestures.transformable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.requiredHeight
import androidx.compose.foundation.layout.requiredWidth
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AddPhotoAlternate
import androidx.compose.material.icons.filled.Brush
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.FitScreen
import androidx.compose.material.icons.filled.Undo
import androidx.compose.material.icons.filled.ZoomIn
import androidx.compose.material.icons.filled.ZoomOut
import androidx.compose.material.icons.outlined.AutoFixOff
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LocalContentColor
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.clipToBounds
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.StrokeJoin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.layout.onSizeChanged
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.IntOffset
import androidx.compose.ui.unit.IntSize
import androidx.compose.ui.unit.dp
import androidx.compose.ui.zIndex
import com.moji.note.data.Attachment
import com.moji.note.data.MojiEvent
import com.moji.note.ui.components.MarkdownView
import com.moji.note.ui.components.decodeImage
import com.moji.note.ui.theme.darkScheme
import com.moji.note.ui.theme.lightScheme
import com.moji.note.ui.theme.parseColor
import kotlinx.coroutines.delay
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import kotlin.math.roundToInt

private const val PAGE_WIDTH = 820f
private const val PAGE_HEIGHT = 1200f
private const val DEFAULT_TEXT_W = 680f

private data class InkStroke(
    val color: String,
    val width: Float,
    val points: List<Offset>,
)

private data class PageImage(
    val path: String,
    val x: Float,
    val y: Float,
    val w: Float,
    val h: Float,
    val rot: Float,
    val z: Float,
)

private data class PageLayoutData(
    val textX: Float = 40f,
    val textY: Float = 40f,
    val textZ: Float = 10f,
    val textW: Float = DEFAULT_TEXT_W,
    val images: List<PageImage> = emptyList(),
    val hidden: List<String> = emptyList(),
    val ink: List<InkStroke> = emptyList(),
)

private fun parseLayout(raw: String?): PageLayoutData {
    if (raw.isNullOrBlank()) return PageLayoutData()
    return runCatching {
        val root = JSONObject(raw)
        val text = root.optJSONObject("text")
        val images = mutableListOf<PageImage>()
        root.optJSONArray("images")?.let { arr ->
            for (i in 0 until arr.length()) {
                val o = arr.optJSONObject(i) ?: continue
                images.add(
                    PageImage(
                        path = o.optString("path", ""),
                        x = o.optDouble("x", 60.0).toFloat(),
                        y = o.optDouble("y", 200.0).toFloat(),
                        w = o.optDouble("w", 240.0).toFloat(),
                        h = o.optDouble("h", 160.0).toFloat(),
                        rot = o.optDouble("rot", 0.0).toFloat(),
                        z = o.optDouble("z", 20.0).toFloat(),
                    ),
                )
            }
        }
        val hidden = mutableListOf<String>()
        root.optJSONArray("hidden")?.let { arr ->
            for (i in 0 until arr.length()) hidden.add(arr.optString(i))
        }
        val ink = mutableListOf<InkStroke>()
        root.optJSONArray("ink")?.let { arr ->
            for (i in 0 until arr.length()) {
                val o = arr.optJSONObject(i) ?: continue
                val points = mutableListOf<Offset>()
                o.optJSONArray("points")?.let { pts ->
                    for (j in 0 until pts.length()) {
                        val p = pts.optJSONArray(j) ?: continue
                        if (p.length() >= 2) {
                            points.add(Offset(p.optDouble(0).toFloat(), p.optDouble(1).toFloat()))
                        }
                    }
                }
                if (points.isNotEmpty()) {
                    ink.add(
                        InkStroke(
                            color = o.optString("color", "#1f2430"),
                            width = o.optDouble("width", 4.0).toFloat(),
                            points = points,
                        ),
                    )
                }
            }
        }
        PageLayoutData(
            textX = text?.optDouble("x", 40.0)?.toFloat() ?: 40f,
            textY = text?.optDouble("y", 40.0)?.toFloat() ?: 40f,
            textZ = text?.optDouble("z", 10.0)?.toFloat() ?: 10f,
            textW = text?.optDouble("w", 680.0)?.toFloat() ?: DEFAULT_TEXT_W,
            images = images,
            hidden = hidden,
            ink = ink,
        )
    }.getOrDefault(PageLayoutData())
}

@Composable
fun PageBoard(
    event: MojiEvent,
    attachments: List<Attachment>,
    initialLayout: String,
    resolveFile: (Attachment) -> File?,
    onSave: (String) -> Unit,
    modifier: Modifier = Modifier,
) {
    val context = LocalContext.current
    val darkPage = MaterialTheme.colorScheme.background.luminance() < 0.5f
    val accent = MaterialTheme.colorScheme.primary
    val pageScheme = if (darkPage) darkScheme(accent) else lightScheme(accent)
    // 深色模式下整页反相：纸变深、黑墨变白，彩色取补色，保持可读性。
    val inkColor: (String) -> Color = { hex ->
        val c = parseColor(hex)
        if (darkPage) Color(1f - c.red, 1f - c.green, 1f - c.blue, c.alpha) else c
    }
    val initial = remember(event.id) { parseLayout(initialLayout) }
    val strokes = remember(event.id) { mutableStateListOf<InkStroke>().apply { addAll(initial.ink) } }
    val images = remember(event.id) { mutableStateListOf<PageImage>().apply { addAll(initial.images) } }
    var mode by remember { mutableStateOf("select") }
    var color by remember { mutableStateOf("#1f2430") }
    var width by remember { mutableStateOf(4f) }
    var revision by remember { mutableIntStateOf(0) }
    var lastSavedRevision by remember { mutableIntStateOf(0) }
    var savedOnce by remember { mutableStateOf(true) }
    var showImagePicker by remember { mutableStateOf(false) }
    var zoom by remember(event.id) { mutableStateOf(1f) }
    var pan by remember(event.id) { mutableStateOf(Offset.Zero) }
    var viewport by remember { mutableStateOf(IntSize.Zero) }

    fun clampPan(p: Offset, z: Float = zoom): Offset {
        if (viewport.width <= 0 || viewport.height <= 0) return Offset.Zero
        val vw = viewport.width.toFloat()
        val vh = viewport.height.toFloat()
        // 页面按“适宽”排版，所以基准宽度就是视口宽度。
        val pageH = vw * PAGE_HEIGHT / PAGE_WIDTH
        return Offset(
            p.x.coerceIn(minOf(0f, vw - vw * z), 0f),
            p.y.coerceIn(minOf(0f, vh - pageH * z), 0f),
        )
    }

    fun zoomAt(target: Float, focal: Offset) {
        val next = target.coerceIn(1f, 4f)
        if (next == zoom) return
        val ratio = next / zoom
        zoom = next
        pan = clampPan(
            Offset(
                focal.x + (pan.x - focal.x) * ratio,
                focal.y + (pan.y - focal.y) * ratio,
            ),
            next,
        )
    }

    fun zoomTo(target: Float) {
        zoomAt(
            target,
            Offset(viewport.width / 2f, viewport.height / 2f),
        )
    }

    val transformState = rememberTransformableState { zoomChange, panChange, _ ->
        val next = (zoom * zoomChange).coerceIn(1f, 4f)
        val focal = Offset(viewport.width / 2f, viewport.height / 2f)
        val ratio = next / zoom
        zoom = next
        pan = clampPan(
            Offset(
                focal.x + (pan.x - focal.x) * ratio + panChange.x,
                focal.y + (pan.y - focal.y) * ratio + panChange.y,
            ),
            next,
        )
    }

    // 旋转屏幕/视口变化后，重新把平移量收进边界。
    LaunchedEffect(viewport) { pan = clampPan(pan) }

    val layoutBuilder = {
        val root = JSONObject()
        root.put(
            "text",
            JSONObject()
                .put("x", initial.textX.toDouble())
                .put("y", initial.textY.toDouble())
                .put("z", initial.textZ.toDouble())
                .put("w", initial.textW.toDouble()),
        )
        root.put(
            "images",
            JSONArray().apply {
                images.forEach { image ->
                    put(
                        JSONObject()
                            .put("path", image.path)
                            .put("x", image.x.toDouble())
                            .put("y", image.y.toDouble())
                            .put("w", image.w.toDouble())
                            .put("h", image.h.toDouble())
                            .put("rot", image.rot.toDouble())
                            .put("z", image.z.toDouble()),
                    )
                }
            },
        )
        root.put("hidden", JSONArray(initial.hidden))
        root.put(
            "ink",
            JSONArray().apply {
                strokes.forEach { stroke ->
                    put(
                        JSONObject()
                            .put("color", stroke.color)
                            .put("width", stroke.width.toDouble())
                            .put(
                                "points",
                                JSONArray().apply {
                                    stroke.points.forEach { p ->
                                        put(JSONArray().put(p.x.toDouble()).put(p.y.toDouble()))
                                    }
                                },
                            ),
                    )
                }
            },
        )
        root.toString()
    }

    LaunchedEffect(revision) {
        if (revision > 0 && revision != lastSavedRevision) {
            delay(700)
            lastSavedRevision = revision
            savedOnce = true
            onSave(layoutBuilder())
        }
    }

    Column(modifier.fillMaxSize()) {
        Surface(
            shape = RoundedCornerShape(18.dp),
            color = MaterialTheme.colorScheme.surfaceContainerLow,
            modifier = Modifier.fillMaxWidth(),
        ) {
            Row(
                Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState())
                    .padding(horizontal = 10.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                FilterChip(
                    selected = mode == "select",
                    onClick = { mode = "select" },
                    label = { Text("选择") },
                    shape = RoundedCornerShape(10.dp),
                )
                FilterChip(
                    selected = mode == "pen",
                    onClick = { mode = "pen" },
                    label = { Text("墨迹") },
                    leadingIcon = { Icon(Icons.Filled.Brush, null, modifier = Modifier.size(16.dp)) },
                    shape = RoundedCornerShape(10.dp),
                )
                FilterChip(
                    selected = mode == "erase",
                    onClick = { mode = "erase" },
                    label = { Text("橡皮") },
                    leadingIcon = { Icon(Icons.Outlined.AutoFixOff, null, modifier = Modifier.size(16.dp)) },
                    shape = RoundedCornerShape(10.dp),
                )
                IconButton(
                    onClick = { zoomTo(zoom / 1.25f) },
                    modifier = Modifier.size(32.dp),
                ) {
                    Icon(Icons.Filled.ZoomOut, "缩小页面", modifier = Modifier.size(18.dp))
                }
                Text(
                    "${(zoom * 100).toInt()}%",
                    style = MaterialTheme.typography.labelMedium,
                    modifier = Modifier.width(38.dp),
                    textAlign = TextAlign.Center,
                )
                IconButton(
                    onClick = { zoomTo(zoom * 1.25f) },
                    modifier = Modifier.size(32.dp),
                ) {
                    Icon(Icons.Filled.ZoomIn, "放大页面", modifier = Modifier.size(18.dp))
                }
                IconButton(
                    onClick = {
                        zoom = 1f
                        pan = Offset.Zero
                    },
                    modifier = Modifier.size(32.dp),
                ) {
                    Icon(Icons.Filled.FitScreen, "适应宽度", modifier = Modifier.size(18.dp))
                }
                for (hex in listOf("#1f2430", "#4f6ef7", "#e5484d", "#12a594", "#f5a524")) {
                    Box(
                        Modifier
                            .size(26.dp)
                            .clip(CircleShape)
                            .background(inkColor(hex))
                            .border(
                                width = if (color == hex) 2.dp else 1.dp,
                                color = if (color == hex) MaterialTheme.colorScheme.onSurface
                                else MaterialTheme.colorScheme.outlineVariant,
                                shape = CircleShape,
                            )
                            .clickable { color = hex },
                    )
                }
                for (w in listOf(2f, 4f, 8f)) {
                    Surface(
                        onClick = { width = w },
                        shape = RoundedCornerShape(9.dp),
                        color = if (width == w) MaterialTheme.colorScheme.primaryContainer
                        else Color.Transparent,
                    ) {
                        Text(
                            "${w.toInt()}",
                            style = MaterialTheme.typography.labelMedium,
                            modifier = Modifier.padding(horizontal = 8.dp, vertical = 4.dp),
                        )
                    }
                }
                IconButton(
                    onClick = {
                        if (strokes.isNotEmpty()) {
                            strokes.removeAt(strokes.lastIndex)
                            revision++
                        }
                    },
                    modifier = Modifier.size(34.dp),
                ) {
                    Icon(Icons.Filled.Undo, "撤销笔迹", modifier = Modifier.size(18.dp))
                }
                IconButton(
                    onClick = {
                        if (strokes.isNotEmpty()) {
                            strokes.clear()
                            revision++
                        }
                    },
                    modifier = Modifier.size(34.dp),
                ) {
                    Icon(Icons.Filled.DeleteSweep, "清空笔迹", modifier = Modifier.size(18.dp))
                }
                IconButton(
                    onClick = { showImagePicker = true },
                    modifier = Modifier.size(34.dp),
                ) {
                    Icon(Icons.Filled.AddPhotoAlternate, "添加图片到页面", modifier = Modifier.size(19.dp))
                }
                Text(
                    if (savedOnce && revision == lastSavedRevision) "已保存"
                    else "编辑中…",
                    style = MaterialTheme.typography.labelMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        Spacer(Modifier.height(10.dp))
        BoxWithConstraints(
            Modifier
                .weight(1f)
                .fillMaxWidth()
                .shadow(3.dp, RoundedCornerShape(6.dp))
                .clip(RoundedCornerShape(6.dp))
                .background(if (darkPage) Color(0xFF1E2230) else Color(0xFFFFFEFA)),
        ) {
            val pageWidth = maxWidth
            val scale = pageWidth.value / PAGE_WIDTH
            val pageHeight = (PAGE_HEIGHT * scale).dp
            Box(
                Modifier
                    .fillMaxSize()
                    .clipToBounds()
                    .onSizeChanged { viewport = it }
                    .then(if (mode == "select") Modifier.transformable(transformState) else Modifier),
            ) {
                Box(
                    Modifier
                        .offset { IntOffset(pan.x.roundToInt(), pan.y.roundToInt()) }
                        .requiredWidth(pageWidth)
                        .requiredHeight(pageHeight),
                ) {
                    Box(
                        Modifier
                            .fillMaxSize()
                            .graphicsLayer {
                                scaleX = zoom
                                scaleY = zoom
                                transformOrigin = TransformOrigin(0f, 0f)
                            },
                    ) {
                MaterialTheme(colorScheme = pageScheme) {
                    Box(
                        Modifier
                            .offset((initial.textX * scale).dp, (initial.textY * scale).dp)
                            .width((initial.textW * scale).dp)
                            .zIndex(initial.textZ),
                    ) {
                        MarkdownView(markdown = event.note, compact = true)
                    }
                }
                images.forEachIndexed { index, image ->
                    val file = attachments.firstOrNull { it.path == image.path }?.let(resolveFile)
                    var bitmap by remember(file?.path, file?.lastModified()) {
                        mutableStateOf<ImageBitmap?>(null)
                    }
                    LaunchedEffect(file?.path, file?.lastModified()) {
                        bitmap = file?.let { decodeImage(it, 1400) }
                    }
                    val bmp = bitmap
                    if (bmp != null) {
                        Image(
                            bitmap = bmp,
                            contentDescription = null,
                            contentScale = ContentScale.Fit,
                            modifier = Modifier
                                .offset((image.x * scale).dp, (image.y * scale).dp)
                                .width((image.w * scale).dp)
                                .height((image.h * scale).dp)
                                .zIndex(image.z)
                                .graphicsLayer { rotationZ = image.rot }
                                .pointerInput(image.path) {
                                    detectDragGestures { change, drag ->
                                        change.consume()
                                        val sx = size.width / image.w.toFloat()
                                        val sy = size.height / image.h.toFloat()
                                        val updated = images[index].copy(
                                            x = images[index].x + drag.x / (sx.coerceAtLeast(0.01f)),
                                            y = images[index].y + drag.y / (sy.coerceAtLeast(0.01f)),
                                        )
                                        images[index] = updated
                                        revision++
                                    }
                                },
                        )
                    }
                }
                val inkModifier = if (mode == "select") {
                    Modifier.matchParentSize().zIndex(1000f)
                } else {
                    Modifier
                        .matchParentSize()
                        .zIndex(1000f)
                        .pointerInput(mode, color, width) {
                            val pxPerUnit = size.width / PAGE_WIDTH
                            detectDragGestures(
                                onDragStart = { offset ->
                                    if (mode == "pen") {
                                        val p = Offset(offset.x / pxPerUnit, offset.y / pxPerUnit)
                                        strokes.add(InkStroke(color, width, listOf(p)))
                                        revision++
                                    } else {
                                        eraseAt(
                                            strokes,
                                            Offset(offset.x / pxPerUnit, offset.y / pxPerUnit),
                                        )
                                        revision++
                                    }
                                },
                                onDrag = { change, drag ->
                                    change.consume()
                                    val p = Offset(change.position.x / pxPerUnit, change.position.y / pxPerUnit)
                                    if (mode == "pen") {
                                        if (strokes.isNotEmpty()) {
                                            val last = strokes.last()
                                            strokes[strokes.lastIndex] = last.copy(points = last.points + p)
                                            revision++
                                        }
                                    } else {
                                        eraseAt(strokes, p)
                                        revision++
                                    }
                                },
                            )
                        }
                }
                Canvas(inkModifier) {
                    val pxPerUnit = size.width / PAGE_WIDTH
                    for (stroke in strokes) {
                        if (stroke.points.isEmpty()) continue
                        val path = Path()
                        val first = stroke.points.first()
                        path.moveTo(first.x * pxPerUnit, first.y * pxPerUnit)
                        for (point in stroke.points.drop(1)) {
                            path.lineTo(point.x * pxPerUnit, point.y * pxPerUnit)
                        }
                        if (stroke.points.size == 1) {
                            drawCircle(
                                color = inkColor(stroke.color),
                                radius = (stroke.width / 2f) * pxPerUnit,
                                center = Offset(first.x * pxPerUnit, first.y * pxPerUnit),
                            )
                        } else {
                            drawPath(
                                path = path,
                                color = inkColor(stroke.color),
                                style = Stroke(
                                    width = stroke.width * pxPerUnit,
                                    cap = StrokeCap.Round,
                                    join = StrokeJoin.Round,
                                ),
                            )
                        }
                    }
                }
                    }
                }
            }
        }
        Spacer(Modifier.height(8.dp))
        Text(
            "双指缩放 · 选择模式下拖动平移 · 笔迹自动保存（页面 820 × 1200，与 PC 一致）",
            style = MaterialTheme.typography.labelMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
    }

    if (showImagePicker) {
        val candidates = attachments.filter { it.isImage && it.path !in initial.hidden }
        AlertDialog(
            onDismissRequest = { showImagePicker = false },
            title = { Text("添加图片到页面") },
            text = {
                Column {
                    if (candidates.isEmpty()) {
                        Text("没有可用的图片附件，请先在详情页添加图片。")
                    }
                    candidates.forEach { attachment ->
                        Text(
                            attachment.name,
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .clickable {
                                    images.add(
                                        PageImage(
                                            path = attachment.path,
                                            x = 80f,
                                            y = 620f,
                                            w = 260f,
                                            h = 180f,
                                            rot = 0f,
                                            z = 20f + images.size,
                                        ),
                                    )
                                    revision++
                                    showImagePicker = false
                                }
                                .padding(vertical = 12.dp, horizontal = 8.dp),
                        )
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { showImagePicker = false }) { Text("关闭") }
            },
        )
    }
}

private fun eraseAt(strokes: MutableList<InkStroke>, point: Offset, radius: Float = 16f) {
    for (i in strokes.indices.reversed()) {
        if (strokes[i].points.any { p ->
                val dx = p.x - point.x
                val dy = p.y - point.y
                dx * dx + dy * dy <= radius * radius
            }
        ) {
            strokes.removeAt(i)
        }
    }
}
