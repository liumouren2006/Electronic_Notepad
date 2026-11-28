package com.moji.note.ui.components

import android.graphics.BitmapFactory
import android.graphics.Matrix
import android.graphics.Movie
import android.graphics.Path
import android.graphics.RectF
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.graphics.drawscope.drawIntoCanvas
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

/**
 * 与 PC 端一致的 Logo 变换。dx/dy 使用相对画框的比例（-0.5..0.5），
 * 这样同一份配置在 50dp 的标题和 220dp 的预览里都能正确显示。
 */
data class BrandTransform(
    val zoom: Float = 1f,
    val dx: Float = 0f,
    val dy: Float = 0f,
    val shape: String = "rounded",
    val stretch: Boolean = false,
) {
    fun toJson(): String = JSONObject().apply {
        put("zoom", zoom.toDouble())
        put("dx", dx.toDouble())
        put("dy", dy.toDouble())
        put("shape", shape)
        put("stretch", stretch)
    }.toString()

    companion object {
        fun parse(raw: String?): BrandTransform {
            if (raw.isNullOrBlank()) return BrandTransform()
            return runCatching {
                val o = JSONObject(raw)
                BrandTransform(
                    zoom = o.optDouble("zoom", 1.0).toFloat().coerceIn(0.5f, 4f),
                    dx = o.optDouble("dx", 0.0).toFloat().coerceIn(-1f, 1f),
                    dy = o.optDouble("dy", 0.0).toFloat().coerceIn(-1f, 1f),
                    shape = o.optString("shape", "rounded"),
                    stretch = o.optBoolean("stretch", false),
                )
            }.getOrDefault(BrandTransform())
        }
    }
}

private fun buildShapePath(w: Float, h: Float, shape: String): Path {
    val path = Path()
    val r = RectF(0f, 0f, w, h)
    when (shape) {
        "circle" -> path.addOval(r, Path.Direction.CW)
        "square" -> path.addRect(r, Path.Direction.CW)
        "hexagon" -> {
            val cx = w / 2f
            val cy = h / 2f
            val rx = w / 2f
            val ry = h / 2f
            for (i in 0..5) {
                val a = Math.PI / 6.0 + i * Math.PI / 3.0
                val x = (cx + rx * cos(a)).toFloat()
                val y = (cy + ry * sin(a)).toFloat()
                if (i == 0) path.moveTo(x, y) else path.lineTo(x, y)
            }
            path.close()
        }
        else -> {
            val radius = min(w, h) * 0.32f
            path.addRoundRect(r, radius, radius, Path.Direction.CW)
        }
    }
    return path
}

/**
 * 渲染品牌 Logo：静态图或 GIF，支持缩放、平移与形状裁剪。
 * [file] 为空时由调用方显示文字占位，这里只画底色。
 */
@Composable
fun BrandLogo(
    file: File?,
    transform: BrandTransform,
    size: Dp,
    modifier: Modifier = Modifier,
    placeholderColor: Color = Color(0x33FFFFFF),
) {
    val path = file?.path
    // 同名文件被覆盖时（例如重新选择 logo.png），用修改时间强制重新解码。
    val fileKey = file?.let { "${it.path}:${it.lastModified()}:${it.length()}" }
    var bitmap by remember(fileKey) { mutableStateOf<ImageBitmap?>(null) }
    var movie by remember(fileKey) { mutableStateOf<Movie?>(null) }
    LaunchedEffect(fileKey) {
        bitmap = if (path == null) null else withContext(Dispatchers.IO) {
            runCatching { decodeScaled(path)?.asImageBitmap() }.getOrNull()
        }
        movie = if (path == null || !path.endsWith(".gif", ignoreCase = true)) null else
            withContext(Dispatchers.IO) {
                runCatching { Movie.decodeFile(path) }.getOrNull()
            }
    }
    var frameTime by remember { mutableFloatStateOf(0f) }
    LaunchedEffect(movie) {
        val m = movie ?: return@LaunchedEffect
        if (m.duration() <= 0) return@LaunchedEffect
        val start = withFrameNanos { it }
        while (true) {
            withFrameNanos { now ->
                frameTime = ((now - start) / 1_000_000L % m.duration()).toFloat()
            }
        }
    }

    Box(modifier = modifier.size(size), contentAlignment = Alignment.Center) {
        Canvas(Modifier.size(size)) {
            val w = this.size.width
            val h = this.size.height
            val clip = buildShapePath(w, h, transform.shape)
            drawIntoCanvas { canvas ->
                val native = canvas.nativeCanvas
                val save = native.save()
                native.clipPath(clip)
                val bmp = bitmap
                val mov = movie?.takeIf { it.width() > 0 && it.height() > 0 }
                if (mov != null) {
                    drawTransformedMovie(native, mov, w, h, transform, frameTime)
                } else if (bmp != null) {
                    drawTransformedBitmap(native, bmp, w, h, transform)
                } else {
                    native.drawColor(placeholderColor.toArgb())
                }
                native.restoreToCount(save)
            }
        }
    }
}

/** Logo 不需要原图分辨率；按 768px 上限采样，避免用户选大图时 OOM。 */
private fun decodeScaled(path: String, maxPx: Int = 768): android.graphics.Bitmap? {
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeFile(path, bounds)
    if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null
    var sample = 1
    while (bounds.outWidth / (sample * 2) >= maxPx || bounds.outHeight / (sample * 2) >= maxPx) {
        sample *= 2
    }
    val options = BitmapFactory.Options().apply { inSampleSize = sample }
    return BitmapFactory.decodeFile(path, options)
}

private fun drawTransformedBitmap(
    canvas: android.graphics.Canvas,
    bitmap: ImageBitmap,
    w: Float,
    h: Float,
    t: BrandTransform,
) {
    val b = bitmap.asAndroidBitmap()
    if (b.width <= 0 || b.height <= 0) return
    val baseX = if (t.stretch) w / b.width else max(w / b.width, h / b.height)
    val baseY = if (t.stretch) h / b.height else baseX
    val dw = b.width * baseX * t.zoom
    val dh = b.height * baseY * t.zoom
    val left = (w - dw) / 2f + t.dx * w
    val top = (h - dh) / 2f + t.dy * h
    val dst = android.graphics.RectF(left, top, left + dw, top + dh)
    canvas.drawBitmap(b, null, dst, android.graphics.Paint(android.graphics.Paint.FILTER_BITMAP_FLAG))
}

private fun drawTransformedMovie(
    canvas: android.graphics.Canvas,
    movie: Movie,
    w: Float,
    h: Float,
    t: BrandTransform,
    timeMs: Float,
) {
    if (movie.width() <= 0 || movie.height() <= 0) return
    val baseX = if (t.stretch) w / movie.width() else max(w / movie.width(), h / movie.height())
    val baseY = if (t.stretch) h / movie.height() else baseX
    val scaleX = baseX * t.zoom
    val scaleY = baseY * t.zoom
    val left = (w - movie.width() * scaleX) / 2f + t.dx * w
    val top = (h - movie.height() * scaleY) / 2f + t.dy * h
    val matrix = Matrix().apply {
        postScale(scaleX, scaleY)
        postTranslate(left, top)
    }
    movie.setTime(timeMs.toInt())
    canvas.save()
    canvas.concat(matrix)
    movie.draw(canvas, 0f, 0f)
    canvas.restore()
}
