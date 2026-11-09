// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:atrium/features/pod/data/pod_permissions.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 10 (#7): three nights to learn the usual range, then into the app.
class BaselineView extends ConsumerWidget {
  const BaselineView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final t = AtriumColors.of(context);
    void finish() {
      ref.read(appStateProvider.notifier).update((s) => s.copyWith(onboarded: true));
      context.go('/today');
    }

    return OnPhotoScaffold(
      image: 'assets/img/sky-night.jpg',
      content: [
        Expanded(
          child: Center(
            child: Padding(
              padding: const EdgeInsets.only(top: 40),
              child: SizedBox(width: 190, child: ArchGauge(value: 0, color: t.balance, learning: true, onPhoto: true)),
            ),
          ),
        ),
        const ObTitle('Atrium learns you first', color: AtriumColors.onPhoto),
        ObLede(
          "Wear it for three nights. Recovery and Sleep start on the third morning, measured against your own usual range. Until then you'll see the raw readings.",
          color: AtriumColors.onPhoto.withValues(alpha: 0.75),
        ),
        Row(
          children: [
            for (var i = 0; i < 3; i++) ...[
              if (i > 0) const SizedBox(width: 6),
              Expanded(
                child: Container(
                  height: 6,
                  decoration: BoxDecoration(color: AtriumColors.onPhoto.withValues(alpha: 0.22), borderRadius: BorderRadius.circular(3)),
                ),
              ),
            ],
          ],
        ),
        const SizedBox(height: 8),
        Text('0 of 3 nights', style: AtriumType.caption(AtriumColors.onPhoto.withValues(alpha: 0.55))),
        const SizedBox(height: 22),
      ],
      footer: [
        PrimaryButton(
          'Remind me at bedtime',
          onPhoto: true,
          onPressed: () async {
            // Android 13+ asks for notifications here; a refusal turns the bedtime reminder off, nothing else.
            final ok = !kRealData || await PodPermissions.requestNotifications();
            ref.read(appStateProvider.notifier).update((s) => s.copyWith(toggles: {...s.toggles, 'bedtime': ok}));
            if (context.mounted) finish();
          },
        ),
        PlainButton('Not now', color: AtriumColors.onPhoto, onPressed: finish),
      ],
    );
  }
}
