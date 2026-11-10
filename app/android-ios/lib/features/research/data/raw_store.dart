// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/research/domain/raw_packets.dart';
import 'package:flutter/foundation.dart' show compute, immutable;

/// One recorded session as the list shows it.
@immutable
class RawSession {
  const RawSession({
    required this.id,
    required this.file,
    required this.bytes,
    this.startMs,
    this.endMs,
    this.ppgSamples = const {},
    this.motionSamples = 0,
    this.ppgMissed = 0,
    this.ppgGaps = 0,
    this.motionMissed = 0,
    this.motionGaps = 0,
    this.mode,
    this.ppgRateHz,
    this.motionRateHz,
    this.demo = false,
    this.open = false,
  });

  /// The file name without extension, `raw-<start ms>`.
  final String id;
  final File file;
  final int bytes;
  final int? startMs;
  final int? endMs;
  final Map<String, int> ppgSamples;
  final int motionSamples;
  final int ppgMissed;
  final int ppgGaps;
  final int motionMissed;
  final int motionGaps;
  final int? mode;
  final int? ppgRateHz;
  final double? motionRateHz;

  /// Recorded from the demo build's synthetic signal, not a pod.
  final bool demo;

  /// Still being written.
  final bool open;

  DateTime? get start => startMs == null ? null : DateTime.fromMillisecondsSinceEpoch(startMs!);
  Duration get duration => startMs == null || endMs == null ? Duration.zero : Duration(milliseconds: endMs! - startMs!);
  int get ppgTotal => ppgSamples.values.fold(0, (a, b) => a + b);

  static RawSession fromStats(String id, File file, int bytes, SessionStats s, {bool demo = false, bool open = false}) => RawSession(
    id: id,
    file: file,
    bytes: bytes,
    startMs: s.startMs,
    endMs: s.endMs,
    ppgSamples: Map.unmodifiable(s.ppgSamples),
    motionSamples: s.motionSamples,
    ppgMissed: s.ppgGaps.missed,
    ppgGaps: s.ppgGaps.gaps,
    motionMissed: s.motionGaps.missed,
    motionGaps: s.motionGaps.gaps,
    mode: s.mode,
    ppgRateHz: s.ppgRateHz,
    motionRateHz: s.motionRateHz,
    demo: demo,
    open: open,
  );

  static RawSession? fromJson(String id, File file, int bytes, Map<String, dynamic> j) {
    int? i(String k) => j[k] is num ? (j[k] as num).toInt() : null;
    final ppg = j['ppg_samples'];
    return RawSession(
      id: id,
      file: file,
      bytes: bytes,
      startMs: i('start_ms'),
      endMs: i('end_ms'),
      ppgSamples: ppg is Map ? {for (final e in ppg.entries) '${e.key}': e.value is num ? (e.value as num).toInt() : 0} : const {},
      motionSamples: i('motion_samples') ?? 0,
      ppgMissed: i('ppg_missed') ?? 0,
      ppgGaps: i('ppg_gaps') ?? 0,
      motionMissed: i('motion_missed') ?? 0,
      motionGaps: i('motion_gaps') ?? 0,
      mode: i('mode'),
      ppgRateHz: i('ppg_rate_hz'),
      motionRateHz: j['motion_rate_hz'] is num ? (j['motion_rate_hz'] as num).toDouble() : null,
      demo: j['demo'] == true,
    );
  }
}

/// The recording would pass the store's size limit: recording stops, nothing is deleted.
class RawCapReached implements Exception {
  const RawCapReached(this.capBytes);
  final int capBytes;
  @override
  String toString() => 'Raw recordings reached the ${capBytes ~/ (1000 * 1000)} MB limit.';
}

/// Raw recordings on this phone: `<support>/raw/raw-<start ms>.bin` (the session file, `RawFile`) and a `.json`
/// summary beside it, rewritten while recording and at the end. Kept out of backups with the rest of the data folder.
class RawStore {
  RawStore(this.dir, {this.capBytes = defaultCapBytes});

  /// 500 MB: about a month of continuous recording in the everyday modes (about 0.7 MB an hour), or about two days in
  /// the 200 Hz research modes.
  static const int defaultCapBytes = 500 * 1000 * 1000;

  final Directory dir;
  final int capBytes;

  static Directory dirIn(String supportDir) => Directory('$supportDir${Platform.pathSeparator}raw');

  /// Bytes the session files use (the summaries are a few hundred bytes each and not counted against the limit).
  int usedBytes() {
    if (!dir.existsSync()) return 0;
    var n = 0;
    for (final f in dir.listSync().whereType<File>()) {
      if (f.path.endsWith('.bin')) n += f.lengthSync();
    }
    return n;
  }

  /// Every byte in the folder, summaries included (for "On this phone").
  int totalBytes() {
    if (!dir.existsSync()) return 0;
    var n = 0;
    for (final f in dir.listSync(recursive: true).whereType<File>()) {
      n += f.lengthSync();
    }
    return n;
  }

  /// The sessions, newest first. A file with no summary (the app stopped mid-session) is summarised from its bytes.
  Future<List<RawSession>> list({String? openId}) async {
    if (!dir.existsSync()) return const [];
    final out = <RawSession>[];
    for (final f in dir.listSync().whereType<File>().where((f) => f.path.endsWith('.bin'))) {
      final id = f.uri.pathSegments.last.replaceAll('.bin', '');
      final bytes = f.lengthSync();
      final side = File(f.path.replaceAll('.bin', '.json'));
      RawSession? s;
      if (side.existsSync()) {
        try {
          s = RawSession.fromJson(id, f, bytes, jsonDecode(side.readAsStringSync()) as Map<String, dynamic>);
        } on Object {
          s = null;
        }
      }
      if (s == null || id == openId) {
        final stats = await compute(RawFile.stats, f.readAsBytesSync());
        s = RawSession.fromStats(id, f, bytes, stats, demo: s?.demo ?? false, open: id == openId);
        if (id != openId) side.writeAsStringSync(statsJson(stats, demo: s.demo));
      }
      out.add(s);
    }
    out.sort((a, b) => (b.startMs ?? 0).compareTo(a.startMs ?? 0));
    return out;
  }

  Future<void> delete(RawSession s) async {
    for (final f in [s.file, File(s.file.path.replaceAll('.bin', '.json'))]) {
      if (f.existsSync()) await f.delete();
    }
  }

  /// Deletes every recording. [keep] names a session still being written, which stays.
  Future<void> deleteAll({String? keep}) async {
    if (!dir.existsSync()) return;
    for (final f in dir.listSync().whereType<File>()) {
      if (keep != null && f.uri.pathSegments.last.startsWith(keep)) continue;
      await f.delete();
    }
  }

  /// Starts a session file; [nowMs] names it.
  RawSessionWriter open(int nowMs, {bool demo = false}) {
    dir.createSync(recursive: true);
    return RawSessionWriter._(this, 'raw-$nowMs', demo: demo);
  }
}

/// Appends records to one session file and keeps its summary. Writes are buffered and flushed every [flushBytes] or
/// on [flush]; the summary is saved on [flush] and [close].
class RawSessionWriter {
  RawSessionWriter._(this.store, this.id, {required this.demo}) : file = File('${store.dir.path}${Platform.pathSeparator}$id.bin') {
    _sink = file.openSync(mode: FileMode.append)..writeFromSync(RawFile.magic);
    _written = RawFile.magic.length;
    _usedAtStart = store.usedBytes();
  }

  final RawStore store;
  final String id;
  final File file;
  final bool demo;
  final stats = SessionStats();
  late final RandomAccessFile _sink;
  final _buffer = BytesBuilder(copy: false);
  int _written = 0;
  int _usedAtStart = 0;
  bool _closed = false;

  static const int flushBytes = 64 * 1024;

  int get bytes => _written + _buffer.length;
  bool get closed => _closed;

  /// Adds one notification. Throws [RawCapReached] (and writes nothing) when it would pass the store's limit.
  void add(int char, Uint8List value, int rxMs) {
    if (_closed) return;
    final rec = RawFile.record(char, value, rxMs);
    if (_usedAtStart + _buffer.length + rec.length > store.capBytes) throw RawCapReached(store.capBytes);
    _buffer.add(rec);
    stats.add(char, value, rxMs);
    if (_buffer.length >= flushBytes) _flushBytes();
  }

  void _flushBytes() {
    if (_buffer.isEmpty) return;
    final b = _buffer.takeBytes();
    _sink.writeFromSync(b);
    _written += b.length;
    _usedAtStart += b.length;
  }

  void flush() {
    if (_closed) return;
    _flushBytes();
    _sink.flushSync();
    _saveSummary();
  }

  void _saveSummary() => File('${store.dir.path}${Platform.pathSeparator}$id.json').writeAsStringSync(statsJson(stats, demo: demo));

  RawSession get session => RawSession.fromStats(id, file, bytes, stats, demo: demo, open: !_closed);

  /// Ends the session. A session with no packets leaves no files.
  void close() {
    if (_closed) return;
    _flushBytes();
    _sink.closeSync();
    _closed = true;
    if (stats.packets == 0) {
      if (file.existsSync()) file.deleteSync();
      return;
    }
    _saveSummary();
  }
}
