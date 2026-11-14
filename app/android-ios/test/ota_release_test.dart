// Copyright (c) 2026 Het Bhavsar
// SPDX-License-Identifier: Apache-2.0

import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:atrium/features/device/domain/firmware_image.dart';
import 'package:flutter_test/flutter_test.dart';

/// The pod software update against the real release: GitHub's `releases/latest` reply for v0.1.3-evt (trimmed, in
/// test/fixtures/github) and, when `ATRIUM_POD_IMAGE` names a local copy of its `atrium-pod-0.1.1.bin`, the image
/// itself (not stored here: it is the release's own file).
void main() {
  final json = jsonDecode(File('test/fixtures/github/releases_latest_v0.1.3-evt.json').readAsStringSync()) as Map<String, dynamic>;

  test('releases/latest v0.1.3-evt: the pod image is picked over the APKs and the hex, with its digest', () {
    final r = FirmwareRelease.fromGitHub(json)!;
    expect(r.assetName, 'atrium-pod-0.1.1.bin');
    expect(r.version, const PodVersion(0, 1, 1));
    expect(r.tag, 'v0.1.3-evt');
    expect(r.size, 213968);
    expect(r.size, lessThanOrEqualTo(maxImageBytes), reason: "fits the pod's image slot");
    expect(r.sha256, '864a3418d642a9a03e3b2fa40d28a82a6d7a4873814be120c4cf5370b7b7f7dd');
    expect(r.url.scheme, 'https');
    expect(json['prerelease'], isFalse, reason: 'releases/latest skips pre-releases');
  });

  test('version compare: a pod on 0.1.0 is offered 0.1.1; 0.1.1 and newer are up to date', () {
    final r = FirmwareRelease.fromGitHub(json)!;
    expect(r.version > PodVersion.tryParse('0.1.0+41f1d65')!, isTrue);
    expect(r.version > PodVersion.tryParse('0.1.1+9ad053c')!, isFalse);
    expect(r.version > PodVersion.tryParse('0.2.0')!, isFalse);
  });

  final path = Platform.environment['ATRIUM_POD_IMAGE'];
  test('the released image parses: MCUboot header, version 0.1.1, its own SHA-256 TLV, and the GitHub digest', () {
    final bytes = Uint8List.fromList(File(path!).readAsBytesSync());
    final img = McubootImage.parse(bytes);
    expect(img.version, const PodVersion(0, 1, 1));
    expect(hexOf(sha256Of(bytes)), FirmwareRelease.fromGitHub(json)!.sha256);
    expect(img.sha256, hasLength(32));
  }, skip: path == null ? 'Set ATRIUM_POD_IMAGE to a local atrium-pod-0.1.1.bin to check the real image' : false);
}
