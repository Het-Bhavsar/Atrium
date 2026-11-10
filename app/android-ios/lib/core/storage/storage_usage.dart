// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';

import 'package:flutter/foundation.dart' show immutable;

/// What Atrium keeps on this phone, by kind, in bytes ("On this phone" in Settings).
@immutable
class StorageUsage {
  const StorageUsage({this.readings = 0, this.waiting = 0, this.raw = 0, this.app = 0, this.exports = 0});

  /// The pod's 30-second records (`epochs.bin`, with its cursor and sync time).
  final int readings;

  /// Pages native background sync downloaded that the app hasn't taken in yet (`pod_spool`).
  final int waiting;

  /// Raw signal recordings (`raw`).
  final int raw;

  /// Settings, profile and the saved screen numbers (`hive`).
  final int app;

  /// Copies made for sharing: exports waiting in the cache folder until the system clears it.
  final int exports;

  int get total => readings + waiting + raw + app + exports;

  /// [support] is the app support folder; [cache] the temporary (cache) folder.
  static StorageUsage measure(Directory support, Directory? cache) {
    int size(FileSystemEntity e) {
      if (e is File) return e.existsSync() ? e.lengthSync() : 0;
      if (e is Directory && e.existsSync()) {
        var n = 0;
        for (final f in e.listSync(recursive: true, followLinks: false).whereType<File>()) {
          n += f.lengthSync();
        }
        return n;
      }
      return 0;
    }

    String p(String name) => '${support.path}${Platform.pathSeparator}$name';
    final readings = ['epochs.bin', 'epochs.bin.cursor', 'epochs.bin.synced'].fold(0, (a, n) => a + size(File(p(n))));
    final exports = cache == null
        ? 0
        : size(Directory('${cache.path}${Platform.pathSeparator}atrium-export')) + size(Directory('${cache.path}${Platform.pathSeparator}atrium-raw-export'));
    return StorageUsage(
      readings: readings,
      waiting: size(Directory(p('pod_spool'))),
      raw: size(Directory(p('raw'))),
      app: size(Directory(p('hive'))),
      exports: exports,
    );
  }
}

/// "1.2 MB", "340 KB", "12 bytes" (decimal units, as phones show storage).
String formatBytes(int n) {
  if (n < 1000) return '$n ${n == 1 ? 'byte' : 'bytes'}';
  const units = ['KB', 'MB', 'GB'];
  var v = n / 1000;
  var u = 0;
  while (v >= 1000 && u < units.length - 1) {
    v /= 1000;
    u++;
  }
  return '${v >= 100 ? v.toStringAsFixed(0) : v.toStringAsFixed(1)} ${units[u]}';
}
