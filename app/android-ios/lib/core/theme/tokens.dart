// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:flutter/material.dart';

/// Design tokens from `design/prototype/index.html` (see BLUEPRINT.md section 2).
/// Widgets read colours through [AtriumColors.of]; nothing hardcodes a colour.
@immutable
class AtriumColors extends ThemeExtension<AtriumColors> {
  const AtriumColors({
    required this.ground,
    required this.bg,
    required this.card,
    required this.cardSolid,
    required this.ink,
    required this.ink2,
    required this.ink3,
    required this.hair,
    required this.track,
    required this.button,
    required this.buttonInk,
    required this.accent,
    required this.green,
    required this.red,
    required this.ir,
    required this.cool,
    required this.barBg,
    required this.scrim,
    required this.glow,
  });

  final Color ground;
  final Color bg;
  final Color card;
  final Color cardSolid;
  final Color ink;
  final Color ink2;
  final Color ink3;
  final Color hair;
  final Color track;
  final Color button;
  final Color buttonInk;
  final Color accent;

  /// Light-colour data: green for heart, HRV and Recovery (the pod's green LED).
  final Color green;

  /// Red for effort (the pod's red LED). Never for temperature: it isn't measured with light.
  final Color red;

  /// Infrared, drawn violet, for sleep.
  final Color ir;
  final Color cool;
  final Color barBg;
  final Color scrim;

  /// Gauge halo strength (stronger in the dark).
  final double glow;

  Color get heart => green;
  Color get balance => green;
  Color get sleep => ir;
  Color get effort => red;

  /// Text and marks on photographs, in both themes.
  static const Color onPhoto = Color(0xFFF6F2EA);

  static const AtriumColors light = AtriumColors(
    ground: Color(0xFFFFFFFF),
    bg: Color(0xFFFFFFFF),
    card: Color(0xFFF4F4F6),
    cardSolid: Color(0xFFFFFFFF),
    ink: Color(0xFF111113),
    ink2: Color(0xFF6B6B72),
    ink3: Color(0xFF9D9DA4),
    hair: Color(0x17000000),
    track: Color(0x12000000),
    button: Color(0xFF111113),
    buttonInk: Color(0xFFFFFFFF),
    accent: Color(0xFF2B5FC7),
    green: Color(0xFF1E9E5A),
    red: Color(0xFFD8452F),
    ir: Color(0xFF7A4FD6),
    cool: Color(0xFF2F6FD8),
    barBg: Color(0xCCFFFFFF),
    scrim: Color(0x4D000000),
    glow: 0.35,
  );

  static const AtriumColors dark = AtriumColors(
    ground: Color(0xFF050506),
    bg: Color(0xFF0B0B0C),
    card: Color(0x0EFFFFFF),
    cardSolid: Color(0xFF18181A),
    ink: Color(0xFFF3EFE8),
    ink2: Color(0xA3F3EFE8),
    ink3: Color(0x6BF3EFE8),
    hair: Color(0x1AFFFFFF),
    track: Color(0x17FFFFFF),
    button: Color(0xFFF3EFE8),
    buttonInk: Color(0xFF121212),
    accent: Color(0xFFA9C8FF),
    green: Color(0xFF5FE39A),
    red: Color(0xFFFF6F5C),
    ir: Color(0xFFB48CFF),
    cool: Color(0xFF8AB8FF),
    barBg: Color(0xA80B0B0C),
    scrim: Color(0x8C000000),
    glow: 0.9,
  );

  static AtriumColors of(BuildContext context) => Theme.of(context).extension<AtriumColors>()!;

  @override
  AtriumColors copyWith() => this;

  @override
  AtriumColors lerp(AtriumColors? other, double t) {
    if (other == null) return this;
    Color c(Color a, Color b) => Color.lerp(a, b, t)!;
    return AtriumColors(
      ground: c(ground, other.ground),
      bg: c(bg, other.bg),
      card: c(card, other.card),
      cardSolid: c(cardSolid, other.cardSolid),
      ink: c(ink, other.ink),
      ink2: c(ink2, other.ink2),
      ink3: c(ink3, other.ink3),
      hair: c(hair, other.hair),
      track: c(track, other.track),
      button: c(button, other.button),
      buttonInk: c(buttonInk, other.buttonInk),
      accent: c(accent, other.accent),
      green: c(green, other.green),
      red: c(red, other.red),
      ir: c(ir, other.ir),
      cool: c(cool, other.cool),
      barBg: c(barBg, other.barBg),
      scrim: c(scrim, other.scrim),
      glow: glow + (other.glow - glow) * t,
    );
  }
}

/// Spacing scale (4-point).
abstract final class Space {
  static const double xs = 4;
  static const double s = 8;
  static const double m = 12;
  static const double l = 16;
  static const double xl = 20;
  static const double xxl = 24;
  static const double xxxl = 32;

  /// Horizontal page gutter.
  static const double gutter = 16;
}

abstract final class Radii {
  static const double card = 22;
  static const double tile = 22;
  static const double sheet = 24;
  static const double small = 12;
  static const double pill = 999;
}

abstract final class Motion {
  static const Curve ease = Cubic(0.2, 0.8, 0.2, 1);
  static const Duration tap = Duration(milliseconds: 200);
  static const Duration meter = Duration(milliseconds: 600);
  static const Duration gauge = Duration(milliseconds: 1100);
  static const Duration page = Duration(milliseconds: 380);
  static const Duration chart = Duration(milliseconds: 700);
  static const Duration fade = Duration(milliseconds: 260);
  static const Duration reveal = Duration(milliseconds: 420);

  /// True when the system asks for less motion (iOS Reduce Motion, Android "Remove animations").
  static bool reduced(BuildContext context) => MediaQuery.maybeDisableAnimationsOf(context) ?? false;

  /// [d], or zero when the system asks for less motion.
  static Duration of(BuildContext context, Duration d) => reduced(context) ? Duration.zero : d;
}

/// Type: Newsreader (serif, bundled) for headings and numbers; the system sans for the rest.
abstract final class AtriumType {
  static const String serif = 'Newsreader';

  static TextStyle serifAt(double size, {double weight = 400, Color? color, double height = 1.1}) => TextStyle(
    fontFamily: serif,
    fontSize: size,
    height: height,
    letterSpacing: -0.01 * size,
    color: color,
    fontVariations: [FontVariation('wght', weight), FontVariation('opsz', size.clamp(6, 72).toDouble())],
  );

  static TextStyle largeTitle(Color c) => serifAt(40, color: c, height: 1.02);
  static TextStyle section(Color c) => serifAt(24, color: c, height: 1.15);
  static TextStyle story(Color c) => serifAt(23, color: c, height: 1.28);
  static TextStyle coverNumber(Color c) => serifAt(76, weight: 300, color: c, height: 0.9);
  static TextStyle tileNumber(Color c) => serifAt(34, weight: 300, color: c, height: 1);

  // Sans styles pin letterSpacing to 0: Material 3 typography would otherwise add 0.25-0.5 px of tracking.
  static TextStyle body(Color c) => TextStyle(letterSpacing: 0, fontSize: 16, height: 1.42, color: c);
  static TextStyle bodyStrong(Color c) => TextStyle(letterSpacing: 0, fontSize: 16, height: 1.35, fontWeight: FontWeight.w600, color: c);
  static TextStyle secondary(Color c) => TextStyle(letterSpacing: 0, fontSize: 14, height: 1.42, color: c);
  static TextStyle caption(Color c) => TextStyle(letterSpacing: 0, fontSize: 12.5, height: 1.4, color: c);
  static TextStyle label(Color c) => TextStyle(fontSize: 11.5, height: 1, fontWeight: FontWeight.w600, letterSpacing: 1.6, color: c);
  static TextStyle unit(Color c) => TextStyle(letterSpacing: 0, fontSize: 13, fontWeight: FontWeight.w500, color: c);
  static TextStyle button(Color c) => TextStyle(letterSpacing: 0, fontSize: 17, fontWeight: FontWeight.w600, color: c);
}
