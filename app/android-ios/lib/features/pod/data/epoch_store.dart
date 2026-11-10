// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/pod/data/log_record.dart';
import 'package:flutter/foundation.dart' show compute;
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

  /// When the phone last had every reading the pod held (a full sync, or live records arriving); null before the first.
  DateTime? lastSyncAt;

  static Future<EpochStore> open() async {
    final dir = await getApplicationSupportDirectory();
    final store = EpochStore(File('${dir.path}/epochs.bin'));
    await store.load();
    return store;
  }

  File get _cursorFile => File('${file.path}.cursor');
  File get _syncedFile => File('${file.path}.synced');

  /// Files above this are parsed off the UI isolate (about 3 days of readings), so a launch with months of
  /// readings doesn't stall the first frames.
  static const int isolateAbove = 128 * 1024;

  Future<void> load() async {
    _rows.clear();
    if (await file.exists()) {
      final b = await file.readAsBytes();
      _rows.addAll(b.length > isolateAbove ? await compute(parseRows, b) : parseRows(b));
    }
    if (await _cursorFile.exists()) syncedToPage = int.tryParse(await _cursorFile.readAsString()) ?? 0;
    lastSyncAt = null;
    if (await _syncedFile.exists()) {
      final s = int.tryParse((await _syncedFile.readAsString()).trim());
      if (s != null) lastSyncAt = DateTime.fromMillisecondsSinceEpoch(s * 1000);
    }
  }

  /// Rows by time; a later row for the same time wins, as on load before.
  static Map<int, Uint8List> parseRows(Uint8List b) {
    final out = <int, Uint8List>{};
    final bd = ByteData.sublistView(b);
    for (var o = 0; o + rowSize <= b.length; o += rowSize) {
      out[bd.getUint32(o, Endian.little)] = Uint8List.fromList(b.sublist(o + 4, o + rowSize));
    }
    return out;
  }

  /// Unix seconds in `epochs.bin.synced`, so Today and Device show the age of the last sync after a restart.
  Future<void> setLastSync(DateTime at) async {
    lastSyncAt = at;
    await _syncedFile.writeAsString('${at.millisecondsSinceEpoch ~/ 1000}', flush: true);
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

  /// Every row in the file format (u32 Unix time, then the 12 record bytes), oldest first: for the data export.
  Uint8List rowBytes() {
    final keys = _rows.keys.toList()..sort();
    final out = Uint8List(keys.length * rowSize);
    final bd = ByteData.sublistView(out);
    for (var i = 0; i < keys.length; i++) {
      bd.setUint32(i * rowSize, keys[i], Endian.little);
      out.setRange(i * rowSize + 4, (i + 1) * rowSize, _rows[keys[i]]!);
    }
    return out;
  }

  Future<void> clear() async {
    _rows.clear();
    syncedToPage = 0;
    lastSyncAt = null;
    if (await file.exists()) await file.delete();
    if (await _cursorFile.exists()) await _cursorFile.delete();
    if (await _syncedFile.exists()) await _syncedFile.delete();
  }
}
