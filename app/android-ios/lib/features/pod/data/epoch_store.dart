// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/pod/data/log_record.dart';
import 'package:path_provider/path_provider.dart';

/// On-phone store of synced epochs: an append-only file of 16-byte rows (u32 Unix time, then the
/// 12 record bytes exactly as logged), so it re-decodes with `log_record.dart`. Rows are unique by time.
/// Readings stay on the phone (#23); "Delete all data" removes the file.
class EpochStore {
  EpochStore(this.file);

  static const int rowSize = 16;
  final File file;
  final Map<int, Uint8List> _rows = {};
  int syncedToPage = 0;

  static Future<EpochStore> open() async {
    final dir = await getApplicationSupportDirectory();
    final store = EpochStore(File('${dir.path}/epochs.bin'));
    await store.load();
    return store;
  }

  File get _cursorFile => File('${file.path}.cursor');

  Future<void> load() async {
    _rows.clear();
    if (await file.exists()) {
      final b = await file.readAsBytes();
      final bd = ByteData.sublistView(b);
      for (var o = 0; o + rowSize <= b.length; o += rowSize) {
        _rows[bd.getUint32(o, Endian.little)] = Uint8List.fromList(b.sublist(o + 4, o + rowSize));
      }
    }
    if (await _cursorFile.exists()) syncedToPage = int.tryParse(await _cursorFile.readAsString()) ?? 0;
  }

  /// Adds epochs with a real clock (uptime-stamped ones are rebased by the app first, or dropped).
  /// Returns how many were new.
  Future<int> add(Iterable<TimedEpoch> epochs) async {
    final out = BytesBuilder();
    var added = 0;
    for (final e in epochs) {
      if (e.isUptime || e.raw == null || _rows.containsKey(e.time)) continue;
      _rows[e.time] = e.raw!;
      final row = ByteData(rowSize)..setUint32(0, e.time, Endian.little);
      for (var i = 0; i < recordSize; i++) {
        row.setUint8(4 + i, e.raw![i]);
      }
      out.add(row.buffer.asUint8List());
      added++;
    }
    if (added > 0) await file.writeAsBytes(out.takeBytes(), mode: FileMode.append, flush: true);
    return added;
  }

  Future<void> setCursor(int page) async {
    syncedToPage = page;
    await _cursorFile.writeAsString('$page', flush: true);
  }

  List<TimedEpoch> all() {
    final keys = _rows.keys.toList()..sort();
    return [for (final t in keys) TimedEpoch(t, false, Epoch.decode(_rows[t]!), _rows[t])];
  }

  int get count => _rows.length;

  Future<void> clear() async {
    _rows.clear();
    syncedToPage = 0;
    if (await file.exists()) await file.delete();
    if (await _cursorFile.exists()) await _cursorFile.delete();
  }
}
