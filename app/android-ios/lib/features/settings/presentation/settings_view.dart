// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/core/app_state.dart';
import 'package:atrium/core/config.dart';
import 'package:atrium/core/routes/app_router.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/graphics.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/device/presentation/device_sheets.dart';
import 'package:atrium/features/device/presentation/device_widgets.dart';
import 'package:atrium/features/insights/data/metrics_provider.dart';
import 'package:atrium/features/onboarding/presentation/onboarding_widgets.dart';
import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/domain/pod.dart';
import 'package:atrium/features/profile/data/profile_store.dart';
import 'package:atrium/features/settings/data/data_export.dart';
import 'package:atrium/features/settings/data/log_out.dart';
import 'package:flutter/cupertino.dart';
import 'package:flutter/material.dart' show showLicensePage;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:go_router/go_router.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';
import 'package:url_launcher/url_launcher.dart';

/// Labs features still in research (name, one line).
const List<(String, String)> kLabs = [
  ('Oxygen trend', 'Overnight relative trend on Sleep after 7 nights, never a percentage'),
  ('Thermal trends', 'Skin and pod temperature through the day'),
  ('Perfusion trend', 'How strong the pulse signal is over time'),
  ('Eating prompt', 'Asks before logging a meal it noticed'),
  ('Cycle view', 'Temperature-based phase estimate, opt-in'),
  ('Ask Atrium', 'Questions about your own data, opt-in'),
];

/// Settings tab: profile, notifications, measurement, data, Labs and about.
class SettingsView extends ConsumerWidget {
  const SettingsView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final me = ref.watch(profileProvider);
    final s = ref.watch(appStateProvider);
    final t = AtriumColors.of(context);
    final n = ref.read(appStateProvider.notifier);

    Widget sw(String key) => AtriumSwitch(value: s.toggles[key] ?? false, onChanged: (_) => n.toggle(key), label: key);
    final chev = Padding(
      padding: const EdgeInsets.only(left: 6),
      child: AIcon('chev', size: 16, color: t.ink3),
    );

    final fitLine = switch (s.glasses) {
      true => 'Glasses: just above the arm',
      false => 'No glasses: temporal hollow',
      null => 'Where the pod sits',
    };

    return PlainPage(
      title: 'Settings',
      children: [
        ListGroup(
          children: [
            Pressable(
              onTap: () => context.pushDetail('/profile'),
              child: Padding(
                padding: const EdgeInsets.symmetric(horizontal: Space.l, vertical: 14),
                child: Row(
                  children: [
                    Container(
                      width: 52,
                      height: 52,
                      decoration: BoxDecoration(shape: BoxShape.circle, color: t.track),
                      alignment: Alignment.center,
                      child: AIcon('person', size: 28, color: t.ink2),
                    ),
                    const SizedBox(width: 14),
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(me.hasName ? me.name : 'Add your name', style: AtriumType.bodyStrong(me.hasName ? t.ink : t.ink2).copyWith(fontSize: 18)),
                          const SizedBox(height: 2),
                          Text(me.summary.isEmpty ? 'Add your age and size' : me.summary, style: AtriumType.secondary(t.ink2).copyWith(fontSize: 13.5)),
                        ],
                      ),
                    ),
                    chev,
                  ],
                ),
              ),
            ),
          ],
        ),
        const ListFoot('Age and size set your heart rate zones and Effort.'),
        const SectionHeader('Notifications'),
        ListGroup(
          children: [
            ListRow(icon: 'balance', title: 'Morning summary', subtitle: '07:30', trailing: sw('morning')),
            ListRow(icon: 'moon', title: 'Bedtime reminder', subtitle: '22:45', trailing: sw('bedtime')),
            ListRow(icon: 'bolt', title: 'Pod battery', subtitle: "When it's below 20%", trailing: sw('charge')),
            ListRow(icon: 'pad', title: 'Pad reminders', subtitle: 'Every 2 days and at every charge', trailing: sw('pad')),
          ],
        ),
        const SectionHeader('Measurement'),
        ListGroup(
          children: [
            ListRow(icon: 'signal', title: 'Mode', value: s.mode.title, chevron: true, onTap: () => context.go('/device')),
            ListRow(
              icon: 'ruler',
              title: 'Fit guide',
              subtitle: fitLine,
              value: s.side == WearSide.left ? 'Left' : 'Right',
              chevron: true,
              onTap: () => context.push('/fit'),
            ),
            ListRow(icon: 'pad', title: 'Skin pad', subtitle: '${s.padsLeft} left on the card', chevron: true, onTap: () => context.pushDetail('/pad')),
          ],
        ),
        const SectionHeader('Data'),
        ListGroup(
          children: [
            ListRow(icon: 'health', title: 'Health', subtitle: 'Write heart rate, HRV and sleep', trailing: sw('health')),
            if (!s.isEvt)
              ListRow(icon: 'export', title: 'Export data', subtitle: 'Your readings as CSV', chevron: true, onTap: () => _showExport(context, ref)),
            const ListRow(icon: 'db', title: 'On this phone', value: '182 MB'),
            const ListRow(icon: 'globe', title: 'Units', value: '°C · metric', chevron: true),
            ListRow(title: 'Delete all data', danger: true, onTap: () => _showDelete(context, ref)),
          ],
        ),
        const SectionHeader('Labs'),
        ListGroup(
          children: [
            ListRow(
              icon: 'lab',
              title: 'Show Labs',
              subtitle: 'Early research features, not validated yet',
              trailing: AtriumSwitch(
                value: s.labs,
                label: 'labs',
                onChanged: (v) => n.update((x) => x.copyWith(labs: v)),
              ),
            ),
            if (s.labs)
              for (final (name, line) in kLabs)
                ListRow(
                  icon: 'lab',
                  title: name,
                  subtitle: line,
                  trailing: Padding(
                    padding: const EdgeInsets.only(left: Space.s),
                    child: Text('In research', style: AtriumType.secondary(t.ink2).copyWith(fontSize: 13, fontWeight: FontWeight.w500)),
                  ),
                  onTap: () => _showLabs(context),
                ),
          ],
        ),
        const ListFoot('Labs features stay off until their signal is checked on the bench. None of them gives a medical reading.'),
        const SectionHeader('About'),
        ListGroup(
          children: [
            ListRow(
              icon: 'today',
              title: 'Why Atrium',
              subtitle: 'The story behind the name and the mark',
              chevron: true,
              onTap: () => context.pushDetail('/story'),
            ),
            ListRow(
              icon: 'code',
              title: 'Open source',
              subtitle: 'App and firmware Apache-2.0, hardware CERN-OHL-S',
              trailing: chev,
              onTap: () => showLicensePage(context: context, applicationName: 'Atrium', applicationVersion: '$kAppVersion ($kAppBuild) · by $kMaker'),
            ),
            ListRow(
              leading: const LocalForgeLogo(),
              title: 'Made by $kMaker',
              value: kMakerSite,
              trailing: chev,
              onTap: () => launchUrl(Uri.https(kMakerSite), mode: LaunchMode.externalApplication),
            ),
            const ListRow(icon: 'info', title: 'App version', value: '$kAppVersion ($kAppBuild)'),
          ],
        ),
        const ListFoot(
          'Atrium by $kMaker · $kMakerSite. Atrium is for wellness and research. It does not diagnose, treat or prevent any condition. Talk to a doctor about any health concern.',
        ),
        const SizedBox(height: Space.l),
        ListGroup(
          children: [ListRow(title: 'Log out', danger: true, onTap: () => _showLogOut(context, ref))],
        ),
        ListFoot(
          me.hasName
              ? 'Logged in as ${me.name} on this phone. There is no account; logging out clears this phone.'
              : 'There is no account; logging out clears this phone.',
        ),
      ],
    );
  }

  /// No account and no cloud: logging out means this phone forgets you, your readings and the pod.
  static Future<void> _showLogOut(BuildContext context, WidgetRef ref) => showAtriumSheet<void>(
    context,
    title: 'Log out of Atrium?',
    paragraphs: const [
      "Atrium has no account, so logging out clears this phone: your name and profile, every reading, and the pod's pairing. It can't be undone. Export first if you want a copy.",
      'Readings still on the pod (about the last day) come back if you pair it again.',
    ],
    actions: (sheet) => [
      DangerButton(
        'Log out',
        onPressed: () async {
          Navigator.pop(sheet);
          await PhoneData.logOut(ref);
          if (context.mounted) context.go('/welcome');
        },
      ),
      GreyButton('Cancel', onPressed: () => Navigator.pop(sheet)),
    ],
  );

  /// Export (#22): the stored data as CSV, handed to the share sheet. The range is the real span of what is stored;
  /// the demo exports its demo days (it has no 30-second records).
  static Future<void> _showExport(BuildContext context, WidgetRef ref) async {
    final days = [...ref.read(metricsProvider).days];
    Uint8List? rows;
    if (kRealData) {
      try {
        rows = (await ref.read(epochStoreProvider.future)).rowBytes();
      } on Object {
        rows = null;
      }
    }
    final plan = ExportPlan(days: days, records: rows == null ? 0 : rows.length ~/ EpochStore.rowSize, recordRows: rows);
    final include = ValueNotifier<bool>(plan.hasRecords);
    if (!context.mounted) return;
    await showAtriumSheet<void>(
      context,
      title: 'Export your data',
      paragraphs: [
        if (plan.isEmpty) const NothingToExport().toString() else "Your daily summaries and the pod's 30-second records as CSV, from what is stored on this phone. You choose where they go; the columns are listed in the app's README.",
      ],
      content: ListGroup(
        children: [
          ListRow(title: 'Range', value: plan.rangeLabel),
          ListRow(
            title: '30-second records',
            subtitle: plan.hasRecords ? '${plan.records} records' : null,
            value: plan.hasRecords ? null : (kRealData ? 'None stored' : 'Not in the demo'),
            trailing: plan.hasRecords
                ? ValueListenableBuilder<bool>(
                    valueListenable: include,
                    builder: (_, on, _) => AtriumSwitch(value: on, onChanged: (v) => include.value = v, label: 'Include 30-second records'),
                  )
                : null,
          ),
        ],
      ),
      actions: (sheet) => [
        PrimaryButton(
          'Export',
          onPressed: plan.isEmpty
              ? null
              : () {
                  Navigator.pop(sheet);
                  unawaited(_export(context, plan, records: include.value));
                },
        ),
      ],
    );
  }

  static Future<void> _export(BuildContext context, ExportPlan plan, {required bool records}) async {
    final box = context.findRenderObject() as RenderBox?;
    final origin = box != null && box.hasSize ? box.localToGlobal(Offset.zero) & box.size : null;
    try {
      final tmp = await getTemporaryDirectory();
      final dir = Directory('${tmp.path}/atrium-export');
      if (await dir.exists()) await dir.delete(recursive: true);
      final files = await writeExport(plan, dir, withRecords: records);
      await SharePlus.instance.share(
        ShareParams(
          files: [for (final f in files) XFile(f.path, mimeType: 'text/csv')],
          subject: 'Atrium data export',
          sharePositionOrigin: origin,
        ),
      );
    } on NothingToExport catch (e) {
      if (context.mounted) showAtriumToast(context, '$e');
    } on Object {
      if (context.mounted) showAtriumToast(context, "Couldn't prepare the export. Try again.");
    }
  }

  static Future<void> _showDelete(BuildContext context, WidgetRef ref) => showAtriumSheet<void>(
    context,
    title: 'Delete all data?',
    paragraphs: const ["This removes every reading from this phone. It can't be undone. Export first if you want a copy."],
    actions: (sheet) => [
      DangerButton(
        'Delete',
        onPressed: () async {
          Navigator.pop(sheet);
          await PhoneData.deleteReadings(ref);
          if (context.mounted) showAtriumToast(context, 'Readings deleted from this phone');
        },
      ),
      GreyButton('Cancel', onPressed: () => Navigator.pop(sheet)),
    ],
  );

  static Future<void> _showLabs(BuildContext context) => showAtriumSheet<void>(
    context,
    title: 'Still in research',
    paragraphs: const [
      "This feature is being checked on the bench before it's built. Labs features never give a medical reading, and each one stays off until you turn it on.",
    ],
    actions: (sheet) => [PrimaryButton('OK', onPressed: () => Navigator.pop(sheet))],
  );
}

/// iOS-style switch in the theme green.
class AtriumSwitch extends StatelessWidget {
  const AtriumSwitch({required this.value, required this.onChanged, this.label, super.key});
  final bool value;
  final ValueChanged<bool> onChanged;
  final String? label;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Padding(
      padding: const EdgeInsets.only(left: Space.s),
      child: Semantics(
        label: label,
        child: CupertinoSwitch(value: value, onChanged: onChanged, activeTrackColor: t.green, inactiveTrackColor: t.track),
      ),
    );
  }
}
