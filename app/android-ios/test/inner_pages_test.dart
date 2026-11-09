// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/routes/app_router.dart';
import 'package:atrium/core/theme/app_theme.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:flutter_test/flutter_test.dart';

/// The owner's rules for inner pages: photo cover with the reading, one serif sentence, one range switch, and no
/// uppercase tag pills, highlight blurbs or explainer paragraphs. Data colours only from the sensor's light.
void main() {
  const pages = ['recovery', 'heart', 'sleep', 'effort', 'temperature'];
  const covers = {'recovery': 'welcome', 'heart': 'heart', 'sleep': 'sleep', 'effort': 'effort', 'temperature': 'temp'};

  for (final page in pages) {
    testWidgets('$page: one range switch, no explainer or pill, its own photo', (tester) async {
      final previous = FlutterError.onError;
      FlutterError.onError = (e) {
        if (!e.toString().contains('overflowed')) previous?.call(e);
      };
      addTearDown(() => FlutterError.onError = previous);
      tester.view.physicalSize = const Size(1206, 2622);
      tester.view.devicePixelRatio = 3;
      addTearDown(tester.view.reset);
      await tester.pumpWidget(
        ProviderScope(
          child: MaterialApp.router(theme: buildTheme(Brightness.light), routerConfig: buildRouter(initial: '/today/$page')),
        ),
      );
      for (var i = 0; i < 5; i++) {
        await tester.pump(const Duration(milliseconds: 200));
      }
      FlutterError.onError = previous;
      final scroll = find.byType(Scrollable).first;
      await tester.dragUntilVisible(find.byType(RangeSwitch), scroll, const Offset(0, -200));
      expect(find.byType(RangeSwitch, skipOffstage: false), findsOneWidget);
      expect(find.byWidgetPredicate((w) => w is Segmented, skipOffstage: false), findsOneWidget, reason: 'no second, bare switch');
      expect(find.byType(FootNote, skipOffstage: false), findsNothing, reason: 'no explainer paragraph');
      expect(find.textContaining('Highlights', skipOffstage: false), findsNothing);
      final images = tester.widgetList<Image>(find.byType(Image, skipOffstage: false)).map((i) => '${i.image}').join(' ');
      expect(images, contains('assets/img/${covers[page]}.'));
    });
  }

  test('heart is green, sleep IR violet, effort red: the pod\'s own light', () {
    const t = AtriumColors.light;
    expect(t.heart, t.green);
    expect(t.balance, t.green);
    expect(t.sleep, t.ir);
    expect(t.effort, t.red);
    expect(t.bg, const Color(0xFFFFFFFF), reason: 'pure white, never cream');
    expect(t.card, const Color(0xFFF4F4F6));
  });

  test('every inner page has a different photo, and none is the Today sky', () {
    expect(covers.values.toSet().length, covers.length);
    expect(covers.values.where((c) => c.startsWith('sky')), isEmpty);
  });
}
