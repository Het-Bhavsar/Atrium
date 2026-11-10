// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:async';
import 'dart:io';

import 'package:atrium/core/storage/storage_usage.dart';
import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/core/widgets/scaffolds.dart';
import 'package:atrium/features/device/presentation/device_sheets.dart';
import 'package:atrium/features/device/presentation/device_widgets.dart';
import 'package:atrium/features/research/data/raw_recorder.dart';
import 'package:atrium/features/research/data/raw_store.dart';
import 'package:atrium/features/research/domain/raw_packets.dart';
import 'package:atrium/features/settings/data/storage_provider.dart';
import 'package:flutter/foundation.dart' show compute;
import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';
import 'package:share_plus/share_plus.dart';

/// "3 h 12 min", "42 min", "40 s".
String durationLabel(Duration d) {
  if (d.inMinutes < 1) return '${d.inSeconds} s';
  if (d.inHours < 1) return '${d.inMinutes} min';
  return '${d.inHours} h ${d.inMinutes % 60} min';
}

String _when(DateTime? t) {
  if (t == null) return 'Unknown start';
  const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
  return '${t.day} ${months[t.month - 1]}, ${t.hour.toString().padLeft(2, '0')}:${t.minute.toString().padLeft(2, '0')}';
}

String _count(int n) {
  final s = '$n';
  final b = StringBuffer();
  for (var i = 0; i < s.length; i++) {
    if (i > 0 && (s.length - i) % 3 == 0) b.write(',');
    b.write(s[i]);
  }
  return b.toString();
}

/// One line for the list: duration, samples and gaps.
String sessionLine(RawSession s) {
  final ppg = [for (final e in s.ppgSamples.entries) '${e.key} ${_count(e.value)}'].join(', ');
  final missed = s.ppgMissed + s.motionMissed;
  return [
    durationLabel(s.duration),
    if (ppg.isNotEmpty) ppg,
    'motion ${_count(s.motionSamples)}',
    if (missed == 0)
      'no gaps'
    else
      '$missed missed ${missed == 1 ? 'packet' : 'packets'} in ${s.ppgGaps + s.motionGaps} ${s.ppgGaps + s.motionGaps == 1 ? 'gap' : 'gaps'}',
  ].join(' · ');
}

/// Raw recordings: what is being recorded now, and every session on this phone, newest first.
class RecordingsView extends ConsumerWidget {
  const RecordingsView({super.key});

  @override
  Widget build(BuildContext context, WidgetRef ref) {
    final rec = ref.watch(rawRecorderProvider);
    final sessions = ref.watch(rawSessionsProvider);
    final list = sessions.value ?? const <RawSession>[];
    final used = list.fold(0, (a, s) => a + s.bytes);
    return PlainPage(
      title: 'Raw recordings',
      back: 'Settings',
      children: [
        if (rec.capReached)
          const NoticeCard(
            icon: 'storage',
            title: 'Recording stopped at the limit',
            body: 'Raw recordings reached 500 MB, so recording stopped. Nothing was deleted. Export what you need, then delete recordings to record again.',
          ),
        if (rec.capReached) const SizedBox(height: Space.m),
        ListGroup(
          children: [
            ListRow(
              icon: 'wave',
              title: rec.recording ? 'Recording now' : (rec.enabled ? 'Waiting for the pod' : 'Recording is off'),
              subtitle: rec.recording ? sessionLine(rec.current!) : (rec.enabled ? 'Records whenever the pod is connected' : 'Turn it on in Settings'),
              value: rec.recording ? formatBytes(rec.current!.bytes) : null,
            ),
            ListRow(icon: 'storage', title: 'Stored', value: '${formatBytes(used)} of ${formatBytes(RawStore.defaultCapBytes)}'),
          ],
        ),
        const SectionHeader('Sessions'),
        if (sessions.isLoading && list.isEmpty)
          const ListGroup(children: [ListRow(title: 'Reading recordings…')])
        else if (list.isEmpty)
          const ListGroup(children: [ListRow(title: 'No recordings yet')])
        else
          ListGroup(
            children: [
              for (final s in list)
                ListRow(
                  title: '${_when(s.start)}${s.demo ? ' · demo' : ''}${s.open ? ' · recording' : ''}',
                  subtitle: sessionLine(s),
                  value: formatBytes(s.bytes),
                  chevron: !s.open,
                  onTap: s.open ? null : () => _showSession(context, ref, s),
                ),
            ],
          ),
        const ListFoot(
          "Each session is one connection: the pod's raw light and motion samples exactly as they arrived. Export gives decoded CSV for the light and motion samples and the raw session file. Gaps count notifications the phone missed, from the pod's sequence numbers.",
        ),
      ],
    );
  }

  static Future<void> _showSession(BuildContext context, WidgetRef ref, RawSession s) => showAtriumSheet<void>(
    context,
    title: _when(s.start),
    paragraphs: [
      [
        'Lasted ${durationLabel(s.duration)}.',
        if (s.ppgTotal > 0)
          'Light samples: ${[for (final e in s.ppgSamples.entries) '${e.key} ${_count(e.value)}'].join(', ')}${s.ppgRateHz != null ? ' at ${s.ppgRateHz} Hz' : ''}.',
        'Motion samples: ${_count(s.motionSamples)}.',
        'Missed: ${s.ppgMissed} light and ${s.motionMissed} motion notifications.',
        if (s.demo) "Recorded from the demo's synthetic signal, not a pod.",
      ].join(' '),
    ],
    actions: (sheet) => [
      PrimaryButton(
        'Export',
        onPressed: () {
          Navigator.pop(sheet);
          unawaited(exportSession(context, s).then((_) => ref.invalidate(storageUsageProvider)));
        },
      ),
      DangerButton(
        'Delete',
        onPressed: () async {
          Navigator.pop(sheet);
          await ref.read(rawRecorderProvider.notifier).delete(s);
        },
      ),
    ],
  );
}

/// Writes the session's PPG and motion CSVs beside a copy of its raw file and opens the share sheet.
Future<void> exportSession(BuildContext context, RawSession s) async {
  final box = context.findRenderObject() as RenderBox?;
  final origin = box != null && box.hasSize ? box.localToGlobal(Offset.zero) & box.size : null;
  try {
    final files = await writeSessionExport(s, Directory('${(await getTemporaryDirectory()).path}/atrium-raw-export'));
    await SharePlus.instance.share(
      ShareParams(
        files: [for (final f in files) XFile(f.path, mimeType: f.path.endsWith('.csv') ? 'text/csv' : 'application/octet-stream')],
        subject: 'Atrium raw recording',
        sharePositionOrigin: origin,
      ),
    );
  } on Object {
    if (context.mounted) showAtriumToast(context, "Couldn't prepare the export. Try again.");
  }
}

/// The export files for [s] in [dir] (emptied first): `<id>-ppg.csv`, `<id>-motion.csv` and `<id>.bin`.
Future<List<File>> writeSessionExport(RawSession s, Directory dir) async {
  if (dir.existsSync()) dir.deleteSync(recursive: true);
  dir.createSync(recursive: true);
  final bytes = await s.file.readAsBytes();
  final (ppg, motion) = bytes.length > 256 * 1024 ? await compute(sessionCsvs, bytes) : sessionCsvs(bytes);
  final name = s.demo ? '${s.id}-demo' : s.id;
  return [
    await File('${dir.path}/$name-ppg.csv').writeAsString(ppg, flush: true),
    await File('${dir.path}/$name-motion.csv').writeAsString(motion, flush: true),
    await s.file.copy('${dir.path}/$name.bin'),
  ];
}
