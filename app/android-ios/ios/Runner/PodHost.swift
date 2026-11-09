// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

// Native pod link for iOS: CoreBluetooth behind the Pigeon PodHostApi (pigeons/pod.dart).
// State restoration lets iOS relaunch the app for BLE events, so the log can sync in the background (#20).
// GATT layout: docs/ble_gatt.md rev 13. Custom UUIDs share the base 8f2aXXXX-4c46-4154-5249-554d2e706f64.
import CoreBluetooth
import Flutter
import Foundation

final class PodHost: NSObject, PodHostApi, CBCentralManagerDelegate, CBPeripheralDelegate {
  private let events: PodFlutterApi
  /// Created on first use, so the Bluetooth prompt comes at the onboarding step that explains it, not at launch.
  /// With a pod already paired it starts at launch, which state restoration needs to wake the app for the pod.
  private var central: CBCentralManager?
  private var authWaiters: [CheckedContinuation<Bool, Never>] = []
  private var peripheral: CBPeripheral?
  private var chars: [UInt16: CBCharacteristic] = [:]
  private var found: [UUID: CBPeripheral] = [:]
  private var connectWaiter: CheckedContinuation<Void, Error>?
  private var readWaiters: [UInt16: [CheckedContinuation<Data, Error>]] = [:]
  private var writeWaiters: [UInt16: CheckedContinuation<Void, Error>] = [:]
  private var syncWaiter: CheckedContinuation<Void, Error>?
  private var syncFirstPage: UInt32 = 0
  private var syncPages: [UInt32: Data] = [:]
  private var syncComplete = Set<UInt32>()
  private var syncTimeout: DispatchWorkItem?
  /// The pod ends the stream early if its ring drops the oldest page mid-sync: after this long with no
  /// 0x0502/0x0503 traffic the app keeps the pages that arrived.
  static let syncIdleSeconds = 10.0
  private var syncExpected = 0
  private var wantScan = false

  static let restoreId = "open.atrium.pod"
  static let powerService = CBUUID(string: PodHost.uuidString(0x0100))
  private static let bondKey = "atrium.bondedPod"
  private static let pageSize = 4096

  init(messenger: FlutterBinaryMessenger) {
    events = PodFlutterApi(binaryMessenger: messenger)
    super.init()
    if UserDefaults.standard.string(forKey: PodHost.bondKey) != nil { _ = ensureCentral() }
  }

  @discardableResult
  private func ensureCentral() -> CBCentralManager {
    if let c = central { return c }
    let c = CBCentralManager(delegate: self, queue: nil, options: [CBCentralManagerOptionRestoreIdentifierKey: PodHost.restoreId])
    central = c
    return c
  }

  /// True when Atrium may use Bluetooth.
  static var bluetoothAllowed: Bool { CBManager.authorization == .allowedAlways }

  /// Starts Bluetooth (the system asks the first time) and waits for the answer.
  func requestBluetooth() async -> Bool {
    switch CBManager.authorization {
    case .allowedAlways: _ = ensureCentral(); return true
    case .denied, .restricted: return false
    default: break
    }
    return await withCheckedContinuation { (k: CheckedContinuation<Bool, Never>) in
      authWaiters.append(k)
      ensureCentral()
    }
  }

  static func uuidString(_ id: UInt16) -> String { String(format: "8F2A%04X-4C46-4154-5249-554D2E706F64", id) }

  /// 16-bit id for a characteristic: SIG ids (0x2Axx) as is, custom ones from the shared base.
  private static func shortId(_ uuid: CBUUID) -> UInt16? {
    let s = uuid.uuidString.uppercased()
    if s.count == 4 { return UInt16(s, radix: 16) }
    if s.hasPrefix("8F2A"), s.hasSuffix("-4C46-4154-5249-554D2E706F64") {
      return UInt16(String(s.dropFirst(4).prefix(4)), radix: 16)
    }
    return nil
  }

  private static func error(_ code: String, _ message: String) -> PigeonError {
    PigeonError(code: code, message: message, details: nil)
  }

  private func send(_ body: @escaping @MainActor (PodFlutterApi) async throws -> Void) {
    let api = events
    Task { @MainActor in try? await body(api) }
  }

  private func emit(_ state: LinkState) { send { try await $0.onLinkState(state: state) } }

  // MARK: PodHostApi

  func startScan() throws {
    wantScan = true
    let central = ensureCentral()
    guard central.state == .poweredOn else { return }
    emit(.scanning)
    central.scanForPeripherals(withServices: [PodHost.powerService], options: nil)
  }

  func stopScan() throws {
    wantScan = false
    central?.stopScan()
  }

  func connect(id: String) async throws {
    let central = ensureCentral()
    guard let uuid = UUID(uuidString: id),
          let p = found[uuid] ?? central.retrievePeripherals(withIdentifiers: [uuid]).first else {
      throw PodHost.error("not_found", "Pod \(id) is not nearby")
    }
    central.stopScan()
    peripheral = p
    p.delegate = self
    emit(.connecting)
    try await withCheckedThrowingContinuation { (c: CheckedContinuation<Void, Error>) in
      connectWaiter = c
      central.connect(p, options: nil)
    }
  }

  /// Also the hand-over to MCUmgr for a pod software update (#21): MCUmgr opens its own CBCentralManager to the same
  /// peripheral, so this one lets go first. Cancelling a connect that is still pending fails its waiter here, since
  /// CoreBluetooth may not call back for it.
  func disconnect() throws {
    if let p = peripheral { central?.cancelPeripheralConnection(p) }
    if let k = connectWaiter {
      connectWaiter = nil
      k.resume(throwing: PodHost.error("cancelled", "Connect cancelled"))
      emit(.disconnected)
    }
  }

  func bondedPodId() throws -> String? { UserDefaults.standard.string(forKey: PodHost.bondKey) }

  /// Log out: drop the link and the stored pod so nothing reconnects. The system pairing stays until the user
  /// forgets "Atrium" in Settings > Bluetooth; the pod replaces it at the next pairing in the lift window.
  func forgetPod() throws {
    if let p = peripheral { central?.cancelPeripheralConnection(p) }
    peripheral = nil
    chars = [:]
    let d = UserDefaults.standard
    d.removeObject(forKey: PodHost.bondKey)
    d.removeObject(forKey: "atrium.backgroundSync")
    d.removeObject(forKey: "atrium.backgroundSyncHours")
  }

  func read(characteristic: Int64) async throws -> FlutterStandardTypedData {
    let id = UInt16(characteristic)
    guard let p = peripheral, let c = chars[id] else { throw PodHost.error("no_char", String(format: "0x%04X not available", id)) }
    let data = try await withCheckedThrowingContinuation { (k: CheckedContinuation<Data, Error>) in
      readWaiters[id, default: []].append(k)
      p.readValue(for: c)
    }
    return FlutterStandardTypedData(bytes: data)
  }

  func write(characteristic: Int64, value: FlutterStandardTypedData, withResponse: Bool) async throws {
    let id = UInt16(characteristic)
    guard let p = peripheral, let c = chars[id] else { throw PodHost.error("no_char", String(format: "0x%04X not available", id)) }
    if !withResponse {
      p.writeValue(value.data, for: c, type: .withoutResponse)
      return
    }
    try await withCheckedThrowingContinuation { (k: CheckedContinuation<Void, Error>) in
      writeWaiters[id] = k
      p.writeValue(value.data, for: c, type: .withResponse)
    }
  }

  func setNotify(characteristic: Int64, enabled: Bool) throws {
    let id = UInt16(characteristic)
    if let p = peripheral, let c = chars[id] { p.setNotifyValue(enabled, for: c) }
  }

  /// Log Control 0x0502 op 0x01 (read from page, count 0 = to the head); pages stream on Log Data 0x0503.
  func syncLog(fromPage: Int64) async throws {
    guard let p = peripheral, let ctl = chars[0x0502], let data = chars[0x0503] else {
      throw PodHost.error("not_connected", "Pod not connected")
    }
    syncFirstPage = UInt32(fromPage)
    syncPages = [:]
    syncComplete = []
    syncExpected = 0
    p.setNotifyValue(true, for: ctl)
    p.setNotifyValue(true, for: data)
    var req = Data([0x01])
    withUnsafeBytes(of: UInt32(fromPage).littleEndian) { req.append(contentsOf: $0) }
    withUnsafeBytes(of: UInt32(0).littleEndian) { req.append(contentsOf: $0) }
    try await withCheckedThrowingContinuation { (k: CheckedContinuation<Void, Error>) in
      syncWaiter = k
      armSyncTimeout()
      p.writeValue(req, for: ctl, type: .withResponse)
    }
  }

  func setBackgroundSync(enabled: Bool, maxHours: Int64) throws {
    // State restoration plus the bluetooth-central background mode keep the bonded pod reachable; the app
    // reconnects and syncs whenever iOS wakes it. A BGAppRefreshTask hook is the next step.
    UserDefaults.standard.set(enabled, forKey: "atrium.backgroundSync")
    UserDefaults.standard.set(maxHours, forKey: "atrium.backgroundSyncHours")
  }

  // MARK: CBCentralManagerDelegate

  func centralManagerDidUpdateState(_ c: CBCentralManager) {
    if c.state != .unknown, c.state != .resetting, !authWaiters.isEmpty {
      let allowed = PodHost.bluetoothAllowed
      authWaiters.forEach { $0.resume(returning: allowed) }
      authWaiters.removeAll()
    }
    switch c.state {
    case .poweredOn:
      if wantScan { try? startScan() } else { emit(.idle) }
    case .poweredOff: emit(.bluetoothOff)
    case .unauthorized: emit(.unauthorized)
    default: emit(.idle)
    }
  }

  func centralManager(_ c: CBCentralManager, willRestoreState dict: [String: Any]) {
    if let ps = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral], let p = ps.first {
      peripheral = p
      p.delegate = self
    }
  }

  func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi: NSNumber) {
    found[p.identifier] = p
    let name = (advertisementData[CBAdvertisementDataLocalNameKey] as? String) ?? p.name ?? "Atrium"
    let pod = FoundPod(id: p.identifier.uuidString, name: name, rssi: rssi.int64Value)
    send { try await $0.onFound(pod: pod) }
  }

  func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) {
    p.discoverServices(nil)
  }

  func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
    emit(.disconnected)
    connectWaiter?.resume(throwing: PodHost.error("connect_failed", error?.localizedDescription ?? "Could not connect"))
    connectWaiter = nil
  }

  func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
    chars = [:]
    // Nothing answers on a dropped link: fail what is waiting, so Dart can reconnect instead of hanging.
    let gone = PodHost.error("not_connected", "Pod disconnected")
    connectWaiter?.resume(throwing: gone)
    connectWaiter = nil
    for waiting in readWaiters.values {
      for k in waiting { k.resume(throwing: gone) }
    }
    readWaiters = [:]
    for k in writeWaiters.values { k.resume(throwing: gone) }
    writeWaiters = [:]
    emit(.disconnected)
  }

  // MARK: CBPeripheralDelegate

  func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
    for s in p.services ?? [] { p.discoverCharacteristics(nil, for: s) }
  }

  func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
    for c in s.characteristics ?? [] {
      if let id = PodHost.shortId(c.uuid) { chars[id] = c }
    }
    if chars[0x0101] != nil, let k = connectWaiter {
      UserDefaults.standard.set(p.identifier.uuidString, forKey: PodHost.bondKey)
      emit(.connected)
      connectWaiter = nil
      k.resume()
    }
  }

  func peripheral(_ p: CBPeripheral, didUpdateValueFor c: CBCharacteristic, error: Error?) {
    guard let id = PodHost.shortId(c.uuid) else { return }
    let value = c.value ?? Data()
    if var waiting = readWaiters[id], !waiting.isEmpty {
      let k = waiting.removeFirst()
      readWaiters[id] = waiting
      if let error { k.resume(throwing: error) } else { k.resume(returning: value) }
      return
    }
    if id == 0x0502 { handleLogControl(value); return }
    if id == 0x0503 { handleLogData(value); return }
    let bytes = FlutterStandardTypedData(bytes: value)
    send { try await $0.onNotify(characteristic: Int64(id), value: bytes) }
  }

  func peripheral(_ p: CBPeripheral, didWriteValueFor c: CBCharacteristic, error: Error?) {
    guard let id = PodHost.shortId(c.uuid), let k = writeWaiters.removeValue(forKey: id) else { return }
    if let error { k.resume(throwing: error) } else { k.resume() }
  }

  // MARK: log download

  private func le32(_ d: Data, _ at: Int) -> UInt32 {
    UInt32(d[d.startIndex + at]) | UInt32(d[d.startIndex + at + 1]) << 8 | UInt32(d[d.startIndex + at + 2]) << 16 | UInt32(d[d.startIndex + at + 3]) << 24
  }

  private func handleLogControl(_ v: Data) {
    // Rev 13 end of read: [0x90, reason, u32 whole pages sent], after the last Log Data packet. Reason 1 = the page
    // being sent left the ring. Finish now instead of waiting out the idle timeout (kept for disconnects and older
    // firmware); finishSync hands Dart only the complete pages either way.
    if v.count >= 6, v[v.startIndex] == 0x90 { finishSync(); return }
    guard v.count >= 6, v[v.startIndex] == 0x81 else { return }
    armSyncTimeout()
    let status = v[v.startIndex + 1]
    if status != 0 {
      syncTimeout?.cancel()
      syncWaiter?.resume(throwing: PodHost.error("log_status_\(status)", "Log read refused"))
      syncWaiter = nil
      return
    }
    syncExpected = Int(le32(v, 2))
    if syncExpected == 0 { finishSync() }
  }

  private func handleLogData(_ v: Data) {
    guard v.count > 6, syncWaiter != nil else { return }
    armSyncTimeout()
    let page = le32(v, 0)
    let offset = Int(UInt16(v[v.startIndex + 4]) | UInt16(v[v.startIndex + 5]) << 8)
    var buf = syncPages[page] ?? Data(repeating: 0xFF, count: PodHost.pageSize)
    let chunk = v.subdata(in: (v.startIndex + 6)..<v.endIndex)
    if offset + chunk.count <= PodHost.pageSize { buf.replaceSubrange(offset..<(offset + chunk.count), with: chunk) }
    syncPages[page] = buf
    if offset + chunk.count >= PodHost.pageSize {
      syncComplete.insert(page)
      let progress = SyncProgress(pagesDone: Int64(syncComplete.count), pagesTotal: Int64(syncExpected))
      send { try await $0.onSyncProgress(progress: progress) }
      if syncExpected > 0, syncComplete.count >= syncExpected { finishSync() }
    }
  }

  private func armSyncTimeout() {
    syncTimeout?.cancel()
    let w = DispatchWorkItem { [weak self] in self?.finishSync() }
    syncTimeout = w
    DispatchQueue.main.asyncAfter(deadline: .now() + PodHost.syncIdleSeconds, execute: w)
  }

  /// Hands Dart the complete pages, in order, from the first requested page up to the first gap.
  private func finishSync() {
    syncTimeout?.cancel()
    syncTimeout = nil
    guard syncWaiter != nil else { return }
    var all = Data()
    var k = syncFirstPage
    while syncComplete.contains(k), let page = syncPages[k] {
      all.append(page)
      k &+= 1
    }
    let first = Int64(syncFirstPage)
    let bytes = FlutterStandardTypedData(bytes: all)
    send { try await $0.onLogPages(firstPage: first, pages: bytes) }
    syncWaiter?.resume()
    syncWaiter = nil
  }
}
