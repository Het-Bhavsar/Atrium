// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// The pod connection as one state machine, independent of Flutter and of the BLE stack. Pure Dart.
///
/// noPod -> connecting -> connected
/// connected -> waiting (link dropped) -> connecting (the next try, after [reconnectDelay])
/// any -> needsPermission | bluetoothOff, and back to waiting when that is fixed
/// any but noPod -> handedOver (pod software update owns the link) -> connecting (taken back)
/// any -> noPod (logged out)
///
/// While handed over, native link events (the drop when MCUmgr takes over, its own connects) change nothing: only the
/// update hands the link back.
library;

enum LinkPhase {
  /// No pod paired with this phone.
  noPod,

  /// Bluetooth permission not given (or taken back in Settings).
  needsPermission,

  /// Bluetooth is off.
  bluetoothOff,

  /// A pod is paired but not connected; a reconnect is scheduled (or the pod is out of reach).
  waiting,
  connecting,
  connected,

  /// A pod software update owns the link: no reconnects, syncs or writes from the app.
  handedOver,
}

enum LinkEvent {
  permissionDenied,
  permissionGranted,
  bluetoothOff,
  bluetoothOn,

  /// A connect started (pairing, reconnect, or after an update).
  connecting,
  connected,

  /// The link dropped, or a connect failed.
  dropped,
  handOver,
  takeBack,

  /// Logged out, or no bonded pod found.
  forgotten,
}

/// The next phase. Unknown moves keep the phase (and are reported by [allowed]).
LinkPhase nextLink(LinkPhase p, LinkEvent e) {
  if (e == LinkEvent.forgotten) return LinkPhase.noPod;
  if (p == LinkPhase.handedOver) return e == LinkEvent.takeBack ? LinkPhase.connecting : p;
  return switch (e) {
    LinkEvent.permissionDenied => LinkPhase.needsPermission,
    LinkEvent.permissionGranted => p == LinkPhase.needsPermission ? LinkPhase.waiting : p,
    LinkEvent.bluetoothOff => LinkPhase.bluetoothOff,
    LinkEvent.bluetoothOn => p == LinkPhase.bluetoothOff ? LinkPhase.waiting : p,
    LinkEvent.connecting => p == LinkPhase.needsPermission || p == LinkPhase.bluetoothOff ? p : LinkPhase.connecting,
    LinkEvent.connected => LinkPhase.connected,
    LinkEvent.dropped => p == LinkPhase.noPod || p == LinkPhase.needsPermission || p == LinkPhase.bluetoothOff ? p : LinkPhase.waiting,
    LinkEvent.handOver => p == LinkPhase.noPod ? p : LinkPhase.handedOver,
    LinkEvent.takeBack => p,
    LinkEvent.forgotten => LinkPhase.noPod,
  };
}

/// Whether [e] moves [p] somewhere (tests and debug asserts).
bool allowed(LinkPhase p, LinkEvent e) => nextLink(p, e) != p;

/// Wait before reconnect attempt [attempt] (0 = the first after a drop): quick at first, since most drops are a pod
/// lifted off the Plinth or a phone pocketed for a moment, then backing off to spare both batteries. The bonded pod
/// is also reconnected the moment the app comes to the front.
Duration reconnectDelay(int attempt) {
  const steps = [1, 2, 5, 10, 30, 60, 120];
  return Duration(seconds: steps[attempt.clamp(0, steps.length - 1)]);
}

/// True when the app may run its own GATT traffic (sync, writes).
bool linkUsable(LinkPhase p) => p == LinkPhase.connected;

/// True when the app should keep trying to reach the pod.
bool shouldReconnect(LinkPhase p) => p == LinkPhase.waiting;
