package com.moji.note.ui.theme

import android.app.Activity
import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Shapes
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.lerp
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat

enum class ThemeMode(val label: String, val stored: String) {
    SYSTEM("跟随系统", "system"),
    LIGHT("浅色", "light"),
    DARK("深色", "dark");

    companion object {
        fun from(stored: String?): ThemeMode =
            entries.firstOrNull { it.stored == stored } ?: SYSTEM
    }
}

val AccentOptions = listOf(
    "#4f6ef7", "#7c5cff", "#00a884", "#f97316",
    "#ef476f", "#14b8a6", "#eab308", "#64748b",
)

fun parseColor(hex: String, fallback: Color = Color(0xFF4F6EF7)): Color =
    runCatching {
        val clean = hex.trim().removePrefix("#")
        val value = clean.toLong(16)
        if (clean.length == 8) Color(value) else Color(0xFF000000 or value)
    }.getOrDefault(fallback)

private fun onColorFor(color: Color): Color {
    val luminance = 0.299f * color.red + 0.587f * color.green + 0.114f * color.blue
    return if (luminance > 0.62f) Color(0xFF11151E) else Color.White
}

fun lightScheme(accent: Color) = lightColorScheme(
    primary = accent,
    onPrimary = onColorFor(accent),
    primaryContainer = lerp(accent, Color.White, 0.82f),
    onPrimaryContainer = lerp(accent, Color(0xFF09122B), 0.45f),
    secondary = lerp(accent, Color(0xFF64748B), 0.45f),
    onSecondary = Color.White,
    secondaryContainer = lerp(accent, Color.White, 0.88f),
    onSecondaryContainer = lerp(accent, Color(0xFF111827), 0.55f),
    tertiary = lerp(accent, Color(0xFF00A884), 0.55f),
    background = Color(0xFFF6F7FC),
    onBackground = Color(0xFF171B26),
    surface = Color(0xFFFFFFFF),
    onSurface = Color(0xFF171B26),
    surfaceVariant = Color(0xFFE9EDF7),
    onSurfaceVariant = Color(0xFF525C72),
    surfaceContainer = Color(0xFFF0F2FA),
    surfaceContainerLow = Color(0xFFFAFBFF),
    surfaceContainerHigh = Color(0xFFE8EBF5),
    outline = Color(0xFFC6CCDC),
    outlineVariant = Color(0xFFE1E6F1),
    error = Color(0xFFD64545),
    onError = Color.White,
    errorContainer = Color(0xFFFFE3E3),
    onErrorContainer = Color(0xFF5B1111),
)

fun darkScheme(accent: Color) = darkColorScheme(
    primary = lerp(accent, Color.White, 0.24f),
    onPrimary = onColorFor(lerp(accent, Color.White, 0.24f)),
    primaryContainer = lerp(accent, Color(0xFF0B1020), 0.55f),
    onPrimaryContainer = lerp(accent, Color.White, 0.75f),
    secondary = lerp(accent, Color(0xFF9AA7C7), 0.5f),
    onSecondary = Color(0xFF0C1220),
    secondaryContainer = Color(0xFF232B3E),
    onSecondaryContainer = Color(0xFFDCE3F7),
    tertiary = lerp(accent, Color(0xFF2DD4BF), 0.6f),
    background = Color(0xFF0E1117),
    onBackground = Color(0xFFE9EDF7),
    surface = Color(0xFF151A23),
    onSurface = Color(0xFFE9EDF7),
    surfaceVariant = Color(0xFF202634),
    onSurfaceVariant = Color(0xFFA8B2C7),
    surfaceContainer = Color(0xFF1A202B),
    surfaceContainerLow = Color(0xFF12161E),
    surfaceContainerHigh = Color(0xFF222A38),
    outline = Color(0xFF3A4356),
    outlineVariant = Color(0xFF2A3243),
    error = Color(0xFFFF8A8A),
    onError = Color(0xFF3A0A0A),
    errorContainer = Color(0xFF4A1A1A),
    onErrorContainer = Color(0xFFFFD9D9),
)

private val MojiTypography = Typography(
    displaySmall = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Bold,
        fontSize = 34.sp,
        lineHeight = 40.sp,
        letterSpacing = (-0.5).sp,
    ),
    headlineMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Bold,
        fontSize = 26.sp,
        lineHeight = 32.sp,
        letterSpacing = (-0.3).sp,
    ),
    headlineSmall = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.SemiBold,
        fontSize = 22.sp,
        lineHeight = 28.sp,
    ),
    titleLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.SemiBold,
        fontSize = 19.sp,
        lineHeight = 25.sp,
    ),
    titleMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.SemiBold,
        fontSize = 16.sp,
        lineHeight = 22.sp,
    ),
    bodyLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Normal,
        fontSize = 15.sp,
        lineHeight = 22.sp,
    ),
    bodyMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Normal,
        fontSize = 13.5.sp,
        lineHeight = 20.sp,
    ),
    labelLarge = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Medium,
        fontSize = 13.sp,
        lineHeight = 18.sp,
    ),
    labelMedium = TextStyle(
        fontFamily = FontFamily.SansSerif,
        fontWeight = FontWeight.Medium,
        fontSize = 11.5.sp,
        lineHeight = 16.sp,
    ),
)

private val MojiShapes = Shapes(
    extraSmall = androidx.compose.foundation.shape.RoundedCornerShape(8.dp),
    small = androidx.compose.foundation.shape.RoundedCornerShape(12.dp),
    medium = androidx.compose.foundation.shape.RoundedCornerShape(16.dp),
    large = androidx.compose.foundation.shape.RoundedCornerShape(22.dp),
    extraLarge = androidx.compose.foundation.shape.RoundedCornerShape(28.dp),
)

@Composable
fun MojiTheme(
    mode: ThemeMode,
    accentHex: String,
    dynamicColor: Boolean,
    content: @Composable () -> Unit,
) {
    val context = LocalContext.current
    val dark = when (mode) {
        ThemeMode.SYSTEM -> isSystemInDarkTheme()
        ThemeMode.LIGHT -> false
        ThemeMode.DARK -> true
    }
    val accent = parseColor(accentHex)
    val scheme = when {
        dynamicColor && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ->
            if (dark) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
        dark -> darkScheme(accent)
        else -> lightScheme(accent)
    }
    val view = LocalView.current
    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as? Activity)?.window ?: return@SideEffect
            window.statusBarColor = Color.Transparent.toArgb()
            window.navigationBarColor = Color.Transparent.toArgb()
            WindowCompat.getInsetsController(window, view).apply {
                isAppearanceLightStatusBars = !dark
                isAppearanceLightNavigationBars = !dark
            }
        }
    }
    MaterialTheme(
        colorScheme = scheme,
        typography = MojiTypography,
        shapes = MojiShapes,
        content = content,
    )
}
