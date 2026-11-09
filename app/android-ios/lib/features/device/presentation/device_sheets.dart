// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/device/presentation/device_widgets.dart';
import 'package:atrium/features/pod/data/pod_session.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

/// Bottom sheet in the Atrium style: grab handle, serif title, muted paragraphs, then actions.
/// [actions] builds with the sheet's own context so buttons can pop it.
Future<T?> showAtriumSheet<T>(
  BuildContext context, {
  required String title,
  List<String> paragraphs = const [],
  Widget? top,
  Widget? content,
  required List<Widget> Function(BuildContext sheet) actions,
}) {
  final t = AtriumColors.of(context);
  return showModalBottomSheet<T>(
    context: context,
    isScrollControlled: true,
    useSafeArea: true,
    backgroundColor: t.bg,
    barrierColor: t.scrim,
    shape: const RoundedRectangleBorder(borderRadius: BorderRadius.vertical(top: Radius.circular(Radii.sheet))),
    builder: (sheet) {
      final acts = actions(sheet);
      return SingleChildScrollView(
        padding: EdgeInsets.fromLTRB(Space.xl, Space.s, Space.xl, MediaQuery.paddingOf(sheet).bottom + Space.xxl),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Center(
              child: Container(
                width: 36,
                height: 5,
                margin: const EdgeInsets.only(bottom: 14),
                decoration: BoxDecoration(color: t.hair, borderRadius: BorderRadius.circular(3)),
              ),
            ),
            if (top != null) Padding(padding: const EdgeInsets.only(top: 4, bottom: 14), child: top),
            Padding(
              padding: const EdgeInsets.only(top: 6, bottom: 8),
              child: Text(title, style: AtriumType.serifAt(24, color: t.ink, height: 1.12)),
            ),
            for (final p in paragraphs)
              Padding(
                padding: const EdgeInsets.only(bottom: Space.m),
                child: Text(p, style: AtriumType.body(t.ink2)),
              ),
            if (content != null)
              Padding(
                padding: const EdgeInsets.only(bottom: Space.l),
                child: content,
              ),
            for (var i = 0; i < acts.length; i++) ...[if (i > 0) const SizedBox(height: Space.m), acts[i]],
          ],
        ),
      );
    },
  );
}

List<Widget> Function(BuildContext) _ok() =>
    (sheet) => [PrimaryButton('OK', onPressed: () => Navigator.pop(sheet))];

/// How charging works (Device, "How charging works").
Future<void> showPlinthInfo(BuildContext context) => showAtriumSheet<void>(
  context,
  title: 'How charging works',
  paragraphs: const [
    'Take the pad off first: it covers the charging contacts. Then set the pod on the Plinth either way round. Magnets pull it into place.',
    "The Plinth's light ring breathes while it charges. A full charge lasts about 4 days. The pod charges between about 0 and 45 °C, more slowly when cold.",
  ],
  actions: _ok(),
);

/// What's in the box, with the opened box photo.
Future<void> showBoxContents(BuildContext context) => showAtriumSheet<void>(
  context,
  title: "What's in the box",
  top: const CutCard(image: 'box-open', semanticLabel: 'The Atrium box opened, the pod on its Plinth in the tray', maxImageHeight: 240, widthFactor: 1),
  content: const ListGroup(
    children: [
      ListRow(title: 'Atrium pod'),
      ListRow(title: 'Plinth charger'),
      ListRow(title: 'USB-C cable'),
      ListRow(title: 'Pad card', value: '6 skin pads'),
    ],
  ),
  actions: _ok(),
);

/// Turn off for storage. Only possible on the Plinth: Turn off, then lift. Off it, explains that first.
Future<void> showStorageOff(BuildContext context, WidgetRef ref) {
  final onPlinth = ref.read(appStateProvider).pod.onPlinth;
  if (!onPlinth) {
    return showAtriumSheet<void>(
      context,
      title: 'Put the pod on the Plinth first',
      paragraphs: const ['The pod can only be switched off while it sits on the Plinth. Take the pad off, set it on the Plinth, then turn it off here.'],
      actions: _ok(),
    );
  }
  return showAtriumSheet<void>(
    context,
    title: 'Turn off for storage?',
    paragraphs: const ['Tap Turn off, then lift the pod off the Plinth. It switches fully off and uses no battery until the next time it sits on the Plinth.'],
    actions: (sheet) => [
      PrimaryButton(
        'Turn off',
        onPressed: () async {
          Navigator.pop(sheet);
          final ok = !kRealData || await ref.read(podSessionProvider.notifier).shipMode();
          if (context.mounted) showAtriumToast(context, ok ? 'Now lift the pod off. It wakes on the Plinth.' : "The pod didn't answer. Try again near it.");
        },
      ),
      GreyButton('Cancel', onPressed: () => Navigator.pop(sheet)),
    ],
  );
}

/// Confirm resetting the pod. Resolves true when the user resets.
Future<bool> showResetPod(BuildContext context) async {
  final r = await showAtriumSheet<bool>(
    context,
    title: 'Reset the pod?',
    paragraphs: const ["This erases the pod's stored readings, settings and pairing, then restarts it. Readings already on this phone stay here."],
    actions: (sheet) => [
      DangerButton('Reset pod', onPressed: () => Navigator.pop(sheet, true)),
      GreyButton('Cancel', onPressed: () => Navigator.pop(sheet, false)),
    ],
  );
  if (r == true && context.mounted) showAtriumToast(context, 'Pod reset. Pair it again to keep using it.');
  return r ?? false;
}

/// Confirm unpairing the pod. Resolves true when the user forgets it.
Future<bool> showForgetPod(BuildContext context, [String name = 'Atrium 3F2A']) async {
  final r = await showAtriumSheet<bool>(
    context,
    title: 'Forget $name?',
    paragraphs: const ['The pod unpairs from this phone. Readings already synced stay here. Readings still on the pod sync if you pair it again.'],
    actions: (sheet) => [
      DangerButton('Forget pod', onPressed: () => Navigator.pop(sheet, true)),
      GreyButton('Cancel', onPressed: () => Navigator.pop(sheet, false)),
    ],
  );
  return r ?? false;
}
