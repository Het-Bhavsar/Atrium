// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/theme/tokens.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart';

ThemeData buildTheme(Brightness b) {
  final c = b == Brightness.light ? AtriumColors.light : AtriumColors.dark;
  final scheme = ColorScheme.fromSeed(seedColor: c.green, brightness: b).copyWith(surface: c.bg, onSurface: c.ink, primary: c.button, onPrimary: c.buttonInk);
  return ThemeData(
    useMaterial3: true,
    brightness: b,
    colorScheme: scheme,
    scaffoldBackgroundColor: c.bg,
    canvasColor: c.bg,
    splashFactory: NoSplash.splashFactory,
    highlightColor: Colors.transparent,
    textTheme: TextTheme(bodyMedium: AtriumType.body(c.ink), bodyLarge: AtriumType.body(c.ink), bodySmall: AtriumType.secondary(c.ink2)),
    dividerColor: c.hair,
    extensions: [c],
    pageTransitionsTheme: const PageTransitionsTheme(
      builders: {TargetPlatform.android: CupertinoPageTransitionsBuilder(), TargetPlatform.iOS: CupertinoPageTransitionsBuilder()},
    ),
  );
}
