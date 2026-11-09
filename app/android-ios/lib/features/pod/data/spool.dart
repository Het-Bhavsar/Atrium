// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/pod/data/epoch_store.dart';
import 'package:atrium/features/pod/data/log_record.dart';
import 'package:atrium/features/pod/data/pod_link.dart';
import 'package:flutter/foundation.dart' show compute;

/// Background sync (#20): when the app isn't running, native code (Android `PodSyncWorker`, iOS `PodHost` woken by
/// CoreBluetooth state restoration) downloads the log the same way the app does and leaves the raw pages here. Native
/// only moves bytes; decoding stays in Dart, so there is one codec. The app takes the files in at launch and whenever
/// it comes to the front.
///
/// File `<support dir>/pod_spool/<unix>-<first page>.bin`, little-endian:
///   0 u32 magic 'ASP1' (0x31505341), 4 u32 first page, 8 u32 pod clock read at connect (bit 31 = uptime, the pod had
///   lost its clock; 0 when unread), 12 u32 phone Unix time at connect, 16.. whole 4 KB pages from the first page on.
/// Native reads the next page to fetch from `epochs.bin.cursor`, the same cursor the app keeps, and moves it on after
/// a background sync, so both resume from one place (a stale copy in the app only re-reads a page; rows are unique). A file is written to `.tmp` and renamed, so a half-written one is never read.
abstract final class Spool {
  static const int magic = 0x31505341;
  static const int headerSize = 16;
  static const String dirName = 'pod_spool';

  static Directory dirIn(String supportDir) => Directory('$supportDir/$dirName');

  /// One spool file in the format above (tests, and the shape native writes).
  static Uint8List encode({required int firstPage, required int podClock, required int phoneUnix, required Uint8List pages}) {
    final h = ByteData(headerSize)
      ..setUint32(0, magic, Endian.little)
      ..setUint32(4, firstPage, Endian.little)
      ..setUint32(8, podClock, Endian.little)
      ..setUint32(12, phoneUnix, Endian.little);
    return (BytesBuilder(copy: false)
          ..add(h.buffer.asUint8List())
          ..add(pages))
        .takeBytes();
  }

  /// Decodes one file into dated epochs, the cursor it reached and when it was read; null when it isn't a spool file.
  static SpoolBatch? decode(Uint8List b) {
    if (b.length < headerSize) return null;
    final d = ByteData.sublistView(b);
    if (d.getUint32(0, Endian.little) != magic) return null;
    final first = d.getUint32(4, Endian.little);
    final clock = d.getUint32(8, Endian.little);
    final at = d.getUint32(12, Endian.little);
    final pages = Uint8List.sublistView(b, headerSize, headerSize + (b.length - headerSize) ~/ pageSize * pageSize);
    var epochs = decodeBlocks(pages);
    if (clock & uptimeBit != 0 && at > 0) epochs = PodLink.rebaseLastRun(epochs, uptimeNow: clock & ~uptimeBit, unixNow: at);
    return SpoolBatch(
      epochs: epochs,
      cursor: PodLink.nextCursor(first: first, bytes: pages.length, cursor: first),
      at: DateTime.fromMillisecondsSinceEpoch(at * 1000),
      pages: pages.length ~/ pageSize,
    );
  }

  /// Takes every spool file in [dir] into [store], oldest first, and deletes it. Decoding runs off the UI isolate.
  /// Returns the readings added and the latest read time (null when there was nothing).
  static Future<(int, DateTime?)> ingest(Directory dir, EpochStore store) async {
    if (!dir.existsSync()) return (0, null);
    final files = dir.listSync().whereType<File>().where((f) => f.path.endsWith('.bin')).toList()..sort((a, b) => a.path.compareTo(b.path));
    if (files.isEmpty) return (0, null);
    final paths = [for (final f in files) f.path];
    final batches = await compute(_decodeFiles, paths);
    var added = 0;
    DateTime? latest;
    for (var i = 0; i < files.length; i++) {
      final b = batches[i];
      if (b != null) {
        added += await store.add(b.epochs);
        if (b.pages > 0 && b.cursor > store.syncedToPage) await store.setCursor(b.cursor);
        if (latest == null || b.at.isAfter(latest)) latest = b.at;
      }
      files[i].deleteSync();
    }
    if (latest != null && (store.lastSyncAt == null || latest.isAfter(store.lastSyncAt!))) await store.setLastSync(latest);
    return (added, latest);
  }
}

class SpoolBatch {
  const SpoolBatch({required this.epochs, required this.cursor, required this.at, required this.pages});
  final List<TimedEpoch> epochs;
  final int cursor;
  final DateTime at;
  final int pages;
}

List<SpoolBatch?> _decodeFiles(List<String> paths) => [for (final p in paths) Spool.decode(File(p).readAsBytesSync())];
