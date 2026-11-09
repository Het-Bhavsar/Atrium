// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

/// Pod software (#21): versions, the signed update image and the rules for starting an update. Pure Dart.
///
/// The pod runs MCUboot with two 220 KB slots; updates go over MCUmgr SMP (docs/ble_gatt.md section 9). The image
/// file is `zephyr.signed.bin`, published on each GitHub release as `atrium-pod-<version>.bin`.
library;

import 'dart:typed_data';

import 'package:crypto/crypto.dart';

/// MCUmgr SMP over BLE (docs/ble_gatt.md section 9). `mcumgr_flutter` finds these by itself; kept here as the contract.
abstract final class Smp {
  static const String service = '8d53dc1d-1db7-4cd3-868b-8a527460aa84';
  static const String characteristic = 'da2e7828-fbce-4e01-ae9e-261174997c48';
}

/// The largest image the pod's slot takes: 216.9 KB (docs/ble_gatt.md section 9; the linker region is 216938 B). A larger
/// file would be refused part-way through the upload, so it is refused here, before the link is handed over.
const int maxImageBytes = 216938;

/// Off the dock, the pod refuses an update below 3.60 V VSYS (ble_gatt.md section 9, firmware README).
const int updateMinVsysMv = 3600;

/// Where releases are published.
const String releaseRepo = 'Het-Bhavsar/Atrium';
const String latestReleaseUrl = 'https://api.github.com/repos/$releaseRepo/releases/latest';

/// A pod software version, major.minor.patch. MCUboot calls the third number the revision; its build number and the
/// DIS commit suffix (`0.1.0+41f1d65`) are not part of the comparison.
class PodVersion implements Comparable<PodVersion> {
  const PodVersion(this.major, this.minor, this.patch);
  final int major;
  final int minor;
  final int patch;

  static final _re = RegExp(r'(\d+)\.(\d+)\.(\d+)');

  /// Reads `0.1.0`, `v0.1.0`, `0.1.0+41f1d65`, `0.1.0-dirty` or MCUmgr's `0.1.0.0`; null when there is no version.
  static PodVersion? tryParse(String? s) {
    if (s == null) return null;
    final m = _re.firstMatch(s);
    if (m == null) return null;
    return PodVersion(int.parse(m[1]!), int.parse(m[2]!), int.parse(m[3]!));
  }

  @override
  int compareTo(PodVersion o) => major != o.major
      ? major.compareTo(o.major)
      : minor != o.minor
      ? minor.compareTo(o.minor)
      : patch.compareTo(o.patch);

  bool operator >(PodVersion o) => compareTo(o) > 0;

  @override
  bool operator ==(Object other) => other is PodVersion && compareTo(other) == 0;

  @override
  int get hashCode => Object.hash(major, minor, patch);

  @override
  String toString() => '$major.$minor.$patch';
}

/// The MCUboot image header (bootutil `image.h`), little-endian:
/// 0 u32 magic 0x96f3b83d, 4 u32 load address, 8 u16 header size, 10 u16 protected TLV size, 12 u32 image size,
/// 16 u32 flags, 20 u8 major, 21 u8 minor, 22 u16 revision, 24 u32 build number. The TLV areas follow the image: the
/// protected one first when there is one (info magic 0x6908, covered by the hash and the signature), then the
/// unprotected one (0x6907). Each starts with `u16 magic, u16 total size` and holds `u16 type, u16 length, data`
/// entries; type 0x10 is the SHA-256 of the header, the image and the protected TLVs.
class McubootImage {
  const McubootImage({required this.version, required this.build, required this.headerSize, required this.imageSize, required this.sha256});
  final PodVersion version;
  final int build;
  final int headerSize;
  final int imageSize;

  /// The image hash from its TLV, checked against the bytes. MCUmgr names the image by it (test, confirm, slot list).
  final Uint8List sha256;

  static const int magic = 0x96f3b83d;
  static const int tlvMagic = 0x6907;
  static const int protectedTlvMagic = 0x6908;
  static const int tlvSha256 = 0x10;

  /// Checks [b] is a whole MCUboot image that fits the pod's slot and that its SHA-256 matches its bytes; throws
  /// [FormatException] with a plain reason. The pod's bootloader still checks the signature; this catches a damaged
  /// or wrong file before the link is handed over.
  static McubootImage parse(Uint8List b) {
    if (b.length < 32) throw const FormatException('This is not pod software (too short).');
    if (b.length > maxImageBytes) throw const FormatException('This file is larger than the pod can take.');
    final d = ByteData.sublistView(b);
    if (d.getUint32(0, Endian.little) != magic) throw const FormatException('This is not a signed pod software image.');
    final hdr = d.getUint16(8, Endian.little);
    final protectedSize = d.getUint16(10, Endian.little);
    final size = d.getUint32(12, Endian.little);
    var tlv = hdr + size;
    if (hdr < 32 || tlv + 4 > b.length) throw const FormatException('The pod software file is incomplete.');
    final first = d.getUint16(tlv, Endian.little);
    if (first != tlvMagic && first != protectedTlvMagic) throw const FormatException('The pod software file has no signature block.');
    final hashed = tlv + (first == protectedTlvMagic ? protectedSize : 0);
    if (first == protectedTlvMagic) {
      tlv = hashed;
      if (tlv + 4 > b.length || d.getUint16(tlv, Endian.little) != tlvMagic) throw const FormatException('The pod software file has no signature block.');
    }
    final end = tlv + d.getUint16(tlv + 2, Endian.little);
    if (end > b.length) throw const FormatException('The pod software file is incomplete.');
    Uint8List? want;
    for (var o = tlv + 4; o + 4 <= end;) {
      final type = d.getUint16(o, Endian.little);
      final len = d.getUint16(o + 2, Endian.little);
      if (o + 4 + len > end) break;
      if (type == tlvSha256 && len == 32) want = Uint8List.fromList(b.sublist(o + 4, o + 36));
      o += 4 + len;
    }
    if (want == null) throw const FormatException('The pod software file has no image hash.');
    final got = sha256Of(Uint8List.sublistView(b, 0, hashed));
    if (!_same(got, want)) throw const FormatException('The pod software file is damaged (its hash does not match). Nothing was sent to the pod.');
    return McubootImage(
      version: PodVersion(d.getUint8(20), d.getUint8(21), d.getUint16(22, Endian.little)),
      build: d.getUint32(24, Endian.little),
      headerSize: hdr,
      imageSize: size,
      sha256: want,
    );
  }
}

/// SHA-256 of [b].
Uint8List sha256Of(List<int> b) => Uint8List.fromList(sha256.convert(b).bytes);

String hexOf(List<int> b) => [for (final x in b) x.toRadixString(16).padLeft(2, '0')].join();

bool _same(List<int> a, List<int> b) {
  if (a.length != b.length) return false;
  var diff = 0;
  for (var i = 0; i < a.length; i++) {
    diff |= a[i] ^ b[i];
  }
  return diff == 0;
}

/// One published pod software release: the `atrium-pod-<version>.bin` asset of a GitHub release.
class FirmwareRelease {
  const FirmwareRelease({
    required this.version,
    required this.tag,
    required this.assetName,
    required this.url,
    required this.size,
    this.notes = '',
    this.published,
    this.sha256,
  });
  final PodVersion version;
  final String tag;
  final String assetName;
  final Uri url;
  final int size;
  final String notes;
  final DateTime? published;

  /// The file's SHA-256 as GitHub publishes it (asset `digest`, `sha256:<hex>`), when there is one.
  final String? sha256;

  static final _asset = RegExp(r'^atrium-pod-(\d+\.\d+\.\d+)\.bin$');

  /// Picks the pod image from a GitHub `releases/latest` reply; the highest version if there are several. Null when
  /// the release has no pod image, or its link isn't https.
  static FirmwareRelease? fromGitHub(Map<String, dynamic> json) {
    FirmwareRelease? best;
    final assets = json['assets'];
    if (assets is! List) return null;
    for (final a in assets.whereType<Map<String, dynamic>>()) {
      final name = a['name'];
      final link = a['browser_download_url'];
      if (name is! String || link is! String) continue;
      final m = _asset.firstMatch(name);
      final url = Uri.tryParse(link);
      if (m == null || url == null || url.scheme != 'https') continue;
      final r = FirmwareRelease(
        version: PodVersion.tryParse(m[1])!,
        tag: '${json['tag_name'] ?? ''}',
        assetName: name,
        url: url,
        size: a['size'] is int ? a['size'] as int : 0,
        notes: (json['body'] is String ? json['body'] as String : '').trim(),
        published: DateTime.tryParse('${json['published_at'] ?? ''}'),
        sha256: _digest(a['digest']),
      );
      if (best == null || r.version > best.version) best = r;
    }
    return best;
  }

  static String? _digest(Object? d) {
    if (d is! String || !d.startsWith('sha256:')) return null;
    final h = d.substring(7).toLowerCase();
    return RegExp(r'^[0-9a-f]{64}$').hasMatch(h) ? h : null;
  }
}

/// Why an update can't start now, or null when it can. The pod refuses below 3.60 V off the dock; when the app has no
/// voltage reading, it asks for the dock instead of guessing.
String? updateBlocker({required bool connected, required bool docked, int? vsysMv, bool batteryLow = false}) {
  if (!connected) return 'Connect to the pod first: bring it close, then open Device.';
  if (docked) return null;
  if (vsysMv == null || vsysMv <= 0) {
    return "Put the pod on the Plinth to update. Atrium can't read its battery voltage right now, and the pod only updates above 3.60 V off the Plinth.";
  }
  if (batteryLow || vsysMv < updateMinVsysMv) return 'The battery is too low to update off the Plinth. Put the pod on the Plinth, then try again.';
  return null;
}
