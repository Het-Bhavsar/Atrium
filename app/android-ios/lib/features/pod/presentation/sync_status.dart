// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'package:atrium/core/theme/tokens.dart';
import 'package:atrium/core/widgets/components.dart';
import 'package:atrium/features/pod/domain/sync_age.dart';
import 'package:flutter/material.dart';

/// Real-data builds: the age of the last log sync, on Today and Device (#20). Red once it is overdue.
class LastSyncedLine extends StatelessWidget {
  const LastSyncedLine({required this.lastSync, required this.now, this.center = false, super.key});
  final DateTime? lastSync;
  final DateTime now;
  final bool center;

  @override
  Widget build(BuildContext context) {
    final t = AtriumColors.of(context);
    return Text(
      lastSyncedSentence(lastSync, now),
      textAlign: center ? TextAlign.center : TextAlign.start,
      style: AtriumType.secondary(syncOverdue(lastSync, now) ? t.red : t.ink2),
    );
  }
}

/// The warning once the last sync is older than [syncWarnAfter]: the pod's log starts overwriting at about 20 h.
class SyncDueNotice extends StatelessWidget {
  const SyncDueNotice({required this.lastSync, required this.now, this.onTap, super.key});
  final DateTime lastSync;
  final DateTime now;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) =>
      NoticeCard(icon: 'storage', iconColor: AtriumColors.of(context).red, title: 'Sync soon', body: syncOverdueText(lastSync, now), onTap: onTap);
}
