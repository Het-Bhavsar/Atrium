// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/device/presentation/device_widgets.dart';
import 'package:atrium/features/profile/data/profile_store.dart';
import 'package:atrium/features/profile/domain/profile.dart';
import 'package:atrium/features/profile/presentation/profile_form.dart';
import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';

/// Settings > profile: edit the name, age and size.
class ProfileView extends ConsumerStatefulWidget {
  const ProfileView({super.key});

  @override
  ConsumerState<ProfileView> createState() => _ProfileViewState();
}

class _ProfileViewState extends ConsumerState<ProfileView> {
  Profile? _edited;

  @override
  Widget build(BuildContext context) {
    final current = ref.read(profileProvider);
    final changed = _edited != null && _edited != current;
    return PlainPage(
      title: 'You',
      back: 'Settings',
      children: [
        const SizedBox(height: Space.s),
        ProfileForm(initial: current, onChanged: (p) => setState(() => _edited = p)),
        const ListFoot('Age and size set your heart rate zones and Effort. They stay on this phone; Atrium has no account and no cloud.'),
        const SizedBox(height: Space.l),
        PrimaryButton(
          'Save',
          onPressed: !changed
              ? null
              : () async {
                  await ref.read(profileProvider.notifier).save(_edited!);
                  if (!context.mounted) return;
                  showAtriumToast(context, 'Saved');
                  context.pop();
                },
        ),
      ],
    );
  }
}
