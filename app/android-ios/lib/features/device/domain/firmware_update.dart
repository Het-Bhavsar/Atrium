// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Pod software update (#21) as a state machine, independent of Flutter and of the BLE stack:
///
/// idle -> checking -> available | upToDate
/// available -> downloading -> handingOver -> uploading -> restarting -> verifying -> done
/// any working step -> failed; downloading or uploading -> cancelled; done/failed/cancelled/upToDate -> checking
///
/// Success is only claimed after the pod has restarted, reconnected and reported the new version.
library;

import 'dart:async';

import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:flutter/foundation.dart';

enum OtaPhase { idle, checking, available, upToDate, downloading, handingOver, uploading, restarting, verifying, done, failed, cancelled }

/// Allowed moves; [FirmwareUpdateFlow] asserts every step against it.
const Map<OtaPhase, Set<OtaPhase>> otaTransitions = {
  OtaPhase.idle: {OtaPhase.checking},
  OtaPhase.checking: {OtaPhase.available, OtaPhase.upToDate, OtaPhase.failed},
  OtaPhase.available: {OtaPhase.downloading, OtaPhase.checking},
  OtaPhase.upToDate: {OtaPhase.checking},
  OtaPhase.downloading: {OtaPhase.handingOver, OtaPhase.failed, OtaPhase.cancelled},
  OtaPhase.handingOver: {OtaPhase.uploading, OtaPhase.failed},
  OtaPhase.uploading: {OtaPhase.restarting, OtaPhase.failed, OtaPhase.cancelled},
  OtaPhase.restarting: {OtaPhase.verifying, OtaPhase.failed},
  OtaPhase.verifying: {OtaPhase.done, OtaPhase.failed},
  OtaPhase.done: {OtaPhase.checking},
  OtaPhase.failed: {OtaPhase.checking},
  OtaPhase.cancelled: {OtaPhase.checking},
};

@immutable
class OtaState {
  const OtaState({this.phase = OtaPhase.idle, this.current, this.release, this.progress = 0, this.message});
  final OtaPhase phase;

  /// The version the pod last reported (DIS 0x2A26), or null when unknown.
  final PodVersion? current;
  final FirmwareRelease? release;

  /// 0-1 within the current step (download, then upload).
  final double progress;

  /// What went wrong, or why the update can't start yet.
  final String? message;

  static const _working = {OtaPhase.checking, OtaPhase.downloading, OtaPhase.handingOver, OtaPhase.uploading, OtaPhase.restarting, OtaPhase.verifying};

  /// True from the download to the check after the restart: the pod link belongs to the update.
  bool get busy => _working.contains(phase) && phase != OtaPhase.checking;
  bool get working => _working.contains(phase);
  bool get canCancel => phase == OtaPhase.downloading || phase == OtaPhase.uploading;

  OtaState copyWith({OtaPhase? phase, PodVersion? current, FirmwareRelease? release, double? progress, String? message}) => OtaState(
    phase: phase ?? this.phase,
    current: current ?? this.current,
    release: release ?? this.release,
    progress: progress ?? this.progress,
    message: message,
  );
}

/// The user stopped the update.
class OtaCancelled implements Exception {
  const OtaCancelled();
}

/// A step failed; [message] is shown as is.
class OtaFailure implements Exception {
  const OtaFailure(this.message);
  final String message;
  @override
  String toString() => message;
}

/// Where releases come from (GitHub in the app).
abstract interface class FirmwareSource {
  /// The latest release with a pod image, or null when there is none.
  Future<FirmwareRelease?> latest();

  /// The image bytes. Throws [OtaCancelled] once [isCancelled] turns true.
  Future<Uint8List> download(FirmwareRelease r, {required void Function(int got, int total) onProgress, required bool Function() isCancelled});
}

/// Uploads over MCUmgr SMP (mcumgr_flutter in the app).
abstract interface class DfuTransport {
  /// Uploads [image] to [deviceId], marks it for test and resets the pod. Completes once the pod has been told to
  /// reset. Throws [OtaCancelled] after [cancel], or [OtaFailure].
  Future<void> upload(String deviceId, Uint8List image, {required void Function(int sent, int total) onProgress, void Function()? onReset});
  Future<void> cancel();
}

/// The app's own pod link, which has to step aside while MCUmgr uses its BLE stack.
abstract interface class PodHandover {
  /// The pod's version as it reports it now, or null when it can't be read.
  Future<PodVersion?> readVersion();

  /// Why an update can't start now (battery, connection), or null.
  String? blocker();

  /// Stops syncing, lets the link go and returns the pod's platform id for MCUmgr.
  Future<String> release();

  /// Takes the link back (reconnects, resumes syncing) and returns the version the pod then reports; null when the
  /// pod wasn't reached within [timeout].
  Future<PodVersion?> reclaim(Duration timeout);
}

class FirmwareUpdateFlow {
  FirmwareUpdateFlow({
    required this.source,
    required this.dfu,
    required this.pod,
    this.onChange,
    this.swapWait = const Duration(seconds: 10),
    this.reconnectTimeout = const Duration(minutes: 3),
    Future<void> Function(Duration)? wait,
  }) : _wait = wait ?? Future<void>.delayed;

  final FirmwareSource source;
  final DfuTransport dfu;
  final PodHandover pod;
  final void Function(OtaState)? onChange;

  /// After the reset, MCUboot swaps the slots before the new image advertises; reconnecting earlier could still
  /// reach the old image [ASSUMED: about 10 s for a 220 KB swap on the nRF52832].
  final Duration swapWait;
  final Duration reconnectTimeout;
  final Future<void> Function(Duration) _wait;

  OtaState _s = const OtaState();
  OtaState get state => _s;
  bool _cancel = false;

  /// Phases the flow went through, in order (tests read it).
  final List<OtaPhase> history = [];

  void _set(OtaState s) {
    if (s.phase != _s.phase) {
      assert(otaTransitions[_s.phase]!.contains(s.phase), 'OTA: ${_s.phase} -> ${s.phase} is not allowed');
      history.add(s.phase);
    }
    _s = s;
    onChange?.call(s);
  }

  void _go(OtaPhase p, {double progress = 0, String? message, PodVersion? current}) =>
      _set(_s.copyWith(phase: p, progress: progress, message: message, current: current));

  /// Reads the pod's version and the latest release.
  Future<void> check() async {
    if (_s.working) return;
    _go(OtaPhase.checking);
    PodVersion? current;
    try {
      current = await pod.readVersion();
    } on Object {
      current = null;
    }
    final FirmwareRelease? r;
    try {
      r = await source.latest();
    } on OtaFailure catch (e) {
      _go(OtaPhase.failed, message: e.message, current: current);
      return;
    } on Object {
      _go(OtaPhase.failed, message: "Couldn't check for pod software. Check your internet connection and try again.", current: current);
      return;
    }
    if (r == null) {
      _go(OtaPhase.failed, message: 'No pod software has been published yet.', current: current);
      return;
    }
    _set(OtaState(phase: OtaPhase.checking, current: current ?? _s.current, release: r));
    _go(current != null && !(r.version > current) ? OtaPhase.upToDate : OtaPhase.available);
  }

  /// Downloads, hands the link over, uploads, waits for the restart and checks the version. Refuses (and stays
  /// available, with the reason in [OtaState.message]) when [PodHandover.blocker] says no.
  Future<void> start() async {
    if (_s.phase != OtaPhase.available) return;
    final r = _s.release!;
    final from = _s.current;
    final why = pod.blocker();
    if (why != null) {
      _set(_s.copyWith(message: why));
      return;
    }
    _cancel = false;
    var released = false;
    try {
      _go(OtaPhase.downloading);
      final bytes = await source.download(
        r,
        onProgress: (got, total) => _set(_s.copyWith(progress: total > 0 ? got / total : 0)),
        isCancelled: () => _cancel,
      );
      if (_cancel) throw const OtaCancelled();
      final McubootImage img;
      try {
        img = McubootImage.parse(bytes);
      } on FormatException catch (e) {
        throw OtaFailure(e.message);
      }
      if (img.version != r.version) throw OtaFailure('The downloaded file is version ${img.version}, not ${r.version}. Nothing was sent to the pod.');

      _go(OtaPhase.handingOver);
      // Counted as released before the call, so a hand-over that fails half-way still gets the link back.
      released = true;
      final id = await pod.release();

      _go(OtaPhase.uploading);
      await dfu.upload(
        id,
        bytes,
        onProgress: (sent, total) {
          if (_s.phase == OtaPhase.uploading) _set(_s.copyWith(progress: total > 0 ? sent / total : 0));
        },
        onReset: () {
          if (_s.phase == OtaPhase.uploading) _go(OtaPhase.restarting, progress: 1);
        },
      );
      if (_s.phase == OtaPhase.uploading) _go(OtaPhase.restarting, progress: 1);

      await _wait(swapWait);
      _go(OtaPhase.verifying, progress: 1);
      final now = await pod.reclaim(reconnectTimeout);
      released = false;
      if (now == null) {
        _go(
          OtaPhase.failed,
          message:
              "Couldn't reach the pod after it restarted, so the update isn't confirmed. Keep it close and open Device. If the new software doesn't start properly, the pod goes back to ${from ?? 'its previous software'} by itself.",
        );
      } else if (now == r.version) {
        _go(OtaPhase.done, progress: 1, current: now);
      } else {
        _go(OtaPhase.failed, current: now, message: "The pod restarted on $now, not ${r.version}, so the update didn't take. Nothing else changed.");
      }
    } on OtaCancelled {
      final now = released ? await _reclaimQuietly() : from;
      _go(OtaPhase.cancelled, current: now, message: 'Update stopped. The pod is still on ${now ?? from ?? 'its current software'}.');
    } on Object catch (e) {
      final now = released ? await _reclaimQuietly() : from;
      final why = e is OtaFailure ? e.message : "The update didn't finish.";
      final still = now == null ? '' : ' The pod is on $now.';
      _go(OtaPhase.failed, current: now, message: '$why$still');
    }
  }

  Future<PodVersion?> _reclaimQuietly() async {
    try {
      return await pod.reclaim(reconnectTimeout);
    } on Object {
      return null;
    }
  }

  /// Stops a download or an upload. After the reset there is nothing left to stop.
  Future<void> cancel() async {
    if (!_s.canCancel) return;
    _cancel = true;
    if (_s.phase == OtaPhase.uploading) await dfu.cancel();
  }
}
