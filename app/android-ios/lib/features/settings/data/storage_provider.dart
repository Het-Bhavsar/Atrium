// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';

import 'package:atrium/core/storage/storage_usage.dart';
import 'package:atrium/features/research/data/raw_recorder.dart';
import 'package:flutter/foundation.dart' show compute;
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:path_provider/path_provider.dart';

/// What this phone holds for Atrium, measured on disk; refreshed as recordings grow or are deleted, and whenever
/// [refreshStorage] is called (after a delete or an export).
final storageUsageProvider = FutureProvider<StorageUsage>((ref) async {
  ref
    ..watch(rawRecorderProvider.select((s) => s.listVersion))
    ..watch(rawRecorderProvider.select((s) => (s.current?.bytes ?? 0) ~/ (1000 * 1000)));
  final support = await getApplicationSupportDirectory();
  final cache = await getTemporaryDirectory();
  return compute(_measure, (support.path, cache.path));
});

StorageUsage _measure((String, String) dirs) => StorageUsage.measure(Directory(dirs.$1), Directory(dirs.$2));

void refreshStorage(WidgetRef ref) => ref.invalidate(storageUsageProvider);

/// Deletes the copies made for sharing (data and raw exports in the cache folder): they hold readings too.
Future<void> clearShareCopies() async {
  try {
    final cache = (await getTemporaryDirectory()).path;
    for (final name in ['atrium-export', 'atrium-raw-export']) {
      final d = Directory('$cache/$name');
      if (d.existsSync()) await d.delete(recursive: true);
    }
  } on Object {
    // Nothing to clear.
  }
}
