// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:atrium/features/profile/data/profile_store.dart';
import 'package:atrium/features/profile/domain/profile.dart';
import 'package:atrium/features/profile/presentation/profile_form.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Onboarding 2: who is wearing Atrium. No account: the name and sizes stay on this phone.
class YouView extends ConsumerStatefulWidget {
  const YouView({super.key});

  @override
  ConsumerState<YouView> createState() => _YouViewState();
}

class _YouViewState extends ConsumerState<YouView> {
  Profile? _edited;

  @override
  Widget build(BuildContext context) {
    final current = ref.read(profileProvider);
    return OnboardingScaffold(
      content: [
        const StepsDots(1),
        const ObTitle('About you'),
        const ObLede("There's no account. Your name, age and size stay on this phone and set your heart rate zones and Effort."),
        ProfileForm(initial: current == Profile.demo ? const Profile() : current, onChanged: (p) => setState(() => _edited = p)),
        const Spacer(),
      ],
      footerGap: 24,
      footer: [
        PrimaryButton(
          'Continue',
          onPressed: _edited == null
              ? null
              : () async {
                  await ref.read(profileProvider.notifier).save(_edited!);
                  if (context.mounted) await context.push('/bluetooth');
                },
        ),
      ],
    );
  }
}
